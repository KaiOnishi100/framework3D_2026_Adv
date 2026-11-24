#pragma once

class SlowSystem; 
class TutorialManager;

//============================================================
// 衝突管理クラス
// ・Collider 同士の総当たり
// ・攻撃ヒット処理
// ・キャラクター同士の押し出し
// ・地形などの Resolve 呼び出し
//============================================================
class CollisionManager
{
private:
    //========================================================
    // 衝突ペアの処理用コンテキスト
    // ・毎回 a / b / charA / charB を個別に持たずに済む
    //========================================================
    struct CollisionContext
    {
        Collider* a = nullptr;
        Collider* b = nullptr;

        Character* charA = nullptr;
        Character* charB = nullptr;
    };

private:
    // ペアが有効かどうか
    static bool IsValidPair(const Collider* a, const Collider* b);

    // 衝突ペアから Character 情報を組み立てる
    static CollisionContext BuildContext(Collider* a, Collider* b);

    // 種別判定補助
    static bool IsPlayerCharacter(const Character* c);
    static bool IsBossCharacter(const Character* c);

    // 攻撃ヒット本体
    static void DoAttackHit(
        Collider* atkCol,
        Collider* bodyCol,
        Character* atkCh,
        Character* bodyCh,
        SlowSystem* slow,
        bool& skipCharacterResolve);

    // ジャスト回避スロー中の強制命中
    static void HandleJustAvoidForcedHit(
        const CollisionContext& ctx,
        SlowSystem* slow,
        bool& skipCharacterResolve);

    // 通常の Attack vs Body
    static void HandleNormalAttackHit(
        const CollisionContext& ctx,
        SlowSystem* slow,
        bool& skipCharacterResolve);

    // Body vs Body の押し出し
    static void HandleBodyResolve(
        const CollisionContext& ctx,
        bool skipCharacterResolve);

    // Mesh 等の Resolve
    static void HandlePhysicsResolve(Collider* a, Collider* b);

    //============================================================
    // Tagged Gameplay Attack
    //============================================================
    static void HandleTaggedAttack(const CollisionContext& ctx);

public:
    static void Process(
        std::vector<Collider*>& colliders,
        SlowSystem* slow,
        TutorialManager* tutorial);
};