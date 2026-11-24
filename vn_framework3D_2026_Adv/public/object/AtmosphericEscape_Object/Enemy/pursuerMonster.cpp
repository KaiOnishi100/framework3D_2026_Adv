#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "pursuerMonster.h"

#include "../../../noise/noiseSystem.h"
#include "../../../interface/INavigationDoor.h"

#include <algorithm>
#include <cmath>

//============================================================
// Initialize
//============================================================
bool PursuerMonster::Initialize(vnScene* scene, const XMFLOAT3& position)
{
    if (scene == nullptr)
    {
        return false;
    }

    //========================================================
    // Initial Position
    //========================================================
    mPosition = position;

    mAudioPreviousPosition = position;
    mStepSoundTimer = 0.0f;
    mBreathSoundTimer = 1.2f;
    mStepSoundVariant = 0;
    mAttackSoundPending = false;

    //========================================================
    // Model
    //========================================================
    mModel = new SkeletalModel(L"data/model/enemy/", L"Enemy.vnm",L"Enemy.bone");


    if (mModel == nullptr)
    {
        return false;
    }


    //========================================================
    // Transform
    //========================================================
    mModel->setPosition(position.x, position.y, position.z);

    //========================================================
    // Material
    //========================================================
    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);
    mModel->SetShadowMobility(ShadowMobility::Dynamic);
    if (auto* visual = mModel->GetModel())
    {
        visual->SetShadowMobility(ShadowMobility::Dynamic);
    }

    //============================================================
    // Pursuer Body Collider
    //============================================================
    constexpr float BodyRadius = 0.38f;
    constexpr float BodyHalfSegmentHeight = 0.65f;
    constexpr float BodyOffsetY = BodyRadius + BodyHalfSegmentHeight;

    //============================================================
    // Pursuer Body Collider
    //============================================================
    mBodyCollider =
        std::make_unique<CapsuleCollider>(
            mModel,
            BodyRadius,
            BodyHalfSegmentHeight,
            XMVectorSet(0.0f, BodyOffsetY, 0.0f, 0.0f)
        );

    //============================================================
    // Body
    //============================================================
    mBodyCollider->SetType(ColliderType::Body);

    //============================================================
    // Gameplay Tag
    //============================================================
    mBodyCollider->SetTag(ColliderTag::Pursuer);

    //============================================================
    // Surface
    //============================================================
    mBodyCollider->SetSurfaceType(SurfaceType::Flesh);

    //============================================================
    // Mass
    //============================================================
    mBodyCollider->SetMass(4.0f);

    //============================================================
    // Attack Collider
    //============================================================
    mAttackCollider = std::make_unique<SphereCollider>(mModel, 1.5f);

    //============================================================
    // Physics Type
    //============================================================
    mAttackCollider->SetType(ColliderType::Attack);

    //============================================================
    // Gameplay Tag
    //============================================================
    mAttackCollider->SetTag(ColliderTag::PursuerAttack);

    //============================================================
    // Damage
    //============================================================
    mAttackCollider->SetAttackDamage(mAttackDamage);

    //============================================================
    // 通常時は無効
    //============================================================
    mAttackCollider->SetEnabled(false);

    //========================================================
    // Scene
    //========================================================
    mModel->RegisterObject(scene);

    //========================================================
    // AI
    //========================================================
    mState = State::Patrol;
    mPatrolIndex = 0;
    mRecentPatrolPoints.fill(SIZE_MAX);
    mPatrolRetryTimer = 0.0f;
    mFinalChase = false;
    mFinalChasePathRetry = 0.0f;


    return true;
}

bool PursuerMonster::InitializeAnimations(const PursuerAnimationSet& animations)
{
    if (mModel == nullptr)
    {
        return false;
    }

    //========================================================
    // 現在保持しているSkeletalModelへMotionを登録
    //========================================================
    return mAnimation.Initialize(*mModel, animations);
}

//============================================================
// Terminate
//============================================================
void PursuerMonster::Terminate(vnScene* scene)
{
    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);

        mModel = nullptr;
    }


    mPatrolPoints.clear();

    mState = State::Patrol;
    mPatrolIndex = 0;
    mRecentPatrolPoints.fill(SIZE_MAX);
    mPatrolRetryTimer = 0.0f;
    mFinalChase = false;
    mFinalChasePathRetry = 0.0f;
}

//============================================================
// Update
//============================================================
void PursuerMonster::Update(float dt, ExplorationPlayer& player)
{
    dt = (std::min)(dt, 0.1f);
    
    //========================================================
    // AI更新前の位置
    // このFrameで実際に移動したかを判定するため保持
    //========================================================
    const XMFLOAT3 previousPosition = mPosition;
    mAudioPreviousPosition = previousPosition;
    //============================================================
    // Attack Cooldown
    //============================================================
    if (mAttackCooldownTimer > 0.0f)
    {
        mAttackCooldownTimer -= dt;

        if (mAttackCooldownTimer < 0.0f)
        {
            mAttackCooldownTimer = 0.0f;
        }
    }

    //============================================================
    // Hearing
    // Chase中は視認しているのでNoiseは無視
    //============================================================
    if (mState != State::Chase && mState != State::Attack)
    {
        XMFLOAT3 heardPosition;

        if (TryHearNoise(heardPosition))
        {
            //====================================================
            // 聞こえた場所を調査対象にする
            //====================================================
            mInvestigatePosition = heardPosition;

            //====================================================
            // Investigate中なら目的地だけ更新
            // Patrol / SearchならInvestigateへ
            //====================================================
            if (mState != State::Investigate)
            {
                ChangeState(State::Investigate);
            }
        }
    }

    //========================================================
    // State
    //========================================================
    switch (mState)
    {
    case State::Patrol:
    {
        UpdatePatrol(dt, player);
        break;
    }


    case State::Investigate:
    {
        UpdateInvestigate(dt, player);
        break;
    }


    case State::Chase:
    {
        UpdateChase(dt, player);
        break;
    }


    case State::Search:
    {
        UpdateSearch(dt, player);
        break;
    }

    case State::Attack:
    {
        UpdateAttack(dt, player);
        break;
    }
    }


    //============================================================
    // Pursuer Animation Update
    //============================================================
    if (mModel != nullptr)
    {
        //========================================================
        // 今回のFrameにおけるXZ平面の移動量
        //========================================================
        const float dx = mPosition.x - previousPosition.x;
        const float dz = mPosition.z - previousPosition.z;

        const float moveDistanceSq = dx * dx + dz * dz;

        //========================================================
        // 微小な座標変化を無視する
        //========================================================
        constexpr float MoveThreshold = 0.0001f;

        const bool moving = moveDistanceSq > MoveThreshold * MoveThreshold;

        //========================================================
        // デフォルトはIdle
        //========================================================
        PursuerAnimationState animationState = PursuerAnimationState::Idle;

        //========================================================
        // 現在のAI Stateからアニメーションを決定
        //========================================================
        switch (mState)
        {
        case State::Patrol:
        case State::Investigate:
        case State::Search:
        {
            //====================================================
            // 巡回・調査・捜索中
            // 実際に移動していればWalk停止していればIdle
            //====================================================
            animationState = moving ? PursuerAnimationState::Walk : PursuerAnimationState::Idle;

            break;
        }

        case State::Chase:
        {
            //====================================================
            // 追跡中
            // 移動中はRun
            // ドア待ちなどで停止していればIdle
            //====================================================
            animationState = moving ? PursuerAnimationState::Run : PursuerAnimationState::Idle;

            break;
        }

        case State::Attack:
        {
            //====================================================
            // Attack中は移動状態に関係なくAttackを優先
            //====================================================
            animationState = PursuerAnimationState::Attack;

            break;
        }
        }

#if defined(_DEBUG)

        //============================================================
        // Pursuer Animation Debug
        // ・Clipが登録されているか
        // ・現在のClip名
        // ・再生時間が進んでいるか
        // ・Motionが再生中か
        // ・VNM / SkeletonにBoneが存在するか
        // 1秒に1回だけ出力してログの増加を抑える
        //============================================================
        static float animationDebugTimer = 0.0f;

        animationDebugTimer += dt;

        if (animationDebugTimer >= 1.0f)
        {
            animationDebugTimer = 0.0f;

            //========================================================
            // 現在のMotion状態
            //========================================================
            const std::wstring& clipName =
                mModel->GetCurrentMotionName();

            const float motionTime =
                mModel->GetMotionTime();

            const float motionLength =
                mModel->GetMotionLength();

            const bool playing =
                mModel->IsMotionPlaying();

            //========================================================
            // Bone数
            //========================================================
            const int skeletonBoneCount =
                mModel->GetBoneCount();

            const int skinBoneCount =
                mModel->GetModel() != nullptr ? mModel->GetModel()->getBoneNum() : 0;

            //========================================================
            // AI位置と描画モデルのRoot位置を比較
            //========================================================
            const XMVECTOR modelPosition =
                *mModel->getPosition();

            wchar_t buffer[512] = {};

            swprintf_s(
                buffer,
                L"[PursuerDebug] "
                L"AI=%d Anim=%d "
                L"Clip=%ls "
                L"Playing=%d "
                L"Time=%.3f/%.3f "
                L"SkeletonBones=%d SkinBones=%d "
                L"AIY=%.3f ModelY=%.3f\n",

                static_cast<int>(mState),
                static_cast<int>(animationState),
                clipName.c_str(),
                playing ? 1 : 0,
                motionTime,
                motionLength,
                skeletonBoneCount,
                skinBoneCount,
                mPosition.y,
                XMVectorGetY(modelPosition)
            );

            OutputDebugStringW(buffer);
        }

#endif

        //========================================================
        // Animation Controllerへ通知
        // 実際のBlendToMotionはControllerが管理する
        //========================================================
        mAnimation.Update(*mModel, animationState);

        mModel->execute(dt);
    }

    mModel->setPosition(
        mPosition.x,
        mPosition.y,
        mPosition.z
    );

    UpdateAttackCollider();
}

void PursuerMonster::UpdateAudio(float dt, const ExplorationPlayer& player)
{
    if (mModel == nullptr || player.IsDead() || dt <= 0.0f)
    {
        return;
    }

    dt = (std::min)(dt, 0.1f);

    //========================================================
    // 聞く位置はプレイヤーの耳元 方向はカメラに合わせる
    // 三人称でも、プレイヤーと敵の距離を基準に音量が変わる
    //========================================================
    XMFLOAT3 listenerPosition{};
    XMStoreFloat3(&listenerPosition, *player.getPosition());
    listenerPosition.y += player.IsCrouching() ? 0.95f : 1.60f;

    const XMVECTOR forward = vnCamera::getForward();
    const XMVECTOR right = vnCamera::getRight();
    const XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));

    XMFLOAT3 listenerForward{};
    XMFLOAT3 listenerUp{};
    XMStoreFloat3(&listenerForward, forward);
    XMStoreFloat3(&listenerUp, up);

    //========================================================
    // 声は顔付近 足音は足元から聞こえるようにする
    //========================================================
    XMFLOAT3 voicePosition = mPosition;
    voicePosition.y += 1.50f;

    const auto voiceMix = vnSound::BuildSpatialMix(
        listenerPosition, listenerForward, listenerUp, voicePosition, 16.0f);
    const auto breathMix = vnSound::BuildSpatialMix(
        listenerPosition, listenerForward, listenerUp, voicePosition, 48.0f);
    const auto stepMix = vnSound::BuildSpatialMix(
        listenerPosition, listenerForward, listenerUp, mPosition, 36.0f);

    if (voiceMix.channelCount == 0 || stepMix.channelCount == 0|| breathMix.channelCount == 0)
    {
        return;
    }

    SoundBank* sound = SoundBank::getInstance();

    static constexpr SoundID stepSounds[] =
    {
        SoundID::AE_PursuerStep,
        SoundID::AE_PursuerStep2,
        SoundID::AE_PursuerStep3,
    };

    //========================================================
    // 毎フレーム更新するので吐息の途中で振り向いても音の方向が追従する
    //========================================================
    sound->SetSfxSpatialMix(SoundID::AE_PursuerAttack, voiceMix);
    sound->SetSfxSpatialMix(SoundID::AE_PursuerBreath, voiceMix);
    sound->SetSfxSpatialMix(SoundID::AE_PursuerBreath, breathMix);

    for (SoundID id : stepSounds)
    {
        sound->SetSfxSpatialMix(id, stepMix);
    }

    mStepSoundTimer -= dt;
    mBreathSoundTimer -= dt;

    if (mAttackSoundPending)
    {
        mAttackSoundPending = false;
        sound->PlaySfx(SoundID::AE_PursuerAttack, 0.62f);

        //========================================================
        // 攻撃の声が出た直後は新しい吐息を重ねない
        //========================================================
        mBreathSoundTimer = 3.4f;
    }

    if (mState != State::Attack && mBreathSoundTimer <= 0.0f)
    {
        sound->PlaySfx(SoundID::AE_PursuerBreath, 0.28f);
        mBreathSoundTimer = 4.0f;
    }

    //========================================================
    // 壁やドアの前でAIが止まったときは足音も止める
    //========================================================
    const float dx = mPosition.x - mAudioPreviousPosition.x;
    const float dz = mPosition.z - mAudioPreviousPosition.z;
    const bool moved = dx * dx + dz * dz > 0.000001f;

    const auto& motion = mModel->GetCurrentMotionName();
    const bool running = motion == L"Run";

    if (mStepSoundTimer <= 0.0f && moved && (running || motion == L"Walk"))
    {
        sound->PlaySfx(stepSounds[mStepSoundVariant], running ? 0.48f : 0.36f);

        mStepSoundVariant = (mStepSoundVariant + 1) % 3;
        mStepSoundTimer = running ? 0.44f : 0.75f;
    }
}

//============================================================
// Patrol Point
//============================================================
void PursuerMonster::SetPresenceEnabled(bool enabled)
{
    if (mModel != nullptr)
    {
        mModel->setRenderEnable(enabled);
        mModel->setCastShadow(enabled);

        // RegisterObject()はSkeletalModel本体と内部のvnModelを別々に登録する
        if (auto* visual = mModel->GetModel())
        {
            visual->setRenderEnable(enabled);
            visual->setCastShadow(enabled);
        }
    }

    if (mBodyCollider != nullptr)
    {
        mBodyCollider->SetEnabled(enabled);
    }
    if (mAttackCollider != nullptr)
    {
        // 攻撃判定はAttack中の有効フレームでのみ再び有効にする
        mAttackCollider->SetEnabled(false);
    }
}

void PursuerMonster::BeginFinalChase(const XMFLOAT3& startPosition, const XMFLOAT3& target)
{
    if (mFinalChase || mModel == nullptr)
    {
        return;
    }

    mFinalChase = true;
    mPosition = startPosition;
    mModel->setPosition(mPosition.x, mPosition.y, mPosition.z);
    mLastKnownPlayerPosition = target;
    mChaseLostSightTimer = 0.0f;

    // テレポート分を足音用の移動量に含めない
    mAudioPreviousPosition = startPosition;
    mStepSoundTimer = 0.0f;

    mAttackTimer = 0.0f;
    mAttackHitProcessed = false;
    mAttackSoundPending = false;
    mAttackCooldownTimer = 0.65f;
    if (mAttackCollider != nullptr)
    {
        mAttackCollider->SetEnabled(false);
    }

    const float dx = target.x - startPosition.x;
    const float dz = target.z - startPosition.z;
    if (dx * dx + dz * dz > 0.000001f)
    {
        mCurrentYaw = std::atan2(dx, dz);
        SetFacingYaw(mCurrentYaw);
    }

    // すでにChaseだった場合も旧位置の経路を捨てる
    ClearNavigationPath();
    mDoorDetourRetry = 0.0f;
    mFinalChasePathRetry = 0.0f;
    ChangeState(State::Chase);
}

void PursuerMonster::AddPatrolPoint(const XMFLOAT3& point)
{
    mPatrolPoints.push_back(point);
}

void PursuerMonster::UpdatePatrol(float dt, const ExplorationPlayer& player)
{
    //========================================================
    // Player Detection
    //========================================================
    if (CanSeePlayer(player, mDetectionDistance))
    {
        //====================================================
        // Player Position
        //====================================================
        const XMVECTOR playerPosition = *player.getPosition();


        //====================================================
        // 最後にPlayerを確認した位置
        // Chase → Searchになった時
        // この位置を探索対象として使用する
        //====================================================
        mLastKnownPlayerPosition =
        {
            XMVectorGetX(playerPosition),
            XMVectorGetY(playerPosition),
            XMVectorGetZ(playerPosition)
        };


        //====================================================
        // Patrol → Chase
        //====================================================
        ChangeState(State::Chase);


        //====================================================
        // State変更したFrameではPatrol移動を行わない
        // 次FrameからUpdateChase()へ入る
        //====================================================
        return;
    }


    if (mPatrolPoints.empty())
    {
        return;
    }

    if (mPatrolIndex < mPatrolPoints.size() &&
        (mHasNavigationPath || mNavigation == nullptr) &&
        IsNearPosition(mPatrolPoints[mPatrolIndex], mPatrolArrivalDistance))
    {
        // 実際に到着した地点だけを記憶する
        std::move_backward(mRecentPatrolPoints.begin(),
            mRecentPatrolPoints.end() - 1, mRecentPatrolPoints.end());
        mRecentPatrolPoints.front() = mPatrolIndex;
        mPatrolIndex = SIZE_MAX;
        ClearNavigationPath();
    }

    if ((mNavigation != nullptr && !mHasNavigationPath) ||
        mPatrolIndex >= mPatrolPoints.size())
    {
        mPatrolRetryTimer = (std::max)(0.0f, mPatrolRetryTimer - dt);
        if (mPatrolRetryTimer > 0.0f)
        {
            return;
        }

        if (!SelectReachablePatrolPoint())
        {
            // 扉や接続状況が変わるまで全候補の経路検索頻度を抑える
            mPatrolRetryTimer = 0.5f;
            return;
        }
    }

    MoveTowardsNavigated(mPatrolPoints[mPatrolIndex], mPatrolSpeed, dt);
}

void PursuerMonster::UpdateChase(float dt, const ExplorationPlayer& player)
{
    //========================================================
    // Chase継続判定
    // Chase中はFOVを要求しない
    // Navigationで一時的に横を向いても
    // LOSが通っていればPlayerを追跡し続ける
    //========================================================
    const bool playerVisible = !mFinalChase && CanMaintainChase(player, mLoseDistance);

    //========================================================
    // 最終脱出中は視線が切れても、通常時は見えている間だけ最新位置を追う
    //========================================================
    if (mFinalChase || playerVisible)
    {
        //====================================================
        // 見失いTimer Reset
        //====================================================
        mChaseLostSightTimer = 0.0f;

        //====================================================
        // Player Position
        //====================================================
        const XMVECTOR playerPosition = *player.getPosition();

        const XMFLOAT3 target =
        {
            XMVectorGetX(playerPosition),
            XMVectorGetY(playerPosition),
            XMVectorGetZ(playerPosition)
        };

        //============================================================
        // Playerとの距離
        //============================================================
        const float attackDx = target.x - mPosition.x;
        const float attackDz = target.z - mPosition.z;
        const float attackDistanceSq = attackDx * attackDx + attackDz * attackDz;
        const float attackStartDistanceSq = mAttackStartDistance * mAttackStartDistance;
        //============================================================
        // Attack Vertical Difference
        // 上下階にいるPlayerへFloor越しに攻撃しない
        //============================================================
        const float attackDy = std::fabs(target.y - mPosition.y);

        constexpr float AttackVerticalTolerance = 1.75f;

        //============================================================
        // Attack可能
        //============================================================
        mLastKnownPlayerPosition = target;

        // 強制追跡中でも扉・壁で遮られていれば攻撃を開始しない
        if (mAttackCooldownTimer <= 0.0f &&
            attackDistanceSq <= attackStartDistanceSq &&
            attackDy <= AttackVerticalTolerance &&
            (!mFinalChase || HasLineOfSightToPlayer(player)))
        {
            ChangeState(State::Attack);
            return;
        }


        float chaseSpeed = mChaseSpeed;

        if (mFinalChase)
        {
            const float distance = std::sqrt(attackDistanceSq);
            constexpr float nearDistance = 2.5f;
            constexpr float farDistance = 7.0f;

            // 2.5m以内で4.7m/s、7m以上で5.3m/s
            // 間の距離では速度を連続的に変えて境目で急加速しないようにする
            const float t = (std::clamp)(
                (distance - nearDistance) / (farDistance - nearDistance), 0.0f, 1.0f);

            chaseSpeed = mFinalChaseNearSpeed + (mFinalChaseFarSpeed - mFinalChaseNearSpeed) * t;
        }

        MoveTowardsNavigated(target, chaseSpeed, dt);
        return;
    }


    //========================================================
    // Playerを現在は視認できていない
    //========================================================
    mChaseLostSightTimer += dt;


    //========================================================
    // Grace Time以内
    //========================================================
    if (mChaseLostSightTimer < mChaseLostSightGraceTime)
    {
        //====================================================
        // 最後に見た位置へ向かう
        //====================================================
        MoveTowardsNavigated(mLastKnownPlayerPosition, mChaseSpeed, dt);

        return;
    }


    //========================================================
    // 一定時間Playerを確認できなかった Searchへ
    //========================================================
    ChangeState(State::Search);
}

void PursuerMonster::UpdateSearch(float dt, const ExplorationPlayer& player)
{
    //========================================================
    // Search中でもPlayer発見判定は毎Frame行う
    //========================================================
    if (CanSeePlayer(player, mDetectionDistance))
    {
        //====================================================
        // Player Position
        //====================================================
        const XMVECTOR playerPosition = *player.getPosition();


        //====================================================
        // Last Known更新
        //====================================================
        mLastKnownPlayerPosition =
        {
            XMVectorGetX(playerPosition),
            XMVectorGetY(playerPosition),
            XMVectorGetZ(playerPosition)
        };


        //====================================================
        // Search → Chase
        //====================================================
        ChangeState(State::Chase);


        return;
    }


    //========================================================
    // Search Phase
    //========================================================
    switch (mSearchPhase)
    {
        //========================================================
        // 最後にPlayerを確認した位置へ移動
        //========================================================
    case SearchPhase::MoveToLastKnown:
    {
        //====================================================
        // 到着
        //====================================================
        if (IsNearPosition(mLastKnownPlayerPosition, mSearchArrivalDistance))
        {
            ClearNavigationPath();

            //================================================
            // 到着したので周囲を見る
            //================================================
            BeginLookAround();


            mSearchPhase = SearchPhase::LookAroundLastKnown;


#if defined(_DEBUG)

            OutputDebugStringW(
                L"[PursuerMonster] "
                L"Search -> LookAroundLastKnown\n"
            );

#endif
            return;
        }

        //====================================================
        // LastKnownへ向かう
        //====================================================
        MoveTowardsNavigated(mLastKnownPlayerPosition, mPatrolSpeed, dt);

        break;
    }


    //========================================================.
    // LastKnown位置で周囲を見る
    //========================================================
    case SearchPhase::LookAroundLastKnown:
    {
        //====================================================
        // 見回しが終わるまで待つ
        //====================================================
        if (!UpdateLookAround(dt))
        {
            break;
        }


        //====================================================
        // 周辺にSearch Pointがあるか探す
        //====================================================
        if (SelectSearchPoint())
        {
            mSearchPhase = SearchPhase::MoveToSearchPoint;


#if defined(_DEBUG)

            OutputDebugStringW(
                L"[PursuerMonster] "
                L"Search -> MoveToSearchPoint\n"
            );

#endif
        }
        else
        {
            //================================================
            // Search Point無し
            // これ以上調べる場所が無いのでPatrolへ戻る
            //================================================
            ChangeState(State::Patrol);
        }


        break;
    }


    //========================================================
    // Search Pointへ移動
    //========================================================
    case SearchPhase::MoveToSearchPoint:
    {
        //====================================================
        // Safety
        //====================================================
        if (!mHasSearchPoint || mCurrentSearchPointIndex >= mSearchPoints.size())
        {
            ChangeState(State::Patrol);

            break;
        }

        const XMFLOAT3& target = mSearchPoints[mCurrentSearchPointIndex];

        //====================================================
        // Search Point到着
        //====================================================
        if (IsNearPosition(target, mSearchArrivalDistance))
        {
            ClearNavigationPath();

            BeginLookAround();

            mSearchPhase = SearchPhase::LookAroundSearchPoint;


#if defined(_DEBUG)

            OutputDebugStringW(
                L"[PursuerMonster] "
                L"Search -> LookAroundSearchPoint\n"
            );

#endif
            return;
        }

        //====================================================
        // Search Pointへ移動
        //====================================================
        MoveTowardsNavigated(target, mPatrolSpeed, dt);

        break;
    }


    //========================================================
    // Search Pointで周囲を見る
    //========================================================
    case SearchPhase::LookAroundSearchPoint:
    {
        //====================================================
        // 見回し終了
        //====================================================
        if (UpdateLookAround(dt))
        {
#if defined(_DEBUG)

            OutputDebugStringW(
                L"[PursuerMonster] "
                L"Search complete -> Patrol\n"
            );

#endif

            ChangeState(State::Patrol);
        }


        break;
    }
    }
}

void PursuerMonster::UpdateAttack(float dt, ExplorationPlayer& player)
{
    //========================================================
    // Player死亡済み
    //========================================================
    if (player.IsDead())
    {
        return;
    }


    //========================================================
    // Attack Time
    //========================================================
    mAttackTimer += dt;

    //========================================================
    // Player方向を向く
    //========================================================
    const XMVECTOR playerPositionVector = *player.getPosition();


    const XMFLOAT3 playerPosition =
    {
        XMVectorGetX(playerPositionVector),
        XMVectorGetY(playerPositionVector),
        XMVectorGetZ(playerPositionVector)
    };

    //============================================================
    // Player方向
    //============================================================
    const float dx = playerPosition.x - mPosition.x;
    const float dz = playerPosition.z - mPosition.z;
    const float lengthSq = dx * dx + dz * dz;

    //============================================================
    // Attack中もPlayer方向を向く
    //============================================================
    if (lengthSq > 0.000001f)
    {
        const float length = sqrtf(lengthSq);
        const float dirX = dx / length;
        const float dirZ = dz / length;


        mForward =
        {
            dirX,
            0.0f,
            dirZ
        };


        //========================================================
        // ModelもPlayer方向へ向ける
        //========================================================
        const float yaw = atan2f(dirX, dirZ);
        mCurrentYaw = yaw;

        if (mModel != nullptr)
        {
            mModel->setRotationY(yaw);
        }
    }

    //============================================================
    // 現在Attackモーションが選択されているか確認
    //============================================================
    const bool hasAttackMotion =
        mModel != nullptr && mModel->GetCurrentMotionName() == L"Attack";

    //============================================================
    // モーションの長さと現在時刻を取得
    // Attackが正常にロードされていない場合は
    // AIタイマーへフォールバックする
    //============================================================
    const float motionLength = hasAttackMotion ? mModel->GetMotionLength() : mAttackEndTime;

    const float motionTime = hasAttackMotion ? mModel->GetMotionTime() : mAttackTimer;

    //============================================================
    // 再生進行率を0.0～1.0へ正規化
    // モーションが1.0秒でも2.0秒でも
    // 同じ割合で攻撃判定を制御できる
    //============================================================
    const float progress =
        motionLength > 0.0001f ? std::clamp(motionTime / motionLength, 0.0f, 1.0f) : 0.0f;

    //============================================================
    // 攻撃判定が有効な範囲
    // 最初は45%～60%に設定する
    // 実際の腕の振り下ろし位置を見て調整する
    //============================================================
    constexpr float AttackHitStartRatio = 0.45f;
    constexpr float AttackHitEndRatio = 0.60f;

    const bool attackActive =
        progress >= AttackHitStartRatio && progress <= AttackHitEndRatio &&
        (!mFinalChase || HasLineOfSightToPlayer(player));

    //============================================================
    // Attack Colliderの有効・無効を切り替える
    //============================================================
    if (mAttackCollider != nullptr)
    {
        mAttackCollider->SetEnabled(attackActive);
    }

    //============================================================
    // Attack終了判定
    // .motが再生されている場合: 非ループアニメーションの終了を待つ
    // .motが存在しない場合: 従来のmAttackEndTimeで終了する
    //============================================================
    bool attackFinished = false;

    if (hasAttackMotion && motionLength > 0.0001f)
    {
        //========================================================
        // SkeletalModelは非ループ再生が最後まで進むと
        // IsMotionPlaying()がfalseになる
        // 再生時間も確認し一時停止を終了と誤認しない
        //========================================================
        attackFinished = !mModel->IsMotionPlaying() && motionTime >= motionLength - 0.001f;
    }
    else
    {
        attackFinished = mAttackTimer >= mAttackEndTime;
    }

    //============================================================
    // 攻撃が終了していない場合はAttack Stateを維持
    //============================================================
    if (!attackFinished)
    {
        return;
    }

    //============================================================
    // 攻撃終了
    //============================================================

    // 次の攻撃までCooldown
    mAttackCooldownTimer = mAttackCooldownDuration;

    //============================================================
    // Attack Colliderを確実に無効化
    //============================================================
    if (mAttackCollider != nullptr)
    {
        mAttackCollider->SetEnabled(false);
    }

    //============================================================
    // プレイヤー死亡時はChaseへ戻さない
    //============================================================
    if (player.IsDead())
    {
        return;
    }

    //============================================================
    // モーションの最後まで再生してからChaseへ戻す
    // 次のPursuerAnimationController::Update()で
    // Attack → RunへCross Fadeする
    //============================================================
    ChangeState(State::Chase);
}

void PursuerMonster::UpdateAttackCollider()
{
    if (mAttackCollider == nullptr)
    {
        return;
    }

    //========================================================
    // Pursuer前方へAttack Sphereを配置
    // Y: 胸～腕付近
    // XZ: mForward方向へ約0.9m
    //========================================================
    const XMVECTOR offset =
        XMVectorSet(
            mForward.x * 0.90f,
            0.00f,
            mForward.z * 0.90f,
            0.0f
        );

    mAttackCollider->SetOffset(offset);
}

void PursuerMonster::SyncPositionAfterCollision()
{
    if (mModel == nullptr)
    {
        return;
    }

    //========================================================
    // CollisionManagerによって押し出された
    // Model Positionを取得する。
    //========================================================
    const XMVECTOR position = *mModel->getPosition();

    //========================================================
    // AI位置へ反映
    //========================================================
    mPosition.x = XMVectorGetX(position);
    mPosition.y = XMVectorGetY(position);
    mPosition.z = XMVectorGetZ(position);
}

void PursuerMonster::UpdateInvestigate(float dt, const ExplorationPlayer& player)
{
    //========================================================
    // Playerを視認
    //========================================================
    if (CanSeePlayer(player, mDetectionDistance))
    {
        //====================================================
        // 視認したのでLast Knownを更新
        //====================================================
        const XMVECTOR playerPosition = *player.getPosition();


        mLastKnownPlayerPosition =
        {
            XMVectorGetX(playerPosition),
            XMVectorGetY(playerPosition),
            XMVectorGetZ(playerPosition)
        };


        ChangeState(State::Chase);


        return;
    }


    //========================================================
    // 音源へ到着
    //========================================================
    if (IsNearPosition(mInvestigatePosition, mInvestigateArrivalDistance))
    {
        //====================================================
        // 音がした位置を
        // Search開始地点として扱う
        //====================================================
        mLastKnownPlayerPosition = mInvestigatePosition;


        ChangeState(State::Search);


#if defined(_DEBUG)

        OutputDebugStringW(
            L"[PursuerMonster] "
            L"Investigate reached -> Search\n"
        );

#endif

        return;
    }

    //========================================================
    // 音がした位置へ移動
    //========================================================
    MoveTowardsNavigated(mInvestigatePosition, mPatrolSpeed, dt);
}

bool PursuerMonster::CanDetectPlayer(const ExplorationPlayer& player, float maxDistance) const
{
    //========================================================
    // Player Position
    //========================================================
    const XMVECTOR playerPosition = *player.getPosition();

    const float playerX = XMVectorGetX(playerPosition);
    const float playerZ = XMVectorGetZ(playerPosition);


    //========================================================
    // XZ Distance
    //========================================================
    const float dx = playerX - mPosition.x;
    const float dz = playerZ - mPosition.z;
    const float distanceSq = dx * dx + dz * dz;
    const float detectionDistanceSq = maxDistance * maxDistance;

    return distanceSq <= detectionDistanceSq;
}

bool PursuerMonster::CanSeePlayer(const ExplorationPlayer& player, float maxDistance) const
{
    //========================================================
    // Player Position
    //========================================================
    const XMVECTOR playerPosition = *player.getPosition();

    const float playerX = XMVectorGetX(playerPosition);
    const float playerZ = XMVectorGetZ(playerPosition);


    //========================================================
    // Monster → Player
    //========================================================
    float dx = playerX - mPosition.x;
    float dz = playerZ - mPosition.z;
    const float distanceSq = dx * dx + dz * dz;


    //========================================================
    // Distance Test
    //========================================================
    const float maxDistanceSq = maxDistance * maxDistance;

    if (distanceSq > maxDistanceSq)
    {
        return false;
    }


    //========================================================
    // MonsterとPlayerがほぼ同じ位置
    //========================================================
    if (distanceSq <= 0.000001f)
    {
        return true;
    }


    //========================================================
    // Player Direction Normalize
    //========================================================
    const float distance = sqrtf(distanceSq);
    dx /= distance;
    dz /= distance;


    //========================================================
    // View Cone Test
    // Dot:
    //  1.0 = 完全に前
    //  0.0 = 真横
    // -1.0 = 完全に後ろ
    //========================================================
    const float dot =
        mForward.x * dx + mForward.z * dz;


    //========================================================
    // Full View Angle → Half Angle
    //========================================================
    const float halfAngleRadians =
        XMConvertToRadians(mViewAngleDegrees * 0.5f);


    //========================================================
    // 視野境界のDot値
    //========================================================
    const float minViewDot = cosf(halfAngleRadians);

    if (dot < minViewDot)
    {
        return false;
    }

    if (!HasLineOfSightToPlayer(player))
    {
        return false;
    }


    //========================================================
    // Distance + View Angleを通過
    //========================================================
    return true;
}

bool PursuerMonster::CanMaintainChase(const ExplorationPlayer& player, float maxDistance) const
{
    //========================================================
    // Player Position
    //========================================================
    const XMVECTOR playerPosition = *player.getPosition();
    const float playerX = XMVectorGetX(playerPosition);
    const float playerZ = XMVectorGetZ(playerPosition);

    //========================================================
    // Monster → Player Distance
    //========================================================
    const float dx = playerX - mPosition.x;
    const float dz = playerZ - mPosition.z;
    const float distanceSq = dx * dx + dz * dz;
    const float maxDistanceSq = maxDistance * maxDistance;


    //========================================================
    // 完全に追跡可能距離外なら見失う
    //========================================================
    if (distanceSq > maxDistanceSq)
    {
        return false;
    }

    //========================================================
    // Wall / Doorなどによる遮蔽だけ確認
    //========================================================
    if (!HasLineOfSightToPlayer(player))
    {
        return false;
    }


    return true;
}

void PursuerMonster::MoveTowards(const XMFLOAT3& target, float speed, float dt)
{
    //========================================================
    // Direction
    //========================================================
    float dx = target.x - mPosition.x;
    float dz = target.z - mPosition.z;

    const float lengthSq = dx * dx + dz * dz;

    //========================================================
    // ほぼ同じ位置
    //========================================================
    if (lengthSq <= 0.000001f)
    {
        return;
    }

    const float length = sqrtf(lengthSq);
    dx /= length;
    dz /= length;

    //========================================================
    // Facing Direction
    // 移動方向 = 現在Monsterが向いている方向
    // として扱う
    //========================================================
    mForward =
    {
        dx,
        0.0f,
        dz
    };

    //========================================================
    // Move
    //========================================================
    mPosition.x += dx * speed * dt;
    mPosition.z += dz * speed * dt;

    //============================================================
    // Target Direction
    //============================================================
    const float targetYaw = atan2f(dx, dz);

    //============================================================
    // Angle Difference
    // -PI ～ PI に正規化する
    //============================================================
    float yawDifference = targetYaw - mCurrentYaw;


    while (yawDifference > XM_PI)
    {
        yawDifference -= XM_2PI;
    }


    while (yawDifference < -XM_PI)
    {
        yawDifference += XM_2PI;
    }


    //============================================================
    // このFrameで回転可能な最大角度
    //============================================================
    const float maxTurn = mTurnSpeed * dt;

    //============================================================
    // Clamp
    //============================================================
    yawDifference = std::clamp(yawDifference, -maxTurn, maxTurn);

    //============================================================
    // Current Yaw更新
    //============================================================
    mCurrentYaw += yawDifference;

    //============================================================
    // AI Forwardも滑らかなYawから作る
    //============================================================
    mForward.x = sinf(mCurrentYaw);
    mForward.y = 0.0f;
    mForward.z = cosf(mCurrentYaw);
    //============================================================
    // Model Rotation
    //============================================================
    if (mModel != nullptr)
    {
        mModel->setRotationY(mCurrentYaw);
    }
}

void PursuerMonster::MoveTowardsNavigationWaypoint(const XMFLOAT3& target, float speed, float dt)
{
    //========================================================
   // XYZ Direction
   //========================================================
    float dx = target.x - mPosition.x;
    float dy = target.y - mPosition.y;
    float dz = target.z - mPosition.z;

    const float lengthSq =
        dx * dx + dy * dy + dz * dz;


    if (lengthSq <= 0.000001f)
    {
        return;
    }


    const float length = sqrtf(lengthSq);

    //========================================================
    // このFrameで移動可能な距離
    //========================================================
    const float moveDistance = speed * dt;

    //========================================================
    // Waypointを飛び越えない
    //========================================================
    if (moveDistance >= length)
    {
        mPosition = target;

        return;
    }


    dx /= length;
    dy /= length;
    dz /= length;


    //========================================================
    // XYZ Move
    // Stair Nodeへ向かう場合は
    // ここでYも徐々に変化する
    //========================================================
    mPosition.x += dx * moveDistance;
    mPosition.y += dy * moveDistance;
    mPosition.z += dz * moveDistance;


    //========================================================
    // Facingは水平成分だけ使用
    //========================================================
    const float horizontalLengthSq = dx * dx + dz * dz;


    if (horizontalLengthSq > 0.000001f)
    {
        const float targetYaw = atan2f(dx, dz);

        float yawDifference = targetYaw - mCurrentYaw;

        while (yawDifference > XM_PI)
        {
            yawDifference -= XM_2PI;
        }


        while (yawDifference < -XM_PI)
        {
            yawDifference += XM_2PI;
        }

        const float maxTurn = mTurnSpeed * dt;

        yawDifference = std::clamp(yawDifference, -maxTurn, maxTurn);

        mCurrentYaw += yawDifference;

        mForward =
        {
            sinf(mCurrentYaw),
            0.0f,
            cosf(mCurrentYaw)
        };

        if (mModel != nullptr)
        {
            mModel->setRotationY( mCurrentYaw );
        }
    }
}

bool PursuerMonster::IsNearNavigationWaypoint(const XMFLOAT3& target, float distance) const
{
    const float dx = target.x - mPosition.x;
    const float dy = target.y - mPosition.y;
    const float dz = target.z - mPosition.z;
    const float distanceSq = dx * dx + dy * dy + dz * dz;
    const float arrivalSq = distance * distance;

    return distanceSq <= arrivalSq;
}

bool PursuerMonster::TryInsertOpenDoorDetour(const XMFLOAT3& waypoint)
{
    if (mModel == nullptr || mBodyCollider == nullptr ||
        mNavigationDoors == nullptr || mSightColliders == nullptr)
    {
        return false;
    }

    // この回り込みは同じ床面上で行う。階段への移動は既存の経路に任せる。
    const XMVECTOR start = XMLoadFloat3(&mPosition);
    const XMVECTOR goal = XMLoadFloat3(&waypoint);
    const XMVECTOR toGoal = goal - start;
    const float goalDistance = XMVectorGetX(XMVector3Length(toGoal));

    if (goalDistance < 0.01f || std::fabs(waypoint.y - mPosition.y) > 0.25f)
    {
        return false;
    }

    const float radius = mBodyCollider->GetRadius();
    const float halfSegment = mBodyCollider->GetHalfSegmentHeight();
    const XMVECTOR modelPosition = *mModel->getPosition();
    const XMVECTOR bodyOffset = mBodyCollider->GetCenter() - modelPosition;

    // only が指定されている場合は、原因の扉板だけを調べる。
    // 回り込み候補では世界のコライダーも調べ、壁側に回り込まないようにする。
    const auto isSegmentClear =
        [&](FXMVECTOR from, FXMVECTOR to, const Collider* only)
        {
            const float distance = XMVectorGetX(XMVector3Length(to - from));
            const int sampleCount = (std::max)(1, static_cast<int>(std::ceil(distance / 0.13f)));

            for (int i = 1; i <= sampleCount; ++i)
            {
                const float t = static_cast<float>(i) / sampleCount;
                const XMVECTOR root = XMVectorLerp(from, to, t);
                const XMVECTOR offset = root - modelPosition + bodyOffset
                    + XMVectorSet(0.0f, 0.05f, 0.0f, 0.0f);

                // 接触面から離れる動きも試せるよう判定用カプセルに少し余裕を持たせる
                CapsuleCollider probe(mModel, radius - 0.02f, halfSegment, offset);

                if (only != nullptr)
                {
                    if (probe.CollisionCheck(only))
                    {
                        return false;
                    }
                    continue;
                }

                for (const Collider* collider : *mSightColliders)
                {
                    if (collider == nullptr || !collider->IsEnabled() || collider->GetOwner() == mModel)
                    {
                        continue;
                    }

                    // 足元の床は通過できる 腰の高さにある壁や家具は確認する
                    if (const auto* box = dynamic_cast<const BoxCollider*>(collider))
                    {
                        const float top = XMVectorGetY(box->GetCenter())
                            + XMVectorGetY(box->GetHalfSize());

                        if (top <= XMVectorGetY(root) + 0.05f)
                        {
                            continue;
                        }
                    }

                    if (probe.CollisionCheck(collider))
                    {
                        return false;
                    }
                }
            }

            return true;
        };

    // 直近の進路を確認して、実際に邪魔をしている扉だけを対象にする
    const XMVECTOR ahead = XMVectorLerp(start, goal, (std::min)(1.0f, 0.50f / goalDistance));

    for (const INavigationDoor* navigationDoor : *mNavigationDoors)
    {
        const auto* door = dynamic_cast<const MansionDoor*>(navigationDoor);
        if (door == nullptr || !door->IsOpen())
        {
            continue;
        }

        const XMVECTOR toDoor = XMLoadFloat3(&door->GetDoorPosition()) - start;
        if (XMVectorGetX(XMVector3LengthSq(toDoor)) > 4.0f * 4.0f)
        {
            continue;
        }

        const Collider* leaf = door->GetCollider();
        XMFLOAT3 a{}, b{};

        if (leaf == nullptr || isSegmentClear(start, ahead, leaf) ||
            !door->GetOpenDetourPoints(radius + 0.10f, a, b))
        {
            continue;
        }

        a.y = mPosition.y;
        b.y = mPosition.y;

        // 片側だけで通れる場合と先端の両側を通る場合を比較する
        const XMFLOAT3 routes[4][2] = { {a, a}, {b, b}, {a, b}, {b, a} };
        const std::size_t counts[] = { 1, 1, 2, 2 };
        int bestRoute = -1;
        float bestDistance = FLT_MAX;

        for (int route = 0; route < 4; ++route)
        {
            XMVECTOR previous = start;
            float distance = 0.0f;
            bool clear = true;

            for (std::size_t i = 0; i <= counts[route]; ++i)
            {
                const XMVECTOR next = i == counts[route] ? goal : XMLoadFloat3(&routes[route][i]);

                if (!isSegmentClear(previous, next, nullptr))
                {
                    clear = false;
                    break;
                }

                distance += XMVectorGetX(XMVector3Length(next - previous));
                previous = next;
            }

            if (clear && distance < bestDistance)
            {
                bestRoute = route;
                bestDistance = distance;
            }
        }

        if (bestRoute >= 0)
        {
            // 元のWaypointは残しその手前に回り込み用の点を挿入する
            mDoorDetourRemaining = counts[bestRoute];
            mNavigationPath.insert(
                mNavigationPath.begin() + mNavigationPathIndex,
                routes[bestRoute],
                routes[bestRoute] + mDoorDetourRemaining
            );
            return true;
        }
    }

    return false;
}

//============================================================
// Change State
//============================================================
void PursuerMonster::ChangeState(State state)
{
    if (mState == state)
    {
        return;
    }

    ClearNavigationPath();

    mState = state;

    switch (mState)
    {
    case State::Patrol:
    {
        //====================================================
        // Searchなどから戻ったら、その時点で通れる次の巡回先を選び直す。
        //====================================================
        mPatrolRetryTimer = 0.0f;

#if defined(_DEBUG)

        OutputDebugStringW(
            L"[PursuerMonster] "
            L"State -> Patrol\n"
        );

#endif

        break;
    }


    case State::Investigate:
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[PursuerMonster] "
            L"State -> Investigate\n"
        );

#endif


        break;
    }


    case State::Chase:
    {
        mChaseLostSightTimer = 0.0f;        // 見失いTimer Reset

#if defined(_DEBUG)

        OutputDebugStringW(
            L"[PursuerMonster] "
            L"State -> Chase\n"
        );

#endif
        break;
    }

    case State::Search:
    {
        //====================================================
        // Search開始時は、
        // まずPlayerを最後に見た位置へ向かう
        //====================================================
        mSearchPhase = SearchPhase::MoveToLastKnown;

        //====================================================
        // Search Pointはまだ未選択
        //====================================================
        mHasSearchPoint = false;

        //====================================================
        // Look Timer Reset
        //====================================================
        mSearchLookTimer = 0.0f;


#if defined(_DEBUG)

        OutputDebugStringW(
            L"[PursuerMonster] "
            L"State -> Search\n"
        );

#endif
        break;
    }

    case State::Attack:
    {
        //========================================================
        // Navigation停止
        // Attack中にPath Movementが残らないようにする
        //======================================================== 
        ClearNavigationPath();

        mAttackSoundPending = true;
        //========================================================
        // Attack Timeline Reset
        //========================================================
        mAttackTimer = 0.0f;
        mAttackHitProcessed = false;


#if defined(_DEBUG)

        OutputDebugStringW(
            L"[PursuerMonster] State -> Attack\n"
        );

#endif
        break;
    }

    }
}

bool PursuerMonster::HasLineOfSightToPlayer(const ExplorationPlayer& player) const
{
    //========================================================
    // Collider情報が無いなら、
    // 現段階では遮蔽物無しとして扱う
    //========================================================
    if (mSightColliders == nullptr)
    {
        return true;
    }


    //========================================================
    // Monster Eye Position
    //========================================================
    const XMVECTOR eyePosition =
        XMVectorSet(
            mPosition.x,
            mPosition.y + 0.75f,
            mPosition.z,
            1.0f
        );


    //========================================================
    // Player Target Position
    //========================================================
    const XMVECTOR playerBase = *player.getPosition();


    const XMVECTOR playerTarget =
        XMVectorAdd(
            playerBase,
            XMVectorSet(0.0f, 0.9f, 0.0f, 0.0f)
        );


    //========================================================
    // Eye → Player
    //========================================================
    const XMVECTOR toPlayer = playerTarget - eyePosition;

    const float playerDistance = XMVectorGetX(XMVector3Length(toPlayer));


    if (playerDistance <= 0.0001f)
    {
        return true;
    }

    const XMVECTOR rayDirection = XMVector3Normalize(toPlayer);

    //========================================================
    // Player直前までRayCast
    // ※Player自身のColliderを誤って遮蔽物判定しにくくするため、
    // ※少しだけ短くする
    //========================================================
    const float rayMaxDistance = (std::max)(playerDistance - 0.05f, 0.0f);


    //========================================================
    // Scene Collider
    //========================================================
    for (const Collider* collider : *mSightColliders)
    {
        if (collider == nullptr)
        {
            continue;
        }

        //============================================================
        // Pursuer自身のColliderは無視
        //============================================================
        if (collider->GetOwner() == mModel)
        {
            continue;
        }

        //====================================================
        // Disabled Doorなどは遮蔽しない
        //====================================================
        if (!collider->IsEnabled())
        {
            continue;
        }


        //====================================================
        // Player自身は無視する
        //====================================================
        if (collider->GetOwner() == &player)
        {
            continue;
        }

        float hitDistance = 0.0f;

        //====================================================
        // Playerより手前で何かにHitした
        //      ↓
        // 視線が遮られている
        //====================================================
        if (collider->RayCast(eyePosition, rayDirection, rayMaxDistance, hitDistance))
        {
            return false;
        }
    }


    //========================================================
    // 遮蔽物なし
    //========================================================
    return true;
}

bool PursuerMonster::IsNearPosition(const XMFLOAT3& target, float distance) const
{
    const float dx = target.x - mPosition.x;
    const float dz = target.z - mPosition.z;
    const float horizontalDistanceSq = dx * dx + dz * dz;
    const float arrivalSq = distance * distance;

    constexpr float VerticalTolerance = 2.0f;

    const float dy = std::fabs(target.y - mPosition.y);

    if (dy > VerticalTolerance)
    {
        return false;
    }

    return horizontalDistanceSq <= arrivalSq;
}

bool PursuerMonster::SelectSearchPoint()
{
    //========================================================
    // Search Pointなし
    //========================================================
    if (mSearchPoints.empty())
    {
        mHasSearchPoint = false;

        return false;
    }


    //========================================================
    // Search可能最大距離
    //========================================================
    float bestDistanceSq = mSearchPointSearchRadius * mSearchPointSearchRadius;

    bool found = false;
    std::size_t bestIndex = 0;


    //========================================================
    // LastKnownに最も近いPointを探す
    //========================================================
    for (std::size_t i = 0; i < mSearchPoints.size(); ++i)
    {
        const XMFLOAT3& point = mSearchPoints[i];

        const float dx = point.x - mLastKnownPlayerPosition.x;
        const float dy = point.y - mLastKnownPlayerPosition.y;
        const float dz = point.z - mLastKnownPlayerPosition.z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;


        //====================================================
        // 探索範囲内かつ現在のBestより近い
        //====================================================
        if (distanceSq < bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            bestIndex = i;
            found = true;
        }
    }

    //========================================================
    // 候補無し
    //========================================================
    if (!found)
    {
        mHasSearchPoint = false;
        return false;
    }

    //========================================================
    // 採用
    //========================================================
    mCurrentSearchPointIndex = bestIndex;
    mHasSearchPoint = true;

    return true;
}

void PursuerMonster::BeginLookAround()
{
    //========================================================
    // Timer Reset
    //========================================================
    mSearchLookTimer = 0.0f;

    //========================================================
    // 現在のForwardから基準Yawを求める
    //========================================================
    mSearchBaseYaw = atan2f(mForward.x, mForward.z);
}

bool PursuerMonster::UpdateLookAround(float dt)
{
    //========================================================
    // Timer
    //========================================================
    mSearchLookTimer += dt;


    //========================================================
    // 0 ～ 1
    //========================================================
    const float t =
        std::clamp(mSearchLookTimer / mSearchLookDuration, 0.0f, 1.0f);


    //========================================================
    // 左右へ振る角度
    //========================================================
    const float lookOffset =
        sinf(t * XM_2PI) * XMConvertToRadians(mSearchLookAngleDegrees);


    //========================================================
    // Facing
    //========================================================
    SetFacingYaw(mSearchBaseYaw + lookOffset);


    //========================================================
    // Finish
    //========================================================
    if (t >=1.0f)
    {
        //====================================================
        // 最後は元の方向へ戻す
        //====================================================
        SetFacingYaw(mSearchBaseYaw);
        return true;
    }


    return false;
}

void PursuerMonster::SetFacingYaw(float yaw)
{
    //========================================================
    // AI Forward
    //========================================================
    mForward.x = sinf(yaw);
    mForward.y = 0.0f;
    mForward.z = cosf(yaw);

    //========================================================
    // Model
    //========================================================
    if (mModel != nullptr)
    {
        mModel->setRotationY(yaw);
    }
}



bool PursuerMonster::TryHearNoise(XMFLOAT3& outPosition) const
{
    if (mNoiseSystem == nullptr)
    {
        return false;
    }

    //========================================================
    // 一番重要なNoiseを選ぶ
    //========================================================
    bool heard = false;

    float bestScore = -FLT_MAX;

    const auto& events = mNoiseSystem->GetEvents();


    for (const NoiseEvent& event : events)
    {
        //====================================================
        // Monster → Noise
        //====================================================
        const float dx = event.Position.x - mPosition.x;
        const float dy = event.Position.y - mPosition.y;
        const float dz = event.Position.z - mPosition.z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;
        const float radiusSq = event.Radius * event.Radius;


        //====================================================
        // Hearing Radius外
        //====================================================
        if (distanceSq > radiusSq)
        {
            continue;
        }


        //====================================================
        // 距離
        //====================================================
        const float distance = sqrtf(distanceSq);

        //====================================================
        // 0 ～ 1
        //====================================================
        const float distanceFactor =
            1.0f - std::clamp(distance / event.Radius, 0.0f, 1.0f);

        //====================================================
        // Noise Score
        // 距離 + Noise自体の優先度
        //====================================================
        const float score = distanceFactor * event.Priority;

        //====================================================
        // 現在のBestより弱いなら無視
        //====================================================
        if (score <= bestScore)
        {
            continue;
        }

        bestScore = score;

        outPosition = event.Position;

        heard = true;
    }

    return heard;
}

bool PursuerMonster::BuildNavigationPath(const XMFLOAT3& target)
{
    ClearNavigationPath();

    if (mNavigation == nullptr)
    {
        return false;
    }


    if (!mNavigation->FindPath(mPosition, target, mNavigationPath))
    {
        return false;
    }


    if (mNavigationPath.empty())
    {
        return false;
    }


    mNavigationGoal = target;

    mNavigationPathIndex = 0;

    mHasNavigationPath = true;

    return true;
}

void PursuerMonster::MoveTowardsNavigated(const XMFLOAT3& target, float speed, float dt, bool forceRepath)
{
    //============================================================
    // Navigationが存在しない場合
    // ※Navigationなしで同一Floorの移動を行いたいケースに備えて
    // ※直接移動へFallbackする
    //============================================================
    if (mNavigation == nullptr)
    {
        MoveTowards(target, speed, dt);
        return;
    }


    //============================================================
    // Targetが前回Path生成時からどれくらい移動したか
    // Chase中はPlayerが常に移動するため、
    // 毎Frame Pathを作り直すのではなく、
    // 一定距離以上Targetが動いた時だけRepathする
    //============================================================
    const float goalDx = target.x - mNavigationGoal.x;
    const float goalDy = target.y - mNavigationGoal.y;
    const float goalDz = target.z - mNavigationGoal.z;

    const float goalChangeSq =
        goalDx * goalDx + goalDy * goalDy + goalDz * goalDz;


    //============================================================
    // Repath Threshold
    // 1.0m以上Player / Targetが移動したら
    // Navigation Pathを更新する
    //============================================================
    constexpr float RepathDistance = 1.0f;

    const float repathDistanceSq = RepathDistance * RepathDistance;


    //============================================================
    // Pathを作る必要があるか
    // 1. まだPathを持っていない
    // 2. 明示的にRepath要求された
    // 3. Targetが一定以上動いた
    //============================================================
    const bool needsRepath =
        !mHasNavigationPath || forceRepath || goalChangeSq > repathDistanceSq;


    // 最終追跡で目標が頻繁に動いても、開き戸を回り込む途中はその経路を完了する。
    const bool keepDoorDetour = mFinalChase && mHasNavigationPath && mDoorDetourRemaining > 0;
    mFinalChasePathRetry = (std::max)(0.0f, mFinalChasePathRetry - dt);

    if (needsRepath && !keepDoorDetour)
    {
        if (mFinalChase && mFinalChasePathRetry > 0.0f)
        {
            return;
        }

        //========================================================
        // Path生成
        //========================================================
        if (!BuildNavigationPath(target))
        {
            // 施錠や未接続で経路が見つからなくても、脱出中はChaseのまま再試行する。
            if (mFinalChase)
            {
                mFinalChasePathRetry = 0.30f;
            }
            return;
        }
    }


    //============================================================
    // Safety
    // BuildNavigationPath成功後でも
    // Pathが無ければ移動しない
    //============================================================
    if (!mHasNavigationPath || mNavigationPath.empty())
    {
        return;
    }


    //============================================================
    // Path終了確認
    //============================================================
    if (mNavigationPathIndex >= mNavigationPath.size())
    {
        ClearNavigationPath();
        return;
    }

    //============================================================
    // 現在向かっているWaypoint
    //============================================================
    XMFLOAT3 waypoint = mNavigationPath[mNavigationPathIndex];

    const float arrivalDistance = mDoorDetourRemaining > 0 ? 0.06f : mNavigationArrivalDistance;

    //============================================================
    // Waypoint到着判定
    // 3D NavigationなのでXYZ距離で判定する
    //============================================================
    if (IsNearNavigationWaypoint(waypoint, arrivalDistance))
    {
        if (mDoorDetourRemaining > 0)
        {
            --mDoorDetourRemaining;
        }

        //========================================================
        // 次のWaypointへ進める
        //========================================================
        ++mNavigationPathIndex;

        //========================================================
        // Pathの最後まで到着
        //========================================================
        if (mNavigationPathIndex >= mNavigationPath.size())
        {
            ClearNavigationPath();

            return;
        }


        //========================================================
        // 次Waypoint
        //========================================================
        waypoint = mNavigationPath[mNavigationPathIndex];
    }


    //============================================================
    // Door Check
    // 現在のMonster位置
    //      ↓
    // 次Waypoint
    //============================================================
    if (HandleNearbyDoor(waypoint))
    {
        return;
    }

    mDoorDetourRetry = (std::max)(0.0f, mDoorDetourRetry - dt);

    if (mDoorDetourRemaining == 0 && mDoorDetourRetry <= 0.0f)
    {
        // 通れる候補がないときも毎フレーム再検索しない
        mDoorDetourRetry = 0.20f;

        if (TryInsertOpenDoorDetour(waypoint))
        {
            waypoint = mNavigationPath[mNavigationPathIndex];
        }
    }

    //============================================================
    // Navigation Movement
    //============================================================
    MoveTowardsNavigationWaypoint(waypoint, speed, dt);
}

void PursuerMonster::ClearNavigationPath()
{
    mNavigationPath.clear();
    mNavigationPathIndex = 0;
    mDoorDetourRemaining = 0;
    mHasNavigationPath = false;
}

bool PursuerMonster::HandleNearbyDoor(const XMFLOAT3& waypoint)
{
    if (mNavigationDoors == nullptr)
    {
        return false;
    }

    //========================================================
    // Monster → Waypoint
    // 現在Monsterがこれから進むPath Segment
    //========================================================
    const float segmentX = waypoint.x - mPosition.x;
    const float segmentZ = waypoint.z - mPosition.z;
    const float segmentLengthSq = segmentX * segmentX + segmentZ * segmentZ;

    //========================================================
    // Segmentがほぼ0ならDoor判定不要
    //========================================================
    if (segmentLengthSq <= 0.000001f)
    {
        return false;
    }


    for (INavigationDoor* door : *mNavigationDoors)
    {
        if (door == nullptr)
        {
            continue;
        }

        //====================================================
        // 完全に開いているDoorは無視
        //====================================================
        if (door->IsOpen())
        {
            continue;
        }

        const XMFLOAT3& doorPosition = door->GetDoorPosition();

        //====================================================
        // MonsterからDoorまでの距離を確認
        //====================================================
        const float monsterToDoorX = doorPosition.x - mPosition.x;
        const float monsterToDoorZ = doorPosition.z - mPosition.z;
        const float monsterToDoorDistanceSq = monsterToDoorX * monsterToDoorX + monsterToDoorZ * monsterToDoorZ;
        const float interactionDistanceSq = mDoorInteractionDistance * mDoorInteractionDistance;


        if (monsterToDoorDistanceSq > interactionDistanceSq)
        {
            continue;
        }

        //====================================================
        // Doorが
        // Monster → Waypoint
        // のどの位置にあるかをProjectionで求める
        // t = 0 Monster位置
        // t = 1 Waypoint位置
        //====================================================
        float t = (monsterToDoorX * segmentX + monsterToDoorZ * segmentZ) / segmentLengthSq;


        //====================================================
        // DoorがMonsterの後ろ、
        // またはWaypointよりかなり先なら対象外
        //====================================================
        if (t < 0.0f || t >1.0f)
        {
            continue;
        }


        //====================================================
        // Segment上でDoorに最も近い位置
        //====================================================
        const float closestX = mPosition.x + segmentX * t;
        const float closestZ = mPosition.z + segmentZ * t;


        //====================================================
        // Door中心とPath Segmentの距離
        //====================================================
        const float pathDx = doorPosition.x - closestX;
        const float pathDz = doorPosition.z - closestZ;
        const float pathDistanceSq = pathDx * pathDx + pathDz * pathDz;
        const float doorPathWidthSq = mDoorPathWidth * mDoorPathWidth;


        //====================================================
        // 現在のPathから横に離れているDoor
        //====================================================
        if (pathDistanceSq > doorPathWidthSq)
        {
            continue;
        }

        //============================================================
        // Path作成後にDoorが通行不可になったケース
        // 古いPathをそのまま使わず破棄する
        // 次Frameで新しいPathを検索する
        //============================================================
        if (!door->IsPassableForAI())
        {
            ClearNavigationPath();

#if defined(_DEBUG)

            OutputDebugStringW(
                L"[PursuerMonster] "
                L"Navigation path invalidated by locked door.\n"
            );

#endif
            return true;
        }

        //====================================================
        // 現在進もうとしているPath上にDoorがある
        // ClosedならOpening開始
        // RequestOpenFromAI()がboolを返しているなら
        // true = 今この瞬間にOpening開始
        //====================================================
        const bool startedOpening = door->RequestOpenFromAI();


#if defined(_DEBUG)

        if (startedOpening)
        {
            OutputDebugStringW(
                L"[PursuerMonster] "
                L"Opening path door.\n"
            );
        }

#endif

        //====================================================
        // Doorが完全にOpenになるまでは
        // Monsterをその場で待機させる
        //====================================================
        return true;
    }


    return false;
}

bool PursuerMonster::SelectReachablePatrolPoint()
{
    std::vector<std::size_t> candidates;
    candidates.reserve(mPatrolPoints.size());

    for (std::size_t i = 0; i < mPatrolPoints.size(); ++i)
    {
        if (!IsNearPosition(mPatrolPoints[i], mPatrolArrivalDistance))
        {
            candidates.push_back(i);
        }
    }

    // 先にシャッフルし、最近訪れた候補は他に行けないときの予備へ回す。
    std::shuffle(candidates.begin(), candidates.end(), mPatrolRandom);
    const auto recentBegin = std::stable_partition(candidates.begin(), candidates.end(),
        [this](std::size_t i)
        {
            return std::find(mRecentPatrolPoints.begin(), mRecentPatrolPoints.end(), i) ==
                mRecentPatrolPoints.end();
        });

    // 75%は近場を先に試す。残り25%では離れたエリアを先に試す。
    // 実際に選ばれるのは、既存Navigationで通れる候補だけ。
    const bool preferLocal = std::uniform_int_distribution<int>(0, 3)(mPatrolRandom) != 0;
    std::stable_partition(candidates.begin(), recentBegin,
        [this, preferLocal](std::size_t i)
        {
            const XMFLOAT3& target = mPatrolPoints[i];
            const float dx = target.x - mPosition.x;
            const float dz = target.z - mPosition.z;
            const bool local = std::fabs(target.y - mPosition.y) <= 1.6f &&
                dx * dx + dz * dz <= 18.0f * 18.0f;
            return local == preferLocal;
        });

    for (const std::size_t index : candidates)
    {
        if (mNavigation != nullptr && !BuildNavigationPath(mPatrolPoints[index]))
        {
            continue;
        }

        mPatrolIndex = index;
        return true;
    }

    return false;
}



