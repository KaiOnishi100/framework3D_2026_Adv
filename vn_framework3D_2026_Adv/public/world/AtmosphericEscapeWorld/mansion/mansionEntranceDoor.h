#pragma once

#include "../../../interaction/IInteractable.h"


class vnScene;
class InteractionSystem;
class ExplorationPlayer;
class NoiseSystem;


//============================================================
// MansionEntranceDoor
// 屋敷正面玄関
// ゲーム開始時:
//      Open
// Playerが屋敷へ入る:
//      Open
//          ↓
//      Closing
//          ↓
//      Closed
//          ↓
//      Locked
//
// 終盤:
//      UnlockForEscape()
//          ↓
//      PlayerがInteract
//          ↓
//      Opening
//          ↓
//      Open
//============================================================
class MansionEntranceDoor : public IInteractable
{
public:

    //========================================================
    // Door State
    //========================================================
    enum class State
    {
        Closed,
        Opening,
        Open,
        Closing
    };


public:

    MansionEntranceDoor() = default;

    ~MansionEntranceDoor() override = default;


    //========================================================
    // Initialize
    // position: Doorが閉じている位置
    // scale: CSVから取得したDoorサイズ
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const XMFLOAT3& position,
        const XMFLOAT3& scale
    );


    //========================================================
    // Terminate
    //========================================================
    void Terminate(
        vnScene* scene,
        InteractionSystem* interactionSystem
    );


    //========================================================
    // Update
    //========================================================
    void Update(float dt);

    //============================================================
    // セーブデータの進行状況から玄関ドアの状態を即時復元する
    // enteredMansion:
    //   true  = 既に屋敷へ入った
    //   false = ゲーム開始前の開いた玄関
    // escapeUnlocked: true  = 最終脱出中で玄関が解錠されている
    //============================================================
    void RestoreFromProgress(
        bool enteredMansion,
        bool escapeUnlocked
    );

    //========================================================
    // Intro Event
    // PlayerがEntranceHallへ入った瞬間に呼ぶ
    // Doorを閉じ始め
    // 閉じ切った瞬間に自動で施錠する
    //========================================================
    bool BeginCloseAndLock();


    //========================================================
    // Final Escape
    // 終盤に玄関を開けられるようにする
    //========================================================
    void UnlockForEscape();


    //========================================================
    // State
    //========================================================
    State GetState() const
    {
        return mState;
    }


    bool IsClosed() const
    {
        return mState == State::Closed;
    }


    bool IsOpen() const
    {
        return mState == State::Open;
    }


    bool IsLocked() const
    {
        return mLocked;
    }


    //============================================================
    // 玄関の左右2枚のColliderを登録
    //============================================================
    void AppendColliders(std::vector<Collider*>& destination) const
    {
        for (const auto& collider : mLeafColliders)
        {
            if (collider != nullptr)
            {
                destination.push_back(collider.get());
            }
        }
    }

    //========================================================
    // Noise
    //========================================================
    void SetNoiseSystem(NoiseSystem* noiseSystem)
    {
        mNoiseSystem = noiseSystem;
    }

    // World の初期化時に、開扉開始の通知先を設定する。
    void SetOpenedCallback(std::function<void(const XMFLOAT3&)> callback)
    {
        mOpenedCallback = std::move(callback);
    }

public:

    //========================================================
    // IInteractable
    //========================================================
    XMVECTOR GetInteractionPosition() const override;

    bool CanInteract() const override;

    void Interact(ExplorationPlayer& player) override;

    const wchar_t* GetInteractionText() const override;


private:

    //========================================================
    // Opening開始
    //========================================================
    bool BeginOpening();

    //============================================================
    // モデルとColliderのTransform更新
    //============================================================
    void ApplyLeafTransforms();

    //============================================================
    // プレイヤーが扉に接触した通知
    //============================================================
    void OnPlayerContact(const Collider* other);


private:


    //============================================================
    // Main Entrance Double Door
    // index 0 = 左扉
    // index 1 = 右扉
    //============================================================
    std::array<ModelBase*, 2> mLeafModels = { nullptr, nullptr };

    //============================================================
    // 各ドア板に対応したOBB Collider
    //============================================================
    std::array<std::unique_ptr<BoxCollider>, 2> mLeafColliders;

    //============================================================
    // ドア全体の幅から計算した、片扉の幅
    //============================================================
    float mLeafWidth = 1.5f;

    //============================================================
    // 現在の開閉角度
    // 0度  = 閉じている
    // 85度 = 開いている
    // 左扉には正の角度、右扉には負の角度を適用する
    //============================================================
    float mCurrentAngle = XMConvertToRadians(85.0f);
    float mMaxOpenAngle = XMConvertToRadians(85.0f);

    //============================================================
    // 開閉速度
    // 約1.4秒で完全に開閉する
    //============================================================
    float mAngularSpeed = XMConvertToRadians(60.0f);


    //========================================================
    // Door Transform
    //========================================================
    XMFLOAT3 mClosedPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    XMFLOAT3 mOpenPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    XMFLOAT3 mInteractionPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    XMFLOAT3 mScale =
    {
        1.0f,
        1.0f,
        1.0f
    };


    //========================================================
    // State
    //========================================================
    State mState = State::Open;

    bool mLocked = false;

    //========================================================
    // Intro中はPlayer操作を禁止する
    //========================================================
    bool mIntroControlled = true;

    //========================================================
    // Closing完了時に施錠するか
    //========================================================
    bool mLockWhenClosed = false;

    //========================================================
    // Door Animation
    //========================================================
    float mMoveSpeed = 1.8f;


    // 左右の隙間を埋めるために増やす合計幅
    float mVisualWidthExtra = 0.14f;

    // ドア上部の隙間を埋めるために増やす高さ
    float mVisualHeightExtra = 0.23f;

    //========================================================
    // ID
    // Save対応するため保持
    //========================================================
    std::string mPersistentId;

    //========================================================
    // Noise
    //========================================================
    NoiseSystem* mNoiseSystem = nullptr;

    float mDoorNoiseRadius = 8.0f;

    std::function<void(const XMFLOAT3&)> mOpenedCallback;
};