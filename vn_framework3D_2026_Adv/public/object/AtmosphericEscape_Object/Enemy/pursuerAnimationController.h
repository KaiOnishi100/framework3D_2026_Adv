#pragma once

class SkeletalModel;

//============================================================
// Pursuer Animation State
//============================================================
enum class PursuerAnimationState
{
    Idle,
    Walk,
    Run,
    Attack
};

//============================================================
// Pursuer Animation Files
// Scene側でファイルパスを設定する
//============================================================
struct PursuerAnimationSet
{
    std::wstring IdleFile;
    std::wstring WalkFile;
    std::wstring RunFile;
    std::wstring AttackFile;
};

//============================================================
// PursuerAnimationController
// ・Motion読み込み
// ・Motion切り替え
// ・Cross Fade
// ・Loop / NonLoop設定
// AIの判断や移動処理は行わない
//============================================================
class PursuerAnimationController
{
public:

    PursuerAnimationController() = default;

    ~PursuerAnimationController() = default;

    //========================================================
    // 初期化
    // 4種類のMotionをロードしIdleを再生する
    //========================================================
    bool Initialize(
        SkeletalModel& model,
        const PursuerAnimationSet& animations
    );

    //========================================================
    // AI側で決定したAnimation Stateを反映する
    //========================================================
    void Update(
        SkeletalModel& model,
        PursuerAnimationState nextState
    );

    //========================================================
    // 現在のAnimation State
    //========================================================
    PursuerAnimationState GetState() const
    {
        return mCurrentState;
    }

private:

    //========================================================
    // Animation変更
    //========================================================
    bool ChangeState(
        SkeletalModel& model,
        PursuerAnimationState nextState
    );

private:

    PursuerAnimationState mCurrentState = PursuerAnimationState::Idle;

    bool mInitialized = false;
};