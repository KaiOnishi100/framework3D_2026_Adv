#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"meshCollider.h"

MeshCollider::MeshCollider(ModelBase* owner) : Collider(owner), mMeshOwner(owner)
{
    BuildTrianglesFromModel();
}

// vnModel → 三角形配列生成
void MeshCollider::BuildTrianglesFromModel()
{
    //============================================================
    // Safety
    //============================================================
    if (mMeshOwner == nullptr)
    {
        return;
    }


    //============================================================
    // Vertex / Index Information
    //============================================================
    const int indexNum = mMeshOwner->getIndexNum();

    vnVertex3DFile* vertices = mMeshOwner->getFileVertex();

    LPWORD indices = mMeshOwner->getIndex();


    if (vertices == nullptr || indices == nullptr)
    {
        return;
    }


    //============================================================
    // Triangle生成
    //============================================================
    for (int i = 0; i < indexNum; i += 3)
    {
        vnCollide::stTriangle tri;

        const WORD i0 = indices[i + 0];
        const WORD i1 = indices[i + 1];
        const WORD i2 = indices[i + 2];


        const XMVECTOR p0 = XMVectorSet(vertices[i0].x, vertices[i0].y, vertices[i0].z, 1.0f);
        const XMVECTOR p1 = XMVectorSet(vertices[i1].x, vertices[i1].y, vertices[i1].z, 1.0f);
        const XMVECTOR p2 = XMVectorSet(vertices[i2].x, vertices[i2].y, vertices[i2].z, 1.0f);


        tri.fromPoints(&p0, &p1, &p2);

        mTriangles.push_back(tri);
    }
}

bool MeshCollider::CollisionWithSphere(const SphereCollider* sphere) const
{
    vnCollide::stLine line;
    line.Pos = sphere->GetCenter();
    line.Dir = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

    XMVECTOR hit;

    for (const auto& tri : mTriangles)
    {
        if (vnCollide::isCollide(&hit, &line, &tri))
        {
            return true; // 1ポリゴンでも当たればOK
        }
    }
    return false;
}

bool MeshCollider::CollisionWithCapsule(const CapsuleCollider* capsule) const
{
    (void)capsule;

    return false;
}


bool MeshCollider::RayCast(const XMVECTOR& start, const XMVECTOR& dir, float maxDistance, float& outDist) const
{
    //========================================================
    // Disabled Collider
    //========================================================
    if (!IsEnabled())
    {
        return false;
    }

    vnCollide::stLine line;

    line.Pos = start;
    line.Dir = dir;

    bool hitAny = false;

    float closest = maxDistance;


    //========================================================
    // Triangle全検索
    //========================================================
    for (const auto& tri : mTriangles)
    {
        XMVECTOR hit;

        if (vnCollide::isCollide(&hit, &line, &tri))
        {
            const float dist = XMVectorGetX(XMVector3Length(hit - start));


            //================================================
            // Playerまでの距離より遠い壁は無関係
            //================================================
            if (dist > maxDistance)
            {
                continue;
            }


            if (dist < closest)
            {
                closest = dist;
                hitAny = true;
            }
        }
    }


    if (!hitAny)
    {
        return false;
    }

    outDist = closest;

    return true;
}

bool MeshCollider::CollisionCheck(const Collider* other) const
{
    if (!mEnabled || !other->IsEnabled()) return false;
    return other->CollisionWithMesh(this);
}