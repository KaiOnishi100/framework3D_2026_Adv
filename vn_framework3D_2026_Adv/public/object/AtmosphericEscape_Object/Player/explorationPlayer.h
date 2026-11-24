#pragma once

#include "../../../collide/IPhysicsCharacter.h"
#include "../../../interface/IDamageable.h"
#include "../../../input/AtmosphericEscape_InputConfig/IExplorationControllable.h"

#include "explorationPlayerMovement.h"
#include "playerAnimationController.h"
#include "playerInteraction.h"
#include "playerFlashlight.h"
#include "playerNoiseEmitter.h"

class WorldItemPickup;

enum class ExplorationViewMode
{
    ThirdPerson,
    FirstPerson
};

//============================================================
// ExplorationPlayer
// Player全体をまとめるFacade
//============================================================
class ExplorationPlayer : public SkeletalModel, public IExplorationControllable, public IPhysicsCharacter, public IDamageable
{
public:

    ExplorationPlayer(const WCHAR* folder, const WCHAR* modelFile, const WCHAR* boneFile);


    virtual ~ExplorationPlayer() override;

    //========================================================
    // Locomotion Animation初期化
    //========================================================
    bool InitializeLocomotionAnimations(const PlayerLocomotionAnimationSet& animations);

    //============================================================
    // Action Animation初期化
    //============================================================
    bool InitializeActionAnimations(const PlayerActionAnimationSet& animations);

    //========================================================
    // Update
    //========================================================
    void Update(float dt);

    void UpdateFirstPersonView(float dt);

    void UpdateViewAndFlashlight(
        float dt,
        bool useGameplayCamera,
        const std::vector<Collider*>& colliders
    );

    void UpdateDeathPresentation(float dt, bool useGameplayCamera, const std::vector<Collider*>& colliders);

    void PrepareGameplayCamera(float dt, bool useGameplayCamera);

    //============================================================
    // Flashlight Visual Model
    //============================================================
    void SetFlashlightVisualModel(ModelBase* model)
    {
        mFlashlight.SetVisualModel(model);
    }

    //============================================================
    // Flashlight Modelを指定Boneへ装着する
    // model: Sceneが所有するライト本体モデル
    // boneName: 装着先のBone名
    // localOffset: Bone空間での装着位置
    //============================================================
    bool AttachFlashlightVisualModel(
        ModelBase* model,
        const WCHAR* boneName,
        const XMFLOAT3& localOffset
    );

    //============================================================
    // ライトモデルをBoneから取り外す
    //============================================================
    void DetachFlashlightVisualModel();

    void SetFlashlightShadowExcludedCaster( const ObjectBase* caster)
    {
        mFlashlight.SetShadowExcludedCaster(caster);
    }

    //========================================================
    // Input
    //========================================================
    bool IsControlLocked() const override
    {
        return mControlLocked;
    }

    void AddMoveAxis(float x, float y) override;
    void SetSprint(bool sprint) override;
    void Interact() override;
    void ToggleFlashlight() override;
    void ToggleViewMode() override;

    //============================================================
    // Crouch
    //============================================================
    void SetCrouching(bool crouching) override;

    //============================================================
    // 現在のしゃがみ状態
    //============================================================
    bool IsCrouching() const override;

    bool IsFirstPerson() const
    {
        return mViewMode == ExplorationViewMode::FirstPerson;
    }

    //========================================================
    // Physics Interface
    // 実体はPlayerMovementが持つ
    // ExplorationPlayerは単純にForwardする
    //========================================================
    void SetGrounded(bool grounded) override;
    bool IsGrounded() const override;
    void SetVerticalSpeed(float speed) override;
    float GetVerticalSpeed() const override;

    //============================================================
    // アイテムの高さからPickup Motionを自動選択
    //============================================================
    bool PlayPickupAnimationAt(const XMVECTOR& itemPosition);

    //============================================================
    // World Item Pickup開始
    //============================================================
    bool BeginWorldItemPickup(WorldItemPickup& item, const XMVECTOR& itemPosition);

    //============================================================
    // 取得待ちの解除
    // item == nullptr: 現在の取得待ちを無条件に解除
    // item != nullptr: 指定されたItemが取得待ちなら解除
    //============================================================
    void CancelWorldItemPickup(const WorldItemPickup* item = nullptr);

    //========================================================
    // Collider
    //========================================================
    Collider* GetCollider() const
    {
        return mCollider.get();
    }


    void ConfigureCollider(float radius, float halfSegmentHeight, float offsetY);


    //========================================================
    // Control
    //========================================================
    void SetControlLocked(bool locked)
    {
        mControlLocked = locked;
    }

    //============================================================
    // World Collider一覧を登録
    // PlayerMovement: 段差を乗り越える判定に使用
    // ExplorationPlayer: 立ち上がり時の頭上障害物判定に使用
    //============================================================
    void SetStepColliders(const std::vector<Collider*>* colliders)
    {
        mWorldColliders = colliders;
        mMovement.SetStepColliders(colliders);
    }

public:
    //========================================================
    // 状態取得
    //========================================================
    //============================================================
    // 現在移動しているか
    //============================================================
    bool IsMoving() const
    {
        return mMovement.IsMoving();
    }

    //============================================================
    // Sprint入力中か
    // IsMoving()と組み合わせて使う
    // Sprintキーだけ押して静止している場合は
    // 足音を発生させない
    //============================================================
    bool IsSprinting() const
    {
        return mMovement.IsSprinting();
    }

    //============================================================
    // 現在の移動速度
    //============================================================
    float GetCurrentMoveSpeed() const
    {
        return mMovement.GetCurrentSpeed();
    }

    //============================================================
    // Flashlight State
    // Tutorial / Gameplay判定用
    //============================================================
    bool IsFlashlightEnabled() const
    {
        return mFlashlight.IsEnabled();
    }

    //============================================================
    // 1人称用のEyeHeightを取得
    //============================================================
    float GetFirstPersonEyeHeight() const 
    {
        return IsCrouching() ? 0.95f : 1.60f;
    }

public:

    //========================================================
    // Interaction System接続
    //========================================================
    void SetInteractionSystem(InteractionSystem* system)
    {
        mInteraction.SetInteractionSystem(system);
    }

    //============================================================
    // 現在Interaction可能なObjectを取得
    //============================================================
    IInteractable* GetInteractionTarget() const
    {
        return mInteraction.GetCurrentTarget();
    }

    //============================================================
    // 取得の予約時と、インベントリに入れる直前で共通して使う
    //============================================================
    bool IsPickupWithinVerticalReach(const XMVECTOR& itemPosition) const;

    //============================================================
    // Noise System接続
    //============================================================
    void SetNoiseSystem(NoiseSystem* system)
    {
        mNoiseEmitter.SetNoiseSystem(system);
    }

    bool ConsumePickupCommitted()
    {
        // シーン側で一度受け取ったら次のフレームには持ち越さない
        const bool committed = mPickupCommitted;
        mPickupCommitted = false;
        return committed;
    }

    void NotifyPickupCommitted()
    {
        mPickupCommitted = true;
    }

public:

    //============================================================
    // Health
    //============================================================

    //========================================================
    // Damage
    // return:
    //      true  = Damageが実際に入った
    //      false = 無敵中 / 死亡済み
    //========================================================
    bool ApplyDamage(float damage) override;


    bool IsDead() const override
    {
        return mDead;
    }


    float GetHealth() const
    {
        return mHealth;
    }


    float GetMaxHealth() const
    {
        return mMaxHealth;
    }

public:

    void RestoreFromSave(const XMFLOAT3& position, float health);

private:

    //============================================================
    // Standing / Crouching Collider切り替え
    //============================================================
    void ApplyColliderStance(bool crouching);

    //============================================================
    // 頭上に障害物がなく、立ち上がれるか
    //============================================================
    bool CanStandUp();

    //============================================================
    // Pickup取得確定時刻の監視
    //============================================================
    void UpdatePendingPickup();

    //============================================================
    // Pickup対象へ身体を向ける
    //============================================================
    void UpdatePickupFacing(float dt);

    //============================================================
    // Pickup位置合わせ更新
    //============================================================
    void UpdatePickupAlignment(float dt);

    //============================================================
    // 次の位置へ移動しても衝突しないか
    //============================================================
    bool CanMoveForPickup(const XMVECTOR& delta);


private:

    //========================================================
    // Player Modules
    //========================================================
    PlayerMovement mMovement;

    PlayerAnimationController mAnimation;

    PlayerInteraction mInteraction;

    PlayerFlashlight mFlashlight;

    PlayerNoiseEmitter mNoiseEmitter;

    //========================================================
    // Collider
    //========================================================
    std::unique_ptr<CapsuleCollider> mCollider;

    //============================================================
    // World Collider一覧
    // Scene側が所有しているColliderへの非所有参照
    //============================================================
    const std::vector<Collider*>* mWorldColliders = nullptr;

    //========================================================
    // State
    //========================================================
    bool mControlLocked = false;

    bool mWasFirstPersonCamera = false;

    //========================================================
    // Temporary
    //========================================================
    bool mInteractRequested = false;

    //============================================================
    // 取得待ちWorldItem
    //============================================================
    WorldItemPickup* mPendingPickup = nullptr;

    //============================================================
    // 取得待ちを開始したAction種類
    //============================================================
    PlayerActionState mPendingPickupAction = PlayerActionState::None;

    //============================================================
    // Inventory追加を実行する再生時刻
    //============================================================
    float mPendingPickupCommitTime = 0.0f;

private:

    //========================================================
    // Health
    //========================================================
    float mMaxHealth = 100.0f;
    float mHealth = 100.0f;

    //========================================================
    // Damage Invincible
    //========================================================
    float mDamageInvincibleTimer = 0.0f;

    float mDamageInvincibleDuration = 0.75f;

    //========================================================
    // Dead
    //========================================================
    bool mDead = false;

    //============================================================
    // Flashlight Attachment
    //============================================================

    // アタッチ先BoneのIndex
    int mFlashlightBoneIndex = -1;

    // Sceneが所有するライトモデルへの非所有参照
    ModelBase* mFlashlightVisualModel = nullptr;

    //============================================================
    // Pickup Facing
    //============================================================

    // Pickup開始時に保存したアイテムのWorld Position
    XMFLOAT3 mPendingPickupTargetPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };

    //============================================================
    // Pickup時の回転速度
    // 値が大きいほど素早く対象物の方を向く
    //============================================================
    float mPickupRotationSharpness = 14.0f;

    //============================================================
    // Pickup Alignment
    //============================================================

    // 現在 取得位置へ移動中か
    bool mPickupAligning = false;

    //アイテム取得が成功したか
    bool mPickupCommitted = false;

    // 位置合わせに経過した時間
    float mPickupAlignElapsed = 0.0f;

    // アイテムから停止する距離
    float mPickupStopDistance = 0.80f;

    // 位置合わせ中の移動速度
    float mPickupAlignSpeed = 2.4f;
    float mPickupAlignCrouchSpeed = 1.2f;

    // 位置合わせの最大時間
    float mPickupAlignMaxTime = 1.25f;

    // 到着判定の誤差
    float mPickupAlignTolerance = 0.05f;

    // ゲーム開始時は1人称視点から始める
    ExplorationViewMode mViewMode = ExplorationViewMode::FirstPerson;

    //============================================================
    // FirstPersonView Parameter
    //============================================================
    bool mFirstPersonViewReady = false;
    float mSmoothedEyeHeight = 1.60f;
    float mCameraMotionY = 0.0f;
    float mCameraMotionPitch = 0.0f; // ラジアン
    XMFLOAT3 mFlashlightAim = { 0.0f, 0.0f, 1.0f };
};