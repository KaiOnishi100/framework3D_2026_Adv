#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "pursuerAnimationController.h"

//============================================================
// Initialize
//============================================================
bool PursuerAnimationController::Initialize(
    SkeletalModel& model,
    const PursuerAnimationSet& animations)
{
    mInitialized = false;

    //========================================================
    // Animation Clipsをロード
    // Sceneから渡されたパスを使用する
    //========================================================
    const bool idleLoaded = model.LoadMotionClip(L"Idle", animations.IdleFile.c_str());

    const bool walkLoaded = model.LoadMotionClip(L"Walk", animations.WalkFile.c_str());

    const bool runLoaded = model.LoadMotionClip(L"Run", animations.RunFile.c_str());

    const bool attackLoaded = model.LoadMotionClip(L"Attack", animations.AttackFile.c_str());

    //========================================================
    // 1種類でもロードに失敗した場合は初期化中止
    //========================================================
    if (!idleLoaded ||
        !walkLoaded ||
        !runLoaded ||
        !attackLoaded)
    {
        OutputDebugStringW(
            L"[PursuerAnimationController] "
            L"Animation load failed.\n"
        );

        return false;
    }

    //========================================================
    // 初期モーションはIdle
    //========================================================
    if (!model.SetMotion(L"Idle", true, true))
    {
        return false;
    }

    mCurrentState = PursuerAnimationState::Idle;
    mInitialized = true;

    OutputDebugStringW(L"[PursuerAnimationController] Initialized.\n");

    return true;
}

//============================================================
// Update
//============================================================
void PursuerAnimationController::Update(
    SkeletalModel& model,
    PursuerAnimationState nextState)
{
    if (!mInitialized)
    {
        return;
    }

    //========================================================
    // 同じAnimationなら変更しない
    // 毎Frame BlendToMotion()するとMotionが再スタートし
    // Walk/Run/Attackが正常に再生されなくなる
    //========================================================
    if (mCurrentState == nextState)
    {
        return;
    }

    ChangeState(model, nextState);
}

//============================================================
// Change State
//============================================================
bool PursuerAnimationController::ChangeState(
    SkeletalModel& model,
    PursuerAnimationState nextState
)
{
    const wchar_t* motionName = nullptr;

    float blendTime = 0.15f;

    bool loop = true;

    //========================================================
    // Stateに対応するMotionを選択
    //========================================================
    switch (nextState)
    {
    case PursuerAnimationState::Idle:
        motionName = L"Idle";
        blendTime = 0.20f;
        loop = true;
        break;

    case PursuerAnimationState::Walk:
        motionName = L"Walk";
        blendTime = 0.15f;
        loop = true;
        break;

    case PursuerAnimationState::Run:
        motionName = L"Run";
        blendTime = 0.12f;
        loop = true;
        break;

    case PursuerAnimationState::Attack:
        motionName = L"Attack";
        blendTime = 0.08f;
        loop = false; // 攻撃は1回だけ
        break;

    default:
        return false;
    }

    //========================================================
    // Cross Fade
    // restart = true 新しいMotionを先頭から再生する
    // loop
    //   Idle / Walk / RunはLoop
    //   AttackはNonLoop
    //========================================================
    const bool changed =
        model.BlendToMotion(
            motionName,
            blendTime,
            true,
            loop
        );

    //========================================================
    // 成功した場合のみ現在状態を変更
    //========================================================
    if (changed)
    {
        mCurrentState = nextState;
    }

    return changed;
}