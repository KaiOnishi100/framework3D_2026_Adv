#pragma once
#include"../scene/vn_scene.h"

class vnScene;

//====================================================
// ボス戦の「最初の1回だけ」回避チュートリアルを差し込む
// ・ボス確定ローリングが "当たる直前" でヒットストップ
// ・回避入力(Trigger)で解除
//====================================================
class TutorialManager
{
public:
    enum class State
    {
        Disabled,
        WaitingIntroEnd,   // 登場演出が終わるの待ち
        WaitingNearHit,    // 確定ローリング中、当たりそうになったら止める
        PausedForDodge,    // 止めた状態で回避入力待ち
        Done
    };

public:
    void Initialize(bool enabled)
    {
        mState = enabled ? State::WaitingIntroEnd : State::Disabled;

        mBlinkT = 0.0f;
        mBlinkAlpha = 1.0f;

        mPaused = false;

        if (!enabled)
        {
            // 止まってたら解除
            vnScene::SetHitStop(false);
        }
    }
    void Cancel()
    {
        // Tutorialが止めていたなら解除
        vnScene::SetHitStop(false);
        mState = State::Disabled;
        mPaused = false;
    }

    void Update(float dt, Boss* boss, Player* player);

    bool  ShouldShowDodgeHint() const { return (mState == State::PausedForDodge); }
    float GetBlinkAlpha() const { return mBlinkAlpha; }

private:
    bool IsNearHit(Boss* boss, Player* player) const;

    bool IsDodgeTriggered() const;

private:
    State mState = State::Disabled;

    // 点滅
    float mBlinkT = 0.0f;
    float mBlinkAlpha = 1.0f;

    // 停止中か
    bool  mPaused = false;
};