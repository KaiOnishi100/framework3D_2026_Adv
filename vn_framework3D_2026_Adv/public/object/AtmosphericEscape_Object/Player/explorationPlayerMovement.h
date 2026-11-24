#pragma once

//============================================================
// PlayerMovement
// Atmospheric Escape Playerの
// ・移動入力
// ・Walk / Run
// ・Camera基準移動
// ・進行方向への回転
// ・Gravity
// ・Grounded
// をまとめるクラス
// ExplorationPlayer本体からMovement処理を切り離す
//============================================================
class PlayerMovement
{
public:

    //========================================================
    // Movement Parameter
    //========================================================
    struct Settings
    {
        //====================================================
        // 移動速度
        //====================================================
        float WalkSpeed = 2.5f;

        float RunSpeed = 5.0f;

        //========================================================
        // しゃがみ歩行速度
        //========================================================
        float CrouchSpeed = 1.2f;

        //========================================================
        // Step-Up
        // Playerが自動的に乗り越えられる最大段差
        //========================================================
        float MaxStepHeight = 0.30f;


        //========================================================
        // Step判定用Forward Probe
        // Capsule中心から前方へどれくらい先を見るか
        //========================================================
        float StepProbeExtraDistance = 0.12f;


        //========================================================
        // Step上面を調べる際
        // 垂直面より少し奥へ入った場所からRayを下ろす
        //========================================================
        float StepLandingProbeInset = 0.08f;

        //====================================================
        // 方向転換速度
        // 値が大きいほど素早く進行方向を向く
        //====================================================
        float RotationSharpness = 12.0f;


        //====================================================
        // Gravity
        //====================================================
        float Gravity = -20.0f;

        float MaxFallSpeed = -40.0f;


        //====================================================
        // Model Forward補正
        // FBXによって
        // +Z Forward
        // -Z Forward
        // などが異なる場合に使用する
        //====================================================
        float ModelForwardYawOffset = 0.0f;
    };


public:

    PlayerMovement() = default;

    ~PlayerMovement() = default;


    //========================================================
    // Update
    // owner: 実際に移動させるPlayer Object
    //========================================================
    void Update(ObjectBase& owner,  float dt );


    //========================================================
    // Input
    //========================================================
    void AddMoveAxis( float x, float y );


    void SetSprint(  bool sprint )
    {
        mSprint = sprint;
    }


    //========================================================
    // Physics
    //========================================================
    void SetGrounded( bool grounded )
    {
        mGrounded = grounded;
    }


    bool IsGrounded() const
    {
        return mGrounded;
    }


    void SetVerticalSpeed( float speed)
    {
        mVerticalSpeed = speed;
    }


    float GetVerticalSpeed() const
    {
        return mVerticalSpeed;
    }


    //========================================================
    // 状態取得
    // Animation Controllerから利用する
    //========================================================
    bool IsMoving() const
    {
        return mIsMoving;
    }


    bool IsSprinting() const
    {
        return mSprint && !mCrouching;
    }


    float GetCurrentSpeed() const
    {
        return mCurrentSpeed;
    }

    //============================================================
    // Move Input Clear
    //============================================================
    void ClearMoveInput()
    {
        mMoveAxisX = 0.0f;
        mMoveAxisY = 0.0f;
    }


    //============================================================
    // Crouch State
    // true  : しゃがみ
    // false : 通常姿勢
    //============================================================
    void SetCrouching(bool crouching)
    {
        mCrouching = crouching;
    }

    //============================================================
    // 現在しゃがんでいるか
    //============================================================
    bool IsCrouching() const
    {
        return mCrouching;
    }

    //========================================================
    // Settings
    //========================================================
    void SetSettings( const Settings& settings )
    {
        mSettings = settings;
    }


    const Settings& GetSettings() const
    {
        return mSettings;
    }

    //============================================================
    // Step-Up判定に使用するWorld Collider一覧
    // PlayerMovementはColliderを所有しない
    // Scene側のvectorを参照するだけ
    //============================================================
    void SetStepColliders(const std::vector<Collider*>* colliders)
    {
        mStepColliders = colliders;
    }

private:

    //========================================================
    // Movement
    //========================================================
    void UpdateHorizontalMovement(ObjectBase& owner, float dt);

    //========================================================
    // Gravity
    //========================================================
    void UpdateGravity(ObjectBase& owner, float dt);

    //========================================================
    // Rotation
    //========================================================
    void UpdateFacingDirection(ObjectBase& owner, const XMVECTOR& moveDirection, float dt);

    //============================================================
    // Step-Up
    //============================================================
    bool TryStepUp(ObjectBase& owner, const XMVECTOR& moveDirection, float horizontalMoveDistance);

private:

    //========================================================
    // Parameter
    //========================================================
    Settings mSettings;

    //============================================================
    // Step-Up用World Collider
    // 所有権なし
    //============================================================
    const std::vector<Collider*>* mStepColliders = nullptr;

    //========================================================
    // Input
    // 1Frameごとの入力値
    // Update終了後に0へ戻す
    //========================================================
    float mMoveAxisX = 0.0f;

    float mMoveAxisY = 0.0f;


    //========================================================
    // Movement State
    //========================================================
    bool mSprint = false;

    bool mIsMoving = false;

    bool mCrouching = false;

    float mCurrentSpeed = 0.0f;


    //========================================================
    // Physics State
    //========================================================
    bool mGrounded = false;

    float mVerticalSpeed = 0.0f;
};