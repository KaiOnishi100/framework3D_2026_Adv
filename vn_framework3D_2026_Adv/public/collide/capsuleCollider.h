#pragma once

#include "collider.h"


//============================================================
// CapsuleCollider
//============================================================
class CapsuleCollider : public Collider
{
private:

    //========================================================
    // Capsule Radius
    //========================================================
    float mRadius = 0.35f;

    //========================================================
    // Capsule中央Line Segmentの半分の高さ
    //========================================================
    float mHalfSegmentHeight = 0.55f;

    //========================================================
    // Owner PositionからのOffset
    //========================================================
    XMVECTOR mOffset = XMVectorZero();


public:

    CapsuleCollider(
        ObjectBase* owner,
        float radius,
        float halfSegmentHeight,
        const XMVECTOR& offset =  XMVectorZero()
    ) :
        Collider(owner),
        mRadius(radius),
        mHalfSegmentHeight( halfSegmentHeight ),
        mOffset(offset)
    { }


    //========================================================
    // Capsule Center
    //========================================================
    XMVECTOR GetCenter() const
    {
        if (mOwner == nullptr)
        {
            return XMVectorZero();
        }

        return *mOwner->getPosition() + mOffset;
    }


    //========================================================
    // Segment Bottom
    //========================================================
    XMVECTOR GetSegmentBottom() const
    {
        return GetCenter() + XMVectorSet(0.0f, -mHalfSegmentHeight, 0.0f, 0.0f);
    }


    //========================================================
    // Segment Top
    //========================================================
    XMVECTOR GetSegmentTop() const
    {
        return GetCenter() + XMVectorSet(0.0f, mHalfSegmentHeight, 0.0f, 0.0f);
    }


    float GetRadius() const
    {
        return mRadius;
    }


    float GetHalfSegmentHeight() const
    {
        return mHalfSegmentHeight;
    }


    void SetOffset(const XMVECTOR& offset)
    {
        mOffset = offset;
    }

    //============================================================
    // Capsule Size Update
    //============================================================
    void SetHalfSegmentHeight(float height)
    {
        if (!std::isfinite(height) || height < 0.0f)
        {
            return;
        }

        mHalfSegmentHeight = height;
    }

    //========================================================
    // Collision
    //========================================================
    bool CollisionCheck(const Collider* other) const override;
    bool CollisionWithSphere(const SphereCollider* sphere) const override;
    bool CollisionWithBox(const BoxCollider* box) const override;
    bool CollisionWithMesh(const MeshCollider* mesh) const override;
    bool CollisionWithCapsule(const CapsuleCollider* capsule) const override;


    //========================================================
    // Resolve
    //========================================================
    void Resolve(const Collider* other) const override;
    void ResolveCapsule(const CapsuleCollider* other) const;
};