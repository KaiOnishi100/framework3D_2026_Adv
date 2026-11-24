#pragma once

#include "UIExplorationMapOverlay.h"
#include "UIObjectiveOverlay.h"
#include "UIInventoryOverlay.h"
#include "UIDocumentOverlay.h"

//============================================================
// Exploration Menu Page
//============================================================
enum class ExplorationMenuPage
{
    Inventory = 0,
    Map,
    Objectives,
    Documents,

    Count
};


//============================================================
// UIExplorationMenu
// Inventory / Map / Objectivesをまとめる親UI
//============================================================
class UIExplorationMenu : public IUIControllable
{
public:

    enum class Action
    {
        None = 0,
        Close
    };


public:

    bool Initialize(
        vnScene* scene,
        InventoryManager* inventory,
        ExplorationLocationSystem* locationSystem,
        ObjectiveManager* objectiveManager,
        DocumentManager* documentManager
    );


    void Terminate(vnScene* scene);


    //========================================================
    // 開く
    // Iキー: InventoryからOpen
    // Mキー: MapからOpen
    //========================================================
    void Show(ExplorationMenuPage startPage);

    void Hide();

    void Update(float dt);

    void Draw() const;


    //========================================================
    // Tab
    //========================================================
    void MoveTab(int dir);


    ExplorationMenuPage GetCurrentPage() const
    {
        return mCurrentPage;
    }


    //========================================================
    // Action
    //========================================================
    Action ConsumeAction();

    //========================================================
    // IUIControllable
    //========================================================
    void MoveUIHorizontal(int dir) override;

    void MoveUIVertical(int dir) override;

    void Submit() override;

    void Cancel() override;


private:

    void ApplyCurrentPage();

    void BuildTabRects();

    void UpdateTabMouse();


    static const wchar_t* GetTabName(ExplorationMenuPage page);


private:

    bool mVisible = false;

    ExplorationMenuPage mCurrentPage = ExplorationMenuPage::Inventory;

    Action mPending = Action::None;


    //========================================================
    // Child Pages
    //========================================================
    UIInventoryOverlay mInventoryPage;

    UIExplorationMapOverlay mMapPage;

    UIObjectiveOverlay mObjectivePage;

    //============================================================
    // Documents
    //============================================================
    UIDocumentOverlay mDocumentPage;

    static constexpr std::size_t TabCount = static_cast<std::size_t>(ExplorationMenuPage::Count);

    //========================================================
    // Tab Mouse Rect
    //========================================================
    std::array<UIRect, TabCount> mTabRects;
};