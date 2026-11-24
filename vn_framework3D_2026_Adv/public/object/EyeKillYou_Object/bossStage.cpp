#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "bossStage.h"

BossStage::BossStage(const WCHAR* folder, const WCHAR* file,float radius)
    : Object(folder, file), mRadius(radius), mActivated(false)
{
    mCenter = *getPosition();
    mCollider = new MeshCollider(this);
    setReceiveShadow(true);
}

bool BossStage::CheckPlayerInside(const XMVECTOR& playerPos)
{
    XMVECTOR diff = playerPos - mCenter;
    float dist = XMVectorGetX(XMVector3Length(diff));

    if (dist < mRadius)
    {
        mActivated = true;
    }
    return mActivated;
}

//ボスステージ内外判定
bool BossStage::ClampPosition(XMVECTOR& pos)
{
    if (!mActivated) return false;

    XMVECTOR diff = pos - mCenter;
    float dist = XMVectorGetX(XMVector3Length(diff));

    if (dist > mRadius)
    {
        XMVECTOR dir = XMVector3Normalize(diff);
        pos = mCenter + dir * mRadius;
        return true; // 壁ヒット
    }

    return false;
}
