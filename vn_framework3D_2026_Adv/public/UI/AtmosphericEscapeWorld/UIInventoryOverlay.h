#pragma once

class vnScene;
class InventoryManager;


//============================================================
// UIInventoryOverlay
// Inventory一覧と選択Itemの詳細を表示する
// 現段階:
// ・Item一覧
// ・上下選択
// ・Mouse Hover / Click
// ・Item名
// ・説明
// ・所持数
//============================================================
class UIInventoryOverlay : public IUIControllable
{
public:

    enum class Action
    {
        None = 0,
        Close
    };


private:

    //========================================================
    // 画面に表示する1Item分の情報
    //========================================================
    struct ItemVisual
    {
        InventoryEntry Entry;

        UIRect Rect;

        vnSprite* Plate = nullptr;
    };


public:

    UIInventoryOverlay() = default;

    ~UIInventoryOverlay() override = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(vnScene* scene, const InventoryManager* inventory);

    //========================================================
    // Terminate
    //========================================================
    void Terminate(vnScene* scene);


    //========================================================
    // Open / Close
    //========================================================
    void Show();

    void Hide();


    bool IsVisible() const
    {
        return mVisible;
    }


    //========================================================
    // Update
    //
    // Mouse操作など
    //========================================================
    void Update(float dt);


    //========================================================
    // Font描画
    //========================================================
    void Draw() const;


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

    //========================================================
    // InventoryManagerから現在の所持品を再構築
    //========================================================
    void RebuildItems();


    //========================================================
    // 選択状態のVisual更新
    //========================================================
    void RefreshSelectionVisual();


    //========================================================
    // Sprite表示切替
    //========================================================
    static void SetRender(vnSprite* sprite, bool enabled);

    //========================================================
    // Sprite色
    //========================================================
    static void SetSpriteColor(
        vnSprite* sprite,
        float r,
        float g,
        float b,
        float a
    );

    void DrawWrappedText(
        float x,
        float y,
        const std::wstring& text,
        std::size_t maxCharactersPerLine,
        float lineHeight
    ) const;


private:

    //========================================================
    // State
    //========================================================
    bool mVisible = false;

    Action mPending = Action::None;

    int mSelectedIndex = 0;

    //========================================================
    // Data
    //========================================================
    const InventoryManager* mInventory = nullptr;


    //========================================================
    // Background
    //========================================================
    vnSprite* mDimPlate = nullptr;

    vnSprite* mMainPanel = nullptr;

    vnSprite* mDetailPanel = nullptr;


    //========================================================
    // Item Slot
    // Scroll対応可能
    //========================================================
    static constexpr std::size_t MaxVisibleItems = 8;


    std::vector<ItemVisual> mItems;
};