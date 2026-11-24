#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "UIDocumentViewer.h"

//============================================================
// Initialize
//============================================================
bool UIDocumentViewer::Initialize(
    vnScene* scene
)
{
    if (scene == nullptr)
    {
        return false;
    }


    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);


    //============================================================
    // 背景Dim
    // Documentを読んでいる間はWorldをかなり暗くする
    //============================================================
    mDimPlate =
        new vnSprite(
            sw * 0.5f,
            sh * 0.5f,
            sw,
            sh,
            L""
        );


    if (mDimPlate == nullptr)
    {
        return false;
    }


    mDimPlate->setRenderPriority(3100);


    SetSpriteColor(
        mDimPlate,
        0.0f,
        0.0f,
        0.0f,
        0.88f
    );


    scene->registerObject(
        mDimPlate
    );


    //============================================================
    // Document Panel
    // 現段階では暗めの紙
    // 後で古紙Textureなどへ変更する
    //============================================================
    mDocumentPanel =
        new vnSprite(
            sw * 0.5f,
            sh * 0.52f,
            sw * 0.62f,
            sh * 0.76f,
            L""
        );


    if (mDocumentPanel == nullptr)
    {
        return false;
    }


    mDocumentPanel->setRenderPriority(3110);


    //============================================================
    // 完全な白ではなく少し古い紙っぽい色
    //============================================================
    SetSpriteColor(
        mDocumentPanel,
        0.16f,
        0.15f,
        0.13f,
        0.98f
    );


    scene->registerObject(mDocumentPanel);

    Hide();

    return true;
}


//============================================================
// Terminate
//============================================================
void UIDocumentViewer::Terminate(
    vnScene* scene
)
{
    if (scene != nullptr)
    {
        if (mDocumentPanel != nullptr)
        {
            scene->deleteObject(mDocumentPanel);

            mDocumentPanel = nullptr;
        }


        if (mDimPlate != nullptr)
        {
            scene->deleteObject(mDimPlate);

            mDimPlate = nullptr;
        }
    }


    mDocument = nullptr;

    mVisible = false;

    mPending = Action::None;
}


//============================================================
// Show
//============================================================
void UIDocumentViewer::Show(const DocumentDefinition* document)
{
    if (document == nullptr)
    {
        return;
    }


    mDocument = document;

    mVisible = true;

    mPending = Action::None;

    SetRender(mDimPlate, true);
    SetRender(mDocumentPanel, true);
}


//============================================================
// Hide
//============================================================
void UIDocumentViewer::Hide()
{
    mVisible = false;

    mPending = Action::None;

    mDocument = nullptr;

    SetRender(mDimPlate, false);
    SetRender(mDocumentPanel, false);
}


//============================================================
// Update
//============================================================
void UIDocumentViewer::Update(float dt)
{
    //============================================================
    // 現段階ではAnimationなし
    // ・Fade In
    // ・Paper Animation
    // ・Page Sound
    // 追加可能
    //============================================================
    (void)dt;
}


//============================================================
// Draw
//============================================================
void UIDocumentViewer::Draw() const
{
    if (!mVisible || mDocument == nullptr)
    {
        return;
    }

    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);

    //============================================================
    // Document Title
    //============================================================
    AEUIFont::Scope font(AEUIFont::Style::DocumentTitle);
    vnFont::setColor(0xffffdca0);


    vnFont::print(
        sw * 0.25f,
        sh * 0.19f,
        L"%s",
        mDocument->Title.c_str()
    );


    //============================================================
    // Author
    //============================================================
    if (!mDocument->Author.empty())
    {
        font.Use(AEUIFont::Style::Hint);
        vnFont::setColor(0xffb0a898);


        vnFont::print(
            sw * 0.25f,
            sh * 0.25f,
            L"%s",
            mDocument->Author.c_str()
        );
    }


    font.Use(AEUIFont::Style::Hint);

    //============================================================
    // Separator
    // Fontだけで簡易表現
    //============================================================
    vnFont::setColor(0xff807868);


    vnFont::print(
        sw * 0.25f,
        sh * 0.30f,
        L"----------------------------------------"
    );


    font.Use(AEUIFont::Style::DocumentBody);

    //============================================================
    // Body
    //============================================================
    const std::vector<std::wstring> lines = BuildWrappedLines(mDocument->Body, 42);

    const float bodyX = sw * 0.25f;
    float bodyY = sh * 0.35f;

    constexpr float lineHeight = 27.0f;

    const float maximumY = sh * 0.78f;

    vnFont::setColor(0xffe0d8c8);


    for (const std::wstring& line : lines)
    {
        if (bodyY > maximumY)
        {
            break;
        }

        vnFont::print(
            bodyX,
            bodyY,
            L"%s",
            line.c_str()
        );

        bodyY += lineHeight;
    }


    font.Use(AEUIFont::Style::Hint);

    //============================================================
    // Operation
    //============================================================
    vnFont::setColor(0xff908878);


    vnFont::print(
        sw * 0.5f - 110.0f,
        sh * 0.86f,
        L"Enter / Esc : Close"
    );
}

//============================================================
// Consume Action
//============================================================
UIDocumentViewer::Action UIDocumentViewer::ConsumeAction()
{
    const Action action = mPending;

    mPending = Action::None;

    return action;
}


//============================================================
// Horizontal
//============================================================
void UIDocumentViewer::MoveUIHorizontal(int dir)
{
    (void)dir;
}


//============================================================
// Vertical
//============================================================
void UIDocumentViewer::MoveUIVertical(int dir)
{
    (void)dir;
}


//============================================================
// Submit
// Enter / Aでも閉じられるようにする
//============================================================
void UIDocumentViewer::Submit()
{
    if (!mVisible)
    {
        return;
    }


    mPending = Action::Close;
}


//============================================================
// Cancel
// Esc / Bでも閉じられる
//============================================================
void UIDocumentViewer::Cancel()
{
    if (!mVisible)
    {
        return;
    }


    mPending = Action::Close;
}


//============================================================
// Text Wrap
//============================================================
std::vector<std::wstring> UIDocumentViewer::BuildWrappedLines(
    const std::wstring& text,
    std::size_t maxCharactersPerLine
)
{
    std::vector<std::wstring> result;


    if (maxCharactersPerLine == 0)
    {
        return result;
    }


    std::wstring currentLine;


    for (const wchar_t character : text)
    {
        //========================================================
        // CRは無視
        //========================================================
        if (character == L'\r')
        {
            continue;
        }


        //========================================================
        // 元Textの改行を維持
        //========================================================
        if (character == L'\n')
        {
            result.push_back(currentLine);

            currentLine.clear();

            continue;
        }

        currentLine.push_back(character);

        //========================================================
        // 指定文字数でWrap
        //========================================================
        if (currentLine.size() >= maxCharactersPerLine)
        {
            result.push_back(currentLine);

            currentLine.clear();
        }
    }

    if (!currentLine.empty())
    {
        result.push_back(currentLine);
    }

    return result;
}


//============================================================
// Set Render
//============================================================
void UIDocumentViewer::SetRender(vnSprite* sprite, bool enabled)
{
    if (sprite == nullptr)
    {
        return;
    }


    sprite->setRenderEnable(enabled);
    sprite->setExecuteEnable(enabled);
}


//============================================================
// Sprite Color
//============================================================
void UIDocumentViewer::SetSpriteColor(
    vnSprite* sprite,
    float r,
    float g,
    float b,
    float a
)
{
    if (sprite == nullptr)
    {
        return;
    }


    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
    a = std::clamp(a, 0.0f, 1.0f);


    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        sprite->vtx[i].r = r;
        sprite->vtx[i].g = g;
        sprite->vtx[i].b = b;
        sprite->vtx[i].a = a;
    }
}