#include "../framework.h"
#include "../framework/vn_environment.h"

#include "skeletalAnimationController.h"

//============================================================
// Constructor
//============================================================
SkeletalAnimationController::SkeletalAnimationController( SkeletalModel* model) : mModel(model)
{
}


//============================================================
// Initialize
//============================================================
bool SkeletalAnimationController::Initialize()
{
    //========================================================
    // SkeletalModelなし
    //========================================================
    if (mModel == nullptr)
    {
        return false;
    }


    //========================================================
    // 必要なLocomotion Animationが全てロード済みか確認する
    //========================================================
    if (!mModel->HasMotionClip(L"Idle"))
    {
        vnFont::output(
            L"[AnimationController] "
            L"Idle motion is not loaded.\n"
        );

        return false;
    }


    if (!mModel->HasMotionClip(L"Walk"))
    {
        vnFont::output(
            L"[AnimationController] "
            L"Walk motion is not loaded.\n"
        );

        return false;
    }


    if (!mModel->HasMotionClip(L"Run"))
    {
        vnFont::output(
            L"[AnimationController] "
            L"Run motion is not loaded.\n"
        );

        return false;
    }


    //========================================================
    // 初期状態
    //
    // None
    //   ↓
    // Idle
    //========================================================
    mCurrentState = State::None;


    ChangeState(State::Idle);


    return true;
}


//============================================================
// Update
//============================================================
void SkeletalAnimationController::Update(bool isMoving, bool isRunning)
{
    if (mModel == nullptr)
    {
        return;
    }

    //========================================================
    // 現在の入力からLocomotionを決定する
    //========================================================
    mLocomotionState =
        DetermineLocomotionState(
            isMoving,
            isRunning
        );

    //========================================================
    // Combo最優先
    //========================================================
    if (mComboActive)
    {
        UpdateCombo();

        return;
    }

    //========================================================
    // One Shot中
    //========================================================
    if (mOneShotActive)
    {
        //====================================================
        // SkeletalModel側ではOneShot終了後、
        // QueueされたLocomotionへ自動遷移する
        // Motion名がAttack Motionではなくなったら
        // OneShot終了と判断する
        //====================================================
        if (mModel->GetCurrentMotionName() != mOneShotMotionName)
        {
            //================================================
            // Attack終了
            //================================================
            mOneShotActive = false;

            mOneShotMotionName.clear();

            //================================================
            // 現在の入力状態に対応するLocomotionへ
            // 改めて遷移する
            //================================================
            ChangeState(mLocomotionState);
        }


        //====================================================
        // Attack中は通常Locomotionへの遷移を行わない
        //====================================================
        return;
    }


    //========================================================
    // 通常Locomotion
    //========================================================
    if ( mCurrentState == mLocomotionState )
    {
        return;
    }


    //========================================================
    // State変更
    //========================================================
    ChangeState(mLocomotionState);
}


bool SkeletalAnimationController::PlayAttack(const std::wstring& motionName, float blendIn, float blendOut)
{
    if (mModel == nullptr)
    {
        return false;
    }


    //========================================================
    // Motionがロードされているか確認
    //========================================================
    if (!mModel->HasMotionClip(motionName))
    {
        vnFont::output(
            L"[AnimationController] "
            L"Attack Motion not found : %s\n",
            motionName.c_str()
        );

        return false;
    }


    //========================================================
    // すでにOneShot中
    //========================================================
    if (mOneShotActive)
    {
        return false;
    }


    //========================================================
    // Attack終了時の戻り先
    //========================================================
    const wchar_t* returnMotion =
        GetLocomotionMotionName(mLocomotionState);


    //========================================================
    // One Shot再生
    //========================================================
    const bool result =
        mModel->PlayOneShot(
            motionName,
            returnMotion,
            // Locomotion → Attack
            blendIn,
            // Attack → Locomotion
            blendOut,
            // Idle / Walk / RunはLoop
            true
        );


    if (!result)
    {
        return false;
    }


    //========================================================
    // Controller State
    //========================================================
    mOneShotActive = true;

    mOneShotMotionName = motionName;

    mOneShotBlendOutDuration = blendOut;

    mCurrentState = State::Attack;


    return true;
}

void SkeletalAnimationController::ClearComboDefinition()
{
    //========================================================
    // 再生中Comboも終了
    //========================================================
    mComboActive = false;
    mComboIndex = -1;
    mComboNextQueued = false;

    mComboSteps.clear();
}

bool SkeletalAnimationController::AddComboStep(const std::wstring& motionName, float blendInDuration, float inputStartNormalized, float inputEndNormalized, float transitionNormalized)
{
    if (mModel == nullptr)
    {
        return false;
    }


    //========================================================
    // MOTがロード済みか確認
    //========================================================
    if (!mModel->HasMotionClip(motionName))
    {
        vnFont::output(
            L"[AnimationController] "
            L"Combo Motion not found : %s\n",
            motionName.c_str()
        );

        return false;
    }


    //========================================================
    // 0～1へClamp
    //========================================================
    inputStartNormalized =
        std::clamp(
            inputStartNormalized,
            0.0f,
            1.0f
        );


    inputEndNormalized =
        std::clamp(
            inputEndNormalized,
            0.0f,
            1.0f
        );


    transitionNormalized =
        std::clamp(
            transitionNormalized,
            0.0f,
            1.0f
        );


    //========================================================
    // Input Windowがおかしい
    //========================================================
    if (inputStartNormalized > inputEndNormalized)
    {
        vnFont::output(
            L"[AnimationController] "
            L"Invalid Combo Input Window.\n"
        );

        return false;
    }


    ComboStep step;


    step.MotionName = motionName;

    step.BlendInDuration = (std::max)(blendInDuration, 0.0f);

    step.InputStartNormalized = inputStartNormalized;

    step.InputEndNormalized = inputEndNormalized;

    step.TransitionNormalized = transitionNormalized;


    mComboSteps.push_back(step);

    return true;
}

bool SkeletalAnimationController::RequestComboAttack()
{
    if (mModel == nullptr || mComboSteps.empty())
    {
        return false;
    }


    //========================================================
    // Comboしていない
    // → 1段目開始
    //========================================================
    if (!mComboActive)
    {
        //====================================================
        // 通常OneShot中なら今回はCombo開始不可
        //====================================================
        if (mOneShotActive)
        {
            return false;
        }


        return StartComboStep(0);
    }


    //========================================================
    // 既に次Attack予約済み
    //========================================================
    if (mComboNextQueued)
    {
        return false;
    }


    //========================================================
    // 最終段なら次はない
    //========================================================
    if (mComboIndex + 1 >= static_cast<int>(mComboSteps.size()))
    {
        return false;
    }


    const ComboStep& currentStep = mComboSteps[mComboIndex];


    //========================================================
    // 現在Animation進行率
    //========================================================
    const float motionLength = mModel->GetMotionLength();


    if (motionLength <= 0.000001f)
    {
        return false;
    }


    float normalizedTime =
        mModel->GetMotionTime() / motionLength;


    normalizedTime =
        std::clamp(normalizedTime, 0.0f, 1.0f);


    //========================================================
    // Input Window外は入力を受け付けない
    //========================================================
    if (normalizedTime <
        currentStep.InputStartNormalized ||
        normalizedTime >
        currentStep.InputEndNormalized)
    {
        return false;
    }


    //========================================================
    // 次Attack予約
    //========================================================
    mComboNextQueued = true;


    vnFont::output(
        L"[AnimationController] "
        L"Combo Input Queued. "
        L"Step=%d Time=%.2f\n",
        mComboIndex + 1,
        normalizedTime
    );


    return true;
}

SkeletalAnimationController::State SkeletalAnimationController::DetermineLocomotionState(bool isMoving, bool isRunning) const
{
    //========================================================
    // 停止
    //========================================================
    if (!isMoving)
    {
        return State::Idle;
    }

    //========================================================
    // 移動
    //========================================================
    if (isRunning)
    {
        return State::Run;
    }


    return State::Walk;
}

const wchar_t* SkeletalAnimationController::GetLocomotionMotionName(State state) const
{
    switch (state)
    {
    case State::Idle:
        return L"Idle";

    case State::Walk:
        return L"Walk";

    case State::Run:
        return L"Run";

    default:
        return L"Idle";
    }
}


//============================================================
// State Change
//============================================================
void SkeletalAnimationController::ChangeState(State nextState)
{
    if (mModel == nullptr)
    {
        return;
    }


    if (nextState == State::Attack)
    {
        return;
    }
    //========================================================
    // 同一Stateなら何もしない
    //========================================================
    if (nextState == mCurrentState)
    {
        return;
    }


    //========================================================
    // StateごとのMotion設定
    //========================================================
    switch (nextState)
    {
    //========================================================
    // Idle
    //========================================================
    case State::Idle:
    {
        mModel->BlendToMotion(
            L"Idle",
            0.20f,
            true,
            true
        );

        break;
    }


    //========================================================
    // Walk
    //========================================================
    case State::Walk:
    {
        mModel->BlendToMotion(
            L"Walk",
            0.18f,
            true,
            true
        );

        break;
    }


    //========================================================
    // Run
    //========================================================
    case State::Run:
    {
        mModel->BlendToMotion(
            L"Run",
            0.12f,
            true,
            true
        );

        break;
    }


    default:
        return;
    }


    //========================================================
    // State確定
    //========================================================
    mCurrentState = nextState;
}

void SkeletalAnimationController::UpdateCombo()
{
    if (!mComboActive || mModel == nullptr)
    {
        return;
    }


    //========================================================
    // Safety
    //========================================================
    if (mComboIndex < 0 || mComboIndex >= static_cast<int>(mComboSteps.size()))
    {
        FinishCombo();
        return;
    }


    const ComboStep& currentStep =
        mComboSteps[mComboIndex];


    //========================================================
    // Normalized Animation Time
    //========================================================
    const float motionLength = mModel->GetMotionLength();


    float normalizedTime = 1.0f;


    if (motionLength > 0.000001f)
    {
        normalizedTime =
            mModel->GetMotionTime() / motionLength;
    }


    normalizedTime =
        std::clamp(normalizedTime, 0.0f, 1.0f);


    //========================================================
    // 次Attack予約あり
    //========================================================
    if (mComboNextQueued)
    {
        //====================================================
        // Transition地点まで来た
        //====================================================
        if (normalizedTime >= currentStep.TransitionNormalized)
        {
            const int nextIndex = mComboIndex + 1;


            //================================================
            // 次Comboへ
            //================================================
            if (nextIndex < static_cast<int>(mComboSteps.size()))
            {
                StartComboStep(nextIndex);
                return;
            }
        }
    }


    //========================================================
    // Animation終了
    // NonLoop Motionなので終了すると
    // SkeletalModel::IsMotionPlaying() == false
    //========================================================
    if (!mModel->IsMotionPlaying())
    {
        //====================================================
        // 予約があるのにDeltaTime等の都合で
        // Transition地点を飛び越えた場合のSafety
        //====================================================
        if (mComboNextQueued &&
            mComboIndex + 1 <
            static_cast<int>(mComboSteps.size()))
        {
            StartComboStep(mComboIndex + 1);
            return;
        }


        //====================================================
        // Combo終了
        //====================================================
        FinishCombo();
    }
}

bool SkeletalAnimationController::StartComboStep(int comboIndex)
{
    //========================================================
   // Index確認
   //========================================================
    if (comboIndex < 0 ||
        comboIndex >=
        static_cast<int>(mComboSteps.size()))
    {
        return false;
    }


    const ComboStep& step = mComboSteps[comboIndex];


    //========================================================
    // Attack AnimationへCross Fade
    //========================================================
    const bool result =
        mModel->BlendToMotion(
            step.MotionName,
            step.BlendInDuration,
            true,
            false
        );


    if (!result)
    {
        return false;
    }


    //========================================================
    // Combo State
    //========================================================
    mComboActive = true;

    mComboIndex = comboIndex;

    mComboNextQueued = false;


    //========================================================
    // Controller State
    //========================================================
    mCurrentState = State::Attack;


    vnFont::output(
        L"[AnimationController] "
        L"Combo Step %d : %s\n",
        comboIndex + 1,
        step.MotionName.c_str()
    );


    return true;
}

void SkeletalAnimationController::FinishCombo()
{
    mComboActive = false;

    mComboIndex = -1;

    mComboNextQueued =false;


    //========================================================
    // 攻撃終了時点の入力状態へ戻る
    //========================================================
    ReturnToLocomotion( mComboReturnBlendDuration);
}

void SkeletalAnimationController::ReturnToLocomotion(float blendDuration)
{
    if (mModel == nullptr)
    {
        return;
    }

    const wchar_t* motionName =
        GetLocomotionMotionName(mLocomotionState);


    mModel->BlendToMotion(
        motionName,
        blendDuration,
        true,
        true
    );


    mCurrentState = mLocomotionState;
}
