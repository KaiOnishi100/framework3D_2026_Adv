#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "mansionResearchSeal.h"

//============================================================
// Required Item
// items.csvに定義済み
//============================================================
namespace
{
    constexpr const char* kRequiredItemId = "StoneEmblem";
}


//============================================================
// Initialize
//============================================================
bool MansionResearchSeal::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const XMFLOAT3& position,
    const XMFLOAT3& scale,
    const XMFLOAT3& rotation
)
{
    //========================================================
    // 必須Object確認
    //========================================================
    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }


    //========================================================
    // 二重Initialize防止
    //========================================================
    if (mInitialized)
    {
        return true;
    }


    //========================================================
    // Persistent情報
    //========================================================
    mPersistentId = persistentId;

    mPosition = position;

    mScale = scale;


    //========================================================
    // 新規Game時の初期状態
    //========================================================
    mState = State::MissingEmblem;


    //========================================================
    // Model
    //========================================================
    mModel = new ModelBase(L"data/model/mansion/dynamic/gimmick/", L"annex_b1_research_seal_01.vnm");


    if (mModel == nullptr)
    {
        return false;
    }


    //========================================================
    // Transform
    //========================================================
    mModel->setPosition(mPosition.x, mPosition.y, mPosition.z);

    mModel->setRotation(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );

    mModel->setScale(
        mScale.x / 0.70f,
        mScale.y / 0.70f,
        mScale.z / 0.30f
    );

    SetActivated(false);

    mModel->setCastShadow(true);
    mModel->setReceiveShadow( true);
    mModel->SetShadowMobility(ShadowMobility::Static);

    //========================================================
    // SceneへModel登録
    //========================================================
    scene->registerObject(mModel);


    //========================================================
    // InteractionSystemへ登録
    //========================================================
    interactionSystem->Register(this);


    mInitialized = true;

    return true;
}


//============================================================
// Terminate
//============================================================
void MansionResearchSeal::Terminate(vnScene* scene,InteractionSystem* interactionSystem)
{
    if (!mInitialized)
    {
        return;
    }

    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }

    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);
        mModel = nullptr;
    }

    mInventory = nullptr;

    mPersistentId.clear();

    mState = State::MissingEmblem;

    mInitialized = false;
}


//============================================================
// Interaction Position
//============================================================
XMVECTOR MansionResearchSeal::GetInteractionPosition() const
{
    return
        XMVectorSet(
            mPosition.x,
            mPosition.y,
            mPosition.z,
            1.0f
        );
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* MansionResearchSeal::GetInteractionText() const
{
    //========================================================
    // Activated
    //========================================================
    if (IsActivated())
    {
        return L"石の紋章がはめ込まれている";
    }


    //========================================================
    // StoneEmblem所持
    //========================================================
    if (mInventory != nullptr && mInventory->HasItem(kRequiredItemId, 1))
    {
        return L"石の紋章をはめ込む";
    }


    //========================================================
    // StoneEmblem未所持
    //========================================================
    return L"紋章型のくぼみがある";
}


//============================================================
// Interact
//============================================================
void MansionResearchSeal::Interact(ExplorationPlayer& player)
{
    (void)player;


    //========================================================
    // 二重実行防止
    //========================================================
    if (IsActivated())
    {
        return;
    }


    //========================================================
    // Inventory未設定
    //========================================================
    if (mInventory == nullptr)
    {
        return;
    }


    //========================================================
    // StoneEmblemを持っていない
    //========================================================
    if (!mInventory->HasItem( kRequiredItemId, 1))
    {
        SoundBank::getInstance()->PlaySfx(SoundID::AE_ResearchSealInspect, 0.46f);
        return;
    }


    //========================================================
    // StoneEmblemを1個消費
    // Remove成功時だけPuzzleを進める
    //========================================================
    if (!mInventory->RemoveItem(kRequiredItemId, 1))
    {
        return;
    }


    //========================================================
    // Puzzle Complete
    //========================================================
    SetActivated(true);
    SoundBank::getInstance()->PlaySfx(SoundID::AE_ResearchSealSet, 0.78f);
}


//============================================================
// SetActivated
//============================================================
void MansionResearchSeal::SetActivated(bool activated)
{
    mState = activated ? State::Activated : State::MissingEmblem;

    if (mModel == nullptr)
    {
        return;
    }

    constexpr int kIndicatorMaterialId = 2;

    if (activated)
    {
        // StoneEmblem装着済み：金色の発光
        mModel->SetEmissive(
            1.0f, 0.70f, 0.20f, 2.5f,
            kIndicatorMaterialId
        );
    }
    else
    {
        // 未装着：発光しない石の紋章面
        mModel->SetEmissive(
            0.0f, 0.0f, 0.0f, 0.0f,
            kIndicatorMaterialId
        );
    }
}


//============================================================
// Capture Save State
//============================================================
WorldObjectSaveState MansionResearchSeal::CaptureSaveState() const
{
    WorldObjectSaveState state = {};

    state.Id = mPersistentId;

    state.Type = PersistentObjectType::Terminal;

    state.State = IsActivated() ? 1 : 0;

    return state;
}


//============================================================
// Restore Save State
//============================================================
void MansionResearchSeal::RestoreSaveState(const WorldObjectSaveState& state)
{
    //========================================================
    // IMPORTANT
    //========================================================
    SetActivated(state.State != 0);
}