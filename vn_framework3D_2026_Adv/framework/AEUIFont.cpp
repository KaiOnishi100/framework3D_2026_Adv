#include "../framework.h"
#include "vn_environment.h"
#include "AEUIFont.h"

namespace
{
    using AEUIFont::Style;
    using Microsoft::WRL::ComPtr;

    struct FontSpec
    {
        bool serif;
        float size;
        DWRITE_FONT_WEIGHT weight;
    };

    // enum Styleと同じ並び サイズは1980x1080のレイアウトで調整する
    constexpr FontSpec kSpecs[] =
    {
        {true,  30.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD}, // Heading
        {false, 19.0f, DWRITE_FONT_WEIGHT_BOLD},      // Tab
        {false, 21.0f, DWRITE_FONT_WEIGHT_MEDIUM},    // Label
        {false, 19.0f, DWRITE_FONT_WEIGHT_MEDIUM},    // Detail
        {false, 16.0f, DWRITE_FONT_WEIGHT_MEDIUM},    // Hint
        {false, 16.0f, DWRITE_FONT_WEIGHT_MEDIUM},    // MapRoom
        {false, 13.0f, DWRITE_FONT_WEIGHT_BOLD},      // MapMarker
        {true,  29.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD}, // DocumentTitle
        {true,  19.0f, DWRITE_FONT_WEIGHT_MEDIUM},    // DocumentPreview
        {true,  21.0f, DWRITE_FONT_WEIGHT_MEDIUM},    // DocumentBody
    };

    constexpr std::size_t kStyleCount = static_cast<std::size_t>(Style::Count);
    static_assert(sizeof(kSpecs) / sizeof(kSpecs[0]) == kStyleCount);

    std::array<ComPtr<IDWriteTextFormat>, kStyleCount> gFormats;

    const wchar_t* FindFamily(IDWriteFontCollection* collection, bool serif)
    {
        // 日本語の書体を優先する Windows側のインストール状況に合わせて選ぶ
        static constexpr const wchar_t* serifFamilies[] =
        {
            L"Yu Mincho", L"MS Mincho", L"Meiryo", L"Segoe UI"
        };
        static constexpr const wchar_t* sansFamilies[] =
        {
            L"Yu Gothic", L"Meiryo", L"MS Gothic", L"Segoe UI"
        };

        const auto& families = serif ? serifFamilies : sansFamilies;
        for (const wchar_t* family : families)
        {
            UINT32 index = 0;
            BOOL exists = FALSE;
            if (SUCCEEDED(collection->FindFamilyName(family, &index, &exists)) && exists)
            {
                return family;
            }
        }
        return nullptr;
    }
}

void AEUIFont::Initialize(IDWriteFactory* factory)
{
    if (!factory) return;

    ComPtr<IDWriteFontCollection> fonts;
    if (FAILED(factory->GetSystemFontCollection(fonts.GetAddressOf()))) return;

    const wchar_t* serif = FindFamily(fonts.Get(), true);
    const wchar_t* sans = FindFamily(fonts.Get(), false);

    for (std::size_t i = 0; i < kStyleCount; ++i)
    {
        // 各サイズと太さの組み合わせにつき起動時に一度だけ作る
        // キュー内の文字列が同じTextFormatを使うのでフレーム中の再生成は行わない
        if (gFormats[i]) continue;

        const FontSpec& spec = kSpecs[i];
        const wchar_t* family = spec.serif ? serif : sans;
        if (!family) continue;

        ComPtr<IDWriteTextFormat> format;
        const HRESULT result = factory->CreateTextFormat(
            family,
            fonts.Get(),
            spec.weight,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            spec.size,
            L"ja-jp",
            format.GetAddressOf()
        );

        // 作成できなかったスタイルではUse()がvnFont既定フォントを選択する
        if (SUCCEEDED(result))
        {
            gFormats[i].Swap(format);
        }
    }
}

void AEUIFont::Terminate()
{
    for (auto& format : gFormats)
    {
        format.Reset();
    }
}

AEUIFont::Scope::Scope(Style style) : mPreviousFormat(vnFont::getTextFormat()), mPreviousColor(vnFont::getColor())
{
    Use(style);
}

AEUIFont::Scope::~Scope()
{
    // 子画面がフォントを変えても、呼び出し元のタブやデバッグ文字へ波及させない
    vnFont::setTextFormat(mPreviousFormat);
    vnFont::setColor(mPreviousColor);
}

void AEUIFont::Scope::Use(Style style) const
{
    const auto index = static_cast<std::size_t>(style);
    // nullptrは既存のvnFont::setTextFormat()で標準フォントへ戻る
    vnFont::setTextFormat(index < gFormats.size() ? gFormats[index].Get() : nullptr);
}
