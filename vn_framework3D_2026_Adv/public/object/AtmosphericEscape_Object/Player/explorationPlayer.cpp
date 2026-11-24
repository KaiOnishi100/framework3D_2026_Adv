#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "explorationPlayer.h"

//============================================================
// Constructor
//============================================================
ExplorationPlayer::ExplorationPlayer(const WCHAR* folder, const WCHAR* modelFile, const WCHAR* boneFile) : SkeletalModel(folder, modelFile, boneFile)
{
    useQuaternion(true);

    //========================================================
    // Default Collider
    //========================================================
    ConfigureCollider(0.35f, 0.55f, 0.90f);

}

ExplorationPlayer::~ExplorationPlayer()
{
    CancelWorldItemPickup();
    DetachFlashlightVisualModel();
}


bool ExplorationPlayer::InitializeLocomotionAnimations(const PlayerLocomotionAnimationSet& animations)
{
    return mAnimation.Initialize(*this, animations);
}

bool ExplorationPlayer::InitializeActionAnimations(const PlayerActionAnimationSet& animations)
{
    return mAnimation.InitializeActionAnimations(*this, animations);
}

//============================================================
// Update
//============================================================
void ExplorationPlayer::Update(float dt)
{
    dt = (std::min)(dt, 0.1f);


    //============================================================
    // Movement
    //============================================================
    if (!mControlLocked)
    {
        //========================================================
        // Action Animation再生中は移動停止
        //========================================================
        if (mPickupAligning || mAnimation.IsActionPlaying())
        {
            mMovement.ClearMoveInput();
        }

        mMovement.Update(*this, dt);
    }
    else
    {
        //========================================================
        // ※操作ロック中に古い移動入力を残さない
        //========================================================
        mMovement.ClearMoveInput();
    }

    //============================================================
    // Gameplay Noise
    // Playerの移動状態からFootstep Eventを生成
    //============================================================
    mNoiseEmitter.Update(dt, *this, mMovement);

    //========================================================
    // Interaction
    //========================================================
    mInteraction.Update(*this);

    //============================================================
    // Pickup Facing    
    // 取得待ち中だけ、対象物の方向へ身体を向ける
    //============================================================
    UpdatePickupFacing(dt);

    //============================================================
    // Pickup Alignment
    // Itemに近づく
    // 到着したらPickup Motionを開始する
    //============================================================
    UpdatePickupAlignment(dt);

    //============================================================
    // Pickup Commit
    // AnimationControllerが通常移動へ戻す前に
    // 取得確定時刻を確認する
    //============================================================
    UpdatePendingPickup();

    //========================================================
    // Animation
    //========================================================
    mAnimation.Update(*this, mMovement, mPickupAligning);

    //============================================================
    // Damage Invincible Timer
    //============================================================
    if (mDamageInvincibleTimer > 0.0f)
    {
        mDamageInvincibleTimer -= dt;

        if (mDamageInvincibleTimer < 0.0f)
        {
            mDamageInvincibleTimer = 0.0f;
        }
    }

    //========================================================
    // Motion Time更新
    //========================================================
    SkeletalModel::execute(dt);

}

void ExplorationPlayer::UpdateFirstPersonView(float dt)
{
    dt = std::clamp(dt, 0.0f, 0.10f);
    const bool initializeView = !mFirstPersonViewReady;
    const float stanceHeight = GetFirstPersonEyeHeight();

    //========================================================
    // フレームレートによらず 同じ秒数で目標へ近づける
    //========================================================
    const auto blendRate = [dt](float speed)
        {
            return 1.0f - std::exp(-speed * dt);
        };

    //========================================================
    // 0～1 の両端で速度がゼロになる曲線 拾得や被弾の始終を滑らかにする
    //========================================================
    const auto ease = [](float value)
        {
            const float t = std::clamp(value, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        };

    const auto pulse = [&ease](float progress, float riseEnd, float returnStart)
        {
            return ease(progress / riseEnd) *
                (1.0f - ease((progress - returnStart) / (1.0f - returnStart)));
        };

    if (initializeView)
    {
        //========================================================
        // 三人称から入った瞬間は現在の姿勢の高さから始める
        //========================================================
        mSmoothedEyeHeight = stanceHeight;
        mCameraMotionY = 0.0f;
        mCameraMotionPitch = 0.0f;
    }

    //========================================================
    // コライダーと立ち上がり判定は既存処理に任せ、画面の高さだけ補間する
    //========================================================
    mSmoothedEyeHeight += (stanceHeight - mSmoothedEyeHeight) * blendRate(14.0f);

    const float clipTime = GetMotionTime();
    const float clipLength = (std::max)(GetMotionLength(), 0.001f);
    const PlayerActionState action = mAnimation.GetCurrentAction();

    float targetY = 0.0f;
    float targetPitchDegrees = 0.0f;

    switch (action)
    {
    case PlayerActionState::None:
    {
        float amplitude = 0.002f;
        float cyclesPerClip = 1.0f;

        switch (mAnimation.GetState())
        {
        case PlayerLocomotionState::Walk:
            amplitude = 0.042f;
            cyclesPerClip = 2.0f;
            break;

        case PlayerLocomotionState::Run:
            amplitude = 0.062f;
            cyclesPerClip = 2.0f;
            break;

        case PlayerLocomotionState::CrouchWalk:
            amplitude = 0.032f;
            cyclesPerClip = 2.0f;
            break;

        case PlayerLocomotionState::Idle:
        case PlayerLocomotionState::CrouchIdle:
            break;
        }

        //========================================================
        // 1クリップに左右の足が1歩ずつ入る場合は2周期
        // 片足分でループするクリップなら、その状態の値を1.0fにする
        //========================================================
        const float phase = (clipTime / clipLength) * XM_2PI * cyclesPerClip;
        targetY = std::sin(phase) * amplitude;
        break;
    }

    case PlayerActionState::PickupDown:
    {
        //========================================================
        // 実際の操作解除までに、カメラを元の位置へ戻す
        //========================================================
        const float duration = (std::min)(clipLength, PlayerAnimationController::PickupDownLockTime);
        const float amount = pulse(clipTime / duration, 0.28f, 0.55f);
        targetY = -0.18f * amount;
        targetPitchDegrees = -5.5f * amount;
        break;
    }

    case PlayerActionState::PickupMid:
    {
        const float duration = (std::min)(clipLength, PlayerAnimationController::PickupMidLockTime);
        const float amount = pulse(clipTime / duration, 0.25f, 0.42f);
        targetY = -0.035f * amount;
        targetPitchDegrees = -2.5f * amount;
        break;
    }

    case PlayerActionState::HitReaction:
    {
        const float amount = pulse(clipTime / clipLength, 0.17f, 0.20f);
        targetY = -0.020f * amount;
        targetPitchDegrees = 1.4f * amount;
        break;
    }

    case PlayerActionState::Death:
    {
        //========================================================
        // 現在の死亡待ち時間1.50秒のうち、最初の約1秒で沈み込む
        // 終端の値を保ち、ゲームオーバーへのフェードにつなげる
        //========================================================
        const float amount = ease(clipTime / (std::min)(clipLength, 1.0f));
        targetY = -0.65f * amount;
        targetPitchDegrees = -8.0f * amount;
        break;
    }
    }

    //========================================================
    // モーションが切り替わったフレームの段差を吸収する
    //========================================================
    const float motionBlend = blendRate(18.0f);
    mCameraMotionY += (targetY - mCameraMotionY) * motionBlend;

    const float targetPitch = XMConvertToRadians(targetPitchDegrees);
    mCameraMotionPitch += (targetPitch - mCameraMotionPitch) * motionBlend;

    float eyeHeight = (std::max)(0.30f, mSmoothedEyeHeight + mCameraMotionY);

    //========================================================
    // 低い天井の下でしゃがんだ直後だけ補間中の目が天井に残るのを防ぐ
    // 通常のしゃがみ移行ではこの補正は入り続けない
    //========================================================
    if (IsCrouching() && eyeHeight > 0.98f && !CanStandUp())
    {
        mSmoothedEyeHeight = stanceHeight;
        eyeHeight = (std::max)(0.30f, stanceHeight + mCameraMotionY);
    }

    vnCamera::FollowFirstPerson(*getPosition(), eyeHeight, mCameraMotionPitch);

    //========================================================
    // 光源位置は補間済みの目の位置に置く
    // 位置を別に遅らせると、曲がり角で光源が壁の向こうに残るので
    // 懐中電灯の追従感は照射方向だけに加える
    //========================================================
    const XMVECTOR targetAim = vnCamera::getForward();
    XMVECTOR aim = targetAim;

    if (!initializeView)
    {
        const XMVECTOR previousAim = XMLoadFloat3(&mFlashlightAim);

        //========================================================
        // ほぼ反対を向いたときの不安定な補間を避ける
        //========================================================
        if (XMVectorGetX(XMVector3Dot(previousAim, targetAim)) > 0.0f)
        {
            aim = XMVector3Normalize(
                XMVectorLerp(previousAim, targetAim, blendRate(26.0f)));
        }
    }

    XMStoreFloat3(&mFlashlightAim, aim);
    mFirstPersonViewReady = true;

    mFlashlight.SubmitSpotLight(*vnCamera::getPosition(), aim);
}

void ExplorationPlayer::UpdateViewAndFlashlight(float dt, bool useGameplayCamera, const std::vector<Collider*>& colliders)
{
    const bool firstPerson = useGameplayCamera && IsFirstPerson();

    if (ModelBase* model = GetModel())
    {
        model->SetVisibleInMainCamera(!firstPerson);
    }

    if (mFlashlightVisualModel != nullptr)
    {
        mFlashlightVisualModel->SetVisibleInMainCamera(!firstPerson);
    }

    if (firstPerson)
    {
        // カメラを確定し、その姿勢から懐中電灯を一度だけ登録する
        UpdateFirstPersonView(dt);
        mWasFirstPersonCamera = true;
        return;
    }

    // 次に一人称へ入るとき、古い目線や懐中電灯の方向を持ち越さない
    mFirstPersonViewReady = false;

    if (useGameplayCamera && !IsDead())
    {
        vnCamera::FollowTargetWithColliders(*getPosition(), dt, colliders, mWasFirstPersonCamera);
        mWasFirstPersonCamera = false;
    }

    // 三人称とフリーカメラでは胸ボーンを光源位置に使う
    XMMATRIX chestBoneWorld = XMMatrixIdentity();
    const bool hasChestBone =
        mFlashlightBoneIndex >= 0 && GetCurrentBoneWorldMatrix(mFlashlightBoneIndex, chestBoneWorld);

    mFlashlight.Update(*this, IsCrouching(), hasChestBone ? &chestBoneWorld : nullptr);
}

void ExplorationPlayer::UpdateDeathPresentation(float dt, bool useGameplayCamera, const std::vector<Collider*>& colliders)
{
    //========================================================
    // 死亡中はモーションと表示だけを更新する
    // 入力・移動・インタラクトを含む通常のUpdate()は通さない
    //========================================================
    SkeletalModel::execute(std::clamp(dt, 0.0f, 0.10f));
    UpdateViewAndFlashlight(dt, useGameplayCamera, colliders);
}

void ExplorationPlayer::PrepareGameplayCamera(float dt, bool useGameplayCamera)
{
    if (!useGameplayCamera)
    {
        return;
    }

    //========================================================
    // マウス・右スティックの角度更新は、1フレームに一度だけ行う
    //========================================================
    vnCamera::UpdateFollowAngles(dt);

    if (IsFirstPerson())
    {
        //========================================================
        // 移動・探索処理がカメラ方向を参照するときに
        // このフレームの向きを取得できるよう先に反映する
        //========================================================
        vnCamera::FollowFirstPerson(*getPosition(), GetFirstPersonEyeHeight());
    }
}

bool ExplorationPlayer::AttachFlashlightVisualModel(ModelBase* model, const WCHAR* boneName, const XMFLOAT3& localOffset)
{
    if (model == nullptr || boneName == nullptr)
    {
        return false;
    }

    //========================================================
    // Bone名からIndexを検索
    //========================================================
    const int boneIndex = FindBoneIndex(boneName);

    if (boneIndex < 0)
    {
#if defined(_DEBUG)
        OutputDebugStringW(
            L"[Flashlight] Attachment bone not found.\n"
        );
#endif
        return false;
    }

    //========================================================
    // Bone Object取得
    //========================================================
    ObjectBase* bone = GetBone(boneIndex);

    if (bone == nullptr)
    {
        return false;
    }

    //========================================================
    // 以前のアタッチを解除
    //========================================================
    DetachFlashlightVisualModel();

    //========================================================
    // ライトモデルをBoneの子にする
    //========================================================
    model->setParent(bone);

    //========================================================
    // ※PositionはWorld座標ではなくBone Local座標
    //========================================================
    model->setPosition(
        localOffset.x,
        localOffset.y,
        localOffset.z
    );

    //========================================================
    // Local Quaternion
    //========================================================
    model->useQuaternion(true);

    XMVECTOR localRotation = XMQuaternionIdentity();
    model->setQuaternion(&localRotation);

    //========================================================
    // Attachment情報を保存
    //========================================================
    mFlashlightVisualModel = model;
    mFlashlightBoneIndex = boneIndex;

    //========================================================
    // SpotLight側にもモデルを登録
    //========================================================
    mFlashlight.SetVisualModel(model);

    return true;
}

void ExplorationPlayer::DetachFlashlightVisualModel()
{
    //========================================================
    // Boneとの親子関係を解除
    //========================================================
    if (mFlashlightVisualModel != nullptr)
    {
        mFlashlightVisualModel->setParent(nullptr);
    }

    //========================================================
    // PlayerFlashlight側の参照も解除
    //========================================================
    mFlashlight.SetVisualModel(nullptr);

    mFlashlightVisualModel = nullptr;
    mFlashlightBoneIndex = -1;
}

//============================================================
// Move Input
//============================================================
void ExplorationPlayer::AddMoveAxis(float x,float y)
{
    if (mControlLocked || mPickupAligning || mAnimation.IsActionPlaying())
    {
        return;
    }

    mMovement.AddMoveAxis(x, y );
}


//============================================================
// Sprint
//============================================================
void ExplorationPlayer::SetSprint(bool sprint)
{
    if (mControlLocked || mPickupAligning || mAnimation.IsActionPlaying())
    {
        mMovement.SetSprint(false);
        return;
    }

    mMovement.SetSprint(sprint);
}


//============================================================
// Physics
//============================================================
void ExplorationPlayer::SetGrounded(bool grounded)
{
    mMovement.SetGrounded(grounded);
}


bool ExplorationPlayer::IsGrounded() const
{
    return mMovement.IsGrounded();
}


void ExplorationPlayer::SetVerticalSpeed(float speed)
{
    mMovement.SetVerticalSpeed(speed);
}


float ExplorationPlayer::GetVerticalSpeed() const
{
    return mMovement.GetVerticalSpeed();
}

bool ExplorationPlayer::PlayPickupAnimationAt(const XMVECTOR& itemPosition)
{
    //========================================================
    // 死亡中・操作ロック中・Action中は開始しない
    //========================================================
    if (mDead || mControlLocked || mAnimation.IsActionPlaying())
    {
        return false;
    }

    //========================================================
    // Playerの足元Y
    //========================================================
    const float playerY = XMVectorGetY(*getPosition());

    //========================================================
    // ItemのWorld Y
    //========================================================
    const float itemY = XMVectorGetY(itemPosition);

    //========================================================
    // Playerに対するアイテムの相対高さ
    //========================================================
    const float relativeHeight = itemY - playerY;

    //========================================================
    // Ground / Mid判定
    // 0.75m未満: 地面に落ちているアイテムを拾う
    // 0.75m以上: 机や棚などにあるアイテムを拾う
    //========================================================
    constexpr float PickupHeightThreshold = 0.75f;

    const PlayerActionState action =
        relativeHeight < PickupHeightThreshold ? PlayerActionState::PickupDown : PlayerActionState::PickupMid;

    //========================================================
    // Action Animation再生
    //========================================================
    return mAnimation.PlayAction(*this, action);
}

bool ExplorationPlayer::BeginWorldItemPickup(WorldItemPickup& item, const XMVECTOR& itemPosition)
{
    //========================================================
    // Pickup開始不可
    //========================================================
    if (mDead || mControlLocked || mPendingPickup != nullptr || mAnimation.IsActionPlaying())
    {
        return false;
    }

    if (mCollider == nullptr || mWorldColliders == nullptr)
    {
        return false;
    }

    //========================================================
    // 別の階にあるアイテムへ、自動で近づき始めるのを防ぐ
    //========================================================

    if (!IsPickupWithinVerticalReach(itemPosition))
    {
        return false;
    }

    //========================================================
    // 取得対象を予約
    //========================================================
    mPendingPickup = &item;

    mPendingPickupAction = PlayerActionState::None;
    mPendingPickupCommitTime = 0.0f;

    //========================================================
    // 対象位置を保存
    //========================================================
    XMStoreFloat3(
        &mPendingPickupTargetPosition,
        itemPosition
    );

    //========================================================
    // Pickup Alignment開始
    //========================================================
    mPickupAligning = true;
    mPickupAlignElapsed = 0.0f;

    return true;
}

//============================================================
// Cancel World Item Pickup
//============================================================
void ExplorationPlayer::CancelWorldItemPickup(const WorldItemPickup* item)
{
    if (mPendingPickup == nullptr)
    {
        return;
    }

    //========================================================
    // 指定対象と現在の取得待ちが異なる場合は何もしない
    //========================================================
    if (item != nullptr && mPendingPickup != item)
    {
        return;
    }

    //========================================================
    // Itemへの参照を解除
    //========================================================
    WorldItemPickup* pending = mPendingPickup;

    mPendingPickup = nullptr;
    mPendingPickupAction = PlayerActionState::None;
    mPendingPickupCommitTime = 0.0f;
    mPickupAligning = false;
    mPickupAlignElapsed = 0.0f;
    
    //========================================================
    // Item側も取得待ち状態を解除
    //========================================================
    pending->CancelPendingPickup();
}

//============================================================
// Interaction
//============================================================
void ExplorationPlayer::Interact()
{
    //========================================================
    // 拾得・被弾・死亡中は他の操作を開始しない
    //========================================================
    if (mControlLocked || mPickupAligning || mAnimation.IsActionPlaying())
    {
        return;
    }

    mInteraction.RequestInteract();
}


//============================================================
// Flashlight
//============================================================
void ExplorationPlayer::ToggleFlashlight()
{
    mFlashlight.Toggle();
    SoundBank::getInstance()->PlaySfx(SoundID::AE_Flashlight, 0.70f);
}

void ExplorationPlayer::ToggleViewMode()
{
    mViewMode = IsFirstPerson() ? ExplorationViewMode::ThirdPerson : ExplorationViewMode::FirstPerson;
}

//============================================================
// Crouch Input
//============================================================
void ExplorationPlayer::SetCrouching(bool crouching)
{
    //========================================================
    // 操作ロック中
    //========================================================
    if (mControlLocked || mPickupAligning || mAnimation.IsActionPlaying())
    {
        return;
    }

    //========================================================
    // 同じ姿勢なら処理不要
    //========================================================
    if (mMovement.IsCrouching() == crouching)
    {
        return;
    }

    //========================================================
    // 立ち上がるときだけHeadroomを確認
    //========================================================
    if (!crouching)
    {
        if (!CanStandUp())
        {
            //================================================
            // 頭上に障害物があるので立ち上がれない
            // Crouch状態を維持する
            //================================================
            return;
        }
    }

    //========================================================
    // Collider Size変更
    //========================================================
    ApplyColliderStance(crouching);

    //========================================================
    // Movement State変更
    //========================================================
    mMovement.SetCrouching(crouching);
}

bool ExplorationPlayer::IsCrouching() const
{
    return mMovement.IsCrouching();
}

//============================================================
// Collider
//============================================================
void ExplorationPlayer::ConfigureCollider(float radius, float halfSegmentHeight, float offsetY)
{
    mCollider = std::make_unique<CapsuleCollider>(this, radius, halfSegmentHeight, XMVectorSet(0.0f, offsetY, 0.0f, 0.0f));
    mCollider->SetType(ColliderType::Body);
    mCollider->SetMass(1.0f);
    mCollider->SetTag(ColliderTag::Player);
    mCollider->SetSurfaceType(SurfaceType::Flesh);
}

bool ExplorationPlayer::IsPickupWithinVerticalReach(const XMVECTOR& itemPosition) const
{
    //============================================================
    // プレイヤー原点は足元。棚の上や床置きのアイテムを許容しつつ
    // 地上から地下など高さが大きく離れた対象を除外する
    //============================================================
    constexpr float maxVerticalReach = 2.0f;

    const float deltaY = XMVectorGetY(itemPosition) - XMVectorGetY(*getPosition());

    return std::fabs(deltaY) <= maxVerticalReach;
}

bool ExplorationPlayer::ApplyDamage(float damage)
{
    //========================================================
    // 死亡済み
    //========================================================
    if (mDead)
    {
        return false;
    }


    //========================================================
    // Damage無敵中
    //========================================================
    if (mDamageInvincibleTimer > 0.0f)
    { 
        return false;
    }


    //========================================================
    // 無効Damage
    //========================================================
    if (damage <= 0.0f)
    {
        return false;
    }

    //============================================================
    // DamageによるPickupキャンセル
    //============================================================
    CancelWorldItemPickup();

    //========================================================
    // Damage
    //========================================================
    mHealth -= damage;

    if (mHealth < 0.0f)
    {
        mHealth = 0.0f;
    }


    //========================================================
    // 無敵時間開始
    //========================================================
    mDamageInvincibleTimer = mDamageInvincibleDuration;


#if defined(_DEBUG)

    wchar_t buffer[128];

    swprintf_s(
        buffer,
        L"[ExplorationPlayer] Damage %.1f / HP %.1f\n",
        damage,
        mHealth
    );

    OutputDebugStringW(buffer);

#endif


    //========================================================
    // Death
    //========================================================
    if (mHealth <= 0.0f)
    {
        mDead = true;
        SetControlLocked(true);
        mAnimation.PlayAction(*this, PlayerActionState::Death);

#if defined(_DEBUG)

        OutputDebugStringW(L"[ExplorationPlayer] Dead.\n");

#endif
    }
    else
    {
        //========================================================
        // Hit Reaction
        // HPが残っている場合だけ被弾Motionを再生する
        //========================================================
        mAnimation.PlayAction(*this, PlayerActionState::HitReaction);

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[ExplorationPlayer] Hit Reaction.\n"
        );
#endif
    }


    return true;
}

void ExplorationPlayer::RestoreFromSave(const XMFLOAT3& position, float health)
{
    //============================================================
    // Position
    //============================================================
    setPosition(
        position.x,
        position.y,
        position.z
    );


    //============================================================
    // Health
    //============================================================
    mHealth = std::clamp(health, 0.0f, mMaxHealth);


    //============================================================
    // Save File異常時に整合性が崩れないよう
    // HPから再構築する
    //============================================================
    mDead = mHealth <= 0.0f;

    //============================================================
    // Animation Reset
    //============================================================
    mAnimation.ResetAfterLoad(*this);

    // HPが0のセーブデータにも対応する場合
    if (mDead)
    {
        mAnimation.PlayAction(*this, PlayerActionState::Death);
    }

    //============================================================
    // Load直後に以前の無敵時間を引き継がない
    //============================================================
    mDamageInvincibleTimer = 0.0f;

    //============================================================
    // Physics State Reset
    //============================================================
    SetVerticalSpeed(0.0f);
    SetGrounded(false);
    mMovement.SetCrouching(false);
    mMovement.SetSprint(false);
    mMovement.SetCrouching(false);
    ApplyColliderStance(false);
    //============================================================
    // Control State
    //============================================================
    SetControlLocked(mDead);
    //============================================================
    // Load前のPickup予約解除
    //============================================================
    CancelWorldItemPickup();
}

void ExplorationPlayer::ApplyColliderStance(bool crouching)
{
    if (mCollider == nullptr)
    {
        return;
    }

    //========================================================
    // Standing Collider
    //========================================================
    constexpr float StandingHalfSegment = 0.55f;
    constexpr float StandingOffsetY = 0.90f;

    //========================================================
    // Crouching Collider
    //========================================================
    constexpr float CrouchHalfSegment = 0.20f;
    constexpr float CrouchOffsetY = 0.55f;

    //========================================================
    // 現在の姿勢から使用する寸法を決定
    //========================================================
    const float halfSegment = crouching ? CrouchHalfSegment : StandingHalfSegment;

    const float offsetY = crouching ? CrouchOffsetY : StandingOffsetY;

    //========================================================
    // 既存のCapsule Colliderを更新
    //========================================================
    mCollider->SetHalfSegmentHeight(halfSegment);

    mCollider->SetOffset(XMVectorSet(0.0f, offsetY, 0.0f, 0.0f));
}

bool ExplorationPlayer::CanStandUp()
{
    //========================================================
    // Colliderが無い場合は姿勢変更不可
    //========================================================
    if (mCollider == nullptr)
    {
        return false;
    }

    //========================================================
    // 参照するWorld Colliderが登録されていない場合
    // 安全のため立ち上がりを許可しない
    //========================================================
    if (mWorldColliders == nullptr)
    {
        return false;
    }

    //========================================================
    // 既存のCapsule寸法
    //
    // Standing:
    //   Radius = 0.35
    //   SegmentTop = 0.90 + 0.55 = 1.45
    //   TotalTop = 1.80
    //
    // Crouching:
    //   Radius = 0.35
    //   SegmentTop = 0.55 + 0.20 = 0.75
    //   TotalTop = 1.10
    //========================================================
    constexpr float StandingOffsetY = 0.90f;
    constexpr float StandingHalfSegment = 0.55f;

    constexpr float CrouchOffsetY = 0.55f;
    constexpr float CrouchHalfSegment = 0.20f;

    //========================================================
    // 上側Segmentの開始位置と終了位置
    //========================================================
    const float crouchTop =
        CrouchOffsetY + CrouchHalfSegment;

    const float standingTop =
        StandingOffsetY + StandingHalfSegment;

    //========================================================
    // 頭上判定用Capsuleを生成
    // Segment:
    //   Bottom = 0.75m
    //   Top    = 1.45m
    // Radiusを含めた範囲:
    //   0.40m ～ 1.80m
    // 既存Crouch Colliderと一部重なるが、
    // 床との不要な衝突を避けながら
    // 立ち上がりに必要な上側空間を調べられる
    //========================================================
    const float probeCenterY =
        (crouchTop + standingTop) * 0.5f;

    const float probeHalfSegment =
        (standingTop - crouchTop) * 0.5f;

    //========================================================
    // 判定専用の一時Capsule
    //========================================================
    CapsuleCollider headroomProbe(
        this,
        mCollider->GetRadius(),
        probeHalfSegment,
        XMVectorSet(
            0.0f,
            probeCenterY,
            0.0f,
            0.0f
        )
    );

    //========================================================
    // World Colliderとの衝突確認
    //========================================================
    for (const Collider* other : *mWorldColliders)
    {
        if (other == nullptr)
        {
            continue;
        }

        //====================================================
        // 無効Colliderは無視する
        //====================================================
        if (!other->IsEnabled())
        {
            continue;
        }

        //====================================================
        // 自分自身のColliderを除外する
        //====================================================
        if (other->GetOwner() == this)
        {
            continue;
        }

        //====================================================
        // 頭上に障害物がある場合
        //====================================================
        if (headroomProbe.CollisionCheck(other))
        {
            return false;
        }
    }

    //========================================================
    // 立ち上がりに必要な空間が確保されている
    //========================================================
    return true;
}

void ExplorationPlayer::UpdatePendingPickup()
{
    if (mPendingPickup == nullptr)
    {
        return;
    }
    
    if (mPickupAligning)
    {
        return;
    }

    //========================================================
    // 死亡またはActionが別のMotionに切り替わった
    // HitReaction等で中断されたときは取得しない
    //========================================================
    if (mDead || mAnimation.GetCurrentAction() != mPendingPickupAction)
    {
        CancelWorldItemPickup();
        return;
    }

    //========================================================
    // 現在のPickup Motion時刻
    //========================================================
    const float currentTime = GetMotionTime();

    //========================================================
    // まだ取得確定時刻ではない
    // Motionが先に終了した場合は確定処理へ進む
    //========================================================
    if (IsMotionPlaying() && currentTime < mPendingPickupCommitTime)
    {
        return;
    }

    //========================================================
    // 取得確定
    // 呼び出し前にPlayer側の取得待ち参照を解除
    //========================================================
    WorldItemPickup* item = mPendingPickup;

    mPendingPickup = nullptr;
    mPendingPickupAction = PlayerActionState::None;
    mPendingPickupCommitTime = 0.0f;

    //========================================================
    // Inventoryへ登録し、World上から消す
    // 登録失敗時はWorldItemPickup側でItemを残す
    //========================================================
    if (item->CommitPickup(*this))
    {
        mPickupCommitted = true;
    }
}

void ExplorationPlayer::UpdatePickupFacing(float dt)
{
    //========================================================
   // Pickup対象がない場合は何もしない
   //========================================================
    if (mPendingPickup == nullptr || mDead)
    {
        return;
    }

    //========================================================
    // 現在のAction確認
    // HitReactionやDeathに切り替わっていたら
    // 対象物への回転を停止する
    //========================================================
    if (mAnimation.GetCurrentAction() != mPendingPickupAction)
    {
        return;
    }

    //========================================================
    // Player Position
    //========================================================
    const XMVECTOR playerPosition = *getPosition();

    //========================================================
    // Pickup Target Position
    //========================================================
    const XMVECTOR targetPosition =
        XMLoadFloat3(&mPendingPickupTargetPosition);

    //========================================================
    // Player → Item方向を求める
    //========================================================
    XMVECTOR direction =
        XMVectorSubtract(targetPosition, playerPosition);

    //========================================================
    // Y方向を除去
    // アイテムが床にあっても机にあっても
    // Player Rootは水平回転のみ行う
    //========================================================
    direction = XMVectorSetY(direction, 0.0f);

    //========================================================
    // 距離がほぼ0の場合は方向を決定できない
    //========================================================
    const float lengthSq =
        XMVectorGetX(XMVector3LengthSq(direction));

    if (lengthSq < 0.0001f)
    {
        return;
    }

    //========================================================
    // +Z Forward基準でYawを計算
    //========================================================
    float desiredYaw = atan2f(
        XMVectorGetX(direction),
        XMVectorGetZ(direction)
    );

    //========================================================
    // Model Forward補正
    // 通常移動のUpdateFacingDirection()と
    // 同じ補正を使用する
    //========================================================
    desiredYaw +=
        mMovement.GetSettings().ModelForwardYawOffset;

    //========================================================
    // Target Quaternion
    //========================================================
    XMVECTOR targetQuaternion =
        XMQuaternionRotationRollPitchYaw(
            0.0f,
            desiredYaw,
            0.0f
        );

    //========================================================
    // Current Quaternion
    //========================================================
    XMVECTOR currentQuaternion =
        XMQuaternionNormalize(*getQuaternion());

    //========================================================
    // FrameRate Independent Blend
    //========================================================
    const float clampedDt = std::clamp(dt, 0.0f, 0.1f);

    const float blend = 1.0f - expf(-mPickupRotationSharpness * clampedDt);

    //========================================================
    // Quaternion SLERP
    //========================================================
    XMVECTOR result =
        XMQuaternionSlerp(
            currentQuaternion,
            targetQuaternion,
            blend
        );

    result = XMQuaternionNormalize(result);

    //========================================================
    // Player Rootへ適用
    //========================================================
    setQuaternion(&result);
}

void ExplorationPlayer::UpdatePickupAlignment(float dt)
{
    //========================================================
    // Pickup中でなければ処理しない
    //========================================================
    if (!mPickupAligning || mPendingPickup == nullptr)
    {
        return;
    }

    //========================================================
    // 死亡・操作ロック時は中断
    //========================================================
    if (mDead || mControlLocked)
    {
        CancelWorldItemPickup();
        return;
    }

    dt = std::clamp(dt, 0.0f, 0.1f);

    mPickupAlignElapsed += dt;

    //========================================================
    // 現在のPlayer PositionとItem Position
    //========================================================
    const XMVECTOR playerPosition = *getPosition();

    const XMVECTOR targetPosition = XMLoadFloat3(&mPendingPickupTargetPosition);

    //========================================================
    // アイテムの相対高さ
    // 台座・机・棚の上にあるアイテムは
    // 床置きアイテムより遠い位置から取得する
    //========================================================
    const float relativeHeight =
        XMVectorGetY(targetPosition) -
        XMVectorGetY(playerPosition);

    //========================================================
    // 台座・机の上にあるアイテムとみなす高さ
    // 0.45m以上を基準とする
    // PickupDown / PickupMidのアニメーション選択は
    // PlayPickupAnimationAt()の従来判定を維持する
    //========================================================
    constexpr float RaisedItemThreshold = 0.45f;

    const bool isRaisedItem = relativeHeight >= RaisedItemThreshold;

    //========================================================
    // 通常アイテムの停止距離は既存の0.8mを使用
    // 台座上のアイテムは1.20m手前で停止する
    //========================================================
    constexpr float RaisedItemStopDistance = 1.20f;

    const float stopDistance =
        isRaisedItem ? (std::max)(mPickupStopDistance, RaisedItemStopDistance) : mPickupStopDistance;

    //========================================================
    // 台座に衝突して移動できなくなった場合でも
    // この距離以内なら取得モーションを許可する
    // ※Raised Item限定
    //========================================================
    constexpr float RaisedItemMaxReach = 1.55f;

    //========================================================
    // PlayerからItemへの水平ベクトル
    //========================================================
    XMVECTOR direction = targetPosition - playerPosition;

    // Y方向は移動に使わない
    direction = XMVectorSetY(direction, 0.0f);

    const float distance = XMVectorGetX(XMVector3Length(direction));

    //========================================================
    // Pickup Motion開始処理
    // 通常到着、Colliderで停止、Timeout時に共通使用
    // アニメーション開始に成功した場合だけ
    // mPickupAligningを解除し、取得確定時刻を保存する
    //========================================================
    const auto startPickupMotion = [&]() -> bool
        {
            //====================================================
            // アイテムの高さに応じたPickup Motionを開始
            //====================================================
            if (!PlayPickupAnimationAt(targetPosition))
            {
                return false;
            }

            const PlayerActionState action =
                mAnimation.GetCurrentAction();

            //====================================================
            // 取得アニメーションが正しく開始されたか確認
            //====================================================
            if (action != PlayerActionState::PickupDown && action != PlayerActionState::PickupMid)
            {
                return false;
            }

            //====================================================
            // 自動位置合わせ終了
            //====================================================
            mPickupAligning = false;

            //====================================================
            // 取得アニメーション情報を保存
            //====================================================
            mPendingPickupAction = action;

            //====================================================
            // 従来のInventory確定時刻を維持
            //====================================================
            mPendingPickupCommitTime =
                (action == PlayerActionState::PickupDown) ? 1.25f : 0.50f;

            return true;
        };

    //========================================================
    // 目標停止距離に到着した場合
    //========================================================
    if (distance <= stopDistance + mPickupAlignTolerance)
    {
        if (!startPickupMotion())
        {
            CancelWorldItemPickup();
        }

        return;
    }

    //========================================================
    // 位置合わせの最大時間を超えた場合
    // すでに台座上のアイテムへ手が届く範囲なら
    // その場でPickup Motionを実行する
    // 遠い場合はキャンセル
    //========================================================
    if (mPickupAlignElapsed >= mPickupAlignMaxTime)
    {
        if (isRaisedItem && distance <= RaisedItemMaxReach)
        {
            if (!startPickupMotion())
            {
                CancelWorldItemPickup();
            }
        }
        else
        {
            CancelWorldItemPickup();
        }

        return;
    }

    //========================================================
    // 水平方向の移動量を計算
    //========================================================
    direction = XMVector3Normalize(direction);

    const float alignmentSpeed =
        mMovement.IsCrouching() ? mPickupAlignCrouchSpeed : mPickupAlignSpeed;

    //========================================================
    // 今回移動する距離
    // ※stopDistanceより内側へ移動しない
    //========================================================
    const float remainingDistance = distance - stopDistance;

    const float moveDistance = (std::min)(remainingDistance, alignmentSpeed * dt);

    const XMVECTOR delta = XMVectorScale(direction, moveDistance);

    //========================================================
    // Colliderとの衝突確認
    //========================================================
    if (!CanMoveForPickup(delta))
    {
        //====================================================
        // 台座のColliderに阻まれた可能性
        // アイテムが高所にあり手が届く範囲ならその場でPickupを開始する
        //====================================================
        if (isRaisedItem &&
            distance <= RaisedItemMaxReach)
        {
#if defined(_DEBUG)
            OutputDebugStringW(
                L"[PickupAlignment] "
                L"Blocked near raised item. "
                L"Starting pickup in place.\n"
            );
#endif

            if (!startPickupMotion())
            {
                CancelWorldItemPickup();
            }
        }
        else
        {
#if defined(_DEBUG)
            OutputDebugStringW(
                L"[PickupAlignment] "
                L"Blocked outside pickup reach.\n"
            );
#endif

            //================================================
            // 手の届かない位置なら通常キャンセル
            //================================================
            CancelWorldItemPickup();
        }

        return;
    }

    //========================================================
    // 衝突しない場合は従来どおり移動する
    //========================================================
    addPosition(&delta);
}

bool ExplorationPlayer::CanMoveForPickup(const XMVECTOR& delta)
{
    //========================================================
    // 必要なCollision情報がない
    //========================================================
    if (mCollider == nullptr || mWorldColliders == nullptr)
    {
        return false;
    }

    //========================================================
    // 現在のCapsule情報
    //========================================================
    const float radius = mCollider->GetRadius();

    const float halfSegmentHeight = mCollider->GetHalfSegmentHeight();

    const XMVECTOR currentCenter = mCollider->GetCenter();

    //========================================================
    // Player RootからのCollider Offset
    //========================================================
    const XMVECTOR offset = currentCenter - *getPosition();

    //========================================================
    // Pickup移動後のCollider Offset
    //========================================================
    const XMVECTOR probeOffset = offset + delta;

    //========================================================
    // 判定専用Capsule
    //========================================================
    CapsuleCollider probe(
        this,
        radius,
        halfSegmentHeight,
        probeOffset
    );

    //========================================================
    // Playerの足元Yを計算
    //========================================================
    const float playerFeetY = XMVectorGetY(currentCenter) - halfSegmentHeight - radius;

    //========================================================
    // World Colliderを確認
    //========================================================
    for (const Collider* other : *mWorldColliders)
    {
        //====================================================
        // 無効Collider
        //====================================================
        if (other == nullptr || !other->IsEnabled())
        {
            continue;
        }

        //====================================================
        // Player自身は除外
        //====================================================
        if (other->GetOwner() == this)
        {
            continue;
        }

        //====================================================
        // Floor Box判定
        //====================================================
        const BoxCollider* box = dynamic_cast<const BoxCollider*>(other);

        if (box != nullptr)
        {
            const XMVECTOR boxCenter = box->GetCenter();
            const XMVECTOR boxHalfSize = box->GetHalfSize();

            //================================================
            // BoxColliderの上面Y
            //================================================
            const float boxTopY =
                XMVectorGetY(boxCenter) + XMVectorGetY(boxHalfSize);

            //================================================
            // 接地誤差
            // 5cm以内の誤差を許容する
            //================================================
            constexpr float GroundTolerance = 0.05f;

            //================================================
            // 床として扱える高さなら除外
            // 高い壁や家具のColliderは除外しない
            //================================================
            if (boxTopY <= playerFeetY + GroundTolerance)
            {
                continue;
            }
        }

        //====================================================
        // 移動先のCapsuleが障害物へ衝突するか
        //====================================================
        if (probe.CollisionCheck(other))
        {
#if defined(_DEBUG)

            //================================================
            // どのColliderで移動が止められたか調査する
            //================================================
            OutputDebugStringW(
                L"[PickupAlignment] "
                L"Movement blocked by collider.\n"
            );

#endif
            return false;
        }
    }

    //========================================================
    // 移動先に障害物なし
    //========================================================
    return true;
}
