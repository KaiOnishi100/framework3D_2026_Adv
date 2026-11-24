#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"FadeManager.h"

void FadeManager::Initialize(vnScene* scene)
{
    if (!scene) return;
    if (mFadePlate) return;

    const float sw = (float)vnMainFrame::screenWidth;
    const float sh = (float)vnMainFrame::screenHeight;

    // 全画面板（黒）
    mFadePlate = new vnSprite(sw * 0.5f, sh * 0.5f, sw, sh, L"");
    mFadePlate->setRenderPriority(10000); // 最前面（UIより前）
    scene->registerObject(mFadePlate);

    // 黒固定
    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mFadePlate->vtx[i].r = 0.0f;
        mFadePlate->vtx[i].g = 0.0f;
        mFadePlate->vtx[i].b = 0.0f;
        mFadePlate->vtx[i].a = 0.0f;
    }

    // 最初は非表示
    SetRender(false);
    mState = FadeState::None;
    mTimer = 0.0f;
}

void FadeManager::Terminate(vnScene* scene)
{
    if (!scene) return;
    if (mFadePlate)
    {
        scene->deleteObject(mFadePlate);
        mFadePlate = nullptr;
    }
    mState = FadeState::None;
    mOnFadeOutFinished = nullptr;
    mTimer = 0.0f;
}

void FadeManager::SetRender(bool on)
{
    if (!mFadePlate) return;
    mFadePlate->setRenderEnable(on);
    mFadePlate->setExecuteEnable(false);
}

void FadeManager::SetAlpha(float a)
{
    if (!mFadePlate) return;
    a = std::clamp(a, 0.0f, 1.0f);

    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mFadePlate->vtx[i].a = a;
    }
}

void FadeManager::SetFadeColor(FadeColor color)
{
    if (!mFadePlate) return;

    const float rgb = (color == FadeColor::White) ? 1.0f : 0.0f;

    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mFadePlate->vtx[i].r = rgb;
        mFadePlate->vtx[i].g = rgb;
        mFadePlate->vtx[i].b = rgb;
    }
}

void FadeManager::StartFadeOut(float duration, std::function<void()> onFinished, FadeColor color)
{
    mState = FadeState::FadeOut;
    mDuration = (duration > 0.0001f) ? duration : 0.0001f;
    mTimer = 0.0f;
    mOnFadeOutFinished = std::move(onFinished);

    SetRender(true);
    SetAlpha(0.0f);
}

void FadeManager::StartFadeIn(float duration, FadeColor color)
{
    mState = FadeState::FadeIn;
    mDuration = (duration > 0.0001f) ? duration : 0.0001f;
    mTimer = 0.0f;
    mOnFadeOutFinished = nullptr;

    SetRender(true);
    SetAlpha(1.0f);
}

void FadeManager::Update(float dt)
{
    if (mState == FadeState::None)
    {
        return;
    }

    if (!mFadePlate)
    {
        return;
    }


    //============================================================
    // Timer
    //============================================================
    mTimer += dt;


    float t = mTimer / mDuration;


    t = std::clamp(t, 0.0f, 1.0f);


    //============================================================
    // Easing
    //============================================================
    auto EaseIn = [](float x)
        {
            return x * x;
        };


    auto EaseOut = [](float x)
        {
            const float a = 1.0f - x;
            return 1.0f - a * a;
        };


    //============================================================
    // Fade Out
    //============================================================
    if (mState == FadeState::FadeOut)
    {
        const float alpha = EaseIn(t);

        SetAlpha(alpha);


        if (t >= 1.0f)
        {
            //====================================================
            // 完全に黒
            //====================================================
            SetAlpha(1.0f);

            //====================================================
            // Fade状態を終了させる
            //====================================================
            mState = FadeState::None;

            //====================================================
            // Callbackを退避
            //====================================================
            auto callback = std::move(mOnFadeOutFinished);

            mOnFadeOutFinished = nullptr;

            //====================================================
            // Callback実行
            //====================================================
            if (callback)
            {
                callback();
            }
        }
    }


    //============================================================
    // Fade In
    //============================================================
    else if (mState == FadeState::FadeIn)
    {
        const float alpha = 1.0f - EaseOut(t);
        SetAlpha(alpha);

        if (t >= 1.0f)
        {
            SetAlpha(0.0f);
            SetRender(false);
            mState = FadeState::None;
        }
    }
}