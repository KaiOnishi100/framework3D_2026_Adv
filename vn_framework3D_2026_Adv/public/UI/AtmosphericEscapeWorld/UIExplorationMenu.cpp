#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "UIExplorationMenu.h"

bool UIExplorationMenu::Initialize(
    vnScene* scene, 
    InventoryManager* inventory,
    ExplorationLocationSystem* locationSystem, 
    ObjectiveManager* objectiveManager,
    DocumentManager* documentManager
)
{
    if (scene == nullptr || 
        inventory == nullptr || 
        locationSystem == nullptr || 
        objectiveManager == nullptr ||
        documentManager == nullptr)
    {
        return false;
    }


    //========================================================
    // Inventory
    //========================================================
    if (!mInventoryPage.Initialize(
        scene,
        inventory
    ))
    {
        return false;
    }


    //========================================================
    // Map
    //========================================================
    if (!mMapPage.Initialize(
        scene,
        locationSystem,
        objectiveManager
    ))
    {
        return false;
    }


    //========================================================
    // Objectives
    //========================================================
    if (!mObjectivePage.Initialize(
        scene,
        objectiveManager,
        locationSystem
    ))
    {
        return false;
    }

    //========================================================
    // Documents
    //========================================================
    if (!mDocumentPage.Initialize(
        scene,
        documentManager
    ))
    {
        return false;
    }

    BuildTabRects();

    Hide();

    return true;
}

void UIExplorationMenu::Terminate(vnScene* scene)
{
    mDocumentPage.Terminate(scene);
    mObjectivePage.Terminate(scene);
    mMapPage.Terminate(scene);
    mInventoryPage.Terminate(scene);

    mVisible = false;

    mPending = Action::None;
}

void UIExplorationMenu::Show(ExplorationMenuPage startPage)
{
    mVisible = true;
    mPending = Action::None;
    mCurrentPage = startPage;
    ApplyCurrentPage();
}


void UIExplorationMenu::Hide()
{
    mVisible = false;
    mDocumentPage.Hide();
    mInventoryPage.Hide();
    mMapPage.Hide();
    mObjectivePage.Hide();
    mPending = Action::None;
}

void UIExplorationMenu::Update(float dt)
{
    if (!mVisible)
    {
        return;
    }

    //========================================================
    // Mouse Tab
    //========================================================
    UpdateTabMouse();

    //========================================================
    // Current Page
    //========================================================
    switch (mCurrentPage)
    {
    case ExplorationMenuPage::Inventory:
    {
        mInventoryPage.Update(dt);
        break;
    }

    case ExplorationMenuPage::Map:
    {
        mMapPage.Update(dt);
        break;
    }

    case ExplorationMenuPage::Objectives:
    {
        mObjectivePage.Update(dt);
        break;
    }

    case ExplorationMenuPage::Documents:
    {
        mDocumentPage.Update(dt);
        break;
    }

    default:
        break;
    }
}

void UIExplorationMenu::Draw() const
{
    if (!mVisible)
    {
        return;
    }


    //========================================================
    // Child Page
    //========================================================
    switch (mCurrentPage)
    {
    case ExplorationMenuPage::Inventory:
        mInventoryPage.Draw();
        break;


    case ExplorationMenuPage::Map:
        mMapPage.Draw();
        break;


    case ExplorationMenuPage::Objectives:
        mObjectivePage.Draw();
        break;

    case ExplorationMenuPage::Documents:
    {
        mDocumentPage.Draw();
        break;
    }

    default:
        break;
    }


    //========================================================
    // Tabs
    // Childの後からFont描画して上に出す
    //========================================================
    // ChildのDrawが終わってからタブ用のフォントで登録する
    AEUIFont::Scope font(AEUIFont::Style::Tab);
    for (int i = 0; i < TabCount; ++i)
    {
        const ExplorationMenuPage page = static_cast<ExplorationMenuPage>(i);

        const bool selected = page == mCurrentPage;

        vnFont::setColor(selected ? 0xffffd060 : 0xffa0a0a0);

        vnFont::print(
            mTabRects[i].x + 18.0f,
            mTabRects[i].y + 12.0f,
            selected ? L"[ %s ]" : L"%s",
            GetTabName(page)
        );
    }

    font.Use(AEUIFont::Style::Hint);

    //========================================================
    // Tab操作
    //========================================================
    vnFont::setColor(0xff909090);

    vnFont::print(
        40.0f,
        static_cast<float>(vnMainFrame::screenHeight) - 35.0f,
        L"Q / E : Change Tab"
    );
}

void UIExplorationMenu::MoveTab(int dir)
{
    if (!mVisible || dir == 0)
    {
        return;
    }

    int index = static_cast<int>(mCurrentPage);

    index += dir;

    constexpr int pageCount = static_cast<int>( ExplorationMenuPage::Count );

    //========================================================
    // Loop
    //========================================================
    if (index < 0)
    {
        index = pageCount - 1;
    }

    if (index >= pageCount)
    {
        index = 0;
    }

    mCurrentPage = static_cast<ExplorationMenuPage>(index);

    ApplyCurrentPage();
}

UIExplorationMenu::Action UIExplorationMenu::ConsumeAction()
{
    const Action action = mPending;

    mPending = Action::None;

    return action;
}

void UIExplorationMenu::MoveUIHorizontal(int dir)
{
    switch (mCurrentPage)
    {
    case ExplorationMenuPage::Inventory:
    {
        mInventoryPage.MoveUIHorizontal(dir);
        break;
    }

    case ExplorationMenuPage::Map:
    {
        mMapPage.MoveUIHorizontal(dir);
        break;
    }

    case ExplorationMenuPage::Objectives:
    {
        mObjectivePage.MoveUIHorizontal(dir);
        break;
    }

    case ExplorationMenuPage::Documents:
    {
        mDocumentPage.MoveUIHorizontal(dir);
        break;
    }

    default:
        break;
    }
}

void UIExplorationMenu::MoveUIVertical(int dir)
{
    switch (mCurrentPage)
    {
    case ExplorationMenuPage::Inventory:
    {
        mInventoryPage.MoveUIVertical(dir);
        break;
    }

    case ExplorationMenuPage::Map:
    {
        mMapPage.MoveUIVertical(dir);
        break;
    }

    case ExplorationMenuPage::Objectives:
    {
        mObjectivePage.MoveUIVertical(dir);
        break;
    }

    case ExplorationMenuPage::Documents:
    {
        mDocumentPage.MoveUIVertical(dir);
        break;
    }

    default:
        break;
    }
}

void UIExplorationMenu::Submit()
{
    switch (mCurrentPage)
    {
    case ExplorationMenuPage::Inventory:
        mInventoryPage.Submit();
        break;

    case ExplorationMenuPage::Map:
        mMapPage.Submit();
        break;

    case ExplorationMenuPage::Objectives:
        mObjectivePage.Submit();
        break;

    case ExplorationMenuPage::Documents:
        mDocumentPage.Submit();
        break;
    
    default:
        break;
    }
}

void UIExplorationMenu::Cancel()
{
    if (!mVisible)
    {
        return;
    }
    //========================================================
    // Esc/Bはページではなく
    // ExplorationMenu全体を閉じる
    //========================================================
    mPending = Action::Close;
}

void UIExplorationMenu::ApplyCurrentPage()
{
    //========================================================
    // 全部Hide
    //========================================================
    mInventoryPage.Hide();
    mMapPage.Hide();
    mObjectivePage.Hide();
    mDocumentPage.Hide();

    if (!mVisible)
    {
        return;
    }

    //========================================================
    // 選択PageだけShow
    //========================================================
    switch (mCurrentPage)
    {
    case ExplorationMenuPage::Inventory:
    {
        mInventoryPage.Show();
        break;
    }
    case ExplorationMenuPage::Map:
    {
        mMapPage.Show();
        break;
    }
    case ExplorationMenuPage::Objectives:
    {
        mObjectivePage.Show();
        break;
    }
    case ExplorationMenuPage::Documents:
    {
        mDocumentPage.Show();
        break;
    }
    default:
        break;
    }
}

void UIExplorationMenu::BuildTabRects()
{
    const float sw = static_cast<float>(vnMainFrame::screenWidth);

    const float tabWidth = 180.0f;

    const float tabHeight = 44.0f;

    const float gap = 12.0f;


    //============================================================
    // Page数から自動計算
    //============================================================
    const float tabCount = static_cast<float>(TabCount);

    const float totalWidth = tabWidth * tabCount + gap * (tabCount - 1.0f);

    const float startX = sw * 0.5f - totalWidth * 0.5f;

    const float y = 45.0f;

    for (std::size_t i = 0; i < TabCount; ++i)
    {
        mTabRects[i] =
        {
            startX +
            static_cast<float>(i) * (tabWidth + gap),
            y,
            tabWidth,
            tabHeight
        };
    }
}


void UIExplorationMenu::UpdateTabMouse()
{
    for (int i = 0; i < TabCount; ++i)
    {
        if (UI_IsClicked(mTabRects[i]))
        {
            mCurrentPage = static_cast<ExplorationMenuPage>(i);
            ApplyCurrentPage();

            break;
        }
    }
}

const wchar_t* UIExplorationMenu::GetTabName(ExplorationMenuPage page)
{
    switch (page)
    {
    case ExplorationMenuPage::Inventory:
        return L"INVENTORY";

    case ExplorationMenuPage::Map:
        return L"MAP";

    case ExplorationMenuPage::Objectives:
        return L"OBJECTIVES";

    case ExplorationMenuPage::Documents:
        return L"DOCUMENTS";

    default:
        return L"";
    }
}
