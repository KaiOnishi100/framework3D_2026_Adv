#include "../../framework.h"
#include "../../framework/vn_environment.h"
#include "../../framework/AtmosphericEscapeOptions.h"
#include "UIAtmosphericEscapeOptions.h"


namespace
{
    using Settings = AtmosphericEscapeOptions;
    using Effect = Settings::Effect;
    using Quality = Settings::Quality;

    struct Row
    {
        const wchar_t* label;
        // 各行はいずれか1種類の設定を操作する
        int Settings::* number = nullptr;
        Quality Settings::* quality = nullptr;
        bool Settings::* toggle = nullptr;
        Effect effect = Effect::Count;
        int min = 0;
        int max = 0;
        int step = 1;
        const wchar_t* unit = L"";
    };

    constexpr Row kAudio[] =
    {
        {L"マスター音量", &Settings::masterVolume, nullptr, nullptr, Effect::Count, 0, 100, 5, L"%"}
    };
    constexpr Row kPost[] =
    {
        {L"ブルーム", nullptr, nullptr, nullptr, Effect::Bloom},
        {L"アンチエイリアス（FXAA）", nullptr, nullptr, nullptr, Effect::Fxaa},
        {L"周辺減光", nullptr, nullptr, nullptr, Effect::Vignette},
        {L"カラーグレーディング", nullptr, nullptr, nullptr, Effect::ColorGrading},
        {L"色収差", nullptr, nullptr, nullptr, Effect::ChromaticAberration},
        {L"レンズフレア", nullptr, nullptr, nullptr, Effect::LensFlare},
        {L"モーションブラー", nullptr, nullptr, nullptr, Effect::MotionBlur},
        {L"ボリュームフォグ", nullptr, nullptr, nullptr, Effect::VolumetricFog},
        {L"表面下散乱（SSS）", nullptr, nullptr, nullptr, Effect::SssDiffusion},
        {L"間接光（SSGI）", nullptr, nullptr, nullptr, Effect::Ssgi},
        {L"フォグ品質", nullptr, &Settings::fogQuality},
        {L"SSGI品質", nullptr, &Settings::ssgiQuality}
    };
    constexpr Row kShadows[] =
    {
        {L"平行光の影（更新頻度）", nullptr, &Settings::directionalShadow},
        {L"スポットライトの影（灯数）", nullptr, &Settings::spotShadow},
        {L"ポイントライトの影（解像度上限）", nullptr, &Settings::pointShadow}
    };
    constexpr Row kCamera[] =
    {
        {L"視野角（縦FOV）", &Settings::fovYDegrees, nullptr, nullptr, Effect::Count, 45, 80, 1, L"°"},
        {L"マウス感度", &Settings::mouseSensitivity, nullptr, nullptr, Effect::Count, 25, 200, 5, L"%"},
        {L"右スティック感度", &Settings::padSensitivity, nullptr, nullptr, Effect::Count, 25, 200, 5, L"%"},
        {L"視点の上下反転", nullptr, nullptr, &Settings::invertLookY}
    };

    struct Page
    {
        const wchar_t* label;
        const Row* rows;
        int count;
        unsigned int change;
    };

    constexpr Page kPages[] =
    {
        {L"サウンド", kAudio, static_cast<int>(sizeof(kAudio) / sizeof(Row)), UIAtmosphericEscapeOptions::Audio},
        {L"ポストプロセス", kPost, static_cast<int>(sizeof(kPost) / sizeof(Row)), UIAtmosphericEscapeOptions::Post},
        {L"シャドウ", kShadows, static_cast<int>(sizeof(kShadows) / sizeof(Row)), UIAtmosphericEscapeOptions::Shadows},
        {L"カメラ", kCamera, static_cast<int>(sizeof(kCamera) / sizeof(Row)), UIAtmosphericEscapeOptions::Camera}
    };
    constexpr int kPageCount = static_cast<int>(sizeof(kPages) / sizeof(Page));
    constexpr const wchar_t* kQualityNames[] = {L"低", L"標準", L"高"};

    // 1980x1080論理座標。スプライトとマウスの当たりを同じRectから作る。
    UIRect TabRect(int i) { return {416.0f + i * 287.0f, 184.0f, 266.0f, 56.0f}; }
    UIRect RowRect(int i) { return {422.0f, 277.0f + i * 48.0f, 1136.0f, 44.0f}; }
    const UIRect kBackRect = {1405.0f, 909.0f, 153.0f, 56.0f};

    void SetVisible(vnSprite* sprite, bool visible)
    {
        if (!sprite) return;
        sprite->setRenderEnable(visible);
        sprite->setExecuteEnable(visible);
    }

    void Place(vnSprite* sprite, const UIRect& rect)
    {
        if (!sprite) return;
        sprite->posX = rect.x + rect.w * 0.5f;
        sprite->posY = rect.y + rect.h * 0.5f;
        sprite->sizeX = rect.w;
        sprite->sizeY = rect.h;
    }

    vnSprite* MakeFocus(vnScene* scene, int priority, float alpha)
    {
        auto* sprite = new vnSprite(0, 0, 1, 1, L"data/image/UI/white1x1.png");
        sprite->setRenderPriority(priority);
        for (int i = 0; i < vnSprite::VertexNum; ++i)
        {
            sprite->vtx[i].r = 0.72f;
            sprite->vtx[i].g = 0.55f;
            sprite->vtx[i].b = 0.33f;
            sprite->vtx[i].a = alpha;
        }
        scene->registerObject(sprite);
        return sprite;
    }

    void PlayMove()
    {
        SoundBank::getInstance()->PlaySfx(SoundID::UI_Move, 0.55f);
    }
}

void UIAtmosphericEscapeOptions::Initialize(vnScene* scene)
{
    mBackground = new vnSprite(990, 540, 1980, 1080,
        L"data/image/UI/AtmosphericEscape/Options.png");
    mBackground->setRenderPriority(4000);
    scene->registerObject(mBackground);

    mTabFocus = MakeFocus(scene, 4001, 0.29f);
    mRowFocus = MakeFocus(scene, 4002, 0.19f);
    Hide();
}

void UIAtmosphericEscapeOptions::Terminate(vnScene* scene)
{
    Hide();
    if (mRowFocus) scene->deleteObject(mRowFocus);
    if (mTabFocus) scene->deleteObject(mTabFocus);
    if (mBackground) scene->deleteObject(mBackground);
    mRowFocus = mTabFocus = mBackground = nullptr;
}

void UIAtmosphericEscapeOptions::Show()
{
    mVisible = true;
    mWantsClose = false;
    mChanges = 0;
    mPage = 0;
    mRow = 0;
    Place(mTabFocus, TabRect(mPage));
    Place(mRowFocus, RowRect(mRow));
    SetPlatesVisible(true);
}

void UIAtmosphericEscapeOptions::Hide()
{
    mVisible = false;
    mWantsClose = false;
    mChanges = 0;
    SetPlatesVisible(false);
}

void UIAtmosphericEscapeOptions::SetPlatesVisible(bool visible)
{
    SetVisible(mBackground, visible);
    SetVisible(mTabFocus, visible);
    SetVisible(mRowFocus, visible);
}

void UIAtmosphericEscapeOptions::MovePage(int dir)
{
    if (!mVisible || mWantsClose || dir == 0) return;

    mPage = (mPage + (dir > 0 ? 1 : -1) + kPageCount) % kPageCount;
    mRow = 0;
    PlayMove();
}

void UIAtmosphericEscapeOptions::SelectRow(int index)
{
    if (mRow == index) return;
    mRow = index;
    PlayMove();
}

void UIAtmosphericEscapeOptions::MoveUIVertical(int dir)
{
    if (!mVisible || mWantsClose || dir == 0) return;
    const int count = kPages[mPage].count;
    SelectRow((mRow + (dir > 0 ? 1 : -1) + count) % count);
}

void UIAtmosphericEscapeOptions::MoveUIHorizontal(int dir)
{
    if (!mVisible || mWantsClose || dir == 0) return;

    Settings& settings = Settings::Get();
    const Page& page = kPages[mPage];
    const Row& row = page.rows[mRow];
    const int delta = dir > 0 ? 1 : -1;

    // 左右でスライダー・プリセット・ON/OFFをそれぞれ変更する
    if (row.number)
    {
        int& value = settings.*row.number;
        const int next = (std::clamp)(value + delta * row.step, row.min, row.max);
        if (value == next) return;
        value = next;
    }
    else if (row.quality)
    {
        Quality& value = settings.*row.quality;
        const auto next = static_cast<Quality>((std::clamp)(static_cast<int>(value) + delta, 0, 2));
        if (value == next) return;
        value = next;
    }
    else if (row.toggle)
    {
        settings.*row.toggle = !(settings.*row.toggle);
    }
    else
    {
        settings.SetEffectEnabled(row.effect, !settings.IsEffectEnabled(row.effect));
    }

    mChanges |= page.change;

    // 音量を変えた瞬間から操作SEも新しい音量で聞こえる
    if (page.change == Audio) settings.ApplyAudio();
    PlayMove();
}

void UIAtmosphericEscapeOptions::Submit()
{
    // Enter/AはON/OFF項目にも使用できる 数値項目は左右で操作する
    if (!mVisible || mWantsClose) return;
    const Row& row = kPages[mPage].rows[mRow];
    if (!row.number && !row.quality) MoveUIHorizontal(1);
}

void UIAtmosphericEscapeOptions::Cancel()
{
    if (!mVisible || mWantsClose) return;
    mWantsClose = true;
    SoundBank::getInstance()->PlaySfx(SoundID::UI_Submit, 0.7f);
}

unsigned int UIAtmosphericEscapeOptions::ConsumeChanges()
{
    const unsigned int changes = mChanges;
    mChanges = 0;
    return changes;
}

void UIAtmosphericEscapeOptions::Update(int padId)
{
    if (!mVisible || mWantsClose) return;

    if (UI_IsClicked(kBackRect))
    {
        Cancel();
        return;
    }

    const bool hasPad = Pad::isValid(padId);
    if (vnKeyboard::trg(DIK_Q) || (hasPad && Pad::trg(PadButton::LB, padId))) MovePage(-1);
    if (vnKeyboard::trg(DIK_E) || (hasPad && Pad::trg(PadButton::RB, padId))) MovePage(1);

    for (int i = 0; i < kPageCount; ++i)
    {
        if (i != mPage && UI_IsClicked(TabRect(i)))
        {
            mPage = i;
            mRow = 0;
            PlayMove();
            break;
        }
    }

    for (int i = 0; i < kPages[mPage].count; ++i)
    {
        const UIRect rect = RowRect(i);
        const UIRect left = {rect.x + 762.0f, rect.y, 58.0f, rect.h};
        const UIRect right = {rect.x + 1046.0f, rect.y, 68.0f, rect.h};

        // クリックした行に対して矢印を処理する マウスを置きっぱなしでもPad操作できる
        if (UI_IsClicked(left))
        {
            SelectRow(i);
            MoveUIHorizontal(-1);
            break;
        }
        if (UI_IsClicked(right))
        {
            SelectRow(i);
            MoveUIHorizontal(1);
            break;
        }
        if (UI_IsClicked(rect))
        {
            SelectRow(i);
            break;
        }
    }

    Place(mTabFocus, TabRect(mPage));
    Place(mRowFocus, RowRect(mRow));
}

void UIAtmosphericEscapeOptions::Draw() const
{
    if (!mVisible || mWantsClose) return;

    AEUIFont::Scope font(AEUIFont::Style::Tab);

    for (int i = 0; i < kPageCount; ++i)
    {
        const UIRect rect = TabRect(i);
        vnFont::setColor(i == mPage ? 0xffffe2af : 0xffc9c2b4);
        vnFont::print(rect.x + 22.0f, rect.y + 14.0f, L"%s", kPages[i].label);
    }

    // タブと設定行で太さを変えて視線の切り替えを分かりやすくする
    font.Use(AEUIFont::Style::Label);

    const Settings& settings = Settings::Get();
    const Page& page = kPages[mPage];
    for (int i = 0; i < page.count; ++i)
    {
        const Row& row = page.rows[i];
        const UIRect rect = RowRect(i);
        wchar_t value[32]{};
        const wchar_t* label = value;

        if (row.number) std::swprintf(value, 32, L"%d%ls", settings.*row.number, row.unit);
        else if (row.quality) label = kQualityNames[static_cast<int>(settings.*row.quality)];
        else
        {
            const bool enabled = row.toggle ? settings.*row.toggle : settings.IsEffectEnabled(row.effect);
            label = enabled ? L"ON" : L"OFF";
        }

        vnFont::setColor(i == mRow ? 0xffffe2af : 0xffe6e0d4);
        vnFont::print(rect.x + 18.0f, rect.y + 9.0f, L"%s", row.label);
        vnFont::print(rect.x + 784.0f, rect.y + 9.0f, L"<");
        vnFont::print(rect.x + 853.0f, rect.y + 9.0f, L"%s", label);
        vnFont::print(rect.x + 1070.0f, rect.y + 9.0f, L">");
    }

    font.Use(AEUIFont::Style::Hint);

    const bool isPad = InputDeviceTracker::Get().GetDevice() == ActiveDevice::Gamepad;
    vnFont::setColor(0xffc9c2b4);
    vnFont::print(438.0f, 926.0f, isPad
        ? L"LB / RB  タブ    十字キー上下  選択    左右  変更"
        : L"Q / E  タブ    W / S  選択    A / D  変更");
    vnFont::setColor(0xffffe2af);
    vnFont::print(1423.0f, 926.0f, isPad ? L"B 戻る" : L"Esc 戻る");
}
