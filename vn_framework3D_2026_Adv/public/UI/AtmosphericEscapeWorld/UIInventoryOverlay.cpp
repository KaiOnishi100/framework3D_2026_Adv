#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "UIInventoryOverlay.h"

bool UIInventoryOverlay::Initialize(vnScene* scene, const InventoryManager* inventory)
{
    if (scene == nullptr || inventory == nullptr)
    {
        return false;
    }


    mInventory = inventory;


    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);


    //========================================================
    // 全画面Dim
    //========================================================
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

    //========================================================
    // 左側Inventory Panel
    //========================================================
    mMainPanel =
        new vnSprite(
            sw * 0.30f,
            sh * 0.52f,
            sw * 0.42f,
            sh * 0.72f,
            L""
        );


    mMainPanel->setRenderPriority(3010);


    SetSpriteColor(
        mMainPanel,
        0.035f,
        0.045f,
        0.050f,
        0.96f
    );


    scene->registerObject(mMainPanel);

    //========================================================
    // 右側Detail Panel
    //========================================================
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


    //========================================================
    // Item Slot Plate
    // 中身はShow()時にInventoryから作る
    //========================================================
    mItems.reserve(MaxVisibleItems);


    const float slotX = sw * 0.12f;
    const float slotY = sh * 0.23f;
    const float slotW = sw * 0.34f;
    const float slotH = 55.0f;
    const float slotGap = 8.0f;


    for (std::size_t i = 0; i < MaxVisibleItems; ++i)
    {
        ItemVisual item;


        item.Rect =
        {
            slotX,
            slotY + static_cast<float>(i) * (slotH + slotGap),
            slotW,
            slotH
        };


        item.Plate =
            new vnSprite(
                item.Rect.x +
                item.Rect.w * 0.5f,
                item.Rect.y +
                item.Rect.h * 0.5f,
                item.Rect.w,
                item.Rect.h,
                L""
            );


        item.Plate->setRenderPriority(3020);

        SetSpriteColor(item.Plate, 0.10f, 0.11f, 0.12f, 0.92f);

        scene->registerObject(item.Plate);

        mItems.push_back(item);
    }


    Hide();

    return true;
}

void UIInventoryOverlay::Terminate(vnScene* scene)
{
    if (scene == nullptr)
    {
        return;
    }


    //========================================================
    // Slots
    //========================================================
    for (ItemVisual& item : mItems)
    {
        if (item.Plate != nullptr)
        {
            scene->deleteObject(item.Plate);
            item.Plate = nullptr;
        }
    }

    mItems.clear();

    //========================================================
    // Panels
    //========================================================
    if (mDetailPanel != nullptr)
    {
        scene->deleteObject(mDetailPanel);
        mDetailPanel = nullptr;
    }


    if (mMainPanel != nullptr)
    {
        scene->deleteObject(mMainPanel);
        mMainPanel = nullptr;
    }


    if (mDimPlate != nullptr)
    {
        scene->deleteObject(mDimPlate);
        mDimPlate = nullptr;
    }

    mInventory = nullptr;

    mVisible = false;

    mPending = Action::None;

    mSelectedIndex = 0;
}

//============================================================
// Show
//============================================================
void UIInventoryOverlay::Show()
{
    if (mVisible)
    {
        return;
    }

    mVisible = true;

    mPending = Action::None;


    //========================================================
    // Inventory内容を最新化
    // Mapと違いInventoryはGameplay中に増減するため、
    // Open時に毎回取得する
    //========================================================
    RebuildItems();

    SetRender(mDimPlate, true);
    SetRender(mMainPanel, true);
    SetRender(mDetailPanel, true);

    RefreshSelectionVisual();
}


//============================================================
// Hide
//============================================================
void UIInventoryOverlay::Hide()
{
    mVisible = false;


    SetRender(mDimPlate, false);
    SetRender(mMainPanel, false);
    SetRender(mDetailPanel, false);

    //========================================================
    // Slotはすべて消す
    //========================================================
    for (ItemVisual& item : mItems)
    {
        SetRender(item.Plate, false);
    }
}

void UIInventoryOverlay::Update(float dt)
{
    (void)dt;


    if (!mVisible)
    {
        return;
    }

    //========================================================
    // Mouse Hover
    //========================================================
    for (std::size_t i = 0; i < mItems.size(); ++i)
    {
        ItemVisual& item = mItems[i];

        if (item.Entry.ItemId.empty())
        {
            continue;
        }


        if (UI_IsHover(item.Rect))
        {
            mSelectedIndex = static_cast<int>(i);

            RefreshSelectionVisual();

            //================================================
            // 現段階ではClickでも単に選択するだけ
            // Consumable → Use
            // KeyItem    → Examine
            //================================================
            if (UI_IsClicked(item.Rect))
            {
                mSelectedIndex = static_cast<int>(i);
                RefreshSelectionVisual();
            }


            break;
        }
    }
}

void UIInventoryOverlay::Draw() const
{
    if (!mVisible || mInventory == nullptr)
    {
        return;
    }

    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);


    //========================================================
    // Title
    //========================================================
    AEUIFont::Scope font(AEUIFont::Style::Heading);
    vnFont::setColor(0xffffffff);


    vnFont::print(
        45.0f,
        35.0f,
        L"INVENTORY"
    );


    font.Use(AEUIFont::Style::Label);

    //========================================================
    // Item List
    //========================================================
    bool hasItem = false;

    for (std::size_t i = 0; i < mItems.size(); ++i)
    {
        const ItemVisual& item = mItems[i];


        if (item.Entry.ItemId.empty())
        {
            continue;
        }

        hasItem = true;

        const InventoryItemDefinition* definition = mInventory->FindDefinition(item.Entry.ItemId);


        if (definition == nullptr)
        {
            continue;
        }


        const bool selected = static_cast<int>(i) == mSelectedIndex;

        vnFont::setColor(selected ? 0xffffd060 : 0xffeeeeee);


        vnFont::print(
            item.Rect.x + 14.0f,
            item.Rect.y + 17.0f,
            selected ? L"> %s" : L"%s",
            definition->Name.c_str()
        );


        //====================================================
        // Count
        //====================================================
        vnFont::print(
            item.Rect.x + item.Rect.w - 65.0f,
            item.Rect.y + 17.0f,
            L"x%u",
            item.Entry.Count
        );
    }


    //========================================================
    // Empty
    //========================================================
    if (!hasItem)
    {
        vnFont::setColor(0xff909090);


        vnFont::print(
            sw * 0.16f,
            sh * 0.30f,
            L"No items."
        );

        return;
    }


    //========================================================
    // Selected Item Detail
    //========================================================
    if (mSelectedIndex >= 0 && mSelectedIndex < static_cast<int>(mItems.size()))
    {
        const ItemVisual& selected = mItems[mSelectedIndex];


        if (!selected.Entry.ItemId.empty())
        {
            const InventoryItemDefinition* definition = mInventory->FindDefinition(selected.Entry.ItemId);


            if (definition != nullptr)
            {
                //============================================
                // Name
                // Item名は通常通り1行で描画する
                //============================================
                font.Use(AEUIFont::Style::Heading);
                vnFont::setColor(0xffffd060);

                vnFont::print(
                    sw * 0.58f,
                    sh * 0.24f,
                    L"%s",
                    definition->Name.c_str()
                );


                //============================================
                // Count
                //============================================
                font.Use(AEUIFont::Style::Detail);
                vnFont::setColor(0xffcccccc);

                vnFont::print(
                    sw * 0.58f,
                    sh * 0.30f,
                    L"Count: %u",
                    selected.Entry.Count
                );


                //============================================
                // Description
                //============================================
                vnFont::setColor(0xffdddddd);

                DrawWrappedText(
                    sw * 0.58f,                   // 描画開始X
                    sh * 0.38f,                   // 描画開始Y
                    definition->Description,      // 説明文
                    24,                           // 1行最大文字数
                    28.0f                         // 行間
                );
            }
        }
    }

    font.Use(AEUIFont::Style::Hint);

    //========================================================
    // 操作
    //========================================================
    vnFont::setColor(0xff909090);


    vnFont::print(
        sw - 390.0f,
        sh - 45.0f,
        L"W/S: Select  I: Close"
    );
}

UIInventoryOverlay::Action UIInventoryOverlay::ConsumeAction()
{
    const Action action = mPending;

    mPending = Action::None;

    return action;
}

void UIInventoryOverlay::MoveUIHorizontal(int dir)
{
    (void)dir;
}

void UIInventoryOverlay::MoveUIVertical(int dir)
{
    if (!mVisible || dir == 0 || mInventory == nullptr)
    {
        return;
    }

    const auto entries = mInventory->GetEntries();
    const int count = static_cast<int>((std::min)(entries.size(), MaxVisibleItems));

    if (count <= 0)
    {
        return;
    }

    mSelectedIndex += dir;

    //========================================================
    // Loop
    //========================================================
    if (mSelectedIndex < 0)
    {
        mSelectedIndex = count - 1;
    }


    if (mSelectedIndex >= count)
    {
        mSelectedIndex = 0;
    }


    RefreshSelectionVisual();
}

void UIInventoryOverlay::Submit()
{
}

void UIInventoryOverlay::Cancel()
{
    if (!mVisible)
    {
        return;
    }

    mPending = Action::Close;
}


void UIInventoryOverlay::RebuildItems()
{
    if (mInventory == nullptr)
    {
        return;
    }

    //========================================================
    // 現在のInventory
    //========================================================
    const std::vector<InventoryEntry> entries = mInventory->GetEntries();

    //========================================================
    // 全Slotを一旦空にする
    //========================================================
    for (ItemVisual& item : mItems)
    {
        item.Entry =
        {
            "",
            0
        };

        SetRender(item.Plate, false);
    }

    //========================================================
    // 表示可能数までコピー
    //========================================================
    const std::size_t count = (std::min)(entries.size(), MaxVisibleItems);

    for (std::size_t i = 0; i < count; ++i)
    {
        mItems[i].Entry = entries[i];

        SetRender(mItems[i].Plate, true);
    }

    //========================================================
    // Selection Safety
    //========================================================
    if (count == 0)
    {
        mSelectedIndex = 0;
    }
    else if (mSelectedIndex >= static_cast<int>(count))
    {
        mSelectedIndex = static_cast<int>(count) - 1;
    }

    RefreshSelectionVisual();
}

void UIInventoryOverlay::RefreshSelectionVisual()
{
    if (!mVisible)
    {
        return;
    }

    for (std::size_t i = 0; i < mItems.size(); ++i)
    {
        ItemVisual& item = mItems[i];


        if (item.Plate == nullptr || item.Entry.ItemId.empty())
        {
            continue;
        }


        if (static_cast<int>(i) == mSelectedIndex)
        {
            //================================================
            // 選択中
            //================================================
            SetSpriteColor(
                item.Plate,
                0.30f,
                0.34f,
                0.37f,
                0.98f
            );
        }
        else
        {
            //================================================
            // 通常
            //================================================
            SetSpriteColor(
                item.Plate,
                0.10f,
                0.11f,
                0.12f,
                0.92f
            );
        }
    }
}

void UIInventoryOverlay::SetRender(vnSprite* sprite,bool enabled)
{
    if (sprite == nullptr)
    {
        return;
    }


    sprite->setRenderEnable(enabled);
    sprite->setExecuteEnable(enabled);
}


void UIInventoryOverlay::SetSpriteColor(
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


    for (int i = 0;i < vnSprite::VertexNum;++i)
    {
        sprite->vtx[i].r = r;
        sprite->vtx[i].g = g;
        sprite->vtx[i].b = b;
        sprite->vtx[i].a = a;
    }
}

void UIInventoryOverlay::DrawWrappedText(float x, float y, const std::wstring& text, std::size_t maxCharactersPerLine, float lineHeight) const
{
    //========================================================
    // Safety
    //========================================================
    if (text.empty() || maxCharactersPerLine == 0)
    {
        return;
    }

    std::wstring line;

    float currentY = y;

    //========================================================
    // ある程度再確保を防ぐ
    //========================================================
    line.reserve(maxCharactersPerLine);

    for (wchar_t ch : text)
    {
        //====================================================
        // 元文章に明示的な改行がある場合
        // その時点で現在のLineを描画する
        //====================================================
        if (ch == L'\n')
        {
            vnFont::print(
                x,
                currentY,
                L"%s",
                line.c_str()
            );

            line.clear();

            currentY += lineHeight;

            continue;
        }

        //====================================================
        // 文字追加
        //====================================================
        line += ch;

        //====================================================
        // 最大文字数へ到達
        //====================================================
        if (line.length() >= maxCharactersPerLine)
        {
            vnFont::print(
                x,
                currentY,
                L"%s",
                line.c_str()
            );

            line.clear();

            currentY += lineHeight;
        }
    }

    //========================================================
    // 最後に余った文字を描画
    //========================================================
    if (!line.empty())
    {
        vnFont::print(
            x,
            currentY,
            L"%s",
            line.c_str()
        );
    }
}
