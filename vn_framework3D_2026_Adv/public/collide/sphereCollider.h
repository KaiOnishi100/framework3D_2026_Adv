#pragma once

class SphereCollider : public Collider
{
private:
    float radius;
    XMVECTOR offset;   // モデル中心からのズレ
public:
    SphereCollider(ObjectBase* owner, float r, XMVECTOR offset = XMVectorZero())
        : Collider(owner), radius(r), offset(offset) {
    }

    XMVECTOR GetCenter() const { return *mOwner->getPosition() + offset; }
    float GetRadius() const { return radius; }

    //============================================================
    // Offset
    //============================================================
    void SetOffset(const XMVECTOR& newOffset) { offset = newOffset; }

    // 基本の判定
    bool CollisionCheck(const Collider* other) const override;
    bool CollisionWithSphere(const SphereCollider* sphere) const override;
    bool CollisionWithBox(const class BoxCollider* box) const override;
    bool CollisionWithMesh(const MeshCollider* mesh) const override;
    bool CollisionWithCapsule(const CapsuleCollider* capsule) const override;
    //押し出し処理
    // Sphere vs Sphere 専用の押し出し解決
    void ResolveSphere(const SphereCollider* other) const;
    // Collider 基底インターフェース経由の押し出し解決
    // 現状は MeshCollider との組み合わせのみ対応
    void Resolve(const Collider* other)const override;
};