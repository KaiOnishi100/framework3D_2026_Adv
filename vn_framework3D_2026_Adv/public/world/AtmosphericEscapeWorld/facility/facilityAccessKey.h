#pragma once

#include "../../../interaction/IInteractable.h"

class vnScene;
class InteractionSystem;
class ExplorationPlayer;

//============================================================
// FacilityAccessKey
// Security Area内に置かれるKey Item
// ・IInteractable
// ・PlayerがEキーで取得
// ・取得後はModelを非表示
// ・再取得不可
//============================================================
class FacilityAccessKey : public IInteractable, public IPersistentWorldObject
{
public:

    FacilityAccessKey() = default;

    ~FacilityAccessKey() override = default;


    //========================================================
    // Initialize
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
    // State
    //========================================================
    bool IsCollected() const
    {
        return mCollected;
    }


public:

    //========================================================
    // IInteractable
    //========================================================
    XMVECTOR GetInteractionPosition() const override;

    bool CanInteract() const override;

    void Interact(ExplorationPlayer& player) override;

    const wchar_t* GetInteractionText() const override;

    const std::string& GetPersistentId() const
    {
        return mPersistentId;
    }

    WorldObjectSaveState CaptureSaveState() const;

    void RestoreSaveState(const WorldObjectSaveState& data);

    void SetCollectedImmediate(bool collected);

private:

    ModelBase* mModel = nullptr;

    XMFLOAT3 mPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    bool mCollected = false;

    std::string mPersistentId;
};