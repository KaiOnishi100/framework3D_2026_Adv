#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"collisionManager.h"


//============================================================
// 目的:
//   明らかに離れているCollider同士を
//   本格的なCollisionCheckより前に除外する
//============================================================
namespace
{
    //========================================================
    // 静的World Collider判定
    //========================================================
    bool IsStaticWorldCollider(const Collider* collider)
    {
        return
            collider != nullptr && 
            collider->GetOwner() == nullptr &&
            collider->HasTag(ColliderTag::World);
    }

    //========================================================
    // 静的Boxから完全に離れているか判定
    // moving: プレイヤーや敵など、移動するCollider
    // world: MansionBlock等の静的Collider
    // true: 衝突する可能性がないためスキップ可能
    // false:
    //   衝突の可能性あり、または未対応
    //   → 従来のCollisionCheckを実行する
    //========================================================
    bool IsSeparatedFromStaticBox( const Collider* moving, const Collider* world)
    {
        if (moving == nullptr || world == nullptr)
        {
            return false;
        }

        //====================================================
        // 静的WorldがBoxでなければこの簡易判定は使用しない
        //====================================================
        const auto* box = dynamic_cast<const BoxCollider*>(world);

        if (box == nullptr)
        {
            return false;
        }

        XMVECTOR movingCenter = XMVectorZero();

        //====================================================
        // 移動側の外接サイズ
        // X/Z: 半径
        // Y: Capsuleの場合は中央線分の半高さ + 半径
        //====================================================
        float movingHalfX = 0.0f;
        float movingHalfY = 0.0f;
        float movingHalfZ = 0.0f;

        if (const auto* capsule = dynamic_cast<const CapsuleCollider*>(moving))
        {
            const float radius = capsule->GetRadius();

            movingCenter = capsule->GetCenter();

            movingHalfX = radius;
            movingHalfY = capsule->GetHalfSegmentHeight() + radius;
            movingHalfZ = radius;
        }
        else if (const auto* sphere = dynamic_cast<const SphereCollider*>(moving))
        {
            const float radius = sphere->GetRadius();

            movingCenter = sphere->GetCenter();

            movingHalfX = radius;
            movingHalfY = radius;
            movingHalfZ = radius;
        }
        else if (const auto* movingBox = dynamic_cast<const BoxCollider*>(moving))
        {
            //========================================================
            // Boxの現在位置
            // Ownerを持つBoxはOwner座標を取得する
            //========================================================
            movingCenter = movingBox->GetCenter();

            //========================================================
            // Boxローカル空間のHalf Extents
            //========================================================
            const XMVECTOR half = movingBox->GetHalfSize();

            const float hx = XMVectorGetX(half);
            const float hy = XMVectorGetY(half);
            const float hz = XMVectorGetZ(half);

            //========================================================
            // Y軸回転
            // 回転したOBBを完全に包み込むAABBの半サイズ:
            // AABB.X = |cos| * hx + |sin| * hz
            // AABB.Z = |sin| * hx + |cos| * hz
            //========================================================
            const float yaw = movingBox->GetYawRadians();

            const float c = std::fabs(std::cos(yaw));
            const float s = std::fabs(std::sin(yaw));

            movingHalfX = c * hx + s * hz;
            movingHalfY = hy;
            movingHalfZ = s * hx + c * hz;
        }
        else
        {
            //================================================
            // ※Door BoxやMeshなどには適用しない
            //================================================
            return false;
        }

        //====================================================
        // World Boxの中心・Half Size
        //====================================================
        const XMVECTOR worldCenter = box->GetCenter();
        const XMVECTOR half = box->GetHalfSize();

        float boxHalfX = XMVectorGetX(half);
        const float boxHalfY = XMVectorGetY(half);
        float boxHalfZ = XMVectorGetZ(half);

        //====================================================
        // World BoxがY軸回転している場合
        // 回転済みBoxを包み込めるAABBのX/Z半サイズを計算する
        //====================================================
        const float yaw = box->GetYawRadians();

        if (std::fabs(yaw) > 0.00001f)
        {
            const float c = std::fabs(std::cos(yaw));
            const float s = std::fabs(std::sin(yaw));

            const float hx = boxHalfX;
            const float hz = boxHalfZ;

            boxHalfX = c * hx + s * hz;
            boxHalfZ = s * hx + c * hz;
        }

        //====================================================
        // 中心間距離
        //====================================================
        const XMVECTOR diff = movingCenter - worldCenter;
        const float dx = std::fabs(XMVectorGetX(diff));
        const float dy = std::fabs(XMVectorGetY(diff));
        const float dz = std::fabs(XMVectorGetZ(diff));

        //====================================================
        // 誤差対策 境界ぎりぎりで判定を落とさないよう5cmの余裕を持たせる
        //====================================================
        constexpr float margin = 0.05f;

        //====================================================
        // どれか1軸でも十分離れていれば、3D空間上で絶対に接触しない
        //====================================================
        if (dx > boxHalfX + movingHalfX + margin)
        {
            return true;
        }

        if (dy > boxHalfY + movingHalfY + margin)
        {
            return true;
        }

        if (dz > boxHalfZ + movingHalfZ + margin)
        {
            return true;
        }

        //====================================================
        // 外接Boxが重なっている可能性がある
        //====================================================
        return false;
    }
}


bool CollisionManager::IsValidPair(const Collider* a,const Collider* b)
{
    //========================================================
    // nullptr確認
    //========================================================
    if (a == nullptr || b == nullptr)
    {
        return false;
    }

    //========================================================
    // 無効Colliderは処理しない
    //========================================================
    if (!a->IsEnabled() || !b->IsEnabled())
    {
        return false;
    }

    //========================================================
    // 同じColliderを重複処理しない
    //========================================================
    if (a == b)
    {
        return false;
    }

    //========================================================
    // 同じOwnerのCollider同士は判定しない
    // ただしnullptr同士はここでは除外しない
    //========================================================
    if (a->GetOwner() != nullptr && a->GetOwner() == b->GetOwner())
    {
        return false;
    }

    //========================================================
    // モデル非依存の静的World Collider同士
    // 両方ともOwnerが存在せず
    // World Tagが付いている場合は衝突判定不要
    //========================================================
    const bool aStaticWorld =
        a->GetOwner() == nullptr && a->HasTag(ColliderTag::World);

    const bool bStaticWorld =
        b->GetOwner() == nullptr && b->HasTag(ColliderTag::World);

    if (aStaticWorld && bStaticWorld)
    {
        return false;
    }

    return true;
}


//============================================================
// Character 取得をまとめたコンテキスト生成
//============================================================
CollisionManager::CollisionContext CollisionManager::BuildContext(Collider* a, Collider* b)
{
    CollisionContext ctx;
    ctx.a = a;
    ctx.b = b;
    ctx.charA = a ? a->GetOwnerCharacter() : nullptr;
    ctx.charB = b ? b->GetOwnerCharacter() : nullptr;
    return ctx;
}

//============================================================
// 種別判定補助
//============================================================
bool CollisionManager::IsPlayerCharacter(const Character* c)
{
    return dynamic_cast<const Player*>(c) != nullptr;
}

bool CollisionManager::IsBossCharacter(const Character* c)
{
    return dynamic_cast<const Boss*>(c) != nullptr;
}

//============================================================
// 攻撃ヒット処理
// ・Attack 側から Body 側へダメージを入れる
// ・必要ならプレイヤー反動も適用
// ・このフレームのキャラ押し出しはスキップ
//============================================================
void CollisionManager::DoAttackHit(
    Collider* atkCol,
    Collider* bodyCol,
    Character* atkCh,
    Character* bodyCh,
    SlowSystem* slow,
    bool& skipCharacterResolve)
{
    if (!atkCol || !bodyCol || !atkCh || !bodyCh)
        return;

    // 多段ヒット防止
    if (atkCh->HasHit())
        return;

    //--------------------------------------------------------
    // ダメージ
    //--------------------------------------------------------
    bodyCh->TakeDamage(atkCh->GetCurrentAttackInfo());
    atkCh->SetHasHit(true);

    //--------------------------------------------------------
    // プレイヤー攻撃時の反動
    //--------------------------------------------------------
    if (auto* pAtk = dynamic_cast<Player*>(atkCh))
    {
        if (auto* boss = dynamic_cast<Boss*>(bodyCh))
        {
            XMVECTOR playerPos = *pAtk->getPosition();
            XMVECTOR bossPos = *boss->getPosition();

            // ボス中心 → プレイヤー方向
            XMVECTOR pushDir = playerPos - bossPos;
            pushDir = XMVectorSetY(pushDir, 0.0f);

            if (XMVectorGetX(XMVector3LengthSq(pushDir)) > 0.0001f)
            {
                pushDir = XMVector3Normalize(pushDir);
            }
            else
            {
                // 完全に重なった場合は攻撃の逆方向に逃がす
                pushDir = -atkCh->GetCurrentAttackInfo().knockbackDirection;
                pushDir = XMVectorSetY(pushDir, 0.0f);

                if (XMVectorGetX(XMVector3LengthSq(pushDir)) > 0.0001f)
                    pushDir = XMVector3Normalize(pushDir);
                else
                    pushDir = XMVectorSet(0, 0, 1, 0);
            }

            // ボス表面の少し手前にスナップ
            float snapDist = boss->GetRadius() + pAtk->GetRadius() + 0.3f;
            XMVECTOR snapPos = bossPos + pushDir * snapDist;

            // 高さは維持
            snapPos = XMVectorSetY(snapPos, XMVectorGetY(playerPos));
            pAtk->setPosition(&snapPos);

            // その後に反動
            pAtk->AttackKnockback(pushDir, 5.0f);
        }
        else
        {
            XMVECTOR dir = -atkCh->GetCurrentAttackInfo().knockbackDirection;
            dir = XMVectorSetY(dir, 0.0f);

            if (XMVectorGetX(XMVector3LengthSq(dir)) > 0.0001f)
                dir = XMVector3Normalize(dir);
            else
                dir = XMVectorSet(0, 0, 1, 0);

            pAtk->AttackKnockback(dir, 5.0f);
        }

        pAtk->EndAttack();
    }

    //--------------------------------------------------------
    // Attack / Body で当たったフレームは
    // Character 同士の押し出しをしない
    //--------------------------------------------------------
    skipCharacterResolve = true;

    //--------------------------------------------------------
    // ジャスト回避スロー中に
    // Player → Boss が当たったらスロー解除
    //--------------------------------------------------------
    if (slow && slow->IsJustAvoidSlow())
    {
        if (IsPlayerCharacter(atkCh) && IsBossCharacter(bodyCh))
        {
            slow->EndJustAvoidByHit();

            if (auto* player = dynamic_cast<Player*>(atkCh))
            {
                player->EndInvincible();
                player->ResetAttackInfo();
            }
        }
    }
}

//============================================================
// ジャスト回避スロー中の強制命中処理
// ・PlayerAttack vs Boss をタイプ無視で命中扱い
//============================================================
void CollisionManager::HandleJustAvoidForcedHit(
    const CollisionContext& ctx,
    SlowSystem* slow,
    bool& skipCharacterResolve)
{
    if (!slow || !slow->IsJustAvoidSlow())
        return;

    // a が Player、b が Boss
    if (IsPlayerCharacter(ctx.charA) && IsBossCharacter(ctx.charB))
    {
        Player* player = static_cast<Player*>(ctx.charA);
        Boss* boss = static_cast<Boss*>(ctx.charB);

        if (ctx.a->IsAttack() && !player->HasHit())
        {
            DoAttackHit(ctx.a, ctx.b, player, boss, slow, skipCharacterResolve);
            slow->EndJustAvoidByHit();
        }
    }
    // b が Player、a が Boss
    else if (IsPlayerCharacter(ctx.charB) && IsBossCharacter(ctx.charA))
    {
        Player* player = static_cast<Player*>(ctx.charB);
        Boss* boss = static_cast<Boss*>(ctx.charA);

        if (ctx.b->IsAttack() && !player->HasHit())
        {
            DoAttackHit(ctx.b, ctx.a, player, boss, slow, skipCharacterResolve);
            slow->EndJustAvoidByHit();
        }
    }
}

//============================================================
// 通常の Attack vs Body ヒット処理
//============================================================
void CollisionManager::HandleNormalAttackHit(
    const CollisionContext& ctx,
    SlowSystem* slow,
    bool& skipCharacterResolve)
{
    if (skipCharacterResolve)
        return;

    if (ctx.a->IsAttack() && ctx.b->IsBody() && ctx.charA && ctx.charB)
    {
        DoAttackHit(ctx.a, ctx.b, ctx.charA, ctx.charB, slow, skipCharacterResolve);
    }
    else if (ctx.b->IsAttack() && ctx.a->IsBody() && ctx.charA && ctx.charB)
    {
        DoAttackHit(ctx.b, ctx.a, ctx.charB, ctx.charA, slow, skipCharacterResolve);
    }
}

//============================================================
// Body vs Body の押し出し
// ・Character 同士かつ SphereCollider 同士のみ対応
//============================================================
void CollisionManager::HandleBodyResolve(
    const CollisionContext& ctx,
    bool skipCharacterResolve
)
{
    if (skipCharacterResolve)
        return;

    if (!(ctx.a->IsBody() && ctx.b->IsBody()))
        return;

    auto* capsuleA = dynamic_cast<CapsuleCollider*>(ctx.a);
    auto* capsuleB = dynamic_cast<CapsuleCollider*>(ctx.b);

    if (capsuleA != nullptr && capsuleB != nullptr)
    {
        capsuleA->ResolveCapsule(capsuleB);
        return;
    }

    auto* sa = dynamic_cast<SphereCollider*>(ctx.a);
    auto* sb = dynamic_cast<SphereCollider*>(ctx.b);

    if (sa && sb)
    {
        sa->ResolveSphere(sb);
    }
}

//============================================================
// Mesh などの Resolve
// ・攻撃ヒット後でも地面押し出し等は行いたいので常に呼ぶ
//============================================================
void CollisionManager::HandlePhysicsResolve(Collider* a, Collider* b)
{
    if (!a || !b) return;

    if (a->IsAttack() || b->IsAttack())
    {
        return;
    }

    a->Resolve(b);
    b->Resolve(a);
}

void CollisionManager::HandleTaggedAttack(const CollisionContext& ctx)
{
    if (ctx.a == nullptr || ctx.b == nullptr)
    {
        return;
    }

#if defined(_DEBUG)

    //========================================================
    // Attack ColliderがCollisionManagerまで来ているか確認
    //========================================================
    if (ctx.a->HasTag(ColliderTag::PursuerAttack) || ctx.b->HasTag(ColliderTag::PursuerAttack))
    {
        OutputDebugStringW(
            L"[CollisionManager] "
            L"PursuerAttack collider overlap detected.\n"
        );
    }

#endif

    Collider* attackCollider = nullptr;
    Collider* bodyCollider = nullptr;

    //========================================================
    // A = Pursuer Attack
    // B = Player
    //========================================================
    if (ctx.a->HasTag(ColliderTag::PursuerAttack) && ctx.b->HasTag(ColliderTag::Player))
    {
        attackCollider = ctx.a;
        bodyCollider = ctx.b;
    }

    //========================================================
    // B = Pursuer Attack
    // A = Player
    // CollisionManagerのpair順序には依存しない
    //========================================================
    else if (ctx.b->HasTag(ColliderTag::PursuerAttack) && ctx.a->HasTag(ColliderTag::Player))
    {
        attackCollider = ctx.b;
        bodyCollider = ctx.a;
    }
    else
    {
        return;
    }


    //========================================================
    // Damage Receiver
    //========================================================
    IDamageable* damageable = bodyCollider->GetOwnerDamageable();


    if (damageable == nullptr)
    {
        return;
    }


    //========================================================
    // Damage
    //========================================================
    const bool damaged =
        damageable->ApplyDamage(attackCollider->GetAttackDamage());


#if defined(_DEBUG)

    if (damaged)
    {
        OutputDebugStringW(
            L"[CollisionManager] "
            L"PursuerAttack -> Player HIT.\n"
        );
    }

#endif
}

//============================================================
// 総合衝突処理
//============================================================
void CollisionManager::Process(
    std::vector<Collider*>& colliders,
    SlowSystem* slow,
    TutorialManager* tutorial)
{
        (void)tutorial;

#if defined(_DEBUG)
    //========================================================
    // Collision処理時間の計測開始
    //========================================================
    const auto startTime =
        std::chrono::steady_clock::now();

    std::uint64_t candidateCount = 0;
    std::uint64_t narrowPhaseCount = 0;
    std::uint64_t broadPhaseSkipCount = 0;
    std::uint64_t collisionCount = 0;
#endif

    const size_t n = colliders.size();

    //========================================================
    // もとの配列順を維持するため
    // Collider*ではなくインデックスを保持する
    //========================================================
    std::vector<size_t> allIndices;
    std::vector<size_t> nonStaticWorldIndices;

    allIndices.reserve(n);
    nonStaticWorldIndices.reserve(n);

    size_t staticWorldCount = 0;

    //========================================================
    // Colliderの分類
    // allIndices: nullptr以外の全Collider
    // nonStaticWorldIndices: 静的World以外の全Collider
    // ※Disabled Colliderもここでは保持する。
    //   判定時のIsValidPairで無効化を確認するため、
    //   フレーム途中のEnable変更にも対応できる。
    //========================================================
    for (size_t i = 0; i < n; ++i)
    {
        Collider* collider = colliders[i];

        if (collider == nullptr)
        {
            continue;
        }

        allIndices.push_back(i);

        if (IsStaticWorldCollider(collider))
        {
            ++staticWorldCount;
        }
        else
        {
            nonStaticWorldIndices.push_back(i);
        }
    }

    //========================================================
    // 1ペア分の衝突処理
    //========================================================
    const auto processPair = [&](Collider* a, Collider* b)
        {
#if defined(_DEBUG)
            ++candidateCount;
#endif

            //====================================================
            // nullptr / Enabled / Owner / Static World
            //====================================================
            if (!IsValidPair(a, b))
            {
                return;
            }

            //====================================================
            // Broad Phase
            // Static World BoxとCapsule/Sphereが
            // 明らかに離れていれば詳細判定を省略する
            // Colliderの順序は固定ではないので
            // どちらがStatic Worldでも対応する
            //====================================================
            bool separated = false;

            if (IsStaticWorldCollider(a))
            {
                separated = IsSeparatedFromStaticBox(b, a);
            }
            else if (IsStaticWorldCollider(b))
            {
                separated = IsSeparatedFromStaticBox(a, b);
            }

            if (separated)
            {
#if defined(_DEBUG)
                ++broadPhaseSkipCount;
#endif
                return;
            }

            //====================================================
            // Narrow Phase
            //====================================================
#if defined(_DEBUG)
            ++narrowPhaseCount;
#endif

            if (!a->CollisionCheck(b))
            {
                return;
            }

#if defined(_DEBUG)
            ++collisionCount;
#endif

            //====================================================
            // PlayerがDoorに接触した通知
            //====================================================
            if (a->HasTag(ColliderTag::Door) && b->HasTag(ColliderTag::Player))
            {
                if (auto* doorBox = dynamic_cast<BoxCollider*>(a))
                {
                    doorBox->NotifyContact(b);
                }
            }
            else if (b->HasTag(ColliderTag::Door) && a->HasTag(ColliderTag::Player))
            {
                if (auto* doorBox = dynamic_cast<BoxCollider*>(b))
                {
                    doorBox->NotifyContact(a);
                }
            }

            //====================================================
            // CollisionContext構築
            //====================================================
            CollisionContext ctx = BuildContext(a, b);

            bool skipCharacterResolve = false;

            //====================================================
            // 敵の特殊攻撃判定
            //====================================================
            HandleTaggedAttack(ctx);

            //====================================================
            // ジャスト回避スロー中の特殊処理
            //====================================================
            HandleJustAvoidForcedHit(
                ctx,
                slow,
                skipCharacterResolve
            );

            //====================================================
            // 通常攻撃処理
            //====================================================
            HandleNormalAttackHit(
                ctx,
                slow,
                skipCharacterResolve
            );

            //====================================================
            // Body同士の押し出し
            //====================================================
            HandleBodyResolve(
                ctx,
                skipCharacterResolve
            );

            //====================================================
            // 地面・壁・各種Colliderの押し出し
            //====================================================
            HandlePhysicsResolve(a, b);
        };

    //========================================================
    // 衝突ペアの生成
    // iがStatic Worldの場合j側にはStatic World以外だけを使用する
    // これによりStatic World同士の組み合わせを最初から生成しない
    //========================================================
    for (const size_t i : allIndices)
    {
        Collider* a = colliders[i];

        //====================================================
        // 参照するインデックス配列を選択
        //====================================================
        const std::vector<size_t>& candidates =
            IsStaticWorldCollider(a) ? nonStaticWorldIndices : allIndices;

        //====================================================
        // 元と同じく j > i の組み合わせのみ処理
        // upper_boundでiより大きい位置へ直接移動する
        //====================================================
        auto it = std::upper_bound(candidates.begin(), candidates.end(), i);

        for (; it != candidates.end(); ++it)
        {
            Collider* b = colliders[*it];

            processPair(a, b);
        }
    }

#if defined(_DEBUG)

    //========================================================
    // Collision処理時間の計測終了
    //========================================================
    const auto endTime = std::chrono::steady_clock::now();

    const double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    //========================================================
    // 120フレーム分の平均を取得する
    // 毎フレームログ出力すると
    // OutputDebugString自体が負荷になってしまうため
    //========================================================
    static std::uint64_t accumulatedCandidates = 0;
    static std::uint64_t accumulatedNarrow = 0;
    static std::uint64_t accumulatedBroadSkip = 0;
    static std::uint64_t accumulatedHits = 0;

    static double accumulatedMs = 0.0;
    static unsigned int accumulatedFrames = 0;

    accumulatedCandidates += candidateCount;
    accumulatedNarrow += narrowPhaseCount;
    accumulatedBroadSkip += broadPhaseSkipCount;
    accumulatedHits += collisionCount;

    accumulatedMs += elapsedMs;
    ++accumulatedFrames;

    if (accumulatedFrames >= 120)
    {
        const double frameCount =
            static_cast<double>(accumulatedFrames);

        //====================================================
        // Debug出力
        //====================================================
        const std::string message =
            "[CollisionPerf] "
            "Total=" + std::to_string(n) +
            " StaticWorld=" + std::to_string(staticWorldCount) +
            " Other=" + std::to_string(nonStaticWorldIndices.size()) +
            " Candidates/frame=" +
            std::to_string(
                accumulatedCandidates / accumulatedFrames) +
            " BroadSkip/frame=" +
            std::to_string(
                accumulatedBroadSkip / accumulatedFrames) +
            " Narrow/frame=" +
            std::to_string(
                accumulatedNarrow / accumulatedFrames) +
            " Hits/frame=" +
            std::to_string(
                accumulatedHits / accumulatedFrames) +
            " AvgMs=" +
            std::to_string(accumulatedMs / frameCount) +
            "\n";

        OutputDebugStringA(message.c_str());

        //====================================================
        // 次の120フレームに向けて初期化
        //====================================================
        accumulatedCandidates = 0;
        accumulatedNarrow = 0;
        accumulatedBroadSkip = 0;
        accumulatedHits = 0;

        accumulatedMs = 0.0;
        accumulatedFrames = 0;
    }

#endif
}