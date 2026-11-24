#pragma once
#include "../vn_collide.h"

class MeshCollider : public Collider
{
private:
    // モデル頂点からローカル座標系の三角形配列を構築する
    std::vector<vnCollide::stTriangle> mTriangles;

    ModelBase* mMeshOwner = nullptr;
public:
    explicit MeshCollider(ModelBase* owner);

    // vnModel → 三角形配列生成
    void BuildTrianglesFromModel();

    const std::vector<vnCollide::stTriangle>& GetTriangles() const
    {
        return mTriangles;
    }

    // Meshは基本押されない
    void Resolve(const Collider* other) const override {}

    bool RayCast(
        const XMVECTOR& start,
        const XMVECTOR& dir,
        float maxDistance,
        float& outDist
    ) const override;

    bool CollisionCheck(const Collider* other) const override;
    bool CollisionWithSphere(const SphereCollider* sphere) const override;
    bool CollisionWithBox(const BoxCollider* box) const override { return false; }// 現状、Mesh vs Box は使用していないため未対応
    bool CollisionWithMesh(const MeshCollider* mesh) const override { return false; }
    bool CollisionWithCapsule(const CapsuleCollider* capsule) const override;
};