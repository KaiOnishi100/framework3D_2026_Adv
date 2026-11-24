
#pragma once

class TitleUI : public IUIControllable
{
public:
    enum class Action
    {
        None,
        Start,
        Continue,
        Exit,
        Options
    };

    void Initialize(vnScene* scene);
    void Terminate(vnScene* scene);
    void Update(float dt);

    Action ConsumeAction();

    // セーブデータが存在するときだけCONTINUEを選択可能にする
    void SetContinueEnabled(bool enabled);
    void SetMenuVisible(bool visible);

    // 既存のUI入力インターフェース
    void MoveUIHorizontal(int dir) override;
    void MoveUIVertical(int dir) override;
    void Submit() override;
    void Cancel() override;

private:
    void ApplyHoverPlate();
    void ApplySubmitHint();

    // 現在選択しているボタンの当たり判定を取得
    const UIRect& GetSelectedRect() const;

private:
    // 0=New Game, 1=Continue, 2=Exit
    int mSelected = 0;
    Action mPending = Action::None;

    bool mContinueEnabled = false;

    vnSprite* mTitleSprite = nullptr;
    vnSprite* mStartSprite = nullptr;
    vnSprite* mContinueSprite = nullptr;
    vnSprite* mExitSprite = nullptr;
    vnSprite* mOptionsSprite = nullptr;
    vnSprite* mHoverPlate = nullptr;

    UIRect mBtnStart{};
    UIRect mBtnContinue{};
    UIRect mBtnExit{};
    UIRect mBtnOptions{};

    UIPadSubmitHint mSubmitHint;

    float mSubmitOffsetX = 0.0f;
    float mSubmitOffsetY = 18.0f;
};
