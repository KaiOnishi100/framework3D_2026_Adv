#pragma once

class vnScene;

//------------------------------------------------------------
// ポーズ画面オーバーレイ
// ・画面を暗くする黒板（全画面）
// ・Resume / Title / Exit のボタン
// ・ホバー板（白透明）
// ・Submitで決定（パッドなら×の表示も付けられる）
//------------------------------------------------------------
class UIPauseOverlay : public IUIControllable
{
public:
    enum class Action
    {
        None,
        Resume,
        Save,
        ToTitle,
        Exit,
        Options
    };

public:
    void Initialize(vnScene* scene);
    void Terminate(vnScene* scene);

    void Show();     // ポーズ開始（表示ON）
    void Hide();     // ポーズ解除（表示OFF）
    bool IsVisible() const { return mVisible; }

    // dt を受け取る
    void Update(float dt);

    // 決定結果を吸い出す
    Action ConsumeAction();

    // IUIControllable
    void MoveUIHorizontal(int dir) override;
    void MoveUIVertical(int dir) override;
    void Submit() override;
    void Cancel() override;

    void SetSaveEnabled(bool enabled);

private:
    void SetRender(vnSprite* s, bool on);
    void ApplyHoverPlate();

    const UIRect& GetSelectedRect() const;
private:
    bool   mVisible = false;
    bool mSaveEnabled = true;
    int    mSelected = 0;      // 0=Resume,1=save 2=Title, 3=Exit
    Action mPending = Action::None;

    // 暗くする板（全画面）
    vnSprite* mDimPlate = nullptr;

    // ボタン
    vnSprite* mBtnResume = nullptr;
    vnSprite* mBtnSave = nullptr;
    vnSprite* mBtnTitle = nullptr;
    vnSprite* mBtnExit = nullptr;

    // ホバー板
    vnSprite* mHoverPlate = nullptr;

    vnSprite* mBtnOptions = nullptr;

    // Rect（左上基準）
    UIRect mRectResume;
    UIRect mRectSave;
    UIRect mRectTitle;
    UIRect mRectExit;
    UIRect mRectOptions{};

    UIPadSubmitHint mSubmitHint;

    float mSubmitOffsetX = 28.0f; // ボタン右外側への余白
    float mSubmitOffsetY = 0.0f;  // 右側なので基本0
};