#pragma once

#include "../../../interaction/IInteractable.h"
#include "../save/atmosphericEscapeSaveData.h"


class vnScene;
class InteractionSystem;
class ExplorationPlayer;
class InventoryManager;


//============================================================
// WorldItemPickup
// World上に配置されたInventory Item
// 対応用途:
// ・Key
// ・Puzzle Item
// ・Document
// ・Consumable
// Placement:
// Id: World上のPersistent ID
// DataId: Inventory Item ID
// Save:
// State = 0 World上に存在
// State = 1 取得済み
//============================================================
class WorldItemPickup : public IInteractable, public IPersistentWorldObject
{
public:

    WorldItemPickup() = default;

    ~WorldItemPickup() override;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const std::string& itemId,
        const XMFLOAT3& position,
        const XMFLOAT3& scale
    );


    //========================================================
    // Terminate
    //========================================================
    void Terminate(vnScene* scene,InteractionSystem* interactionSystem);


    //========================================================
    // Inventory
    //========================================================
    void SetInventoryManager(InventoryManager* inventoryManager);


    //========================================================
    // State
    //========================================================
    bool IsCollected() const
    {
        return mCollected;
    }


    const std::string& GetItemId() const
    {
        return mItemId;
    }

    //============================================================
    // Pickup取得確定
    // 指定されたPickupアニメーション時刻になったときに
    // ExplorationPlayerから呼び出される
    // Inventoryへの追加はここで初めて行う
    //============================================================
    bool CommitPickup(ExplorationPlayer& player);

    //============================================================
    // Pickup取得予約のキャンセル
    // 被弾・死亡・ロード等で中断されたときに使用する
    //============================================================
    void CancelPendingPickup();

public:

    //========================================================
    // IInteractable
    //========================================================
    XMVECTOR GetInteractionPosition() const override;

    bool CanInteract() const override;

    void Interact(ExplorationPlayer& player) override;

    const wchar_t* GetInteractionText() const override;

    bool CanInteractFrom(const ExplorationPlayer& player) const override;
public:

    //========================================================
    // IPersistentWorldObject
    //========================================================
    const std::string& GetPersistentId() const override
    {
        return mPersistentId;
    }

    WorldObjectSaveState CaptureSaveState() const override;

    void RestoreSaveState(const WorldObjectSaveState& data) override;

    //========================================================
    // Save Load用
    //========================================================
    void SetCollectedImmediate(bool collected);


private:

    //========================================================
    // Inventory情報からInteraction Textを再構築
    //========================================================
    void RefreshInteractionText();


private:

    ModelBase* mModel = nullptr;

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
    // World Object ID
    //========================================================
    std::string mPersistentId;


    //========================================================
    // Inventory Item ID
    //========================================================
    std::string mItemId;

    //========================================================
    // Inventory
    //========================================================
    InventoryManager* mInventoryManager = nullptr;

    bool mCollected = false;

    //============================================================
    // Pickup Pending State
    // true: 取得モーション開始済み
    //============================================================
    bool mPickupPending = false;

    //============================================================
    // 取得を開始したPlayer
    //============================================================
    ExplorationPlayer* mPendingPlayer = nullptr;

    //========================================================
    // UI Text
    //========================================================
    std::wstring mInteractionText = L"Take Item";
};