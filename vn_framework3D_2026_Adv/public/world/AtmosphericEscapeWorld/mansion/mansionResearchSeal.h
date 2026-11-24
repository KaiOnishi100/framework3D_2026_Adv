#pragma once

//============================================================
// MansionResearchSeal
// 別館地下 Central Research Chamber に配置する
// StoneEmblem専用Puzzle Object
//============================================================
class MansionResearchSeal : public IInteractable, public IPersistentWorldObject
{
public:

    //========================================================
    // Seal State
    //========================================================
    enum class State
    {
        MissingEmblem = 0,
        Activated
    };


public:

    MansionResearchSeal() = default;

    ~MansionResearchSeal() override = default;


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
    // Inventory
    //========================================================
    void SetInventoryManager(InventoryManager* inventory)
    {
        mInventory = inventory;
    }


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
    // Activated状態切替
    // GameplayとSave Loadの両方から呼ぶ
    //========================================================
    void SetActivated(bool activated);


private:

    //========================================================
    // Persistent ID
    //========================================================
    std::string mPersistentId;


    //========================================================
    // Inventory
    //========================================================
    InventoryManager* mInventory = nullptr;


    //========================================================
    // Runtime Model
    //========================================================
    ModelBase* mModel = nullptr;


    //========================================================
    // State
    //========================================================
    State mState = State::MissingEmblem;


    //========================================================
    // World Position
    //========================================================
    XMFLOAT3 mPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };

    //========================================================
    // CSV Scale
    //========================================================
    XMFLOAT3 mScale =
    {
        1.0f,
        1.0f,
        1.0f
    };

    //========================================================
    // Initialize済みFlag
    //========================================================
    bool mInitialized = false;
};