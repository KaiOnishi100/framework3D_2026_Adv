#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "UIExplorationControls.h"

//============================================================
// Initialize
//============================================================
void UIExplorationControls::Initialize(vnScene* scene)
{
    if (scene == nullptr || mBackground != nullptr)
    {
        return;
    }

    const float screenW =
        static_cast<float>(vnMainFrame::screenWidth);

    const float screenH =
        static_cast<float>(vnMainFrame::screenHeight);

    //============================================================
    // 1980×1080 基準のUI配置
    //
    // 幅490、高さ420の控えめなパネル。
    // 右端30px、下端32pxの余白を設ける。
    //============================================================
    constexpr float baseWidth = 490.0f;
    constexpr float baseHeight = 420.0f;

    const float scale = (std::min)(
        screenW / 1980.0f,
        screenH / 1080.0f
        );

    const float panelW = baseWidth * scale;
    const float panelH = baseHeight * scale;

    mLeft = screenW - 30.0f * scale - panelW;
    mTop = screenH - 32.0f * scale - panelH;

    //============================================================
    // Background
    // 黒色の半透明パネルを配置する
    // 暗い屋敷の中でも、操作表示が背景に埋もれない
    //============================================================
    mBackground = new vnSprite(
        mLeft + panelW * 0.5f,
        mTop + panelH * 0.5f,
        panelW,
        panelH,
        L"data/image/UI/white1x1.png"
    );

    mBackground->setRenderPriority(2900);

    // 頂点カラーで半透明の暗い色を作る
    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mBackground->vtx[i].r = 0.015f;
        mBackground->vtx[i].g = 0.025f;
        mBackground->vtx[i].b = 0.035f;
        mBackground->vtx[i].a = 0.72f;
    }

    scene->registerObject(mBackground);

    SetVisible(false);
}

//============================================================
// 表示切替
//============================================================
void UIExplorationControls::SetVisible(bool visible)
{
    mVisible = visible;

    if (mBackground == nullptr)
    {
        return;
    }

    mBackground->setRenderEnable(visible);
    mBackground->setExecuteEnable(visible);
}

//============================================================
// Draw
//============================================================
void UIExplorationControls::Draw() const
{
    if (!mVisible)
    {
        return;
    }

    //============================================================
    // 最後に使用した入力デバイス
    //============================================================
    const bool useGamepad = InputDeviceTracker::Get().GetDevice() == ActiveDevice::Gamepad;

    struct ControlRow
    {
        const wchar_t* Key;
        const wchar_t* Description;
    };

    //============================================================
    // Keyboard / Mouse
    //============================================================
    static constexpr ControlRow keyboardRows[] =
    {
        { L"[ E ]",   L"調べる・拾う" },
        { L"[ F ]",   L"懐中電灯" },
        { L"[ C ]",   L"しゃがむ" },
        { L"[ V ]",   L"視点切替" },
        { L"[ M ]",   L"マップ" },
        { L"[ I ]",   L"持ち物" },
        { L"[ TAB ]", L"ポーズ" }
    };

    //============================================================
    // Gamepad
    //============================================================
    static constexpr ControlRow gamepadRows[] =
    {
        { L"[ A ]",     L"調べる・拾う" },
        { L"[ X ]",     L"懐中電灯" },
        { L"[ B ]",     L"しゃがむ" },
        { L"[ R3 ]",    L"視点切替" },
        { L"[ L3 ]",    L"マップ" },
        { L"[ SHARE ]", L"持ち物" },
        { L"[ START ]", L"ポーズ" }
    };

    const ControlRow* rows = useGamepad ? gamepadRows : keyboardRows;

    //============================================================
    // Header
    //============================================================
    AEUIFont::Scope font(AEUIFont::Style::Tab);
    vnFont::setColor(0xffd6b985);

    vnFont::print(
        mLeft + 22.0f,
        mTop + 21.0f,
        L"CONTROLS"
    );

    font.Use(AEUIFont::Style::Hint);
    vnFont::setColor(0xff909ba8);

    vnFont::print(
        mLeft + 22.0f,
        mTop + 47.0f,
        useGamepad ? L"GAMEPAD" : L"KEYBOARD / MOUSE"
    );

    //============================================================
    // Control Rows
    // 左に操作キー 右に日本語の説明を表示
    // 行の高さを一定にして読みやすくする
    //============================================================
    constexpr float firstRowY = 91.0f;
    constexpr float rowHeight = 45.0f;

    for (int i = 0; i < 7; ++i)
    {
        const float y = mTop + firstRowY + rowHeight * i;

        // 操作キーは金色系
        font.Use(AEUIFont::Style::Tab);
        vnFont::setColor(0xffe2c59a);

        vnFont::print(
            mLeft + 24.0f,
            y,
            L"%s",
            rows[i].Key
        );

        // 説明は明るいグレー
        font.Use(AEUIFont::Style::Detail);
        vnFont::setColor(0xffd5dadd);

        vnFont::print(
            mLeft + 157.0f,
            y,
            L"%s",
            rows[i].Description
        );
    }
}

//============================================================
// Terminate
//============================================================
void UIExplorationControls::Terminate(vnScene* scene)
{
    if (scene != nullptr && mBackground != nullptr)
    {
        scene->deleteObject(mBackground);
    }

    mBackground = nullptr;
    mVisible = false;
}