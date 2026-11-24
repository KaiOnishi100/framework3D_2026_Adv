#pragma once

#include "../../../interaction/IInteractable.h"


class vnScene;
class InteractionSystem;
class ExplorationPlayer;
class InventoryManager;


//============================================================
// MansionFuseBox
// 本館地下に配置するヒューズボックス
//============================================================
class MansionFuseBox : public IInteractable, public IPersistentWorldObject
{
public:

    //============================================================
    // State
    //============================================================
    enum class State
    {
        // ヒューズ未装着
        MissingFuse,

        // ヒューズ装着済み
        Activated
    };


public:

    MansionFuseBox() = default;

    ~MansionFuseBox() override = default;


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
    void Terminate(
        vnScene* scene,
        InteractionSystem* interactionSystem
    );


    //============================================================
    // Inventory
    //============================================================
    void SetInventoryManager(InventoryManager* inventoryManager)
    {
        mInventoryManager = inventoryManager;
    }


    //============================================================
    // Collider
    //============================================================
    Collider* GetCollider() const
    {
        return mCollider.get();
    }


    //============================================================
    // State
    //============================================================
    bool IsActivated() const
    {
        return mState == State::Activated;
    }

    //============================================================
    // 配電盤を一度でも調べたか
    // true: Playerが配電盤を調査済み
    //============================================================
    bool IsInspected() const
    {
        return mInspected;
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
    // Interact()を経由するとFuseを消費してしまうため
    // Load時は状態だけ直接復元する
    //============================================================
    void SetActivatedImmediate(bool activated);


private:

    //============================================================
    // Visual
    //============================================================
    void RefreshVisual();


private:

    //============================================================
    // Model
    //============================================================
    ModelBase* mModel = nullptr;

    //============================================================
    // Collider
    //============================================================
    std::unique_ptr<BoxCollider> mCollider;

    //============================================================
    // Inventory
    //============================================================
    InventoryManager* mInventoryManager = nullptr;

    //============================================================
    // Persistent ID
    //============================================================
    std::string mPersistentId;

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
    // Runtime State
    //============================================================
    State mState = State::MissingFuse;

    //============================================================
    // Required Item
    //============================================================
    const std::string mRequiredItemId = "Fuse";

    //============================================================
    // Fuse Box調査状態
    // Save / Load対象
    //============================================================
    bool mInspected = false;
};