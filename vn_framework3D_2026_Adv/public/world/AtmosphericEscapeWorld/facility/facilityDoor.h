#pragma once

#include "../../../interaction/IInteractable.h"
#include "../../AtmosphericEscapeWorld/save/atmosphericEscapeSaveData.h"
#include "../../../interface/INavigationDoor.h"

class vnScene;
class InteractionSystem;
class NoiseSystem;

//============================================================
// FacilityDoor
// Atmospheric Escape
// 施設入口のスライドドア
// ・IInteractableを実装
// ・PlayerがEキーで操作
// ・閉状態から横方向へスライド
// ・BoxColliderもModel Positionへ追従
//============================================================
class FacilityDoor : public IInteractable, public IPersistentWorldObject, public INavigationDoor
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

    FacilityDoor() = default;

    ~FacilityDoor() override = default;


    //========================================================
    // Initialize
    // position: Doorが閉じているときの中心位置
    // interactionSystem: Interaction対象として自身を登録する
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const XMFLOAT3& position
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
    // Door Animation
    //========================================================
    void Update(float dt);


    //========================================================
    // Collider
    // Collider所有権はFacilityDoor側
    // Scene側には非所有Pointerだけ渡す
    //========================================================
    Collider* GetCollider() const
    {
        return mCollider.get();
    }


    //========================================================
    // State
    //========================================================
    State GetState() const
    {
        return mState;
    }

    //============================================================
    // Lock
    //============================================================
    void SetLocked(bool locked)
    {
        //========================================================
        // ※すでに開き始めているDoorを
        // ※後からLockedへ戻すことはしない
        //========================================================
        if (mState != State::Closed)
        {
            return;
        }

        mLocked = locked;
    }


    bool IsLocked() const
    {
        return mLocked;
    }

    //============================================================
    // Close可能設定
    // false: 一度開いたら閉じない
    // true:  PlayerがOpen / Closeを切り替えられる
    //============================================================
    void SetCanClose(bool canClose)
    {
        mCanClose = canClose;
    }


    //============================================================
    // Closed判定
    //============================================================
    bool IsClosed() const
    {
        return mState == State::Closed;
    }

    //============================================================
    // Noise System
    //============================================================
    void SetNoiseSystem(NoiseSystem* noiseSystem)
    {
        mNoiseSystem = noiseSystem;
    }

    //============================================================
    // INavigationDoor
    //============================================================
    bool IsOpen() const override
    {
        return mState == State::Open;
    }

    bool IsPassableForAI() const
    {
        if (IsOpen())
        {
            return true;
        }
        return !mLocked;
    }

    //============================================================
    // Door固定位置
    //============================================================
    const XMFLOAT3& GetDoorPosition() const override
    {
        return mInteractionPosition;
    }


    //============================================================
    // AI Opening Request
    //============================================================
    bool RequestOpenFromAI() override;

public:

    //========================================================
    // IInteractable
    //========================================================

    //========================================================
    // Interaction判定位置
    // ※Doorが開いて横へ移動しても
    // ※入口の位置でInteraction判定したいため
    // ※Model位置ではなく閉状態の入口中央を返す
    //========================================================
    XMVECTOR GetInteractionPosition() const override;


    //========================================================
    // Closedのときだけ操作可能
    //========================================================
    bool CanInteract() const override;


    //========================================================
    // Eキーで実行される
    //========================================================
    void Interact(ExplorationPlayer& player) override;


    //========================================================
    // UI表示用
    //========================================================
    const wchar_t* GetInteractionText() const override;

private:

    //============================================================
    // Begin Opening
    // Player / AI共通のDoor Open開始処理
    //============================================================
    bool BeginOpening();

    //============================================================
    // Closing開始
    //============================================================
    bool BeginClosing();

private:

    //========================================================
    // セーブ関連
    //========================================================
    const std::string& GetPersistentId() const
    {
        return mPersistentId;
    }

    WorldObjectSaveState CaptureSaveState() const;

    void RestoreSaveState(const WorldObjectSaveState& data);

    void SetOpenImmediate();

    void SetClosedImmediate();

private:

    //========================================================
    // Model
    //========================================================
    ModelBase* mModel = nullptr;


    //========================================================
    // Collision
    //========================================================
    std::unique_ptr<BoxCollider> mCollider;


    //========================================================
    // State
    //========================================================
    State mState = State::Closed;


    //========================================================
    // Closed Position
    //========================================================
    XMFLOAT3 mClosedPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    //========================================================
    // Open Position
    // Doorを右側の壁内部へ収納する位置
    //========================================================
    XMFLOAT3 mOpenPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    //========================================================
    // Interaction Position
    // Doorが移動しても入口中央に固定する
    //========================================================
    XMFLOAT3 mInteractionPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    //========================================================
    // Door Parameters
    //========================================================

    // Doorが横へ動く速度 [m/s]
    float mOpenSpeed = 1.8f;

    // Doorが横へ移動する距離 [m]
    float mOpenDistance = 1.95f;

    //============================================================
    // Lock State
    //============================================================
    bool mLocked = false;

    //============================================================
    // このDoorを再び閉められるか
    //============================================================
    bool mCanClose = false;

    //============================================================
    // Noise System
    //============================================================
    NoiseSystem* mNoiseSystem = nullptr;

    //============================================================
    // Door Noise Parameter
    //============================================================
    float mOpenNoiseRadius = 7.0f;

    //========================================================
    // Save用固有ID
    //========================================================
    std::string mPersistentId;
};