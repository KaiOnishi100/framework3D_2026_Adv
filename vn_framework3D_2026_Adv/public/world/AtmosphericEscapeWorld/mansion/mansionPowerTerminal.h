#pragma once

#include "../../../interaction/IInteractable.h"
#include "../save/atmosphericEscapeSaveData.h"


class vnScene;
class InteractionSystem;
class ExplorationPlayer;
class NoiseSystem;


//============================================================
// MansionPowerTerminal
// 屋敷の主電源を復旧するためのTerminal
// 初期状態:
//      Ready
// Playerが操作:
//      Ready
//          ↓
//      Activated
// Activated後:
// ・再操作不可
// ・Emissive色変更
// ・Gameplay Noise発生
// ・Scene側が状態を検出して電源復旧Phaseへ進む
// Save対象
//============================================================
class MansionPowerTerminal : public IInteractable, public IPersistentWorldObject
{
public:

    //============================================================
    // Terminal State
    //============================================================
    enum class State
    {
        Ready,
        Activated
    };


public:

    MansionPowerTerminal() = default;

    ~MansionPowerTerminal() override = default;


    //============================================================
    // Initialize
    //============================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const XMFLOAT3& position,
        const XMFLOAT3& scale,
        const XMFLOAT3& rotation
    );


    //============================================================
    // Terminate
    //============================================================
    void Terminate(vnScene* scene, InteractionSystem* interactionSystem);


    //============================================================
    // Runtime State
    //============================================================
    bool IsActivated() const
    {
        return mState == State::Activated;
    }


    //============================================================
    // Collider
    //============================================================
    Collider* GetCollider() const
    {
        return mCollider.get();
    }


    //============================================================
    // Noise
    //============================================================
    void SetNoiseSystem(NoiseSystem* noiseSystem)
    {
        mNoiseSystem = noiseSystem;
    }

    void SetInteractionEnabled(bool enabled)
    {
        mInteractionEnabled = enabled;
    }

public:

    //============================================================
    // IInteractable
    //============================================================
    XMVECTOR GetInteractionPosition() const override;

    bool CanInteract() const override;

    void Interact(ExplorationPlayer& player) override;

    const wchar_t* GetInteractionText() const override;


public:

    //============================================================
    // IPersistentWorldObject
    //============================================================
    const std::string& GetPersistentId() const override
    {
        return mPersistentId;
    }


    WorldObjectSaveState CaptureSaveState() const override;

    void RestoreSaveState(const WorldObjectSaveState& data) override;


    //============================================================
    // Save Load用
    //============================================================
    void SetActivatedImmediate(bool activated);


private:

    //============================================================
    // 表示状態更新
    // Ready     : 赤系
    // Activated : 緑系
    //============================================================
    void UpdateVisualState();


private:

    ModelBase* mModel = nullptr;

    std::unique_ptr<BoxCollider> mCollider;


    //============================================================
    // Interaction Position
    //============================================================
    XMFLOAT3 mInteractionPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    //============================================================
    // State
    //============================================================
    State mState = State::Ready;


    //============================================================
    // Persistent ID
    //============================================================
    std::string mPersistentId;


    //============================================================
    // Noise
    //============================================================
    NoiseSystem* mNoiseSystem = nullptr;

    float mActivationNoiseRadius = 14.0f;

    bool mInteractionEnabled = true;
};