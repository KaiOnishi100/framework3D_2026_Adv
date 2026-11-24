#include "../framework.h"
#include "vn_environment.h"

//============================================================
// Collision
//============================================================

bool vnCamera::Camera_CheckSphereVsMesh(const XMVECTOR& center, float radius, const MeshCollider* mesh, float* outPush, XMVECTOR* outNormal)
{
	if (!mesh) return false;

	float maxPush = 0.0f;
	XMVECTOR bestNormal = XMVectorZero();
	bool hit = false;

	for (const auto& tri : mesh->GetTriangles())
	{
		// 球中心から三角形への最近点
		XMVECTOR closest = vnCollide::ClosestPointOnTriangle(center, tri);

		XMVECTOR diff = center - closest;
		float dist = XMVectorGetX(XMVector3Length(diff));

		// dist < radius ならめり込み
		if (dist >= radius) continue;

		hit = true;

		float push = radius - dist;
		if (push > maxPush)
		{
			maxPush = push;

			// 法線を求める
			// diffがほぼ0の時は tri.Normal を使う
			if (dist < 1e-6f)
				bestNormal = tri.plane.Normal;
			else
				bestNormal = XMVector3Normalize(diff);
		}
	}

	if (hit)
	{
		if (outPush)   *outPush = maxPush;
		if (outNormal) *outNormal = bestNormal;
	}
	return hit;
}

void vnCamera::Camera_ResolveSphereVsMesh(XMVECTOR& ioCenter, float radius, const MeshCollider* mesh)
{
	float push = 0.0f;
	XMVECTOR n = XMVectorZero();

	if (Camera_CheckSphereVsMesh(ioCenter, radius, mesh, &push, &n))
	{
		// 押し出し（Yも含めて押し出す：カメラはこれが重要）
		ioCenter += n * push;

		// 接触面でのチラつき防止の微小オフセット
		ioCenter += n * 0.02f;
	}
}

XMVECTOR vnCamera::Camera_FindSafePosOnSegment(const XMVECTOR& lookAt, const XMVECTOR& desired, float radius, const MeshCollider* mesh)
{
	if (!mesh) return desired;

	// desired が安全ならそのまま
	{
		float push = 0.0f;
		XMVECTOR n = XMVectorZero();
		if (!Camera_CheckSphereVsMesh(desired, radius, mesh, &push, &n))
			return desired;
	}

	// lookAt 側を安全化しておく（念のため）
	XMVECTOR safeStart = lookAt;
	Camera_ResolveSphereVsMesh(safeStart, radius, mesh);

	float low = 0.0f; // 安全側
	float high = 1.0f; // 危険側

	// 10回で十分。もっと滑らかにしたければ12回にしてOK
	for (int i = 0; i < 10; ++i)
	{
		float mid = (low + high) * 0.5f;

		// safeStart → desired の線分上
		XMVECTOR p = XMVectorLerp(safeStart, desired, mid);

		float push = 0.0f;
		XMVECTOR n = XMVectorZero();
		bool hit = Camera_CheckSphereVsMesh(p, radius, mesh, &push, &n);

		if (hit)
		{
			// 埋まる → もっとプレイヤー側へ
			high = mid;
		}
		else
		{
			// 安全 → もっと desired 側へ
			low = mid;
		}
	}

	// low が安全な最大
	return XMVectorLerp(safeStart, desired, low);
}

bool vnCamera::Camera_RaycastDownGroundY(const XMVECTOR& fromPos, const MeshCollider* mesh, float* outGroundY)
{
	if (!mesh || !outGroundY) return false;

	vnCollide::stLine ray;
	ray.Pos = fromPos;

	// 真下方向
	ray.Dir = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

	bool hitAny = false;
	float bestY = -FLT_MAX;

	XMVECTOR hit = XMVectorZero();

	for (const auto& tri : mesh->GetTriangles())
	{
		// 地面扱い：上向きの面だけ
		const float ny = XMVectorGetY(tri.plane.Normal);
		if (ny < 0.5f) continue; // 0.5ならだいたい上向きを地面とみなす

		if (vnCollide::isCollide(&hit, &ray, &tri))
		{
			float y = XMVectorGetY(hit);

			// fromPos より下側のヒット
			if (y <= XMVectorGetY(fromPos))
			{
				// いちばん高い（=カメラに一番近い）地面を使う
				if (!hitAny || y > bestY)
				{
					bestY = y;
					hitAny = true;
				}
			}
		}
	}

	if (hitAny)
	{
		*outGroundY = bestY;
	}
	return hitAny;
}

bool vnCamera::Camera_CheckSphereVsBox(const XMVECTOR& center, float radius, const BoxCollider* box, float* outPush, XMVECTOR* outNormal)
{
    //========================================================
    // Safety
    //========================================================
    if (box == nullptr || !box->IsEnabled())
    {
        return false;
    }


    //========================================================
    // Box AABB
    //========================================================
    const XMVECTOR boxCenter =
        box->GetCenter();

    const XMVECTOR halfSize =
        box->GetHalfSize();

    const XMVECTOR boxMin =
        boxCenter - halfSize;

    const XMVECTOR boxMax =
        boxCenter + halfSize;


    //========================================================
    // Sphere中心に最も近いBox上のPoint
    //========================================================
    const XMVECTOR closest =
        XMVectorClamp(
            center,
            boxMin,
            boxMax
        );


    const XMVECTOR delta = center - closest;
    const float distanceSq = XMVectorGetX(XMVector3LengthSq(delta));
    const float radiusSq = radius * radius;


    //========================================================
    // Sphere中心がBox外側にある通常ケース
    //========================================================
    if (distanceSq > 0.000001f)
    {
        if (distanceSq >= radiusSq)
        {
            return false;
        }


        const float distance = sqrtf(distanceSq);
        const XMVECTOR normal = XMVectorScale(delta, 1.0f / distance);
        const float push = radius - distance;

        if (outPush != nullptr)
        {
            *outPush = push;
        }

        if (outNormal != nullptr)
        {
            *outNormal = normal;
        }


        return true;
    }


    //========================================================
    // Sphere中心がBox内部にあるケース
    // Clampするとclosest == centerになるため
    // deltaからNormalを求められない
    // 一番近いBox Faceを探し、
    // その方向へCameraを押し出す
    //========================================================
    XMFLOAT3 centerF;
    XMFLOAT3 minF;
    XMFLOAT3 maxF;

    XMStoreFloat3(
        &centerF,
        center
    );

    XMStoreFloat3(
        &minF,
        boxMin
    );

    XMStoreFloat3(
        &maxF,
        boxMax
    );


    //========================================================
    // Box内部か確認
    //========================================================
    const bool inside =
        centerF.x >= minF.x &&
        centerF.x <= maxF.x &&
        centerF.y >= minF.y &&
        centerF.y <= maxF.y &&
        centerF.z >= minF.z &&
        centerF.z <= maxF.z;


    if (!inside)
    {
        return false;
    }


    //========================================================
    // 各Faceまでの距離
    //========================================================
    const float distanceLeft =
        centerF.x - minF.x;

    const float distanceRight =
        maxF.x - centerF.x;

    const float distanceBottom =
        centerF.y - minF.y;

    const float distanceTop =
        maxF.y - centerF.y;

    const float distanceBack =
        centerF.z - minF.z;

    const float distanceFront =
        maxF.z - centerF.z;


    //========================================================
    // 一番近いFaceを探す
    //========================================================
    float nearestDistance = distanceLeft;

    XMVECTOR normal = XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f);


    if (distanceRight < nearestDistance)
    {
        nearestDistance = distanceRight;

        normal = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }


    if (distanceBottom < nearestDistance)
    {
        nearestDistance = distanceBottom;

        normal = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
    }


    if (distanceTop < nearestDistance)
    {
        nearestDistance = distanceTop;

        normal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }


    if (distanceBack < nearestDistance)
    {
        nearestDistance = distanceBack;

        normal = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
    }


    if (distanceFront < nearestDistance)
    {
        nearestDistance = distanceFront;

        normal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }


    //========================================================
    // Camera中心をFaceの外まで出し、
    // さらにSphere Radius分離する
    //========================================================
    const float push = nearestDistance + radius;

    if (outPush != nullptr)
    {
        *outPush = push;
    }

    if (outNormal != nullptr)
    {
        *outNormal = normal;
    }


    return true;
}

void vnCamera::Camera_ResolveSphereVsBoxes(XMVECTOR& ioCenter, float radius, const std::vector<Collider*>& colliders)
{
    constexpr int ResolveIterations = 3;

    for (int iteration = 0; iteration < ResolveIterations; ++iteration)
    {
        bool resolvedAny = false;


        for (Collider* collider : colliders)
        {
            if (collider == nullptr || !collider->IsEnabled())
            {
                continue;
            }


            const BoxCollider* box = dynamic_cast<const BoxCollider*>(collider);


            if (box == nullptr)
            {
                continue;
            }


            float push = 0.0f;

            XMVECTOR normal = XMVectorZero();


            if (!Camera_CheckSphereVsBox(
                ioCenter,
                radius,
                box,
                &push,
                &normal))
            {
                continue;
            }


            //================================================
            // Box外へ押す
            //================================================
            ioCenter += normal * push;


            //================================================
            // 接触面のちらつき防止
            //================================================
            ioCenter += normal * 0.005f;


            resolvedAny = true;
        }


        if (!resolvedAny)
        {
            break;
        }
    }
}

void vnCamera::ClampCameraAboveGround(const MeshCollider* stageMesh)
{
	if (!stageMesh)
		return;

	float groundY = 0.0f;

	XMVECTOR rayStart = Position + XMVectorSet(0.0f, 5.0f, 0.0f, 0.0f);

	if (Camera_RaycastDownGroundY(rayStart, stageMesh, &groundY))
	{
		const float minY = groundY + sCameraRadius;
		float y = XMVectorGetY(Position);

		if (y < minY)
		{
			Position = XMVectorSetY(Position, minY);
		}
	}
}
