#pragma once

//------------------------------------------------------------
// 結果画面オーバーレイ（GameOver / GameClear）
// ・背景画像の切り替え
// ・Title / Exit ボタン
// ・ホバー板（白透明）
// ・クリックでアクションを返す（Sceneは遷移するだけ）
// 注意：vnSprite の delete は vnScene::deleteObject に任せる
//------------------------------------------------------------
class UIResultOverlay : public IUIControllable
{
public:
    enum class Mode
    {
        Hidden,
        GameOver,
        GameClear
    };

    enum class Action
    {
        None,
        Restart,
        ToTitle,
        Exit
    };

public:
    UIResultOverlay() = default;
    ~UIResultOverlay() = default;

    void Initialize(vnScene* scene);
    void Terminate(vnScene* scene);

    // 状態切替（入った瞬間に呼ぶ想定）
    void ShowGameOver();
    void ShowGameClear();
    void Hide();

    // 毎フレーム：ホバー更新
    void Update(float dt);

    // 毎フレーム：クリック結果を返す
    Action ConsumeAction();

    // 強制表示/非表示（まとめ）
    void SetVisible(bool visible);

    // IUIControllable
    void MoveUIHorizontal(int dir) override;
    void MoveUIVertical(int dir) override; // 未使用
    void Submit() override;
    void Cancel() override; // 未使用
private:
    void SetRender(vnSprite* s, bool on);
    void ApplyHoverPlate(); // mSelected に応じてホバー板更新
    const UIRect& GetSelectedRect() const;
private:
    Mode mMode = Mode::Hidden;

    float mSubmitOffsetX = 0.0f;
    float mSubmitOffsetY = 35.0f;
    bool mVisible = false;

    // 背景
    vnSprite* mGameOverBG = nullptr;
    vnSprite* mGameClearBG = nullptr;

    // ボタン
    vnSprite* mBtnTitleSpr = nullptr;
    vnSprite* mBtnExitSpr = nullptr;
    vnSprite* mBtnRestartSpr = nullptr;
    // ホバー板
    vnSprite* mHoverPlate = nullptr;

    // 当たり判定（左上基準）
    UIRect mBtnTitleRect;
    UIRect mBtnExitRect;
    UIRect mBtnRestartRect;

    int mSelected = 0; // 0=Start, 1=Exit
    Action mPending = Action::None;

    // =====================================================
    // パッド決定ヒント（×）とリップル
    // =====================================================
    UIPadSubmitHint mSubmitHint;
};