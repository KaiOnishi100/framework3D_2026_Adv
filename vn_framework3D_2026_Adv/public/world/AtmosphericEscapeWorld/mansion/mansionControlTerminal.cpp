#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "mansionControlTerminal.h"

//============================================================
// Initialize
//============================================================
bool MansionControlTerminal::Initialize(
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
    // Persistent / Transform
    //========================================================
    mPersistentId = persistentId;
    mPosition = position;
    mScale = scale;


    //========================================================
    // New Game時の初期状態
    //========================================================
    mState = State::Inactive;


    //========================================================
    // Model
    //========================================================
    mModel = new ModelBase(L"data/model/mansion/dynamic/gimmick/", L"annex_b1_control_terminal_01.vnm");


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
        mScale.x / 1.60f,
        mScale.y / 1.00f,
        mScale.z / 0.35f
    );

    SetActivated(false);

    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);


    //========================================================
    // Scene登録
    //========================================================
    scene->registerObject(mModel);

    //========================================================
    // Interaction登録
    //========================================================
    interactionSystem->Register(this);

    mInitialized = true;

    return true;
}


//============================================================
// Terminate
//============================================================
void MansionControlTerminal::Terminate(vnScene* scene,InteractionSystem* interactionSystem)
{
    if (!mInitialized)
    {
        return;
    }


    //========================================================
    // Interaction解除
    //========================================================
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }


    //========================================================
    // Model削除
    //========================================================
    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);
        mModel = nullptr;
    }


    mPersistentId.clear();

    mState = State::Inactive;

    mInitialized = false;
}


//============================================================
// Interaction Position
//============================================================
XMVECTOR MansionControlTerminal::GetInteractionPosition() const
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
const wchar_t* MansionControlTerminal::GetInteractionText() const
{
    if (IsActivated())
    {
        return L"制御システムは起動している";
    }

    return L"制御端末を起動する";
}


//============================================================
// Interact
//============================================================
void MansionControlTerminal::Interact(ExplorationPlayer& player)
{
    (void)player;


    //========================================================
    // 一度だけ起動可能
    //========================================================
    if (IsActivated())
    {
        return;
    }


    //========================================================
    // Terminal Activate
    //========================================================
    SetActivated(true);

    SoundBank::getInstance()->PlaySfx(SoundID::AE_ControlTerminalActivate, 0.65f);
}


//============================================================
// Set Activated
//============================================================
void MansionControlTerminal::SetActivated(bool activated)
{
    mState = activated ? State::Activated : State::Inactive;

    if (mModel == nullptr)
    {
        return;
    }

    constexpr int kIndicatorMaterialId = 2;

    if (activated)
    {
        // 起動済み：緑色
        mModel->SetEmissive(
            0.05f, 1.0f, 0.30f, 3.0f,
            kIndicatorMaterialId
        );
    }
    else
    {
        // 未起動：赤色
        mModel->SetEmissive(
            0.8f, 0.03f, 0.02f, 1.8f,
            kIndicatorMaterialId
        );
    }
}


//============================================================
// Save
//============================================================
WorldObjectSaveState MansionControlTerminal::CaptureSaveState() const
{
    WorldObjectSaveState state = {};

    state.Id = mPersistentId;

    state.Type = PersistentObjectType::Terminal;

    state.State = IsActivated() ? 1 : 0;

    return state;
}


//============================================================
// Load
//============================================================
void MansionControlTerminal::RestoreSaveState(const WorldObjectSaveState& state)
{
    //========================================================
    // Runtime Actionは再実行しない
    // Saveされた最終状態だけ復元する
    //========================================================
    SetActivated(state.State != 0);
}