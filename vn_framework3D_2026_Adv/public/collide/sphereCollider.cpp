#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "sphereCollider.h"
#include "capsuleCollider.h"
#include "IPhysicsCharacter.h"


// ダブルディスパッチ用：相手側に SphereCollider として判定させる
bool SphereCollider::CollisionCheck(const Collider* other) const {
    // 有効化されていない場合は判定しない
    if (!this->IsEnabled() || !other->IsEnabled()) {
        return false;
    }
    return other->CollisionWithSphere(this);
}
// 球vs球の衝突判定
bool SphereCollider::CollisionWithSphere(const SphereCollider* sphere) const {
    XMVECTOR delta = GetCenter() - sphere->GetCenter();
    float dist = XMVectorGetX(XMVector3Length(delta));
    return dist < (radius + sphere->GetRadius());
}
// Sphere vs Box は Box 側の判定ロジックを再利用する
bool SphereCollider::CollisionWithBox(const BoxCollider* box) const {

    return box->CollisionWithSphere(this);
}

bool SphereCollider::CollisionWithMesh(const MeshCollider* mesh) const
{
    XMVECTOR center = GetCenter();

    for (const auto& tri : mesh->GetTriangles())
    {
        XMVECTOR closest =
            vnCollide::ClosestPointOnTriangle(center, tri);

        XMVECTOR diff = center - closest;
        float dist = XMVectorGetX(XMVector3Length(diff));

        if (dist < radius)
            return true;
    }
    return false;

}

bool SphereCollider::CollisionWithCapsule(const CapsuleCollider* capsule) const
{
    if (capsule == nullptr)
    {
        return false;
    }

    //========================================================
    // Capsule側の実装を使う
    //========================================================
    return capsule->CollisionWithSphere(this);
}

void SphereCollider::ResolveSphere(const SphereCollider* other) const
{
    XMVECTOR centerA = GetCenter();
    XMVECTOR centerB = other->GetCenter();

    XMVECTOR delta = centerA - centerB;

    float dist = XMVectorGetX(XMVector3Length(delta));
    float totalRadius = radius + other->GetRadius();

    // 当たっていない or 完全一致
    if (dist >= totalRadius || dist <= 0.0001f)
        return;

    float penetration = totalRadius - dist;

    // 法線
    XMVECTOR normal = XMVector3Normalize(delta);

    float massA = this->GetMass();
    float massB = other->GetMass();

    float totalMass = massA + massB;

    // 押し出し割合（軽い方が多く動く）
    float ratioA = massB / totalMass;
    float ratioB = massA / totalMass;

    // キャラクター同士の横押し出しを優先するため、Y方向の押し出しは行わない
    normal = XMVectorSetY(normal, 0.0f);
    normal = XMVector3Normalize(normal);

    *mOwner->getPosition() += normal * (penetration * ratioA);
    *other->mOwner->getPosition() -= normal * (penetration * ratioB);
}

void SphereCollider::Resolve(const Collider* other) const
{
    const MeshCollider* mesh = dynamic_cast<const MeshCollider*>(other);


    if (mesh == nullptr)
    {
        return;
    }

    //============================================================
    // Sphere Center
    //============================================================
    const XMVECTOR center = GetCenter();

    float maxPush = 0.0f;

    XMVECTOR bestNormal = XMVectorZero();

    bool hitGround = false;


    //============================================================
    // Mesh Triangle全部について
    // 最も深くめり込んでいるTriangleを探す。
    //============================================================
    for (const auto& tri : mesh->GetTriangles())
    {
        const XMVECTOR closest = vnCollide::ClosestPointOnTriangle(center, tri);


        const XMVECTOR diff = center - closest;


        const float dist =
            XMVectorGetX(XMVector3Length(diff));


        //========================================================
        // Sphere外なら衝突していない
        //========================================================
        if (dist >= radius)
        {
            continue;
        }


        const float push = radius - dist;


        //========================================================
        // 一番深い衝突を使用
        //========================================================
        if (push > maxPush)
        {
            maxPush = push;


            //====================================================
            // 完全一致の場合はTriangle Normalを使用
            //====================================================
            if (dist < 1e-6f)
            {
                bestNormal = tri.plane.Normal;
            }
            else
            {
                bestNormal = XMVector3Normalize(diff);
            }


            //====================================================
            // 上方向NormalならGroundと判定
            //====================================================
            hitGround = (XMVectorGetY(bestNormal) > 0.5f);
        }
    }


    //============================================================
    // Collision Resolve
    //============================================================
    if (maxPush <= 0.0f)
    {
        return;
    }


    //============================================================
    // Sphere OwnerをMesh外へ押し戻す
    //============================================================
    *mOwner->getPosition() += bestNormal * maxPush;


    //============================================================
    // Ground Hit
    //============================================================
    if (hitGround)
    {
        //========================================================
        // Character具体型ではなく
        // Physics Interfaceのみを見る
        //========================================================
        IPhysicsCharacter* physics = GetOwnerPhysicsCharacter();


        if (physics != nullptr)
        {
            physics->SetGrounded(true);


            //====================================================
            // 落下中だけVertical Speedを停止。
            //
            // 上昇中まで強制停止しない。
            //====================================================
            if (physics->GetVerticalSpeed() <= 0.0f)
            {
                physics->SetVerticalSpeed(0.0f);
            }
        }
    }
}
