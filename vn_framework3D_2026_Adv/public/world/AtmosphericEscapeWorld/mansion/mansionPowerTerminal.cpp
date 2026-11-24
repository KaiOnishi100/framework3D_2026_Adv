#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "mansionPowerTerminal.h"

//============================================================
// Initialize
//============================================================
bool MansionPowerTerminal::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const XMFLOAT3& position,
    const XMFLOAT3& scale,
    const XMFLOAT3& rotation
)
{
    //========================================================
    // Safety
    //========================================================
    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }


    //========================================================
    // Persistent ID
    //========================================================
    mPersistentId = persistentId;


    //========================================================
    // Interaction Position
    //========================================================
    mInteractionPosition = position;


    //========================================================
    // Model
    //========================================================
    mModel = new ModelBase(L"data/model/mansion/dynamic/gimmick/", L"main_power_terminal_01.vnm");


    if (mModel == nullptr)
    {
        return false;
    }


    //========================================================
    // Transform
    //========================================================
    mModel->setPosition(position.x, position.y, position.z);

    mModel->setRotation(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );

    mModel->setScale(
        scale.x / 1.00f,
        scale.y / 2.00f,
        scale.z / 0.50f
    );

    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);
    mModel->SetShadowMobility(ShadowMobility::Static);


    //========================================================
    // Collider
    //========================================================
    mCollider =
        std::make_unique<BoxCollider>(
            mModel,
            XMVectorSet(
                scale.x * 0.5f,
                scale.y * 0.5f,
                scale.z * 0.5f,
                0.0f
            )
        );


    mCollider->SetType(ColliderType::Body);


    //========================================================
    // Initial State
    //========================================================
    mState = State::Ready;

    UpdateVisualState();


    //========================================================
    // Scene
    //========================================================
    scene->registerObject(mModel);


    //========================================================
    // Interaction
    //========================================================
    interactionSystem->Register(this);


    return true;
}


//============================================================
// Terminate
//============================================================
void MansionPowerTerminal::Terminate(vnScene* scene,InteractionSystem* interactionSystem)
{
    //========================================================
    // Pointerが残らないようInteractionから先に外す
    //========================================================
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }


    mCollider.reset();


    if (scene != nullptr &&
        mModel != nullptr)
    {
        scene->deleteObject(mModel);

        mModel = nullptr;
    }


    mState = State::Ready;

    mNoiseSystem = nullptr;
}


//============================================================
// Interaction Position
//============================================================
XMVECTOR MansionPowerTerminal::GetInteractionPosition() const
{
    return
        XMVectorSet(
            mInteractionPosition.x,
            mInteractionPosition.y,
            mInteractionPosition.z,
            0.0f
        );
}


//============================================================
// Can Interact
//============================================================
bool MansionPowerTerminal::CanInteract() const
{
    //========================================================
    // 一度だけ使用可能
    //========================================================
    return mInteractionEnabled && mState == State::Ready;
}

//============================================================
// Interact
//============================================================
void MansionPowerTerminal::Interact(ExplorationPlayer& player)
{
    (void)player;


    if (!CanInteract())
    {
        return;
    }


    //========================================================
    // Terminal Activated
    //========================================================
    mState = State::Activated;


    //========================================================
    // 見た目更新
    //========================================================
    UpdateVisualState();

    SoundBank::getInstance()->PlaySfx(SoundID::AE_PowerRestore, 0.78f);

    //========================================================
    // Gameplay Noise
    // Generator / Breakerを操作した音として、
    // 通常Interactionよりかなり大きなNoiseを発生させる
    //========================================================
    if (mNoiseSystem != nullptr)
    {
        mNoiseSystem->Emit(
            mInteractionPosition,
            mActivationNoiseRadius,
            NoiseType::Terminal,
            1.50f
        );
    }


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[MansionPowerTerminal] "
        L"Activated.\n"
    );

#endif
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* MansionPowerTerminal::GetInteractionText() const
{
    switch (mState)
    {
    case State::Ready:
        return L"Restore Power";


    case State::Activated:
        return L"";


    default:
        return L"";
    }
}


//============================================================
// Visual State
//============================================================
void MansionPowerTerminal::UpdateVisualState()
{
    if (mModel == nullptr)
    {
        return;
    }

    //========================================================
    // Indicator Material ID
    //========================================================
    constexpr int kIndicatorMaterialId = 2;

    if (mState == State::Ready)
    {
        // 未起動：赤色ランプ
        mModel->SetEmissive(
            1.0f, 0.03f, 0.02f, 2.0f,
            kIndicatorMaterialId
        );
    }
    else
    {
        // 起動済み：緑色ランプ
        mModel->SetEmissive(
            0.02f, 1.0f, 0.08f, 4.0f,
            kIndicatorMaterialId
        );
    }
}


//============================================================
// Save
//============================================================
WorldObjectSaveState MansionPowerTerminal::CaptureSaveState() const
{
    WorldObjectSaveState data;

    data.Id = mPersistentId;

    data.Type = PersistentObjectType::Terminal;

    //========================================================
    // 0 = Ready
    // 1 = Activated
    //========================================================
    data.State = IsActivated() ? 1u : 0u;


    return data;
}


//============================================================
// Load
//============================================================
void MansionPowerTerminal::RestoreSaveState(const WorldObjectSaveState& data)
{
    if (data.Id != mPersistentId)
    {
        return;
    }

    //========================================================
    // Interact()は呼ばない
    // Loadした瞬間にNoiseが発生しないようにする
    //========================================================
    SetActivatedImmediate(data.State == 1u);
}


//============================================================
// Load Immediate
//============================================================
void MansionPowerTerminal::SetActivatedImmediate(bool activated)
{
    mState = activated ? State::Activated : State::Ready;

    UpdateVisualState();
}