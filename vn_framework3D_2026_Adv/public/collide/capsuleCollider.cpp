#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "capsuleCollider.h"

//============================================================
// Closest Point On Segment
//============================================================
static XMVECTOR ClosestPointOnSegment(
    const XMVECTOR& point,
    const XMVECTOR& segmentA,
    const XMVECTOR& segmentB
)
{
    const XMVECTOR ab = segmentB - segmentA;
    const float abLengthSq = XMVectorGetX(XMVector3LengthSq(ab));

    if (abLengthSq <= 0.000001f)
    {
        return segmentA;
    }


    const XMVECTOR ap =  point - segmentA;


    float t = XMVectorGetX(XMVector3Dot(ap, ab)) / abLengthSq;


    t = std::clamp(  t,  0.0f,   1.0f );


    return segmentA + ab * t;
}

bool CapsuleCollider::CollisionCheck(const Collider* other) const
{
    //========================================================
    // Disabled
    //========================================================
    if (!IsEnabled() || other == nullptr || !other->IsEnabled())
    {
        return false;
    }

    //========================================================
    // Double Dispatch
    //========================================================
    return other->CollisionWithCapsule(this);
}

bool CapsuleCollider::CollisionWithSphere(const SphereCollider* sphere) const
{
    if (sphere == nullptr)
    {
        return false;
    }

    //========================================================
    // Capsule Segment
    //========================================================
    const XMVECTOR bottom = GetSegmentBottom();
    const XMVECTOR top = GetSegmentTop();

    //========================================================
    // Sphere Centerに最も近いCapsule Segment上の点
    //========================================================
    const XMVECTOR closest =
        ClosestPointOnSegment(
            sphere->GetCenter(),
            bottom,
            top
        );


    //========================================================
    // Distance
    //========================================================
    const XMVECTOR delta = sphere->GetCenter() - closest;


    const float distanceSq = XMVectorGetX(XMVector3LengthSq(delta));
    const float totalRadius = mRadius + sphere->GetRadius();

    return distanceSq < totalRadius * totalRadius;
}

bool CapsuleCollider::CollisionWithBox(const BoxCollider* box) const
{
    if (box == nullptr)
    {
        return false;
    }

    //========================================================
    // Capsule中心をBoxローカル座標へ変換
    //========================================================
    const XMVECTOR localCenter = box->ToLocalPoint(GetCenter());

    const float cx = XMVectorGetX(localCenter);
    const float cy = XMVectorGetY(localCenter);
    const float cz = XMVectorGetZ(localCenter);

    const XMVECTOR half = box->GetHalfSize();

    const float hx = XMVectorGetX(half);
    const float hy = XMVectorGetY(half);
    const float hz = XMVectorGetZ(half);

    //========================================================
    // Boxローカル空間のAABB範囲
    //========================================================
    const float minX = -hx;
    const float maxX = hx;
    const float minY = -hy;
    const float maxY = hy;
    const float minZ = -hz;
    const float maxZ = hz;

    //========================================================
    // Capsule中央線分の上下
    //========================================================
    const float capsuleMinY = cy - mHalfSegmentHeight;
    const float capsuleMaxY = cy + mHalfSegmentHeight;

    //========================================================
    // 水平方向の最近傍点
    //========================================================
    const float closestX = std::clamp(cx, minX, maxX);
    const float closestZ = std::clamp(cz, minZ, maxZ);

    const float dx = cx - closestX;
    const float dz = cz - closestZ;

    //========================================================
    // 縦線分とBoxのY区間の距離
    //========================================================
    float dy = 0.0f;

    if (capsuleMaxY < minY)
    {
        dy = minY - capsuleMaxY;
    }
    else if (capsuleMinY > maxY)
    {
        dy = capsuleMinY - maxY;
    }

    //========================================================
    // Capsule Segment vs OBB
    //========================================================
    const float distanceSq = dx * dx + dy * dy + dz * dz;

    return distanceSq < mRadius * mRadius;
}

bool CapsuleCollider::CollisionWithMesh(const MeshCollider* mesh) const
{
    return false;
}

bool CapsuleCollider::CollisionWithCapsule(const CapsuleCollider* capsule) const
{
    if (capsule == nullptr)
    {
        return false;
    }

    const XMVECTOR centerA = GetCenter();
    const XMVECTOR centerB = capsule->GetCenter();

    //========================================================
    // XZ距離
    //========================================================
    const float dx =
        XMVectorGetX(centerA) -
        XMVectorGetX(centerB);

    const float dz =
        XMVectorGetZ(centerA) -
        XMVectorGetZ(centerB);


    //========================================================
    // Y Segment Range
    //========================================================
    const float aMinY =
        XMVectorGetY(centerA) -
        mHalfSegmentHeight;

    const float aMaxY =
        XMVectorGetY(centerA) +
        mHalfSegmentHeight;


    const float bMinY =
        XMVectorGetY(centerB) -
        capsule->GetHalfSegmentHeight();

    const float bMaxY =
        XMVectorGetY(centerB) +
        capsule->GetHalfSegmentHeight();


    //========================================================
    // Y方向のSegment間距離
    //========================================================
    float dy = 0.0f;


    if (aMaxY < bMinY)
    {
        dy = bMinY - aMaxY;
    }
    else if (bMaxY < aMinY)
    {
        dy = aMinY - bMaxY;
    }

    const float distanceSq = dx * dx + dy * dy + dz * dz;
    const float totalRadius = mRadius + capsule->GetRadius();

    return distanceSq < totalRadius * totalRadius;
}

void CapsuleCollider::Resolve(const Collider* other) const
{
    const BoxCollider* box = dynamic_cast<const BoxCollider*>(other);

    if (box == nullptr || mOwner == nullptr)
    {
        return;
    }

    //========================================================
    // 実際に衝突していない場合は押し出さない
    //========================================================
    if (!CollisionWithBox(box))
    {
        return;
    }

    //========================================================
    // Capsule中心をBoxローカル空間へ変換
    //========================================================
    const XMVECTOR localCenter = box->ToLocalPoint(GetCenter());

    const float cx = XMVectorGetX(localCenter);
    const float cy = XMVectorGetY(localCenter);
    const float cz = XMVectorGetZ(localCenter);

    const XMVECTOR half = box->GetHalfSize();

    const float hx = XMVectorGetX(half);
    const float hy = XMVectorGetY(half);
    const float hz = XMVectorGetZ(half);

    //========================================================
    // Capsule線分の上下
    //========================================================
    const float segMinY = cy - mHalfSegmentHeight;
    const float segMaxY = cy + mHalfSegmentHeight;

    //========================================================
    // Boxの最近傍点（水平成分）
    //========================================================
    const float closestX = std::clamp(cx, -hx, hx);
    const float closestZ = std::clamp(cz, -hz, hz);

    //========================================================
    // Y方向の最近傍点
    //========================================================
    float capsuleY = cy;
    float boxY = cy;

    if (segMaxY < -hy)
    {
        // CapsuleがBoxの下
        capsuleY = segMaxY;
        boxY = -hy;
    }
    else if (segMinY > hy)
    {
        // CapsuleがBoxの上
        capsuleY = segMinY;
        boxY = hy;
    }
    else
    {
        // Capsuleの線分とBoxのY範囲が重なる
        capsuleY = std::clamp(cy, -hy, hy);
        boxY = capsuleY;
    }

    const XMVECTOR capsulePoint = XMVectorSet(cx, capsuleY, cz, 0.0f);

    const XMVECTOR boxPoint = XMVectorSet(closestX, boxY, closestZ, 0.0f);

    const XMVECTOR delta = capsulePoint - boxPoint;

    const float distance = XMVectorGetX(XMVector3Length(delta));

    //========================================================
    // ローカル空間での押し出し法線・距離
    //========================================================
    XMVECTOR localNormal = XMVectorZero();
    float penetration = 0.0f;

    if (distance > 0.0001f)
    {
        //====================================================
        // 最近傍点が異なる通常ケース
        //====================================================
        penetration = mRadius - distance;

        if (penetration <= 0.0f)
        {
            return;
        }

        localNormal = XMVector3Normalize(delta);
    }
    else
    {
        //====================================================
        // Capsuleの中央線分がBox内部に入ったケース
        // 最も短い押し出し方向を選択する
        //====================================================
        const float pushPosX = hx + mRadius - cx;
        const float pushNegX = cx + hx + mRadius;

        const float pushPosZ = hz + mRadius - cz;
        const float pushNegZ = cz + hz + mRadius;

        const float pushUp = hy + mRadius - segMinY;
        const float pushDown = segMaxY + hy + mRadius;

        penetration = pushPosX;
        localNormal = XMVectorSet(1, 0, 0, 0);

        auto TryPush = [&](float distanceToMove, const XMVECTOR& normal)
            {
                if (distanceToMove >= 0.0f && distanceToMove < penetration)
                {
                    penetration = distanceToMove;
                    localNormal = normal;
                }
            };

        TryPush(pushNegX, XMVectorSet(-1, 0, 0, 0));
        TryPush(pushPosZ, XMVectorSet(0, 0, 1, 0));
        TryPush(pushNegZ, XMVectorSet(0, 0, -1, 0));
        TryPush(pushUp, XMVectorSet(0, 1, 0, 0));
        TryPush(pushDown, XMVectorSet(0, -1, 0, 0));
    }

    if (penetration <= 0.0f)
    {
        return;
    }

    //========================================================
    // ローカル法線 → ワールド法線
    // ドアが45度回転していれば押し出し方向も45度回転する
    //========================================================
    const XMVECTOR worldNormal = box->ToWorldDirection(localNormal);

    //========================================================
    // プレイヤーをBox外へ押し出す
    //========================================================
    mOwner->Translate(worldNormal * penetration);

    //========================================================
    // 接地判定
    // Y軸回転なので地面の上方向は変わらない
    //========================================================
    if (XMVectorGetY(worldNormal) > 0.5f)
    {
        IPhysicsCharacter* physics = GetOwnerPhysicsCharacter();

        if (physics != nullptr)
        {
            physics->SetGrounded(true);

            if (physics->GetVerticalSpeed() <= 0.0f)
            {
                physics->SetVerticalSpeed(0.0f);
            }
        }
    }
}

void CapsuleCollider::ResolveCapsule(const CapsuleCollider* other) const
{
    if (other == nullptr || mOwner == nullptr || other->GetOwner() == nullptr)
    {
        return;
    }

    //========================================================
    // Center
    //========================================================
    const XMVECTOR centerA = GetCenter();
    const XMVECTOR centerB = other->GetCenter();


    const float ax =
        XMVectorGetX(centerA);

    const float az =
        XMVectorGetZ(centerA);

    const float bx =
        XMVectorGetX(centerB);

    const float bz =
        XMVectorGetZ(centerB);

    //========================================================
    // XZ Distance
    //========================================================
    const float dx = ax - bx;
    const float dz = az - bz;
    const float distanceSq = dx * dx + dz * dz;

    const float totalRadius =
        GetRadius() + other->GetRadius();


    //========================================================
    // XZ方向で離れている
    //========================================================
    if (distanceSq >= totalRadius * totalRadius)
    {
        return;
    }


    //========================================================
    // Y方向にも実際にCapsuleが重なっているか確認
    //========================================================
    if (!CollisionWithCapsule(other))
    {
        return;
    }


    //========================================================
    // 完全に同じXZ位置
    //========================================================
    float distance = sqrtf(distanceSq);


    XMVECTOR normal;


    if (distance <=
        0.0001f)
    {
        normal =
            XMVectorSet(
                1.0f,
                0.0f,
                0.0f,
                0.0f
            );

        distance = 0.0f;
    }
    else
    {
        normal =
            XMVectorSet(
                dx / distance,
                0.0f,
                dz / distance,
                0.0f
            );
    }


    //========================================================
    // めり込み量
    //========================================================
    const float penetration = totalRadius - distance;


    if (penetration <= 0.0f)
    {
        return;
    }


    //========================================================
    // Mass
    //========================================================
    const float massA =
        (std::max)(GetMass(), 0.001f);

    const float massB =
        (std::max)(other->GetMass(), 0.001f);

    const float totalMass = massA + massB;
    const float ratioA = massB / totalMass;
    const float ratioB = massA / totalMass;


    //========================================================
    // Aを外側へ
    //========================================================
    mOwner->Translate(normal * (penetration * ratioA));

    //========================================================
    // Bを反対側へ
    //========================================================
    other->GetOwner()->Translate(-normal * (penetration * ratioB));
}
