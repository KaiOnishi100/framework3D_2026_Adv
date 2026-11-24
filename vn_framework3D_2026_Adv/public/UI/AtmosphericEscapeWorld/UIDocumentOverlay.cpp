#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "UIDocumentOverlay.h"


//============================================================
// Initialize
//============================================================
bool UIDocumentOverlay::Initialize(vnScene* scene, const DocumentManager* documentManager)
{
    if (scene == nullptr || documentManager == nullptr)
    {
        return false;
    }


    mDocumentManager = documentManager;

    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);


    //============================================================
    // 全画面Dim
    //============================================================
    mDimPlate =
        new vnSprite(
            sw * 0.5f,
            sh * 0.5f,
            sw,
            sh,
            L""
        );


    mDimPlate->setRenderPriority(3000);


    SetSpriteColor(
        mDimPlate,
        0.0f,
        0.0f,
        0.0f,
        0.82f
    );

    scene->registerObject(mDimPlate);

    //============================================================
    // 左側Document List
    //============================================================
    mListPanel =
        new vnSprite(
            sw * 0.30f,
            sh * 0.52f,
            sw * 0.42f,
            sh * 0.72f,
            L""
        );

    mListPanel->setRenderPriority(3010);

    SetSpriteColor(
        mListPanel,
        0.035f,
        0.045f,
        0.050f,
        0.96f
    );

    scene->registerObject(mListPanel);


    //============================================================
    // 右側Document Detail
    //============================================================
    mDetailPanel =
        new vnSprite(
            sw * 0.72f,
            sh * 0.52f,
            sw * 0.34f,
            sh * 0.72f,
            L""
        );


    mDetailPanel->setRenderPriority(3010);


    SetSpriteColor(
        mDetailPanel,
        0.045f,
        0.050f,
        0.055f,
        0.96f
    );


    scene->registerObject(mDetailPanel);

    //============================================================
    // Document Slot
    // 最大8件を同時表示する
    // 9件目以降はScrollする
    //============================================================
    mDocuments.reserve(MaxVisibleDocuments);

    const float slotX = sw * 0.12f;
    const float slotY = sh * 0.23f;
    const float slotW = sw * 0.34f;
    const float slotH = 55.0f;
    const float slotGap = 8.0f;


    for (std::size_t i = 0; i < MaxVisibleDocuments; ++i)
    {
        DocumentVisual visual;


        visual.Rect =
        {
            slotX,
            slotY +
            static_cast<float>(i) * (slotH + slotGap),
            slotW,
            slotH
        };


        visual.Plate =
            new vnSprite(
                visual.Rect.x + visual.Rect.w * 0.5f,
                visual.Rect.y + visual.Rect.h * 0.5f,
                visual.Rect.w,
                visual.Rect.h,
                L""
            );

        visual.Plate->setRenderPriority(3020);

        SetSpriteColor(
            visual.Plate,
            0.10f,
            0.11f,
            0.12f,
            0.92f
        );

        scene->registerObject(visual.Plate);

        mDocuments.push_back(visual);
    }

    Hide();

    return true;
}


//============================================================
// Terminate
//============================================================
void UIDocumentOverlay::Terminate(vnScene* scene)
{
    if (scene == nullptr)
    {
        return;
    }


    //============================================================
    // Document Slots
    //============================================================
    for (DocumentVisual& document : mDocuments)
    {
        if (document.Plate != nullptr)
        {
            scene->deleteObject(document.Plate);
            document.Plate = nullptr;
        }
    }


    mDocuments.clear();


    //============================================================
    // Panels
    //============================================================
    if (mDetailPanel != nullptr)
    {
        scene->deleteObject(mDetailPanel);
        mDetailPanel = nullptr;
    }


    if (mListPanel != nullptr)
    {
        scene->deleteObject(mListPanel);
        mListPanel = nullptr;
    }


    if (mDimPlate != nullptr)
    {
        scene->deleteObject(mDimPlate);
        mDimPlate = nullptr;
    }


    mDocumentManager = nullptr;

    mVisible = false;

    mPending = Action::None;

    mSelectedIndex = 0;

    mScrollOffset = 0;
}


//============================================================
// Show
//============================================================
void UIDocumentOverlay::Show()
{
    if (mVisible)
    {
        return;
    }

    mVisible = true;

    mPending = Action::None;

    //============================================================
    // DocumentはGameplay中に増えるため開くたびに最新状態へ更新する
    //============================================================
    RebuildDocuments();

    SetRender(mDimPlate, true);
    SetRender(mListPanel, true);
    SetRender(mDetailPanel, true);

    RefreshSelectionVisual();
}


//============================================================
// Hide
//============================================================
void UIDocumentOverlay::Hide()
{
    mVisible = false;

    SetRender(mDimPlate, false);
    SetRender(mListPanel, false);
    SetRender(mDetailPanel, false);

    //============================================================
    // Document Slotをすべて非表示
    //============================================================
    for (DocumentVisual& document : mDocuments)
    {
        SetRender(document.Plate, false);
    }
}


//============================================================
// Update
//============================================================
void UIDocumentOverlay::Update(float dt)
{
    (void)dt;


    if (!mVisible)
    {
        return;
    }


    const std::vector<const DocumentDefinition*> collected = mDocumentManager->GetCollectedDocuments();

    const int totalCount = static_cast<int>(collected.size());


    //============================================================
    // Mouse Hover / Click
    //============================================================
    for (std::size_t i = 0; i < mDocuments.size(); ++i)
    {
        const int documentIndex = mScrollOffset + static_cast<int>(i);

        if (documentIndex < 0 || documentIndex >= totalCount)
        {
            continue;
        }


        if (UI_IsHover(mDocuments[i].Rect))
        {
            mSelectedIndex = documentIndex;

            RefreshSelectionVisual();

            if (UI_IsClicked(mDocuments[i].Rect))
            {
                mSelectedIndex = documentIndex;
                RefreshSelectionVisual();
            }


            break;
        }
    }
}


//============================================================
// Draw
//============================================================
void UIDocumentOverlay::Draw() const
{
    if (!mVisible || mDocumentManager == nullptr)
    {
        return;
    }


    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);

    //============================================================
    // Title
    //============================================================
    AEUIFont::Scope font(AEUIFont::Style::Heading);
    vnFont::setColor(0xffffffff);

    vnFont::print(
        45.0f,
        35.0f,
        L"DOCUMENTS"
    );

    //============================================================
    // 取得済みDocument
    //============================================================
    std::vector<const DocumentDefinition*> collected = mDocumentManager->GetCollectedDocuments();

    //============================================================
    // Story順に並べる
    //============================================================
    std::sort(collected.begin(), collected.end(), [](const DocumentDefinition* a, const DocumentDefinition* b)
        {
            if (a == nullptr)
            {
                return false;
            }

            if (b == nullptr)
            {
                return true;
            }

            return a->DisplayOrder < b->DisplayOrder;
        }
    );


    //============================================================
    // Empty
    //============================================================
    if (collected.empty())
    {
        vnFont::setColor(0xff909090);

        vnFont::print(
            sw * 0.16f,
            sh * 0.30f,
            L"No documents found."
        );


        return;
    }


    font.Use(AEUIFont::Style::Label);

    //============================================================
    // Document List
    //============================================================
    for (std::size_t visualIndex = 0; visualIndex < mDocuments.size(); ++visualIndex)
    {
        const int documentIndex = mScrollOffset + static_cast<int>(visualIndex);

        if (documentIndex < 0 || documentIndex >= static_cast<int>(collected.size()))
        {
            continue;
        }

        const DocumentDefinition* definition = collected[documentIndex];

        if (definition == nullptr)
        {
            continue;
        }


        const bool selected = documentIndex == mSelectedIndex;

        vnFont::setColor(selected ? 0xffffd060 : 0xffeeeeee);

        vnFont::print(
            mDocuments[visualIndex].Rect.x + 14.0f,
            mDocuments[visualIndex].Rect.y + 17.0f,
            selected ? L"> %s" : L"%s",
            definition->Title.c_str()
        );
    }


    //============================================================
    // Selection Safety
    //============================================================
    if (mSelectedIndex < 0 || mSelectedIndex >= static_cast<int>(collected.size()))
    {
        return;
    }

    const DocumentDefinition* selected = collected[mSelectedIndex];

    if (selected == nullptr)
    {
        return;
    }


    font.Use(AEUIFont::Style::DocumentTitle);

    //============================================================
    // Document Title
    //============================================================
    vnFont::setColor(0xffffd060);

    vnFont::print(
        sw * 0.58f,
        sh * 0.22f,
        L"%s",
        selected->Title.c_str()
    );

    font.Use(AEUIFont::Style::Hint);

    //============================================================
    // Author
    //============================================================
    vnFont::setColor(0xffa0a0a0);


    if (!selected->Author.empty())
    {
        vnFont::print(
            sw * 0.58f,
            sh * 0.275f,
            L"%s",
            selected->Author.c_str()
        );
    }


    font.Use(AEUIFont::Style::DocumentPreview);

    //============================================================
    // Body
    // 現在のFont APIでは自動折返しを行っていないため
    // wstringを行単位に分割する
    //============================================================
    const std::vector<std::wstring> bodyLines = BuildWrappedLines(selected->Body, 30);

    vnFont::setColor(0xffdddddd);

    const float bodyX = sw * 0.58f;

    float bodyY = sh * 0.34f;

    constexpr float lineHeight = 27.0f;

    //============================================================
    // ※Panelからはみ出さない範囲だけ表示
    //============================================================
    const float maximumY = sh * 0.80f;

    for (const std::wstring& line : bodyLines)
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
    // 操作説明
    //============================================================
    vnFont::setColor(0xff909090);

    vnFont::print(
        sw - 450.0f,
        sh - 45.0f,
        L"W/S: Select  Q/E: Change Tab"
    );
}


//============================================================
// Action
//============================================================
UIDocumentOverlay::Action UIDocumentOverlay::ConsumeAction()
{
    const Action result = mPending;

    mPending = Action::None;

    return result;
}


//============================================================
// Horizontal
//============================================================
void UIDocumentOverlay::MoveUIHorizontal(int dir)
{
    (void)dir;
}


//============================================================
// Vertical
//============================================================
void UIDocumentOverlay::MoveUIVertical(int dir)
{
    if (!mVisible || dir == 0 || mDocumentManager == nullptr)
    {
        return;
    }

    const auto collected = mDocumentManager->GetCollectedDocuments();

    const int count = static_cast<int>(collected.size());

    if (count <= 0)
    {
        return;
    }


    //============================================================
    // Selection移動
    //============================================================
    mSelectedIndex += dir;

    //============================================================
    // Loop
    //============================================================
    if (mSelectedIndex < 0)
    {
        mSelectedIndex = count - 1;
    }


    if (mSelectedIndex >= count)
    {
        mSelectedIndex = 0;
    }


    //============================================================
    // Scroll
    //
    // 選択対象が表示範囲より下
    //============================================================
    if (mSelectedIndex >= mScrollOffset + static_cast<int>(MaxVisibleDocuments))
    {
        mScrollOffset = mSelectedIndex - static_cast<int>(MaxVisibleDocuments) + 1;
    }


    //============================================================
    // 選択対象が表示範囲より上
    //============================================================
    if (mSelectedIndex < mScrollOffset)
    {
        mScrollOffset = mSelectedIndex;
    }


    RefreshSelectionVisual();
}


//============================================================
// Submit
//============================================================
void UIDocumentOverlay::Submit()
{
    
}

//============================================================
// Cancel
//============================================================
void UIDocumentOverlay::Cancel()
{
    if (!mVisible)
    {
        return;
    }

    mPending = Action::Close;
}


//============================================================
// Rebuild
//============================================================
void UIDocumentOverlay::RebuildDocuments()
{
    if (mDocumentManager == nullptr)
    {
        return;
    }

    std::vector<const DocumentDefinition*> collected = mDocumentManager->GetCollectedDocuments();

    //============================================================
    // Story Order
    //============================================================
    std::sort(collected.begin(), collected.end(), [](const DocumentDefinition* a, const DocumentDefinition* b)
        {
            if (a == nullptr)
            {
                return false;
            }

            if (b == nullptr)
            {
                return true;
            }

            return a->DisplayOrder < b->DisplayOrder;
        }
    );


    //============================================================
    // Selection Safety
    //============================================================
    if (collected.empty())
    {
        mSelectedIndex = 0;
        mScrollOffset = 0;
    }
    else
    {
        if (mSelectedIndex >= static_cast<int>(collected.size()))
        {
            mSelectedIndex = static_cast<int>(collected.size()) - 1;
        }


        if (mSelectedIndex < 0)
        {
            mSelectedIndex = 0;
        }
    }


    //============================================================
    // Slot Data更新
    //============================================================
    for (std::size_t visualIndex = 0; visualIndex < mDocuments.size(); ++visualIndex)
    {
        DocumentVisual& visual = mDocuments[visualIndex];

        const int documentIndex = mScrollOffset + static_cast<int>(visualIndex);

        if (documentIndex >= 0 && documentIndex < static_cast<int>(collected.size()))
        {
            visual.Definition = collected[documentIndex];

            SetRender(visual.Plate, true);
        }
        else
        {
            visual.Definition = nullptr;

            SetRender(visual.Plate, false);
        }
    }

    RefreshSelectionVisual();
}


//============================================================
// Selection Visual
//============================================================
void UIDocumentOverlay::RefreshSelectionVisual()
{
    if (!mVisible)
    {
        return;
    }


    for (std::size_t visualIndex = 0; visualIndex < mDocuments.size(); ++visualIndex)
    {
        DocumentVisual& visual = mDocuments[visualIndex];

        if (visual.Plate == nullptr || visual.Definition == nullptr)
        {
            continue;
        }


        const int documentIndex = mScrollOffset + static_cast<int>(visualIndex);

        if (documentIndex == mSelectedIndex)
        {
            //====================================================
            // Selected
            //====================================================
            SetSpriteColor(
                visual.Plate,
                0.30f,
                0.34f,
                0.37f,
                0.98f
            );
        }
        else
        {
            //====================================================
            // Normal
            //====================================================
            SetSpriteColor(
                visual.Plate,
                0.10f,
                0.11f,
                0.12f,
                0.92f
            );
        }
    }
}


//============================================================
// Text Wrap
//============================================================
std::vector<std::wstring> UIDocumentOverlay::BuildWrappedLines(
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

    for (wchar_t character : text)
    {
        //========================================================
        // CRは無視
        //========================================================
        if (character == L'\r')
        {
            continue;
        }

        //========================================================
        // 元Textの改行
        //========================================================
        if (character == L'\n')
        {
            result.push_back(currentLine);

            currentLine.clear();

            continue;
        }

        currentLine.push_back(character);

        //========================================================
        // 一定文字数で折返す
        //========================================================
        if (currentLine.size() >= maxCharactersPerLine)
        {
            result.push_back(currentLine);

            currentLine.clear();
        }
    }


    //============================================================
    // 最終行
    //============================================================
    if (!currentLine.empty())
    {
        result.push_back(currentLine);
    }


    return result;
}


//============================================================
// Render
//============================================================
void UIDocumentOverlay::SetRender(vnSprite* sprite, bool enabled)
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
void UIDocumentOverlay::SetSpriteColor(
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