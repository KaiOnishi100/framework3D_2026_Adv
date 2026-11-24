#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "facilityPowerLight.h"

//============================================================
// Initialize
//============================================================
bool FacilityPowerLight::Initialize(
    vnScene* scene,
    const XMFLOAT3& position,
    const XMFLOAT3& scale
)
{
    if (scene == nullptr)
    {
        return false;
    }


    //========================================================
    // Position
    //========================================================
    mPosition = position;


    //========================================================
    // Lamp Model
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
    //========================================================
    mModel->SetPBREnabled(true);
    mModel->setDiffuse(0.15f, 0.15f, 0.15f, 1.0f);
    mModel->SetMetallic(0.1f);
    mModel->SetRoughness(0.45f);


    //========================================================
    // Initial State = Power OFF
    //========================================================
    mModel->SetEmissive(
        0.03f,
        0.03f,
        0.03f,
        0.1f
    );


    //========================================================
    // Light fixture自身のShadowは不要
    //========================================================
    mModel->setCastShadow(false);
    mModel->setReceiveShadow(true);

    //========================================================
    // Scene登録
    //========================================================
    scene->registerObject(mModel);

    mPowered = false;


    return true;
}


//============================================================
// Terminate
//============================================================
void FacilityPowerLight::Terminate(vnScene* scene)
{
    if (scene != nullptr &&
        mModel != nullptr)
    {
        scene->deleteObject(
            mModel
        );


        mModel = nullptr;
    }


    mPowered = false;
}


//============================================================
// SetPowered
//============================================================
void FacilityPowerLight::SetPowered(bool powered)
{
    //========================================================
    // 状態変化なし
    // 毎FrameEmissiveを書き換える必要はない
    //========================================================
    if (mPowered == powered)
    {
        return;
    }

    mPowered = powered;

    if (mModel == nullptr)
    {
        return;
    }


    //========================================================
    // Power ON
    //========================================================
    if (mPowered)
    {
        //====================================================
        // 暖色の施設照明
        //====================================================
        mModel->SetEmissive(
            mColor.x,
            mColor.y,
            mColor.z,
            5.0f
        );
    }
    else
    {
        //====================================================
        // Power OFF
        //====================================================
        mModel->SetEmissive(
            0.03f,
            0.03f,
            0.03f,
            0.1f
        );
    }
}


//============================================================
// Update
//============================================================
void FacilityPowerLight::Update()
{
    //========================================================
    // Power OFF中はLocal Lightを登録しない
    //========================================================
    if (!mPowered)
    {
        return;
    }

    const bool registered =
        LocalLightManager::AddPointLight(
            mPosition,
            mRange,
            mColor,
            mIntensity,
            -1
        );


#if defined(_DEBUG)

    if (!registered)
    {
        OutputDebugStringW(
            L"[FacilityPowerLight] "
            L"Failed to register Point Light.\n"
        );
    }

#endif
}