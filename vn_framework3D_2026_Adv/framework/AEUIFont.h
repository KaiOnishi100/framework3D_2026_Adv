#pragma once

#include <dwrite.h>

// Atmospheric Escapeで共有するUI書体 vnFontの文字描画キューを使用する
namespace AEUIFont
{
    enum class Style
    {
        Heading,
        Tab,
        Label,
        Detail,
        Hint,
        MapRoom,
        MapMarker,
        DocumentTitle,
        DocumentPreview,
        DocumentBody,
        Count
    };

    // Direct2D / DirectWriteとvnFontの標準フォントの準備が済んだ後起動時に一度だけ呼ぶ
    void Initialize(IDWriteFactory* factory);

    // 文字描画を終了した後、vnFontの終了処理で呼ぶ
    void Terminate();

    class Scope final
    {
    public:
        explicit Scope(Style style);
        ~Scope();

        // スコープ内では同じインスタンスを使って見出し・本文などを切り替える
        void Use(Style style) const;

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        IDWriteTextFormat* mPreviousFormat;
        unsigned long mPreviousColor;
    };
}
