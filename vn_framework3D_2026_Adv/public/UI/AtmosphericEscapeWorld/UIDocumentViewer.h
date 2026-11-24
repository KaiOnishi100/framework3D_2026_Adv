#pragma once

#include"../../world/AtmosphericEscapeWorld/document/documentManager.h"


class vnScene;


//============================================================
// UIDocumentViewer
// World上でDocumentを初めて取得した瞬間に表示する
// 全画面Document閲覧UI
// ExplorationMenu内のDocuments Pageとは用途を分ける
// DocumentPickup
//      ↓
// Scene
//      ↓
// UIDocumentViewer
//============================================================
class UIDocumentViewer : public IUIControllable
{
public:

    enum class Action
    {
        None = 0,
        Close
    };


public:

    UIDocumentViewer() = default;

    ~UIDocumentViewer() override = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(vnScene* scene);

    //========================================================
    // Terminate
    //========================================================
    void Terminate(vnScene* scene);

    //========================================================
    // Document表示
    // DefinitionはDocumentManager側が所有する
    // Viewer側ではdeleteしない
    //========================================================
    void Show(const DocumentDefinition* document);


    void Hide();


    bool IsVisible() const
    {
        return mVisible;
    }


    //========================================================
    // Update / Draw
    //========================================================
    void Update(float dt);

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
    // 本文を画面幅へ合わせて折り返す
    // 日本語は空白区切りではないので、
    // 現段階では文字数ベースで折り返す
    //========================================================
    static std::vector<std::wstring> BuildWrappedLines(
        const std::wstring& text,
        std::size_t maxCharactersPerLine
    );


    static void SetRender(vnSprite* sprite, bool enabled);


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

    //========================================================
    // 現在読んでいるDocument
    // DocumentManager所有
    //========================================================
    const DocumentDefinition* mDocument = nullptr;

    //========================================================
    // Background
    //========================================================
    vnSprite* mDimPlate = nullptr;

    //========================================================
    // Document Paper
    //
    // 今はGraybox用の単色Panel
    // Asset制作時に紙Textureへ差し替え可能
    //========================================================
    vnSprite* mDocumentPanel = nullptr;
};