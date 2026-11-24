#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "UIRipple.h"

void UIRipple::SetRender(vnSprite* s, bool on)
{
    if (!s) return;
    s->setRenderEnable(on);
    s->setExecuteEnable(on);
}

void UIRipple::SetAlpha(vnSprite* s, float a)
{
    if (!s) return;
    for (int i = 0; i < vnSprite::VertexNum; ++i) s->vtx[i].a = a;
}

void UIRipple::SetColorWhite(vnSprite* s, float a)
{
    if (!s) return;
    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        s->vtx[i].r = 1.0f;
        s->vtx[i].g = 1.0f;
        s->vtx[i].b = 1.0f;
        s->vtx[i].a = a;
    }
}

void UIRipple::Initialize(vnScene* scene, const WCHAR* ringPath, int renderPriority)
{
    if (!scene) return;

    mRing = new vnSprite(0, 0, mBaseSize, mBaseSize, ringPath);
    mRing->setRenderPriority(renderPriority);
    scene->registerObject(mRing);

    mAllVisible = false;
    mActive = false;
    mTimer = 0.0f;

    SetRender(mRing, false);
    SetColorWhite(mRing, 0.0f);
}

void UIRipple::Terminate(vnScene* scene)
{
    if (!scene) return;
    if (mRing) scene->deleteObject(mRing);
    mRing = nullptr;

    mAllVisible = false;
    mActive = false;
    mTimer = 0.0f;
}

void UIRipple::SetVisibleAll(bool v)
{
    mAllVisible = v;

    if (!mAllVisible)
    {
        mActive = false;
        mTimer = 0.0f;
        SetRender(mRing, false);
        SetAlpha(mRing, 0.0f);
        return;
    }

    // 表示中でも、リングは発生中だけON
    if (!mActive) SetRender(mRing, false);
}

void UIRipple::SetCenter(float x, float y)
{
    mCenterX = x;
    mCenterY = y;

    if (mRing)
    {
        mRing->posX = mCenterX;
        mRing->posY = mCenterY;
    }
}

void UIRipple::SetParams(float dur, float maxA, float startScale, float endScale)
{
    mDur = dur; mMaxA = maxA; mStartScale = startScale; mEndScale = endScale;
}

void UIRipple::Trigger()
{
    if (!mAllVisible) return;
    if (!mRing) return;

    mActive = true;
    mTimer = 0.0f;

    // 発生位置を固定
    mEmitX = mCenterX;
    mEmitY = mCenterY;

    SetRender(mRing, true);

    const float size = mBaseSize * mStartScale;
    mRing->posX = mCenterX;
    mRing->posY = mCenterY;
    mRing->sizeX = size;
    mRing->sizeY = size;

    SetColorWhite(mRing, mMaxA);
}

void UIRipple::Update(float dt)
{
    if (!mAllVisible) return;
    if (!mRing) return;

    if (!mActive)
    {
        SetRender(mRing, false);
        return;
    }

    mTimer += dt;

    float t = (mDur > 0.0001f) ? (mTimer / mDur) : 1.0f;
    t = Calculate::Clamp01(t);

    const float ease = powf(t, 0.7f);

    const float scale = Calculate::Lerp(mStartScale, mEndScale, ease);
    const float size = mBaseSize * scale;

    mRing->posX = mEmitX;
    mRing->posY = mEmitY;
    mRing->sizeX = size;
    mRing->sizeY = size;

    const float a = Calculate::Lerp(mMaxA, 0.0f, t);
    SetAlpha(mRing, a);

    if (t >= 1.0f)
    {
        mActive = false;
        SetRender(mRing, false);
    }
}