#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "documentPickup.h"

//============================================================
// Initialize
//============================================================
bool DocumentPickup::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const std::string& documentId,
    const XMFLOAT3& position,
    const XMFLOAT3& scale
)
{
    //========================================================
    // Safety
    //========================================================
    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }


    if (persistentId.empty() || documentId.empty())
    {
        return false;
    }


    //========================================================
    // Data
    //========================================================
    mPersistentId = persistentId;
    mDocumentId = documentId;
    mPosition = position;
    mCollected = false;

    //========================================================
    // Model
    //========================================================
    mModel = new ModelBase(L"data/model/mansion/dynamic/pickup/", L"pickup_document_01.vnm");

    if (mModel == nullptr)
    {
        return false;
    }

    mModel->setPosition(
        position.x,
        position.y,
        position.z
    );


    mModel->setScale(
        scale.x / 0.45f,
        scale.y / 0.06f,
        scale.z / 0.30f
    );

    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);
    scene->registerObject(mModel);

    //========================================================
    // Interaction
    //========================================================
    interactionSystem->Register(this);

    return true;
}

//============================================================
// Interaction Text
//============================================================
void DocumentPickup::RefreshInteractionText()
{
    //========================================================
    // Manager未設定時のFallback
    //========================================================
    mInteractionText = L"Read Document";

    if (mDocumentManager == nullptr)
    {
        return;
    }

    //========================================================
    // DOC_001などからDocument Definitionを取得
    //========================================================
    const DocumentDefinition* definition = mDocumentManager->FindDefinition(mDocumentId);

    if (definition == nullptr)
    {
        return;
    }

    //========================================================
    // Interaction Prompt
    //========================================================
    mInteractionText = L"Read ";

    mInteractionText += definition->Title;
}


//============================================================
// Terminate
//============================================================
void DocumentPickup::Terminate(vnScene* scene, InteractionSystem* interactionSystem)
{
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }


    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);
        mModel = nullptr;
    }

    mDocumentManager = nullptr;

    mCollected = false;
}


void DocumentPickup::SetCollectedVisualState()
{
    if (mModel != nullptr)
    {
        //========================================================
        // 拾った紙をWorldから見えなくする
        //========================================================
        mModel->setRenderEnable(!mCollected);
        mModel->setExecuteEnable(!mCollected);
        mModel->setCastShadow(!mCollected);
    }
}

//============================================================
// Interaction Position
//============================================================
XMVECTOR DocumentPickup::GetInteractionPosition() const
{
    return
        XMVectorSet(
            mPosition.x,
            mPosition.y,
            mPosition.z,
            0.0f
        );
}


//============================================================
// Can Interact
//============================================================
bool DocumentPickup::CanInteract() const
{
    //========================================================
    // 取得済みならInteraction不可
    //========================================================
    if (mCollected)
    {
        return false;
    }

    //========================================================
    // DocumentManager未接続ならInteraction不可
    //========================================================
    if (mDocumentManager == nullptr)
    {
        return false;
    }

    //========================================================
    // Documents.csvに存在するDocumentだけInteraction可能
    //========================================================
    return mDocumentManager->FindDefinition(mDocumentId) != nullptr;
}


//============================================================
// Interact
//============================================================
void DocumentPickup::Interact(ExplorationPlayer& player)
{
    //============================================================
    // 既に取得済み
    //============================================================
    if (mCollected)
    {
        return;
    }


    //============================================================
    // Manager未設定
    //============================================================
    if (mDocumentManager == nullptr)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[DocumentPickup] "
            L"DocumentManager is null.\n"
        );

#endif

        return;
    }


    //============================================================
    // DocumentDefinitionが存在するか確認
    // Documents.csvにないDataIdなら取得させない
    //============================================================
    const DocumentDefinition* definition =
        mDocumentManager->FindDefinition(mDocumentId);


    if (definition == nullptr)
    {
#if defined(_DEBUG)

        OutputDebugStringA(("[DocumentPickup] " "Unknown Document ID : " + mDocumentId + "\n").c_str());

#endif

        return;
    }


    //============================================================
    // DocumentManagerへ取得登録
    // true: 今回初取得
    // false: 既に取得済み
    //============================================================
    const bool collected = mDocumentManager->CollectDocument(mDocumentId);

    //============================================================
    // 既にManager上取得済みの場合も
    // World側Pickupは消してよい
    //============================================================
    if (!collected && !mDocumentManager->IsCollected(mDocumentId))
    {
        return;
    }


    mCollected = true;


    //============================================================
    // World上のDocumentを無効化
    //============================================================
    SetCollectedVisualState();

    player.NotifyPickupCommitted();

    if (collected && mDocumentCollectedCallback)
    {
        mDocumentCollectedCallback(mDocumentId);
    }


#if defined(_DEBUG)

    std::wstring debugMessage =
        L"[DocumentPickup] Collected : ";

    debugMessage += definition->Title;
    debugMessage += L"\n";
    OutputDebugStringW(debugMessage.c_str());

#endif
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* DocumentPickup::GetInteractionText() const
{
    if (mCollected)
    {
        return L"";
    }

    return mInteractionText.c_str();
}


//============================================================
// Save
//============================================================
WorldObjectSaveState DocumentPickup::CaptureSaveState() const
{
    WorldObjectSaveState data;

    data.Id = mPersistentId;

    data.Type = PersistentObjectType::Pickup;

    //========================================================
    // 0 = World上
    // 1 = 取得済み
    //========================================================
    data.State = mCollected ? 1u : 0u;

    return data;
}


//============================================================
// Load
//============================================================
void DocumentPickup::RestoreSaveState(const WorldObjectSaveState& data)
{
    if (data.Id != mPersistentId)
    {
        return;
    }

    SetCollectedImmediate(data.State == 1u);
}


//============================================================
// Immediate State
//============================================================
void DocumentPickup::SetCollectedImmediate(bool collected)
{
    mCollected = collected;

    if (mCollected && mDocumentManager != nullptr)
    {
        mDocumentManager->CollectDocument(mDocumentId);
    }

    SetCollectedVisualState();
}