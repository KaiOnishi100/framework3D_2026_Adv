#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "facilityAccessKey.h"

//============================================================
// Initialize
//============================================================
bool FacilityAccessKey::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const XMFLOAT3& position,
    const XMFLOAT3& scale
)
{
    mPersistentId = persistentId;

    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }


    //========================================================
    // Position
    //========================================================
    mPosition = position;


    //========================================================
    // 仮Model
    //========================================================
    mModel = new ModelBase(L"data/model/primitive/", L"sphere.vnm");


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
    // PBR Material
    //========================================================
    mModel->SetPBREnabled(true);
    mModel->setDiffuse(0.05f, 0.15f, 0.25f, 1.0f);
    mModel->SetMetallic(0.4f);
    mModel->SetRoughness(0.25f);

    //========================================================
    // 発見しやすいように青系Emissive
    //========================================================
    mModel->SetEmissive(0.05f, 0.35f, 1.0f, 4.0f);


    mModel->setCastShadow(false);


    //========================================================
    // Scene登録
    //========================================================
    scene->registerObject(mModel);


    //========================================================
    // Interaction登録
    //========================================================
    interactionSystem->Register(this);

    mCollected = false;

    return true;
}


//============================================================
// Terminate
//============================================================
void FacilityAccessKey::Terminate(
    vnScene* scene,
    InteractionSystem* interactionSystem
)
{
    //========================================================
    // Interactionから先に外す
    //========================================================
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }


    //========================================================
    // Model
    //========================================================
    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);

        mModel = nullptr;
    }


    mCollected = false;
}


//============================================================
// Interaction Position
//============================================================
XMVECTOR FacilityAccessKey::GetInteractionPosition() const
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
bool FacilityAccessKey::CanInteract() const
{
    return !mCollected;
}


//============================================================
// Interact
//============================================================
void FacilityAccessKey::Interact(ExplorationPlayer& player)
{
    (void)player;

    if (mCollected)
    {
        return;
    }


    //========================================================
    // Item取得
    //========================================================
    mCollected = true;


    //========================================================
    // 見た目を消す
    //========================================================
    if (mModel != nullptr)
    {
        mModel->setRenderEnable(false);
    }


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[FacilityAccessKey] "
        L"Access Key collected.\n"
    );

#endif
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* FacilityAccessKey::GetInteractionText() const
{
    if (mCollected)
    {
        return L"";
    }


    return L"Take Access Key";
}

WorldObjectSaveState FacilityAccessKey::CaptureSaveState() const
{
    WorldObjectSaveState data;

    data.Id = mPersistentId;
    data.Type = PersistentObjectType::Pickup;

    //========================================================
    // 0 = World上に存在
    // 1 = 取得済み
    //========================================================
    data.State = mCollected ? 1u : 0u;


    return data;
}

void FacilityAccessKey::RestoreSaveState(const WorldObjectSaveState& data)
{
    if (data.Id != mPersistentId)
    {
        return;
    }

    SetCollectedImmediate(data.State == 1u);
}

void FacilityAccessKey::SetCollectedImmediate(bool collected)
{
    //============================================================
    // State
    //============================================================
    mCollected = collected;


    //============================================================
    // Model表示
    //============================================================
    if (mModel != nullptr)
    {
        mModel->setRenderEnable(!collected);
        mModel->setExecuteEnable(!collected);
    }

    //============================================================
    // Colliderを持っている場合
    //============================================================
    //if (mCollider != nullptr)
    //{
    //    mCollider->SetEnabled(!collected);
    //}

}
