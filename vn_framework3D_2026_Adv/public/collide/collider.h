#pragma once
#include "../object/EyeKillYou_Object/character.h"

class IPhysicsCharacter;
class IDamageable;
class CapsuleCollider;
enum class ColliderType
{
    Body,       // 通常の物理
    Attack,     // 攻撃判定
    Knockback   // ノックバック中の物理状態
};

//============================================================
// SurfaceType
// 接触面の物理材質
//============================================================
enum class SurfaceType
{
    Default,
    Concrete,
    Metal,
    Wood,
    Grass,
    Dirt,
    Carpet,
    Tile,
    Water,
    Flesh
};

//============================================================
// ColliderTag
//============================================================
enum class ColliderTag : std::uint32_t
{
    None = 0,
    //========================================================
    // Character
    //========================================================
    Player = 1u << 0,
    Pursuer = 1u << 1,
    //========================================================
    // World
    //========================================================
    World =1u << 2,
    Door =1u << 3,
    //========================================================
    // Gameplay
    //========================================================
    Interactable =1u << 4,
    PlayerAttack =1u << 5,
    PursuerAttack =1u << 6,
    Pickup =1u << 7,
    Trigger =1u << 8
};

//============================================================
// OR
//============================================================
inline ColliderTag operator|(ColliderTag lhs, ColliderTag rhs)
{
    return static_cast<ColliderTag>(
        static_cast<std::uint32_t>(lhs) |
        static_cast<std::uint32_t>(rhs));
}


//============================================================
// AND
//============================================================
inline ColliderTag operator&(ColliderTag lhs, ColliderTag rhs)
{
    return static_cast<ColliderTag>(
        static_cast<std::uint32_t>(lhs) &
        static_cast<std::uint32_t>(rhs)
        );
}

//============================================================
// Attack Data
//============================================================
struct ColliderAttackData
{
    float Damage = 0.0f;
};

//コライダーの基底クラス
class Collider {
protected:

    ObjectBase* mOwner;      // このコライダーを持つオブジェクト
    bool mEnabled;        // 衝突判定を行うかどうか
    float mMass = 1.0f;   // 押し出し計算に使う質量

    ColliderType mType = ColliderType::Body;
    ColliderAttackData mAttackData;
public:

    explicit Collider(ObjectBase* owner) : mOwner(owner), mEnabled(true) {}
    virtual ~Collider() = default;

    ColliderType GetType() const { return mType; }

    bool IsAttack() const { return mType == ColliderType::Attack; }
    bool IsBody() const { return mType == ColliderType::Body; }
    bool IsKnockBack()const { return mType == ColliderType::Knockback; }

    void SetMass(float m) { mMass = m; }
    float GetMass() const { return mMass; }

    void SetEnabled(bool e) { mEnabled = e; }
    bool IsEnabled() const { return mEnabled; }

    void SetType(ColliderType colType) { mType = colType; }

    ObjectBase* GetOwner() const { return mOwner; }

    // owner は Object 継承型が設定される前提で、
    // Character であればそのポインタを返す
    Character* GetOwnerCharacter() const;

    IPhysicsCharacter* GetOwnerPhysicsCharacter() const;

    //============================================================
    // Damage Interface
    //============================================================
    IDamageable* GetOwnerDamageable() const;

    //============================================================
    // Gameplay Tag
    //============================================================
    ColliderTag mTag = ColliderTag::None;

    SurfaceType mSurfaceType = SurfaceType::Default;

    //============================================================
    // Ray Cast
    //============================================================
    virtual bool RayCast(
        const XMVECTOR& start,
        const XMVECTOR& dir,
        float maxDistance,
        float& outDistance
    ) const
    {
        (void)start;
        (void)dir;
        (void)maxDistance;
        (void)outDistance;

        return false;
    }

    // 押し出し
    virtual void Resolve(const Collider* other) const = 0;

    // 総合衝突チェック
    virtual bool CollisionCheck(const Collider* other) const = 0;

    // ダブルディスパッチ
    virtual bool CollisionWithSphere(const class SphereCollider* sphere) const = 0;
    virtual bool CollisionWithBox(const class BoxCollider* box) const = 0;
    virtual bool CollisionWithMesh(const class MeshCollider* mesh) const = 0;
    virtual bool CollisionWithCapsule(const class CapsuleCollider* capsule) const = 0;

    //============================================================
    // Gameplay Tag
    //============================================================
    void SetTag(ColliderTag tag)
    {
        mTag = tag;
    }

    //============================================================
    // Tag追加
    //============================================================
    void AddTag(ColliderTag tag)
    {
        mTag = mTag | tag;
    }

    //============================================================
    // Tag判定
    //============================================================
    bool HasTag(ColliderTag tag) const
    {
        return
            (static_cast<std::uint32_t>(mTag) & static_cast<std::uint32_t>(tag)) != 0;
    }

    //============================================================
    // Tag取得
    //============================================================
    ColliderTag GetTag() const
    {
        return mTag;
    }

    //============================================================
    // Surface
    //============================================================
    void SetSurfaceType(SurfaceType surfaceType)
    {
        mSurfaceType = surfaceType;
    }

    SurfaceType GetSurfaceType() const
    {
        return mSurfaceType;
    }

    //========================================================
    // Attack Damage
    //========================================================
    void SetAttackDamage(float damage)
    {
        mAttackData.Damage = damage;
    }

    float GetAttackDamage() const
    {
        return mAttackData.Damage;
    }
};
