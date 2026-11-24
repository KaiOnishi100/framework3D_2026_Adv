#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"UIResultOverlay.h"

void UIResultOverlay::SetRender(vnSprite* s, bool on)
{
    if (!s) return;
    s->setRenderEnable(on);
    s->setExecuteEnable(on);
}

void UIResultOverlay::Initialize(vnScene* scene)
{
    if (!scene)
    {
        return;
    }

    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);

    //============================================================
    // UI Scale
    //============================================================
    const float uiScale = (std::min)(sw / 1980.0f, sh / 1080.0f);

    //============================================================
    // Background
    //============================================================
    const float bgScale = (std::max)(sw / 1280.0f, sh / 720.0f);

    const float bgW = 1280.0f * bgScale;
    const float bgH = 720.0f * bgScale;

    mGameOverBG = new vnSprite(
        sw * 0.5f, sh * 0.5f, bgW, bgH,
        L"data/image/UI/AtmosphericEscape/GameOver.png"
    );

    mGameClearBG = new vnSprite(
        sw * 0.5f, sh * 0.5f, bgW, bgH,
        L"data/image/UI/AtmosphericEscape/GameClear.png"
    );

    for (vnSprite* sprite : { mGameOverBG, mGameClearBG })
    {
        sprite->setRenderPriority(2000);
        scene->registerObject(sprite);
        SetRender(sprite, false);
    }

    //============================================================
    // Button Layout
    // Restart / Title / Exit
    //============================================================
    const float buttonW = 540.0f * uiScale;
    const float buttonH = 126.0f * uiScale;
    const float gapX = 40.0f * uiScale;

    const float totalW = buttonW * 3.0f + gapX * 2.0f;
    const float left = (sw - totalW) * 0.5f;

    // 1080基準のY=900へ配置
    const float centerY = sh * (900.0f / 1080.0f);
    const float top = centerY - buttonH * 0.5f;

    mBtnRestartRect = { left, top, buttonW, buttonH };
    mBtnTitleRect = { left + buttonW + gapX, top, buttonW, buttonH };
    mBtnExitRect = { left + (buttonW + gapX) * 2.0f, top, buttonW, buttonH };

    //============================================================
    // Sprite Creation Helper
    // UIRectの中心座標からSpriteを作る
    // ボタンの見た目とクリック判定がずれないようにする
    //============================================================
    auto createButton = [&](const UIRect& rect, const wchar_t* path)
        {
            vnSprite* sprite = new vnSprite(
                rect.x + rect.w * 0.5f,
                rect.y + rect.h * 0.5f,
                rect.w,
                rect.h,
                path
            );

            sprite->setRenderPriority(2100);
            scene->registerObject(sprite);
            SetRender(sprite, false);

            return sprite;
        };

    mBtnRestartSpr = createButton(
        mBtnRestartRect,
        L"data/image/UI/AtmosphericEscape/btn_restart.png"
    );

    mBtnTitleSpr = createButton(
        mBtnTitleRect,
        L"data/image/UI/AtmosphericEscape/btn_title.png"
    );

    mBtnExitSpr = createButton(
        mBtnExitRect,
        L"data/image/UI/AtmosphericEscape/btn_exit.png"
    );

    //============================================================
    // Selection Focus
    //============================================================
    mHoverPlate = new vnSprite(
        0.0f, 0.0f, buttonW, buttonH,
        L"data/image/UI/AtmosphericEscape/Focus.png"
    );

    mHoverPlate->setRenderPriority(2200);

    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mHoverPlate->vtx[i].a = 0.90f;
    }

    scene->registerObject(mHoverPlate);
    SetRender(mHoverPlate, false);

    //============================================================
    // Gamepad Hint
    //============================================================
    mSubmitHint.Initialize(
        scene,
        L"data/image/UI/icons_pad_cross.png",
        L"data/image/UI/Circle.png"
    );

    mSubmitHint.SetVisibleAll(false);

    //============================================================
    // Initial State
    //============================================================
    mMode = Mode::Hidden;
    mVisible = false;
    mSelected = 0;
    mPending = Action::None;
}

void UIResultOverlay::Terminate(vnScene* scene)
{
    if (!scene) return;

    if (mGameOverBG)  scene->deleteObject(mGameOverBG);
    if (mGameClearBG) scene->deleteObject(mGameClearBG);
    if (mBtnTitleSpr) scene->deleteObject(mBtnTitleSpr);
    if (mBtnExitSpr)  scene->deleteObject(mBtnExitSpr);
    if (mHoverPlate)  scene->deleteObject(mHoverPlate);
    if (mBtnRestartSpr) scene->deleteObject(mBtnRestartSpr);
    mSubmitHint.Terminate(scene);

    mGameOverBG = mGameClearBG = nullptr;
    mBtnTitleSpr = mBtnExitSpr = nullptr;
    mHoverPlate = nullptr;
    mBtnRestartSpr = nullptr;
}

void UIResultOverlay::SetVisible(bool visible)
{
    mVisible = visible;

    if (!visible)
    {
        SetRender(mGameOverBG, false);
        SetRender(mGameClearBG, false);
        SetRender(mBtnRestartSpr, false);
        SetRender(mBtnTitleSpr, false);
        SetRender(mBtnExitSpr, false);
        SetRender(mHoverPlate, false);
        mSubmitHint.SetVisibleAll(false);
    }
}

void UIResultOverlay::ApplyHoverPlate()
{
    if (!mHoverPlate)
    {
        return;
    }

    const UIRect& rect = GetSelectedRect();

    mHoverPlate->posX = rect.x + rect.w * 0.5f;
    mHoverPlate->posY = rect.y + rect.h * 0.5f;
    mHoverPlate->sizeX = rect.w;
    mHoverPlate->sizeY = rect.h;

    SetRender(mHoverPlate, true);
}

void UIResultOverlay::ShowGameOver()
{
    mMode = Mode::GameOver;
    mVisible = true;

    SetRender(mGameOverBG, true);
    SetRender(mGameClearBG, false);

    SetRender(mBtnTitleSpr, true);
    SetRender(mBtnExitSpr, true);
    SetRender(mBtnRestartSpr, true);


    mSelected = 0;
    mPending = Action::None;

    ApplyHoverPlate();
    mSubmitHint.SetVisibleAll(true);
}

void UIResultOverlay::ShowGameClear()
{
    mMode = Mode::GameClear;
    mVisible = true;

    SetRender(mGameOverBG, false);
    SetRender(mGameClearBG, true);

    SetRender(mBtnTitleSpr, true);
    SetRender(mBtnExitSpr, true);
    SetRender(mBtnRestartSpr,true);

    mSelected = 0;
    mPending = Action::None;

    ApplyHoverPlate();
    mSubmitHint.SetVisibleAll(true);
}

void UIResultOverlay::Hide()
{
    mMode = Mode::Hidden;
    mVisible = false;

    SetRender(mGameOverBG, false);
    SetRender(mGameClearBG, false);
    SetRender(mBtnRestartSpr,false);
    SetRender(mBtnTitleSpr, false);
    SetRender(mBtnExitSpr, false);
    SetRender(mHoverPlate, false);
    mSubmitHint.SetVisibleAll(false);
    mPending = Action::None;
}

void UIResultOverlay::Update(float dt)
{
    if (!mVisible) return;
    if (mMode == Mode::Hidden) return;
    int prev = mSelected;
    // hoverで選択追従
    if (UI_IsHover(mBtnRestartRect))mSelected = 0;
    if (UI_IsHover(mBtnTitleRect)) mSelected = 1;
    if (UI_IsHover(mBtnExitRect))  mSelected = 2;
    if (mSelected != prev) SoundBank::getInstance()->PlaySfx(SoundID::UI_Move, 0.8f);

    ApplyHoverPlate();

    const bool isPad = (InputDeviceTracker::Get().GetDevice() == ActiveDevice::Gamepad);

    mSubmitHint.SetVisibleAll(true);
    mSubmitHint.SetShowIcon(isPad);

    const UIRect& selectedRect = GetSelectedRect();

    // -----------------------------
    // リップルはボタン中心
    // -----------------------------
    mSubmitHint.SetRippleToCenterOfRect(selectedRect);

    // -----------------------------
    // Padなら×はボタン下
    // -----------------------------
    if (isPad)
    {
        mSubmitHint.SetIconTargetRect(
            selectedRect,
            mSubmitOffsetX,
            selectedRect.h * 0.5f + mSubmitOffsetY
        );
    }

    // -----------------------------
    // クリック決定：リップルは中心
    // -----------------------------
    //============================================================
    // Restart Click
    //============================================================
    if (UI_IsClicked(mBtnRestartRect))
    {
        SoundBank::getInstance()->PlaySfx(SoundID::UI_Submit, 1.0f);
        mSelected = 0;
        ApplyHoverPlate();
        mSubmitHint.SetRippleToCenterOfRect(mBtnRestartRect);
        mSubmitHint.TriggerRipple();
        mPending = Action::Restart;
    }

    //============================================================
    // Title Click
    //============================================================
    else if (UI_IsClicked(mBtnTitleRect))
    {
        SoundBank::getInstance()->PlaySfx(SoundID::UI_Submit, 1.0f);
        mSelected = 1;
        ApplyHoverPlate();
        mSubmitHint.SetRippleToCenterOfRect(mBtnTitleRect);
        mSubmitHint.TriggerRipple();
        mPending = Action::ToTitle;
    }

    //============================================================
    // Exit Click
    //============================================================
    else if (UI_IsClicked(mBtnExitRect))
    {
        SoundBank::getInstance()->PlaySfx(SoundID::UI_Submit, 1.0f);
        mSelected = 2;
        ApplyHoverPlate();
        mSubmitHint.SetRippleToCenterOfRect(mBtnExitRect);
        mSubmitHint.TriggerRipple();
        mPending = Action::Exit;
    }

    mSubmitHint.Update(dt);
}

UIResultOverlay::Action UIResultOverlay::ConsumeAction()
{
    Action a = mPending;
    mPending = Action::None;
    return a;
}

//==============================
// IUIControllable
//==============================
void UIResultOverlay::MoveUIHorizontal(int dir)
{
    if (!mVisible) return;
    if (dir == 0) return;
    int prev = mSelected;
    mSelected += dir;

    // 2択なのでループ
    if (mSelected < 0) mSelected = 2;
    if (mSelected > 2) mSelected = 0;
    if (mSelected != prev) SoundBank::getInstance()->PlaySfx(SoundID::UI_Move, 0.8f);
    ApplyHoverPlate();
}

void UIResultOverlay::MoveUIVertical(int)
{
}

void UIResultOverlay::Submit()
{
    const UIRect& selectedRect = GetSelectedRect();
    SoundBank::getInstance()->PlaySfx(SoundID::UI_Submit, 1.0f);
    // リップルは常にボタン中心
    mSubmitHint.SetRippleToCenterOfRect(selectedRect);
    mSubmitHint.TriggerRipple();

    //============================================================
    // Action
    //============================================================
    switch (mSelected)
    {
    case 0:
    {
        mPending = Action::Restart;
        break;
    }
    case 1:
    {
        mPending = Action::ToTitle;
        break;
    }
    case 2:
    default:
    {
        mPending = Action::Exit;
        break;
    }
    }
}

void UIResultOverlay::Cancel()
{
}

//============================================================
// 現在選択中のButton Rect
// 0 = Restart
// 1 = Title
// 2 = Exit
//============================================================
const UIRect& UIResultOverlay::GetSelectedRect() const
{
    switch (mSelected)
    {
    case 0:
        return mBtnRestartRect;

    case 1:
        return mBtnTitleRect;

    case 2:
    default:
        return mBtnExitRect;
    }
}