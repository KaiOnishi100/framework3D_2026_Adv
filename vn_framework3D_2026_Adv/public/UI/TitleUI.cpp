#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"TitleUI.h"

void TitleUI::Initialize(vnScene* scene)
{
    if (scene == nullptr)
    {
        return;
    }

    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);

    constexpr wchar_t basePath[] =
        L"data/image/UI/AtmosphericEscape/";

    //============================================================
    // Background
    //============================================================
    constexpr float sourceW = 1280.0f;
    constexpr float sourceH = 720.0f;

    const float backgroundScale =
        (std::max)(sw / sourceW, sh / sourceH);

    mTitleSprite = new vnSprite(
        sw * 0.5f,
        sh * 0.5f,
        sourceW * backgroundScale,
        sourceH * backgroundScale,
        L"data/image/UI/AtmosphericEscape/Title.png"
    );

    mTitleSprite->setRenderPriority(0);
    scene->registerObject(mTitleSprite);

    //============================================================
    // Button Layout
    //============================================================
    const float scale = (std::min)(sw / 1980.0f, sh / 1080.0f);
    const float buttonW = 480.0f * scale;
    const float buttonH = 112.0f * scale;
    const float stepY = 120.0f * scale;

    const float centerX = sw * 0.25f;
    const float startY = sh * (550.0f / 1080.0f); // タイトルでは先頭の中心Y

    auto makeRect = [&](int index) -> UIRect
        {
            const float centerY = startY + stepY * index;
            return {
                centerX - buttonW * 0.5f,
                centerY - buttonH * 0.5f,
                buttonW,
                buttonH
            };
        };

    mBtnStart = makeRect(0);
    mBtnContinue = makeRect(1);
    mBtnOptions = makeRect(2);
    mBtnExit = makeRect(3);

    auto createButton = [&](const UIRect& rect, const wchar_t* path)
        {
            // ポーズと同じくクリック判定に使うRectから描画位置を求める
            auto* sprite = new vnSprite(
                rect.x + rect.w * 0.5f,
                rect.y + rect.h * 0.5f,
                rect.w,
                rect.h,
                path
            );
            sprite->setRenderPriority(100);
            scene->registerObject(sprite);
            return sprite;
        };

    mStartSprite = createButton(mBtnStart, L"data/image/UI/AtmosphericEscape/btn_new_game.png");
    mContinueSprite = createButton(mBtnContinue, L"data/image/UI/AtmosphericEscape/btn_continue.png");
    mOptionsSprite = createButton(mBtnOptions, L"data/image/UI/AtmosphericEscape/btn_options.png");
    mExitSprite = createButton(mBtnExit, L"data/image/UI/AtmosphericEscape/btn_exit.png");

    //============================================================
    // Selection Focus
    // Focus.pngは中央が透明な選択枠
    // ボタンの上に重ねるためPriorityを110にする
    //============================================================
    mHoverPlate = new vnSprite(
        centerX, startY, buttonW, buttonH,
        L"data/image/UI/AtmosphericEscape/Focus.png"
    );

    mHoverPlate->setRenderPriority(110);
    scene->registerObject(mHoverPlate);

    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mHoverPlate->vtx[i].a = 0.90f;
    }

    //============================================================
    // Gamepad Submit Hint
    //============================================================
    mSubmitHint.Initialize(
        scene,
        L"data/image/UI/icons_pad_cross.png",
        L"data/image/UI/Circle.png"
    );

    mSubmitHint.SetVisibleAll(true);
    mSubmitHint.SetShowIcon(false);

    //============================================================
    // Initial State
    //============================================================
    mSelected = 0;
    mPending = Action::None;
    mContinueEnabled = false;

    SetContinueEnabled(false);

    ApplyHoverPlate();
    ApplySubmitHint();
}

void TitleUI::Terminate(vnScene* scene)
{
    if (!scene) return;

    mSubmitHint.Terminate(scene);

    if (mTitleSprite)  scene->deleteObject(mTitleSprite);
    if (mStartSprite)  scene->deleteObject(mStartSprite);
    if (mExitSprite)   scene->deleteObject(mExitSprite);
    if (mHoverPlate)   scene->deleteObject(mHoverPlate);
    if (mContinueSprite) scene->deleteObject(mContinueSprite);
    if (mOptionsSprite)  scene->deleteObject(mOptionsSprite);

    mTitleSprite = nullptr;
    mStartSprite = nullptr;
    mContinueSprite = nullptr;
    mExitSprite = nullptr;
    mHoverPlate = nullptr;
    mOptionsSprite = nullptr;

    mPending = Action::None;
}

void TitleUI::ApplyHoverPlate()
{
    if (mHoverPlate == nullptr)
    {
        return;
    }

    const UIRect& rect = GetSelectedRect();

    // 現在選択しているボタンの中心へ選択枠を移動する
    mHoverPlate->posX = rect.x + rect.w * 0.5f;
    mHoverPlate->posY = rect.y + rect.h * 0.5f;
    mHoverPlate->sizeX = rect.w;
    mHoverPlate->sizeY = rect.h;
}

void TitleUI::ApplySubmitHint()
{
    const UIRect& rect = GetSelectedRect();

    // 選択ボタンの少し下にゲームパッドの決定アイコンを表示
    //mSubmitHint.SetIconTargetRect(
    //    rect,
    //    mSubmitOffsetX,
    //    rect.h * 0.5f + mSubmitOffsetY
    //);

    mSubmitHint.SetIconTargetRect(rect, rect.w * 0.5f + 28.0f, 0.0f);
}

const UIRect& TitleUI::GetSelectedRect() const
{
    switch (mSelected)
    {
    case 1: return mBtnContinue;
    case 2: return mBtnOptions;
    case 3: return mBtnExit;
    default: return mBtnStart;
    }
}

void TitleUI::Update(float dt)
{
    const int previous = mSelected;

    //============================================================
    // Mouse Hover
    //============================================================
    const bool mouseMoved = vnMouse::getDX() != 0 || vnMouse::getDY() != 0;
    if (mouseMoved)
    {
        if (UI_IsHover(mBtnStart))                              mSelected = 0;
        else if (mContinueEnabled && UI_IsHover(mBtnContinue))  mSelected = 1;
        else if (UI_IsHover(mBtnOptions))                       mSelected = 2;
        else if (UI_IsHover(mBtnExit))                          mSelected = 3;
    }

    ApplyHoverPlate();
    ApplySubmitHint();

    //============================================================
    // Gamepad Hint
    //============================================================
    const bool isPad =
        InputDeviceTracker::Get().GetDevice() ==
        ActiveDevice::Gamepad;

    mSubmitHint.SetVisibleAll(true);
    mSubmitHint.SetShowIcon(isPad);

    const UIRect& rect = GetSelectedRect();

    mSubmitHint.SetRippleToCenterOfRect(rect);

    //============================================================
    // Mouse Click
    //============================================================
    if (UI_IsClicked(mBtnStart))
    {
        mSelected = 0;
        Submit();
    }
    else if (mContinueEnabled && UI_IsClicked(mBtnContinue))
    {
        mSelected = 1;
        Submit();
    }
    else if (UI_IsClicked(mBtnOptions))
    {
        mSelected = 2;
        Submit();
    }
    else if (UI_IsClicked(mBtnExit))
    {
        mSelected = 3;
        Submit();
    }

    ApplyHoverPlate();
    ApplySubmitHint();

    mSubmitHint.Update(dt);
}

TitleUI::Action TitleUI::ConsumeAction()
{
    Action a = mPending;
    mPending = Action::None;
    return a;
}

void TitleUI::SetContinueEnabled(bool enabled)
{
    mContinueEnabled = enabled;

    // セーブデータが存在しない場合
    // CONTINUEを薄く描画して選択不能であることを示す
    if (mContinueSprite != nullptr)
    {
        const float alpha = enabled ? 1.0f : 0.30f;

        for (int i = 0; i < vnSprite::VertexNum; ++i)
        {
            mContinueSprite->vtx[i].a = alpha;
        }
    }

    if (!enabled && mSelected == 1)
    {
        mSelected = 0;
    }

    ApplyHoverPlate();
    ApplySubmitHint();
}

void TitleUI::SetMenuVisible(bool visible)
{
    for (vnSprite* sprite : { mStartSprite, mContinueSprite, mOptionsSprite, mExitSprite, mHoverPlate })
    {
        if (sprite == nullptr) continue;

        sprite->setRenderEnable(visible);
        sprite->setExecuteEnable(visible);
    }

    const bool isPad =
        InputDeviceTracker::Get().GetDevice() == ActiveDevice::Gamepad;

    mSubmitHint.SetVisibleAll(visible);
    mSubmitHint.SetShowIcon(visible && isPad);

    if (visible)
    {
        ApplyHoverPlate();
        ApplySubmitHint();
    }
}

void TitleUI::MoveUIHorizontal(int dir)
{
    MoveUIVertical(dir);
}

void TitleUI::MoveUIVertical(int dir) 
{
    if (dir == 0)
    {
        return;
    }

    const int previous = mSelected;

    // 上なら-1、下なら+1
    // 最後のボタンから先頭へ戻るよう循環させる
    const int step = dir > 0 ? 1 : -1;

    do
    {
        mSelected = (mSelected + step + 4) % 4;
    } while (!mContinueEnabled && mSelected == 1);

    if (previous != mSelected)
    {
        SoundBank::getInstance()->PlaySfx(SoundID::AE_UIMove, 0.40f);
    }

    ApplyHoverPlate();
    ApplySubmitHint();
}

void TitleUI::Submit()
{
    // 選択ボタンの位置からリップル演出を発生させる。
    const UIRect& rect = GetSelectedRect();

    mSubmitHint.SetRippleToCenterOfRect(rect);
    mSubmitHint.TriggerRipple();

    SoundBank::getInstance()->PlaySfx(SoundID::AE_UISubmit, 0.50f);

    switch (mSelected)
    {
    case 0:
        mPending = Action::Start;
        break;

    case 1:
        // セーブデータがある場合のみ有効
        if (mContinueEnabled)
        {
            mPending = Action::Continue;
        }
        break;

    case 2:
        mPending = Action::Options;
        break;
    case 3:
        mPending = Action::Exit;
        break;
    }
}

void TitleUI::Cancel() {}