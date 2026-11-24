#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"boxCollider.h"
#include"capsuleCollider.h"
#include"IPhysicsCharacter.h"

bool BoxCollider::RayCast(const XMVECTOR& start, const XMVECTOR& dir, float maxDistance, float& outDistance) const
{
    //========================================================
    // Disabled Colliderは視線を遮らない
    // DoorがOpenになってColliderがDisableされた場合も
    // そのまま視線が通る
    //========================================================
    if (!IsEnabled())
    {
        return false;
    }

    const XMVECTOR localStart = ToLocalPoint(start);
    const XMVECTOR localDir = ToLocalDirection(dir);

    //========================================================
    // Box Bounds
    //========================================================
    const XMVECTOR half = GetHalfSize();

    const XMVECTOR boxMin = -half;
    const XMVECTOR boxMax = half;


    //========================================================
    // Vector → scalar
    //========================================================
    const float sx = XMVectorGetX(localStart);
    const float sy = XMVectorGetY(localStart);
    const float sz = XMVectorGetZ(localStart);

    const float dx = XMVectorGetX(localDir);
    const float dy = XMVectorGetY(localDir);
    const float dz = XMVectorGetZ(localDir);

    const float minX = XMVectorGetX(boxMin);
    const float minY = XMVectorGetY(boxMin);
    const float minZ = XMVectorGetZ(boxMin);
    const float maxX = XMVectorGetX(boxMax);
    const float maxY = XMVectorGetY(boxMax);
    const float maxZ = XMVectorGetZ(boxMax);


    //========================================================
    // Ray parameter range
    // start + dir * t
    //========================================================
    float tMin = 0.0f;
    float tMax = maxDistance;


    //========================================================
    // 1軸分のSlab判定
    //========================================================
    auto TestAxis = [&](float rayStart, float rayDir, float slabMin, float slabMax) -> bool
        {
            constexpr float epsilon = 0.000001f;


            //================================================
            // Rayがこの軸にほぼ平行
            //================================================
            if (fabsf(rayDir) < epsilon)
            {
                //============================================
                // 平行な場合、
                // startがSlab内部にいなければ絶対Hitしない
                //============================================
                return rayStart >= slabMin && rayStart <= slabMax;
            }


            //================================================
            // Slabへ入るt / 出るt
            //================================================
            const float invDir = 1.0f / rayDir;
            float t1 = (slabMin - rayStart) * invDir;
            float t2 = (slabMax - rayStart) * invDir;


            //================================================
            // Directionが負だとt1/t2が逆になる
            //================================================
            if (t1 > t2)
            {
                std::swap(t1, t2);
            }


            //================================================
            // 全軸で共通して存在する区間へ絞る
            //================================================
            tMin = (std::max)(tMin, t1);
            tMax = (std::min)(tMax, t2);


            return tMin <= tMax;
        };


    //========================================================
    // X
    //========================================================
    if (!TestAxis(sx, dx, minX, maxX))
    {
        return false;
    }


    //========================================================
    // Y
    //========================================================
    if (!TestAxis(sy, dy, minY, maxY))
    {
        return false;
    }


    //========================================================
    // Z
    //========================================================
    if (!TestAxis(sz, dz, minZ, maxZ))
    {
        return false;
    }


    //========================================================
    // Ray前方かつ範囲内
    //========================================================
    if (tMax < 0.0f)
    {
        return false;
    }

    if (tMin > maxDistance)
    {
        return false;
    }

    //========================================================
    // Hit Distance
    //========================================================
    outDistance = (std::max)(tMin, 0.0f);

    return true;
}

// ダブルディスパッチ用：相手側に boxCollider として判定させる
bool BoxCollider::CollisionCheck(const Collider* other) const {
    // 有効化されていない場合は判定しない
    if (!this->IsEnabled() || !other->IsEnabled()) {
        return false;
    }
    return other->CollisionWithBox(this);
}

/// Box vs Sphereの衝突判定
bool BoxCollider::CollisionWithSphere(const SphereCollider* sphere) const 
{
    if (sphere == nullptr)
    {
        return false;
    }

    //========================================================
    // Sphere中心をBoxローカル空間へ変換
    //========================================================
    const XMVECTOR localCenter =
        ToLocalPoint(sphere->GetCenter());

    //========================================================
    // Boxはローカル空間では回転していないAABB
    //========================================================
    const XMVECTOR boxMin = -halfSize;
    const XMVECTOR boxMax = halfSize;

    // Sphere中心からBox上の最近傍点を取得
    const XMVECTOR closest =
        XMVectorClamp(localCenter, boxMin, boxMax);

    const XMVECTOR delta = localCenter - closest;

    const float distanceSq =
        XMVectorGetX(XMVector3LengthSq(delta));

    const float radius = sphere->GetRadius();

    return distanceSq < radius * radius;
}

// Box vs Box AABBで判定(各軸での重なりをみて重なっていれば衝突)
bool BoxCollider::CollisionWithBox(const BoxCollider* box) const
{
    if (box == nullptr)
    {
        return false;
    }

    //========================================================
    // まず高さ方向の重なりを判定
    //========================================================
    const XMVECTOR centerA = GetCenter();
    const XMVECTOR centerB = box->GetCenter();

    const float distanceY = fabsf(XMVectorGetY(centerA) - XMVectorGetY(centerB));
    const float heightSum = XMVectorGetY(halfSize) + XMVectorGetY(box->GetHalfSize());

    if (distanceY > heightSum)
    {
        return false;
    }

    //========================================================
    // 各Boxの回転したローカルX/Z軸
    //========================================================
    const XMVECTOR unitX = XMVectorSet(1, 0, 0, 0);
    const XMVECTOR unitZ = XMVectorSet(0, 0, 1, 0);

    const XMVECTOR axisAX = ToWorldDirection(unitX);
    const XMVECTOR axisAZ = ToWorldDirection(unitZ);

    const XMVECTOR axisBX = box->ToWorldDirection(unitX);
    const XMVECTOR axisBZ = box->ToWorldDirection(unitZ);

    const XMVECTOR axes[4] =
    {
        axisAX,
        axisAZ,
        axisBX,
        axisBZ
    };

    //========================================================
    // 指定軸へのBoxの投影半径
    //========================================================
    auto ProjectRadius = [](const BoxCollider* target, const XMVECTOR& axis) -> float
        {
            const XMVECTOR xAxis = target->ToWorldDirection(XMVectorSet(1, 0, 0, 0));
            const XMVECTOR zAxis = target->ToWorldDirection(XMVectorSet(0, 0, 1, 0));
            const XMVECTOR half = target->GetHalfSize();

            const float projectionX = fabsf(XMVectorGetX(XMVector3Dot(axis, xAxis)));
            const float projectionZ = fabsf(XMVectorGetX(XMVector3Dot(axis, zAxis)));

            return projectionX * XMVectorGetX(half) + projectionZ * XMVectorGetZ(half);
        };

    //========================================================
    // いずれかの軸で分離していれば衝突なし
    //========================================================
    const XMVECTOR delta = centerB - centerA;

    for (const XMVECTOR& axis : axes)
    {
        const float distance = fabsf(XMVectorGetX(XMVector3Dot(delta, axis)));
        const float radiusA = ProjectRadius(this, axis);
        const float radiusB = ProjectRadius(box, axis);

        if (distance > radiusA + radiusB)
        {
            return false;
        }
    }

    return true;
}

bool BoxCollider::CollisionWithMesh(const MeshCollider* mesh) const
{
    // 今は未実装
    return false;
}

bool BoxCollider::CollisionWithCapsule(const CapsuleCollider* capsule) const
{
    if (capsule == nullptr)
    {
        return false;
    }


    return capsule->CollisionWithBox(this);
}

// 現状の押し出しは SphereCollider との組み合わせのみ対応
void BoxCollider::Resolve(const Collider* other) const
{
    const SphereCollider* sphere = dynamic_cast<const SphereCollider*>(other);


    if (sphere == nullptr)
    {
        return;
    }


    //============================================================
    // Center
    //============================================================
    const XMVECTOR boxCenter = GetCenter();
    const XMVECTOR sphereCenter = sphere->GetCenter();

    const XMVECTOR min = boxCenter - halfSize;
    const XMVECTOR max = boxCenter + halfSize;


    //============================================================
    // Box上の最近傍点
    //============================================================
    const XMVECTOR closest = XMVectorClamp(sphereCenter, min, max);
    const XMVECTOR delta = sphereCenter - closest;


    const float dist = XMVectorGetX(XMVector3Length(delta));
    const float overlap = sphere->GetRadius() - dist;


    if (overlap <= 0.0f)
    {
        return;
    }


    //============================================================
    // Resolve Direction
    //============================================================
    XMVECTOR direction;


    if (dist < 0.0001f)
    {
        //========================================================
        // Sphere中心がBox内部などで
        // Directionを計算できない場合
        // 上方向へ押す
        //========================================================
        direction =
            XMVectorSet(
                0.0f,
                1.0f,
                0.0f,
                0.0f
            );
    }
    else
    {
        direction = XMVector3Normalize(delta);
    }


    //============================================================
    // SphereをBox外へ押し戻す
    //============================================================
    if (sphere->GetOwner() != nullptr)
    {
        sphere->GetOwner()->Translate(direction * overlap);
    }


    //============================================================
    // Ground判定
    //============================================================
    if (XMVectorGetY(direction) > 0.95f)
    {
        IPhysicsCharacter* physics = sphere->GetOwnerPhysicsCharacter();


        if (physics != nullptr)
        {
            physics->SetGrounded(true);


            //====================================================
            // 落下中のみVertical Speed停止
            //====================================================
            if (physics->GetVerticalSpeed() <= 0.0f)
            {
                physics->SetVerticalSpeed(0.0f);
            }
        }
    }
}
