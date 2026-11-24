#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "UIPadSubmitHint.h"

void UIPadSubmitHint::SetRender(vnSprite* s, bool on)
{
    if (!s) return;
    s->setRenderEnable(on);
    s->setExecuteEnable(on);
}

void UIPadSubmitHint::Initialize(vnScene* scene, const WCHAR* crossPath, const WCHAR* ringPath)
{
    if (!scene) return;

    mIcon = new vnSprite(0, 0, mSize, mSize, crossPath);
    mIcon->setRenderPriority(2300);
    scene->registerObject(mIcon);

    mRipple.Initialize(scene, ringPath, 2250);
    mRipple.SetBaseSize(mSize);

    mAllVisible = false;
    mShowIcon = false;

    mIconX = mIconY = 0.0f;
    mRippleX = mRippleY = 0.0f;

    SetRender(mIcon, false);
    mRipple.SetVisibleAll(false);
}

void UIPadSubmitHint::Terminate(vnScene* scene)
{
    if (!scene) return;

    if (mIcon) scene->deleteObject(mIcon);
    mIcon = nullptr;

    mRipple.Terminate(scene);
}

void UIPadSubmitHint::SetVisibleAll(bool v)
{
    mAllVisible = v;

    if (!mAllVisible)
    {
        SetRender(mIcon, false);
        mRipple.SetVisibleAll(false);
        return;
    }

    // 画面が出てるなら、アイコンは mShowIcon に従う
    SetRender(mIcon, mShowIcon);
    mRipple.SetVisibleAll(true);

    // 位置反映（安全のため）
    if (mIcon)
    {
        mIcon->posX = mIconX;
        mIcon->posY = mIconY;
    }
    mRipple.SetCenter(mRippleX, mRippleY);
}

void UIPadSubmitHint::SetShowIcon(bool show)
{
    mShowIcon = show;

    if (!mAllVisible)
    {
        SetRender(mIcon, false);
        return;
    }
    SetRender(mIcon, mShowIcon);
}

void UIPadSubmitHint::SetTargetPos(float x, float y)
{
    mIconX = x; mIconY = y;
    if (mIcon) { mIcon->posX = mIconX; mIcon->posY = mIconY; }
}

void UIPadSubmitHint::SetIconTargetRect(const UIRect& r, float offsetX, float offsetY)
{
    const float cx = r.x + r.w * 0.5f;
    const float cy = r.y + r.h * 0.5f;

    mIconX = cx + offsetX;
    mIconY = cy + offsetY;

    if (mIcon)
    {
        mIcon->posX = mIconX;
        mIcon->posY = mIconY;
    }
}

void UIPadSubmitHint::SetRippleTargetRect(const UIRect& r, float offsetX, float offsetY)
{
    const float cx = r.x + r.w * 0.5f;
    const float cy = r.y + r.h * 0.5f;

    mRippleX = cx + offsetX;
    mRippleY = cy + offsetY;

    mRipple.SetCenter(mRippleX, mRippleY);
}

void UIPadSubmitHint::TriggerRipple()
{
    if (!mAllVisible) return;

    // if (!mShowIcon) return;
    mRipple.SetCenter(mRippleX, mRippleY);
    mRipple.Trigger();
}

void UIPadSubmitHint::Update(float dt)
{
    if (!mAllVisible) return;
    mRipple.Update(dt);
}