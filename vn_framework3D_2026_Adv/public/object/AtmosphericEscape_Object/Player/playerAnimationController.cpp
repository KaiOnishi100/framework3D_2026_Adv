#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "explorationPlayerMovement.h"
#include "playerAnimationController.h"


bool PlayerAnimationController::Initialize(SkeletalModel& model, const PlayerLocomotionAnimationSet& animations)
{
    //========================================================
    // Idle
    //========================================================
    const bool idleLoaded =
        model.LoadMotionClip(
            mIdleMotionName,
            animations.IdleFile.c_str()
        );


    //========================================================
    // Walk
    //========================================================
    const bool walkLoaded =
        model.LoadMotionClip(
            mWalkMotionName,
            animations.WalkFile.c_str()
        );


    //========================================================
    // Run
    //========================================================
    const bool runLoaded =
        model.LoadMotionClip(
            mRunMotionName,
            animations.RunFile.c_str()
        );

    //========================================================
    // Crouching Animations
    //========================================================

    const bool crouchIdleLoaded =
        model.LoadMotionClip(
            mCrouchIdleMotionName,
            animations.CrouchIdleFile.c_str()
        );

    const bool crouchWalkLoaded =
        model.LoadMotionClip(
            mCrouchWalkMotionName,
            animations.CrouchWalkFile.c_str()
        );

    //========================================================
    // 1つでも失敗したら初期化失敗
    //========================================================
    if (!idleLoaded || !walkLoaded || !runLoaded || !crouchIdleLoaded || !crouchWalkLoaded)
    {
        vnFont::output(
            L"[PlayerAnimationController] "
            L"Locomotion animation load failed.\n"
        );

        return false;
    }


    //========================================================
    // 初期状態
    //========================================================
    const bool motionSet = model.SetMotion(mIdleMotionName, true, true);


    if (!motionSet)
    {
        return false;
    }


    mCurrentState = PlayerLocomotionState::Idle;

    mInitialized = true;

    vnFont::output(L"[PlayerAnimationController] " L"Locomotion initialized.\n" );


    return true;
}

//============================================================
// Action Animation Initialize
//============================================================
bool PlayerAnimationController::InitializeActionAnimations(SkeletalModel& model,const PlayerActionAnimationSet& animations)
{
    mActionsInitialized = false;

    //========================================================
    // 地面のアイテムを拾う
    //========================================================
    const bool pickupDownLoaded =
        model.LoadMotionClip(
            mPickupDownMotionName,
            animations.PickupDownFile.c_str()
        );

    //========================================================
    // 机などのアイテムを拾う
    //========================================================
    const bool pickupMidLoaded =
        model.LoadMotionClip(
            mPickupMidMotionName,
            animations.PickupMidFile.c_str()
        );

    //========================================================
    // 被弾リアクション
    //========================================================
    const bool hitLoaded =
        model.LoadMotionClip(
            mHitReactionMotionName,
            animations.HitReactionFile.c_str()
        );

    //========================================================
    // 死亡アニメーション
    //========================================================
    const bool deathLoaded =
        model.LoadMotionClip(
            mDeathMotionName,
            animations.DeathFile.c_str()
        );

    //========================================================
    // すべてロードできたか確認
    //========================================================
    if (!pickupDownLoaded || !pickupMidLoaded || !hitLoaded || !deathLoaded)
    {
        vnFont::output(
            L"[PlayerAnimationController] "
            L"Action animation load failed.\n"
        );

        return false;
    }

    mCurrentAction = PlayerActionState::None;
    mActionsInitialized = true;

    vnFont::output(
        L"[PlayerAnimationController] "
        L"Action animations initialized.\n"
    );

    return true;
}
//============================================================
// Update
//============================================================
void PlayerAnimationController::Update(SkeletalModel& model, const PlayerMovement& movement,bool pickupAligning)
{
    if (!mInitialized)
    {
        return;
    }

    //========================================================
    // Action Animationを優先する
    //========================================================
    if (mCurrentAction != PlayerActionState::None)
    {
        //====================================================
        // Deathは最終Poseを維持
        // SkeletalModel側は非ループ再生終了時に停止する
        // 通常移動へ戻してはいけない
        //====================================================
        if (mCurrentAction == PlayerActionState::Death)
        {
            return;
        }

        //============================================================
        // Pickup Animation Early Exit
        // PickupDown / PickupMidのみ、
        // Motionの終了を待たずにLocomotionへ戻す
        // HitReaction: 最後まで再生する
        // Death: 上のDeath判定で処理済み
        //============================================================
        const bool isPickup =
            mCurrentAction == PlayerActionState::PickupDown ||
            mCurrentAction == PlayerActionState::PickupMid;

        //============================================================
        // Pickup Animation
        //============================================================
        if (isPickup)
        {
            //========================================================
            // 現在のPickup Motion再生時間
            // BlendToMotion()のrestart=trueにより
            // モーション開始時は0秒から計測される
            //========================================================
            const float currentTime = model.GetMotionTime();

            //========================================================
            // 現在のPickup種類に応じてLock時間を決定
            //========================================================
            const float lockTime =
                mCurrentAction == PlayerActionState::PickupDown ? PickupDownLockTime : PickupMidLockTime;

            //========================================================
            // 操作不能時間が経過していない場合
            // Pickup Motionを継続し通常移動Animationへの復帰を禁止する
            // Motion自体が終了している場合はlockTimeに達していなくても復帰を許可する
            //========================================================
            if (model.IsMotionPlaying() && currentTime < lockTime)
            {
                return;
            }

#if defined(_DEBUG)

            //========================================================
            // Pickup終了ログ
            // 実際に何秒で操作不能が解除されたか確認する
            //========================================================
            vnFont::output(
                L"[PlayerAnimationController] "
                L"Pickup Unlock : %.3f sec / MotionLength : %.3f sec\n",
                currentTime,
                model.GetMotionLength()
            );

#endif
        }
        else
        {
            //========================================================
            // HitReactionなどは、これまで通り
            // モーションが終了するまで待つ
            //========================================================
            if (model.IsMotionPlaying())
            {
                return;
            }
        }
        //====================================================
        // Action終了
        //====================================================
        mCurrentAction = PlayerActionState::None;

        //====================================================
        // 再生終了直後の復帰
        //====================================================
        PlayerLocomotionState returnState = PlayerLocomotionState::Idle;

        if (movement.IsCrouching())
        {
            returnState = movement.IsMoving() ? PlayerLocomotionState::CrouchWalk : PlayerLocomotionState::CrouchIdle;
        }
        else if (movement.IsMoving())
        {
            returnState = movement.IsSprinting() ? PlayerLocomotionState::Run : PlayerLocomotionState::Walk;
        }

        ChangeState(model, returnState);

        return;
    }

    //============================================================
    // Pickup Alignment Locomotion
    //============================================================
    if (pickupAligning)
    {
        //========================================================
        // 立ち姿勢 / しゃがみ姿勢でMotionを切り替える
        //========================================================
        const PlayerLocomotionState alignmentState =
            movement.IsCrouching() ? PlayerLocomotionState::CrouchWalk : PlayerLocomotionState::Walk;

        //========================================================
        // 現在のMotionと異なる場合だけ変更
        // 毎フレームBlendToMotion()を呼ぶと
        // アニメーションが先頭に戻ってしまうため
        // 状態が変わったときだけ実行する
        //========================================================
        if (mCurrentState != alignmentState)
        {
            ChangeState(model, alignmentState);
        }

        //========================================================
        // Alignment中は通常のIdle判定を実行しない
        //========================================================
        return;
    }

    //========================================================
    // Movement State
    //========================================================
    PlayerLocomotionState nextState = PlayerLocomotionState::Idle;


    if (movement.IsCrouching())
    {
        if (movement.IsMoving())
        {
            nextState = PlayerLocomotionState::CrouchWalk;
        }
        else
        {
            nextState = PlayerLocomotionState::CrouchIdle;
        }
    }
    else
    {
        //====================================================
        // Standing Locomotion
        //====================================================
        if (movement.IsMoving())
        {
            if (movement.IsSprinting())
            {
                nextState = PlayerLocomotionState::Run;
            }
            else
            {
                nextState = PlayerLocomotionState::Walk;
            }
        }
        else
        {
            nextState = PlayerLocomotionState::Idle;
        }
    }

    //========================================================
    // Stateが変化したときだけMotion変更
    //========================================================
    if (nextState != mCurrentState)
    {
        ChangeState(
            model,
            nextState
        );
    }
}


bool PlayerAnimationController::PlayAction(SkeletalModel& model, PlayerActionState action)
{
    //========================================================
    // 未初期化
    //========================================================
    if (!mInitialized || !mActionsInitialized)
    {
        return false;
    }

    if (action == PlayerActionState::None)
    {
        return false;
    }

    //========================================================
    // Death再生開始後は他のActionを受け付けない
    //========================================================
    if (mCurrentAction == PlayerActionState::Death)
    {
        return false;
    }

    //========================================================
    // Pickupは他のActionを中断できない
    //========================================================
    const bool isPickup =
        action == PlayerActionState::PickupDown || action == PlayerActionState::PickupMid;

    if (isPickup && mCurrentAction != PlayerActionState::None)
    {
        return false;
    }

    //========================================================
    // HitReactionの重複再生を防止
    //========================================================
    if (action == PlayerActionState::HitReaction && mCurrentAction == PlayerActionState::HitReaction)
    {
        return false;
    }

    //========================================================
    // Motion名・Blend時間を選択
    //========================================================
    const std::wstring* motionName = nullptr;
    float blendTime = 0.15f;

    switch (action)
    {
    case PlayerActionState::PickupDown:
        motionName = &mPickupDownMotionName;
        blendTime = 0.18f;
        break;

    case PlayerActionState::PickupMid:
        motionName = &mPickupMidMotionName;
        blendTime = 0.15f;
        break;

    case PlayerActionState::HitReaction:
        motionName = &mHitReactionMotionName;
        blendTime = 0.08f;
        break;

    case PlayerActionState::Death:
        motionName = &mDeathMotionName;
        blendTime = 0.12f;
        break;

    default:
        return false;
    }

    //========================================================
    // ActionへCross Fade
    // restart = true : 先頭から再生
    // loop    = false: 1回だけ再生
    //========================================================
    const bool started =
        model.BlendToMotion(
            *motionName,
            blendTime,
            true,
            false
        );

    //========================================================
    // 成功した場合だけAction状態を変更
    //========================================================
    if (started)
    {
        mCurrentAction = action;
    }

    return started;
}

void PlayerAnimationController::ResetAfterLoad(SkeletalModel& model)
{
    mCurrentAction = PlayerActionState::None;
    mCurrentState = PlayerLocomotionState::Idle;

    if (mInitialized)
    {
        //====================================================
        // 直前のMotionからIdleへ短時間で切り替える
        //====================================================
        model.BlendToMotion(
            mIdleMotionName,
            0.10f,
            true,
            true
        );
    }
}

//============================================================
// Change State
//============================================================
void PlayerAnimationController::ChangeState(SkeletalModel& model, PlayerLocomotionState nextState)
{
    //========================================================
    // Stateに対応するMotionとBlend時間を決定
    //========================================================
    const std::wstring* motionName = nullptr;

    float blendTime = 0.15f;

    switch (nextState)
    {
    case PlayerLocomotionState::Idle:
    {
        motionName = &mIdleMotionName;
        blendTime = mIdleBlendTime;
        break;
    }


    case PlayerLocomotionState::Walk:
    {
        motionName = &mWalkMotionName;
        blendTime = mWalkBlendTime;
        break;
    }


    case PlayerLocomotionState::Run:
    {
        motionName = &mRunMotionName;
        blendTime = mRunBlendTime;
        break;
    }

    case PlayerLocomotionState::CrouchIdle:
    {
        motionName = &mCrouchIdleMotionName;
        blendTime = mCrouchIdleBlendTime;
        break;
    }

    case PlayerLocomotionState::CrouchWalk:
    {
        motionName = &mCrouchWalkMotionName;
        blendTime = mCrouchWalkBlendTime;
        break;
    }

    default:
        return;
    }


    if (motionName == nullptr)
    {
        return;
    }


    //========================================================
    // Cross Fade
    // restart = true
    // loop    = true
    // すべてLoopする
    //========================================================
    const bool changed =
        model.BlendToMotion(
            *motionName,
            blendTime,
            true,
            true
        );


    //========================================================
    // Blend開始に成功した場合のみState更新
    // Motionが見つからない等で失敗した場合
    // Controller上のStateだけ変わるのを防ぐ
    //========================================================
    if (changed)
    {
        mCurrentState = nextState;
    }
}