#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "facilityTerminal.h"

//============================================================
// Initialize
//============================================================
bool FacilityTerminal::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const XMFLOAT3& position,
    const XMFLOAT3& scale
)
{
    //============================================================
    // Persistent ID
    //============================================================
    mPersistentId = persistentId;

    //========================================================
    // Safety
    //========================================================
    if (scene == nullptr ||interactionSystem == nullptr)
    {
        return false;
    }


    //========================================================
    // Interaction Position
    //========================================================
    mInteractionPosition = position;


    //========================================================
    // Model
    //========================================================
    mModel = new ModelBase(L"data/model/primitive/", L"cube.vnm");


    if (mModel == nullptr)
    {
        return false;
    }


    //========================================================
    // Transform
    //========================================================
    mModel->setPosition(
        position.x,
        position.y,
        position.z
    );


    mModel->setScale(
        scale.x,
        scale.y,
        scale.z
    );


    //========================================================
    // Material
    // 暗めの金属製Terminalを想定
    //========================================================
    mModel->SetPBREnabled(true);
    mModel->setDiffuse(0.08f, 0.09f, 0.10f, 1.0f);
    mModel->SetMetallic(0.75f);
    mModel->SetRoughness(0.40f);


    //========================================================
    // Ready状態では弱い赤い発光
    //========================================================
    mModel->SetEmissive(0.8f, 0.02f, 0.01f, 2.0f);


    //========================================================
    // Shadow
    //========================================================
    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);
    mModel->SetShadowMobility(ShadowMobility::Static);


    //========================================================
    // Collider
    //========================================================
    const XMFLOAT3 halfExtents =
    {
        scale.x * 0.5f,
        scale.y * 0.5f,
        scale.z * 0.5f
    };


    mCollider =
        std::make_unique<BoxCollider>(
            mModel,
            XMVectorSet( halfExtents.x,halfExtents.y,halfExtents.z,0.0f )
        );

    mCollider->SetType( ColliderType::Body);


    //========================================================
    // Scene登録
    //========================================================
    scene->registerObject(mModel);


    //========================================================
    // Interaction登録
    // Doorと同じInteractionSystemへ登録する
    //========================================================
    interactionSystem->Register(this );


    //========================================================
    // Initial State
    //========================================================
    mState = State::Ready;


    return true;
}


//============================================================
// Terminate
//============================================================
void FacilityTerminal::Terminate(vnScene* scene,InteractionSystem* interactionSystem)
{
    //========================================================
    // Interactionから先に外す
    // delete後にInteractionSystemから参照されるのを防ぐ
    //========================================================
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }


    //========================================================
    // Collider
    //========================================================
    mCollider.reset();


    //========================================================
    // Model
    //========================================================
    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);

        mModel = nullptr;
    }

    mNoiseSystem = nullptr;

    mState = State::Ready;
}


//============================================================
// Interaction Position
//============================================================
XMVECTOR FacilityTerminal::GetInteractionPosition() const
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
// Ready状態でのみ操作可能
//============================================================
bool FacilityTerminal::CanInteract() const
{
    return mState == State::Ready;
}


//============================================================
// Interact
//============================================================
void FacilityTerminal::Interact(ExplorationPlayer& player)
{
    //========================================================
    // 現段階ではPlayer自体は使用しない
    //========================================================
    (void)player;


    if (!CanInteract())
    {
        return;
    }


    //========================================================
    // Activate
    //========================================================
    mState = State::Activated;


    //========================================================
    // 視覚的Feedback
    // 赤 → 緑へ変更し、
    // 操作済みであることを表示する
    //========================================================
    if (mModel != nullptr)
    {
        mModel->SetEmissive(
            0.02f,
            1.0f,
            0.08f,
            4.0f
        );
    }

    if (mNoiseSystem != nullptr)
    {
        mNoiseSystem->Emit(
            mInteractionPosition,
            mActivationNoiseRadius,
            NoiseType::Terminal,
            1.50f
        );


#if defined(_DEBUG)

        OutputDebugStringW(
            L"[FacilityTerminal] "
            L"Terminal noise emitted.\n"
        );

#endif
    }

#if defined(_DEBUG)

    OutputDebugStringW(L"[FacilityTerminal] Activated.\n");

#endif
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* FacilityTerminal::GetInteractionText() const
{
    switch (mState)
    {
    case State::Ready:
        return L"Use Terminal";


    case State::Activated:
        return L"";


    default:
        return L"";
    }
}

//============================================================
// Persistent ID
//============================================================
const std::string& FacilityTerminal::GetPersistentId() const
{
    return mPersistentId;
}


//============================================================
// Save
//============================================================
WorldObjectSaveState FacilityTerminal::CaptureSaveState() const
{
    WorldObjectSaveState data;

    data.Id = mPersistentId;
    data.Type = PersistentObjectType::Terminal;

    //========================================================
    // 0 = 未起動
    // 1 = 起動済み
    //========================================================
    data.State = (mState == State::Activated) ? 1u : 0u;

    return data;
}


//============================================================
// Load
//============================================================
void FacilityTerminal::RestoreSaveState( const WorldObjectSaveState& data)
{
    if (data.Id != mPersistentId)
    {
        return;
    }

    //========================================================
    // Load時にはInteract()を呼ばない。
    //========================================================
    SetActivatedImmediate(data.State == 1u);
}

void FacilityTerminal::SetActivatedImmediate(bool activated)
{
    //========================================================
    // State
    //========================================================
    mState = activated ? State::Activated : State::Ready;


    //========================================================
    // Visual State
    //========================================================
    if (mModel != nullptr)
    {
        if (activated)
        {
            //================================================
            // Activated
            // 緑色
            //================================================
            mModel->SetEmissive(0.02f, 1.0f, 0.08f, 4.0f);
        }
        else
        {
            //================================================
            // Ready
            // 初期状態の赤色
            //================================================
            mModel->SetEmissive(0.8f, 0.02f, 0.01f, 2.0f);
        }
    }
}
