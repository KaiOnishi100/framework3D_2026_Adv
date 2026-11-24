#pragma once

class PlayerMovement;


//============================================================
// PlayerLocomotionState
//============================================================
enum class PlayerLocomotionState
{
    Idle,
    Walk,
    Run,
    CrouchIdle,
    CrouchWalk
};

enum class PlayerActionState
{
    None,
    PickupDown,
    PickupMid,
    HitReaction,
    Death
};

//============================================================
// PlayerLocomotionAnimationSet
//============================================================
struct PlayerLocomotionAnimationSet
{
    std::wstring IdleFile;
    std::wstring WalkFile;
    std::wstring RunFile;
    std::wstring CrouchIdleFile;
    std::wstring CrouchWalkFile;
};

struct PlayerActionAnimationSet
{
    std::wstring PickupDownFile;
    std::wstring PickupMidFile;
    std::wstring HitReactionFile;
    std::wstring DeathFile;
};

//============================================================
// PlayerAnimationController
//============================================================
class PlayerAnimationController
{
public:

    PlayerAnimationController() = default;

    ~PlayerAnimationController() = default;

    //========================================================
    // 初期化
    // 必要なLocomotion Motionをロードし
    // 初期状態をIdleへ設定する
    //========================================================
    bool Initialize(SkeletalModel& model, const PlayerLocomotionAnimationSet& animations);

    //========================================================
    // ActionAnimation初期化
    //========================================================
    bool InitializeActionAnimations(SkeletalModel& model, const PlayerActionAnimationSet& animations);

    //========================================================
    // Update
    //========================================================
    void Update(SkeletalModel& model, const PlayerMovement& movement, bool pickupAligning = false);


    PlayerLocomotionState GetState() const
    {
        return mCurrentState;
    }

    //============================================================
    // 現在実行しているActionを取得
    // WorldItemPickupから、
    // PickupDown / PickupMidのどちらが再生されたかを確認する
    //============================================================
    PlayerActionState GetCurrentAction() const
    {
        return mCurrentAction;
    }

    //単発アニメーション再生
    bool PlayAction(SkeletalModel& model, PlayerActionState action);

    bool IsActionPlaying()const { return mCurrentAction != PlayerActionState::None; }

    void ResetAfterLoad(SkeletalModel& model);

private:

    void ChangeState(SkeletalModel& model, PlayerLocomotionState nextState);

public:
    // 拾得モーションの操作ロックと視点が戻るタイミングで共有する
    static constexpr float PickupDownLockTime = 2.00f;
    static constexpr float PickupMidLockTime = 0.60f;

private:

    PlayerLocomotionState mCurrentState = PlayerLocomotionState::Idle;
    PlayerActionState mCurrentAction = PlayerActionState::None;

    //========================================================
    // Motion Clip Name
    // LoadMotionClip時につけたName
    //========================================================
    const std::wstring mIdleMotionName = L"Idle";
    const std::wstring mWalkMotionName = L"Walk";
    const std::wstring mRunMotionName = L"Run";
    const std::wstring mCrouchIdleMotionName = L"CrouchIdle";
    const std::wstring mCrouchWalkMotionName = L"CrouchWalk";

    const std::wstring mPickupDownMotionName = L"PickupDown";
    const std::wstring mPickupMidMotionName = L"PickupMid";
    const std::wstring mHitReactionMotionName = L"HitReaction";
    const std::wstring mDeathMotionName = L"Death";

    float mIdleBlendTime = 0.20f;

    float mWalkBlendTime = 0.15f;

    float mRunBlendTime = 0.12f;

    float mCrouchIdleBlendTime = 0.25f;

    float mCrouchWalkBlendTime = 0.18f;

    bool mInitialized = false;

    bool mActionsInitialized = false;
};