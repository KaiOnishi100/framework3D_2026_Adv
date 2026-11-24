#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "shadow.h"

Shadow::Shadow(const WCHAR* folder, const WCHAR* file) : Object(folder, file)
{
	//コライダーは使わないのでnullptr
	mCollider = nullptr;
    setRenderFlag(eRenderFlag::Transparent, true);
}


void Shadow::UpdateShadow(float groundY)
{
    if (!mTarget) return;

    XMVECTOR tpos = *mTarget->getPosition();

    float y = XMVectorGetY(tpos);

    // 高さ→0..1 に正規化（0=地面付近, 1=最大高度）
    float t = 0.0f;
    if (mMaxY > mMinY)
    {
        t = (y - mMinY) / (mMaxY - mMinY);
        t = std::clamp(t, 0.0f, 1.0f);
    }

    // 影スケール：地面付近は大きく、上に行くほど小さく
    float s = mBaseScale + (mMinScale - mBaseScale) * t;

    // 影の位置：XZは追従、Yは地面 + 少し浮かせる
    XMVECTOR spos = XMVectorSet(XMVectorGetX(tpos), groundY + 0.02f, XMVectorGetZ(tpos), 0.0f);
    setPosition(&spos);

    setScale(s, s, s);
}