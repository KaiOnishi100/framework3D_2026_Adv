#pragma once

//============================================================
// MansionControlTerminal
// 別館地下 Control Room に配置する操作端末
//     Inactive
//        ↓ Interact
//     Activated
//============================================================
class MansionControlTerminal : public IInteractable, public IPersistentWorldObject
{
public:

    //========================================================
    // State
    //========================================================
    enum class State
    {
        Inactive = 0,
        Activated
    };


public:

    MansionControlTerminal() = default;

    ~MansionControlTerminal() override = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const XMFLOAT3& position,
        const XMFLOAT3& scale,
        const XMFLOAT3& rotation
    );


    //========================================================
    // Terminate
    //========================================================
    void Terminate(
        vnScene* scene,
        InteractionSystem* interactionSystem
    );


    //========================================================
    // State
    //========================================================
    bool IsActivated() const
    {
        return mState == State::Activated;
    }


    //========================================================
    // IInteractable
    //========================================================
    XMVECTOR GetInteractionPosition() const override;

    bool CanInteract() const override
    {
        return true;
    }

    void Interact(ExplorationPlayer& player) override;

    const wchar_t* GetInteractionText() const override;

    //========================================================
    // IPersistentWorldObject
    //========================================================
    const std::string& GetPersistentId() const override
    {
        return mPersistentId;
    }

    WorldObjectSaveState CaptureSaveState() const override;

    void RestoreSaveState(const WorldObjectSaveState& state) override;


private:

    //========================================================
    // State変更を一か所へ集約する
    // Gameplay: Interact()
    // Save Load: RestoreSaveState()
    //========================================================
    void SetActivated(bool activated);


private:

    //========================================================
    // Persistent ID
    //========================================================
    std::string mPersistentId;


    //========================================================
    // Runtime Model
    //========================================================
    ModelBase* mModel = nullptr;


    //========================================================
    // Transform
    //========================================================
    XMFLOAT3 mPosition =
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
    // Current State
    //========================================================
    State mState = State::Inactive;


    //========================================================
    // Runtime初期化済みか
    //========================================================
    bool mInitialized = false;
};