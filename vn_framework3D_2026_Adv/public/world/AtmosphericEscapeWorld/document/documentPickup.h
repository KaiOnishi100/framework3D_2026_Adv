#pragma once

#include "../../../interaction/IInteractable.h"
#include "../save/atmosphericEscapeSaveData.h"
#include "documentManager.h"


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
class DocumentPickup : public IInteractable, public IPersistentWorldObject
{
public:

    DocumentPickup() = default;

    ~DocumentPickup() override = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const std::string& documentId,
        const XMFLOAT3& position,
        const XMFLOAT3& scale
    );


    //========================================================
    // Terminate
    //========================================================
    void Terminate(vnScene* scene, InteractionSystem* interactionSystem);

    //========================================================
    // State
    //========================================================
    bool IsCollected() const
    {
        return mCollected;
    }

    //============================================================
    // Document ID
    //============================================================
    const std::string& GetDocumentId() const
    {
        return mDocumentId;
    }


public:

    //============================================================
    // Document Open Callback
    // PlayerがWorld上のDocumentを
    // 今回初めて取得した場合のみ通知する
    //============================================================
    using DocumentCollectedCallback = std::function<void(const std::string& documentId)>;

    void SetDocumentCollectedCallback(DocumentCollectedCallback callback)
    {
        mDocumentCollectedCallback = std::move(callback);
    }

    //============================================================
    // DocumentManager設定
    //============================================================
    void SetDocumentManager(DocumentManager* documentManager)
    {
        mDocumentManager = documentManager;
        RefreshInteractionText();
    }

    void SetCollectedVisualState();

public:

    //========================================================
    // IInteractable
    //========================================================
    XMVECTOR GetInteractionPosition() const override;

    bool CanInteract() const override;

    const wchar_t* GetInteractionText() const override;

    void Interact(ExplorationPlayer& player) override;


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

    bool mCollected = false;

    //========================================================
    // UI Text
    //========================================================
    std::wstring mInteractionText = L"Take Item";

private:

    //============================================================
    // Document ID
    //============================================================
    std::string mDocumentId;

    //============================================================
    // Document Manager
    // DocumentDefinition検索と取得状態管理に使用する
    //============================================================
    DocumentManager* mDocumentManager = nullptr;

    //============================================================
    // Document取得通知
    // ※Save LoadによるRestoreでは呼ばない
    // ※実際のPlayer Interaction時だけ呼ぶ
    //============================================================
    DocumentCollectedCallback mDocumentCollectedCallback;

};