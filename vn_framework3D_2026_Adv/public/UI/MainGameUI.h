#pragma once

//------------------------------------------------------------
// メインゲーム用UIをまとめるクラス
//------------------------------------------------------------
class MainGameUI
{
public:
    // Scene側のGameStateに依存したくないので、UI用の結果種別を持つ
    enum class ResultType
    {
        GameOver,
        GameClear
    };

public:
    MainGameUI() = default;
    ~MainGameUI() = default;

    void Initialize(vnScene* scene);
    void Terminate(vnScene* scene);

    // Playing中：HP更新
    void UpdatePlaying(const Player* player, const Boss* boss, float dt);

    // Result中：ホバー更新
    void UpdateResult(float dt);

    // Resultへ入った瞬間（表示切替を一回だけやる）
    void EnterResult(ResultType type);

    // Result入力（Sceneは戻り値を見て遷移するだけ）
    UIResultOverlay::Action HandleResultInput();
    UIPauseOverlay::Action ConsumePauseAction() { return mPause.ConsumeAction(); }

    // 明示的に隠したい場合
    void HideAll();

    //ポーズ画面処理
    void ShowPause() { mPause.Show(); }
    void HidePause() { mPause.Hide(); }
    void UpdatePause(float dt) { mPause.Update(dt); }

    void SetTutorialHintAttention(UIActionHints::Hint hint, bool on, float interval = 0.35f){ mHints.SetHintAttention(hint, on, interval); }
    void ClearTutorialHintAttention(){ mHints.ClearAllHintAttention(); }
    void ClearTutorialVisuals();

    void SetDodgeAttention(bool on) { mHints.SetAttentionAvoid(on); }
    void SetAttackAttention(bool on) { mHints.SetAttentionAttack(on); }

    void SetDim(bool on, float alpha = 0.55f);

    void SetAvoidJustColor(bool on) { mHints.SetAvoidJustColor(on); }

    IUIControllable* GetPauseControllable() { return &mPause; }
    IUIControllable* GetResultControllable() { return &mResult; }
private:
    
    UIHealthBars    mHealth;
    UIResultOverlay mResult;
    UIActionHints   mHints;
    UIPauseOverlay  mPause;

    vnSprite* mDimmer = nullptr;
};