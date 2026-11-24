#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "worldItemPickup.h"

WorldItemPickup::~WorldItemPickup()
{
    if (mPendingPlayer != nullptr)
    {
        mPendingPlayer->CancelWorldItemPickup(this);
    }
}

//============================================================
// Initialize
//============================================================
bool WorldItemPickup::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const std::string& itemId,
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

    if (persistentId.empty() || itemId.empty())
    {
        return false;
    }

    //========================================================
    // Data
    //========================================================
    mPersistentId = persistentId;
    mItemId = itemId;
    mPosition = position;
    mCollected = false;

    //========================================================
    // Pickup Pending State
    // 実際の取得予約はInteract()で設定する
    //========================================================
    mPickupPending = false;
    mPendingPlayer = nullptr;

    //========================================================
    // STEP45 - Item Model Selection
    // CSVのDataId（Inventory Item ID）に応じて
    // 読み込むVNMを切り替える
    // BasementKey -> 古い鉄の鍵
    // AnnexKey    -> 真鍮の鍵
    // Fuse        -> ヒューズ
    // StoneEmblem -> 石製の紋章
    // MansionNote -> 手掛かりメモ
    //========================================================

    const wchar_t* modelFolder = L"data/model/mansion/dynamic/pickup/";

    const wchar_t* modelFile = nullptr;

    //========================================================
    // VNMの基準サイズ
    //========================================================
    XMFLOAT3 modelBaseSize =
    {
        0.35f,
        0.08f,
        0.20f
    };

    //========================================================
    // 本館地下の鍵
    //========================================================
    if (itemId == "BasementKey")
    {
        modelFile = L"pickup_basement_key_01.vnm";
    }

    //========================================================
    // 別館入口の鍵
    //========================================================
    else if (itemId == "AnnexKey")
    {
        modelFile = L"pickup_annex_key_01.vnm";
    }

    //========================================================
    // 本館地下のヒューズ
    //========================================================
    else if (itemId == "Fuse")
    {
        modelFile = L"pickup_fuse_01.vnm";
    }

    //========================================================
    // 別館地下のStone Emblem
    //========================================================
    else if (itemId == "StoneEmblem")
    {
        modelFile = L"pickup_stone_emblem_01.vnm";
    }

    //========================================================
    // 本館の手掛かりメモ
    // 2種類とも共通のVNMを使用する
    // 内容の違いはInventory / Document側で管理する
    //========================================================
    else if (itemId == "MansionNote01" || itemId == "MansionNote02" )
    {
        modelFile = L"pickup_clue_note_01.vnm";

        //====================================================
        // メモ用の基準サイズ
        //====================================================
        modelBaseSize =
        {
            0.45f,
            0.06f,
            0.30f
        };
    }

    //========================================================
    // 未対応Item
    // Cubeへフォールバックする
    //========================================================
    else
    {
        modelFolder = L"data/model/primitive/";
        modelFile = L"cube.vnm";

        //====================================================
        // Cubeは従来のFull Size指定を使用する
        // 専用VNM用の基準サイズでは割らない
        //====================================================
        modelBaseSize =
        {
            1.0f,
            1.0f,
            1.0f
        };
    }

    //========================================================
    // 専用モデルかどうか
    //========================================================
    const bool useDetailedModel =
        itemId == "BasementKey" ||
        itemId == "AnnexKey" ||
        itemId == "Fuse" ||
        itemId == "StoneEmblem" ||
        itemId == "MansionNote01" ||
        itemId == "MansionNote02";

    //========================================================
    // Model Path Check
    // VNMのパス間違いを検出する
    // 未対応ItemのCubeについても確認する
    //========================================================
    const std::filesystem::path modelPath =
        std::filesystem::path(modelFolder) / modelFile;

    if (!std::filesystem::is_regular_file(modelPath))
    {
#if defined(_DEBUG)

        const std::wstring message =
            L"[WorldItemPickup] VNM not found : " +
            modelPath.wstring() +
            L"\n";

        OutputDebugStringW(message.c_str());

#endif

        return false;
    }

    //========================================================
    // VNM Model生成
    //========================================================
    mModel = new ModelBase(modelFolder, modelFile);

    if (mModel == nullptr)
    {
        return false;
    }

    //========================================================
    // Position
    //========================================================
    mModel->setPosition(
        position.x,
        position.y,
        position.z
    );

    //========================================================
    // Scale
    //========================================================
    if (useDetailedModel)
    {
        //====================================================
        // 専用VNM
        //====================================================
        mModel->setScale(
            scale.x / modelBaseSize.x,
            scale.y / modelBaseSize.y,
            scale.z / modelBaseSize.z
        );
    }
    else
    {
        //====================================================
        // 従来のCube
        //====================================================
        mModel->setScale(
            scale.x,
            scale.y,
            scale.z
        );
    }

    //========================================================
    // Material
    //========================================================
    if (!useDetailedModel)
    {
        //====================================================
        // 未対応Itemのみ仮Materialを使用
        //====================================================
        mModel->SetPBREnabled(true);

        mModel->setDiffuse(
            0.62f,
            0.54f,
            0.38f,
            1.0f
        );

        mModel->SetMetallic(0.0f);
        mModel->SetRoughness(0.85f);

        //====================================================
        // Cubeのみ弱く発光
        //====================================================
        mModel->SetEmissive(
            0.25f,
            0.18f,
            0.08f,
            0.8f
        );
    }

    //========================================================
    // Shadow
    //========================================================
    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);

    //========================================================
    // Scene登録
    // ※WorldItemPickup自身はIInteractableだが
    // ※描画モデルはvnSceneへ登録する必要がある
    //========================================================
    scene->registerObject(mModel);

    //========================================================
    // Interaction登録
    //========================================================
    interactionSystem->Register(this);

#if defined(_DEBUG)

    //========================================================
    // Debug Log
    // どのItem IDがどのVNMを読み込んだか確認する
    //========================================================
    const std::string debugMessage =
        "[WorldItemPickup] Initialized : " +
        itemId +
        "\n";

    OutputDebugStringA(debugMessage.c_str());

#endif

    return true;
}


//============================================================
// Inventory Manager
//============================================================
void WorldItemPickup::SetInventoryManager(InventoryManager* inventoryManager)
{
    mInventoryManager = inventoryManager;

    RefreshInteractionText();
}


//============================================================
// Commit Pickup
// アニメーションが取得タイミングに達したとき
// Inventoryへ追加してWorldからアイテムを消す
//============================================================
bool WorldItemPickup::CommitPickup(ExplorationPlayer& player)
{
    //========================================================
    // 正しいPlayerからの取得確定要求か
    //========================================================
    if (!mPickupPending || mPendingPlayer != &player || mCollected || mInventoryManager == nullptr)
    {
        return false;
    }

    //========================================================
    // 取得待ち状態を解除
    // AddItem失敗時にも再度Interactできるようにする
    //========================================================
    CancelPendingPickup();

    //========================================================
    // 予約後に段差などで高さが変わった場合も、取得直前に確認する
    // 予約は直前で解除済みなので、失敗しても近づいて再操作できる
    //========================================================

    if (!player.IsPickupWithinVerticalReach(GetInteractionPosition()))
    {
        return false;
    }

    //========================================================
    // 実際にInventoryへ追加
    //========================================================
    const bool added =
        mInventoryManager->AddItem(mItemId, 1);

    if (!added)
    {
        //====================================================
        // 所持上限などで失敗した場合
        // World上のItemは消さない
        //====================================================
        return false;
    }

    //========================================================
    // Inventoryへの追加が成功
    // ここで初めてアイテムを非表示にする
    //========================================================
    SetCollectedImmediate(true);


    auto* sound = SoundBank::getInstance();

    if (mItemId == "Fuse")
    {
        sound->PlaySfx(SoundID::AE_FusePickup, 0.60f);
    }
    else if (mItemId == "StoneEmblem")
    {
        sound->PlaySfx(SoundID::AE_StoneEmblemPickup, 0.60f);
    }
    else
    {
        sound->PlaySfx(SoundID::AE_Pickup, 0.45f);
    }
#if defined(_DEBUG)
    const std::string message =
        "[WorldItemPickup] Pickup committed : " +
        mItemId + "\n";

    OutputDebugStringA(message.c_str());
#endif

    return true;
}


void WorldItemPickup::CancelPendingPickup()
{
    mPickupPending = false;
    mPendingPlayer = nullptr;
}

//============================================================
// Interaction Text
//============================================================
void WorldItemPickup::RefreshInteractionText()
{
    mInteractionText = L"Take Item";

    if (mInventoryManager == nullptr)
    {
        return;
    }

    const InventoryItemDefinition* definition =
        mInventoryManager->FindDefinition(mItemId);


    if (definition == nullptr)
    {
        return;
    }

    //========================================================
    // "Take " + Item Name
    //========================================================
    mInteractionText = L"Take ";

    mInteractionText += definition->Name;
}


//============================================================
// Terminate
//============================================================
void WorldItemPickup::Terminate(vnScene* scene, InteractionSystem* interactionSystem)
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

    if (mPendingPlayer != nullptr)
    {
        mPendingPlayer->CancelWorldItemPickup(this);
    }


    mInventoryManager = nullptr;

    mCollected = false;
}


//============================================================
// Interaction Position
//============================================================
XMVECTOR WorldItemPickup::GetInteractionPosition() const
{
    return
        XMVectorSet(
            mPosition.x,
            mPosition.y,
            mPosition.z,
            0.0f
        );
}


bool WorldItemPickup::CanInteract() const
{
    //========================================================
    // 取得済み、または取得待ち中
    //========================================================
    if (mCollected || mPickupPending)
    {
        return false;
    }

    //========================================================
    // Inventoryが接続されていない
    //========================================================
    if (mInventoryManager == nullptr)
    {
        return false;
    }

    //========================================================
    // Item Definition確認
    //========================================================
    const InventoryItemDefinition* definition = mInventoryManager->FindDefinition(mItemId);

    if (definition == nullptr)
    {
        return false;
    }

    //========================================================
    // 最大所持数に到達している場合は取得できない
    // 取得モーションだけ再生されるのを防ぐ
    //========================================================
    return mInventoryManager->GetItemCount(mItemId) < definition->MaxCount;
}

//============================================================
// Interact
// Eキーを押したときはPickupモーションの開始だけ行う
// Inventoryの変更はCommitPickup()が呼ばれるまで実行しない
//============================================================
void WorldItemPickup::Interact(ExplorationPlayer& player)
{
    if (!CanInteract())
    {
        return;
    }

    //========================================================
    // Pickup Animation開始
    // モーション開始に失敗した場合は予約しない
    //========================================================
    const bool started = player.BeginWorldItemPickup(*this, GetInteractionPosition());

    if (!started)
    {
        return;
    }

    //========================================================
    // 取得待ち状態へ
    // アイテムはまだWorld上に表示されている
    //========================================================
    mPickupPending = true;
    mPendingPlayer = &player;
}

//============================================================
// Interaction Text
//============================================================
const wchar_t* WorldItemPickup::GetInteractionText() const
{
    if (mCollected)
    {
        return L"";
    }

    return mInteractionText.c_str();
}

bool WorldItemPickup::CanInteractFrom(const ExplorationPlayer& player) const
{
    // 取得可能な状態かを確認したうえで 前回追加した高さ判定を再利用する
    return CanInteract() && player.IsPickupWithinVerticalReach(GetInteractionPosition());
}

//============================================================
// Save
//============================================================
WorldObjectSaveState WorldItemPickup::CaptureSaveState() const
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
void WorldItemPickup::RestoreSaveState(const WorldObjectSaveState& data)
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
void WorldItemPickup::SetCollectedImmediate(bool collected)
{
    mCollected = collected;

    if (mModel != nullptr)
    {
        //====================================================
        // 取得済みならWorld上から消す
        //====================================================
        mModel->setRenderEnable(!collected);
        mModel->setExecuteEnable(!collected);
        mModel->setCastShadow(!collected);
    }

    if (mPendingPlayer != nullptr)
    {
        mPendingPlayer->CancelWorldItemPickup(this);
    }
}
