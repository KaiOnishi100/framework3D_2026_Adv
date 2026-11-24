#pragma once

#include"../../world/AtmosphericEscapeWorld/document/documentManager.h"


class vnScene;


//============================================================
// UIDocumentOverlay
// 取得済みDocument一覧と本文を表示する
// 左: Document Title一覧
// 右:
//   Title
//   Author
//   Body
//============================================================
class UIDocumentOverlay : public IUIControllable
{
public:

    enum class Action
    {
        None = 0,
        Close
    };


private:

    //========================================================
    // 一覧に表示するDocument 1件分
    //========================================================
    struct DocumentVisual
    {
        //====================================================
        // DocumentManager所有のDefinitionへの参照
        // UI側ではdeleteしない
        //====================================================
        const DocumentDefinition* Definition = nullptr;

        UIRect Rect;

        //====================================================
        // 背景Plate
        //====================================================
        vnSprite* Plate = nullptr;
    };


public:

    UIDocumentOverlay() = default;

    ~UIDocumentOverlay() override = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(
        vnScene* scene,
        const DocumentManager* documentManager
    );


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

    void MoveUIVertical(int ) override;

    void Submit() override;

    void Cancel() override;


private:

    //========================================================
    // DocumentManagerから取得済みDocumentを再構築
    //========================================================
    void RebuildDocuments();


    //========================================================
    // ListのPlate色更新
    //========================================================
    void RefreshSelectionVisual();


    //========================================================
    // 本文を指定文字数で折り返す
    // 日本語本文はSpaceがないため
    // Word Wrapではなく文字数ベースで折り返す
    //========================================================
    static std::vector<std::wstring> BuildWrappedLines(
        const std::wstring& text,
        std::size_t maxCharactersPerLine
    );


    //========================================================
    // Sprite Helper
    //========================================================
    static void SetRender(vnSprite* sprite,bool enabled);


    static void SetSpriteColor(
        vnSprite* sprite,
        float r,
        float g,
        float b,
        float a
    );


private:

    //========================================================
    // State
    //========================================================
    bool mVisible = false;

    Action mPending = Action::None;

    int mSelectedIndex = 0;


    //========================================================
    // 一覧Scroll開始位置
    //========================================================
    int mScrollOffset = 0;

    //========================================================
    // Data
    //========================================================
    const DocumentManager* mDocumentManager = nullptr;

    //========================================================
    // Background
    //========================================================
    vnSprite* mDimPlate = nullptr;

    vnSprite* mListPanel = nullptr;

    vnSprite* mDetailPanel = nullptr;

    //========================================================
    // Documents
    //========================================================
    static constexpr std::size_t MaxVisibleDocuments = 8;

    std::vector<DocumentVisual> mDocuments;
};