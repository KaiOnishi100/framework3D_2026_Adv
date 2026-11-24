#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "UIPauseOverlay.h"

void UIPauseOverlay::SetRender(vnSprite* s, bool on)
{
    if (!s) return;
    s->setRenderEnable(on);
    s->setExecuteEnable(on);
}

const UIRect& UIPauseOverlay::GetSelectedRect() const
{
    switch (mSelected)
    {
    case 1: return mRectSave;
    case 2: return mRectOptions;
    case 3: return mRectTitle;
    case 4: return mRectExit;
    default: return mRectResume;
    }
}

void UIPauseOverlay::Initialize(vnScene* scene)
{
    if (!scene)
    {
        return;
    }

    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);

    const float uiScale = (std::min)(sw / 1980.0f, sh / 1080.0f);

    //============================================================
    // Pause Background
    // Pause.pngは半透明の全画面オーバーレイ
    // ゲーム画面を残しながらメニューを表示する
    //============================================================
    const float bgScale = (std::max)(sw / 1280.0f, sh / 720.0f);

    mDimPlate = new vnSprite(
        sw * 0.5f, sh * 0.5f,
        1280.0f * bgScale,
        720.0f * bgScale,
        L"data/image/UI/AtmosphericEscape/Pause.png"
    );

    mDimPlate->setRenderPriority(3000);
    scene->registerObject(mDimPlate);
    SetRender(mDimPlate, false);

    //============================================================
    // Button Layout
    // Resume / Save / Title / Exit
    //============================================================
    const float buttonW = 360.0f * uiScale;
    const float buttonH = 84.0f * uiScale;
    const float stepY = 104.0f * uiScale;

    const float cx = sw * 0.5f;
    const float firstY = sh * (330.0f / 1080.0f);

    //============================================================
    // ボタンRect
    // UIRectは左上を基準にする
    //============================================================
    auto makeRect = [&](int index) -> UIRect
        {
            const float centerY = firstY + stepY * index;

            return {
                cx - buttonW * 0.5f,
                centerY - buttonH * 0.5f,
                buttonW,
                buttonH
            };
        };

    mRectResume = makeRect(0);
    mRectSave = makeRect(1);
    mRectOptions = makeRect(2);
    mRectTitle = makeRect(3);
    mRectExit = makeRect(4);

    //============================================================
    // Button Helper
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

            sprite->setRenderPriority(3100);
            scene->registerObject(sprite);
            SetRender(sprite, false);

            return sprite;
        };

    mBtnResume = createButton(
        mRectResume,
        L"data/image/UI/AtmosphericEscape/btn_resume.png"
    );

    mBtnSave = createButton(
        mRectSave,
        L"data/image/UI/AtmosphericEscape/btn_save.png"
    );

    mBtnTitle = createButton(
        mRectTitle,
        L"data/image/UI/AtmosphericEscape/btn_title.png"
    );

    mBtnExit = createButton(
        mRectExit,
        L"data/image/UI/AtmosphericEscape/btn_exit.png"
    );

    mBtnOptions = createButton(
        mRectOptions,
        L"data/image/UI/AtmosphericEscape/btn_options.png"
    );

    //============================================================
    // Focus
    //============================================================
    mHoverPlate = new vnSprite(
        0.0f, 0.0f, buttonW, buttonH,
        L"data/image/UI/AtmosphericEscape/Focus.png"
    );

    mHoverPlate->setRenderPriority(3200);

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
    mSubmitHint.SetShowIcon(false);

    //============================================================
    // Initial State
    //============================================================
    mVisible = false;
    mSelected = 0;
    mPending = Action::None;

    SetSaveEnabled(mSaveEnabled);
}

void UIPauseOverlay::Terminate(vnScene* scene)
{
    if (!scene) return;

    if (mDimPlate)   scene->deleteObject(mDimPlate);
    if (mBtnResume)  scene->deleteObject(mBtnResume);
    if (mBtnTitle)   scene->deleteObject(mBtnTitle);
    if (mBtnExit)    scene->deleteObject(mBtnExit);
    if (mHoverPlate) scene->deleteObject(mHoverPlate);
    if (mBtnSave)    scene->deleteObject(mBtnSave);
    if (mBtnOptions) scene->deleteObject(mBtnOptions);


    mSubmitHint.Terminate(scene);

    mDimPlate = mBtnOptions = mHoverPlate = mBtnResume = mBtnTitle = mBtnExit =  mBtnSave =  nullptr;
}

void UIPauseOverlay::Show()
{
    mVisible = true;
    mSelected = 0;
    mPending = Action::None;

    SetRender(mDimPlate, true);
    SetRender(mBtnResume, true);
    SetRender(mBtnTitle, true);
    SetRender(mBtnExit, true);
    SetRender(mBtnSave, true);
    SetRender(mBtnOptions, true);

    SetRender(mHoverPlate, true);
    ApplyHoverPlate();

    mSubmitHint.SetVisibleAll(true);
}

void UIPauseOverlay::Hide()
{
    mVisible = false;
    mPending = Action::None;

    SetRender(mDimPlate, false);
    SetRender(mBtnResume, false);
    SetRender(mBtnTitle, false);
    SetRender(mBtnExit, false);
    SetRender(mHoverPlate, false);
    SetRender(mBtnSave, false);
    SetRender(mBtnOptions, false);

    mSubmitHint.SetVisibleAll(false);
}

void UIPauseOverlay::ApplyHoverPlate()
{
    if (!mHoverPlate) return;

    const UIRect& r = GetSelectedRect();

    mHoverPlate->posX = r.x + r.w * 0.5f;
    mHoverPlate->posY = r.y + r.h * 0.5f;
    mHoverPlate->sizeX = r.w;
    mHoverPlate->sizeY = r.h;
}

void UIPauseOverlay::Update(float dt)
{
    if (!mVisible) return;

    const bool isPad = (InputDeviceTracker::Get().GetDevice() == ActiveDevice::Gamepad);

    // マウスが動いたときにだけホバー先へフォーカスを移す
    const bool mouseMoved = vnMouse::getDX() != 0 || vnMouse::getDY() != 0;

    if (mouseMoved)
    {
        if (UI_IsHover(mRectResume))                  mSelected = 0;
        else if (mSaveEnabled && UI_IsHover(mRectSave))     mSelected = 1;
        else if (UI_IsHover(mRectOptions))                 mSelected = 2;
        else if (UI_IsHover(mRectTitle))                   mSelected = 3;
        else if (UI_IsHover(mRectExit))                    mSelected = 4;
    }
    ApplyHoverPlate();

    //==============================
    // クリック決定：リップルは常に中心
    //==============================
    auto ClickCommon = [&](const UIRect& rect, Action act, int sel)
        {
            if (mSelected != sel)
            {
                mSelected = sel;
                SoundBank::getInstance()->PlaySfx(SoundID::UI_Move, 0.8f);
                ApplyHoverPlate();
            }

            SoundBank::getInstance()->PlaySfx(SoundID::UI_Submit, 1.0f);
            // リップルはボタン中心
            mSubmitHint.SetRippleToCenterOfRect(rect);
            mSubmitHint.TriggerRipple();

            // Padなら×は右側（Pauseは横でOK）
            if (isPad)
            {
                mSubmitHint.SetIconTargetRect(rect, rect.w * 0.5f + mSubmitOffsetX, mSubmitOffsetY);
            }

            mPending = act;
        };

    if (UI_IsClicked(mRectResume))                 ClickCommon(mRectResume, Action::Resume, 0);
    else if (mSaveEnabled && UI_IsClicked(mRectSave))    ClickCommon(mRectSave, Action::Save, 1);
    else if (UI_IsClicked(mRectOptions))                ClickCommon(mRectOptions, Action::Options, 2);
    else if (UI_IsClicked(mRectTitle))                  ClickCommon(mRectTitle, Action::ToTitle, 3);
    else if (UI_IsClicked(mRectExit))                   ClickCommon(mRectExit, Action::Exit, 4);

    const UIRect& r = GetSelectedRect();

    mSubmitHint.SetVisibleAll(true);
    mSubmitHint.SetShowIcon(isPad);

    // リップルは常に中心（Pad/Mouse関係なし）
    mSubmitHint.SetRippleToCenterOfRect(r);

    // ×はPadだけ右側へ
    if (isPad)
        mSubmitHint.SetIconTargetRect(r, r.w * 0.5f + mSubmitOffsetX, mSubmitOffsetY);

    mSubmitHint.Update(dt);
}

UIPauseOverlay::Action UIPauseOverlay::ConsumeAction()
{
    Action a = mPending;
    mPending = Action::None;
    return a;
}

//--------------------------------------------
// IUIControllable
//--------------------------------------------
void UIPauseOverlay::MoveUIHorizontal(int)
{
}

void UIPauseOverlay::MoveUIVertical(int dir)
{
    if (!mVisible) return;
    if (dir == 0) return;

    const int previous = mSelected;
    const int step = (dir > 0) ? 1 : -1;

    do
    {
        mSelected = (mSelected + step + 5) % 5;// 0～4のボタンを循環する
    } 
    while (!mSaveEnabled && mSelected == 1);

    if (previous != mSelected)
    {
        SoundBank::getInstance()->PlaySfx(SoundID::UI_Move, 0.80f);
    }

    ApplyHoverPlate();
}

void UIPauseOverlay::Submit()
{
    if (!mVisible) return;

    //========================================================
    // Save禁止時にSaveを選択している
    //========================================================
    if (mSelected == 1 && !mSaveEnabled)
    {
        return;
    }

    SoundBank::getInstance()->PlaySfx(SoundID::UI_Submit, 1.0f);

    const bool isPad = (InputDeviceTracker::Get().GetDevice() == ActiveDevice::Gamepad);
    const UIRect& r = GetSelectedRect();

    // リップルは常に中心
    mSubmitHint.SetRippleToCenterOfRect(r);
    mSubmitHint.TriggerRipple();

    // ×はPadなら右側へ戻す（見た目維持）
    if (isPad)
        mSubmitHint.SetIconTargetRect(r, r.w * 0.5f + mSubmitOffsetX, mSubmitOffsetY);

    switch (mSelected)
    {
    case 0: mPending = Action::Resume;  break;
    case 1: mPending = Action::Save;    break;
    case 2: mPending = Action::Options; break;
    case 3: mPending = Action::ToTitle; break;
    case 4: mPending = Action::Exit;    break;
    }
}

void UIPauseOverlay::Cancel()
{
    if (!mVisible) return;
    mPending = Action::Resume;
}

void UIPauseOverlay::SetSaveEnabled(bool enabled)
{
    mSaveEnabled = enabled;

    if (!mSaveEnabled && mSelected == 1)
    {
        mSelected = 2; // TITLEへ移す
        ApplyHoverPlate();
    }

    if (mBtnSave == nullptr)
    {
        return;
    }

    //========================================================
    // Save禁止状態なら暗くする
    //========================================================
    const float brightness = enabled ? 1.0f : 0.35f;

    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mBtnSave->vtx[i].r = brightness;
        mBtnSave->vtx[i].g = brightness;
        mBtnSave->vtx[i].b = brightness;
        mBtnSave->vtx[i].a = 1.0f;
    }
}
