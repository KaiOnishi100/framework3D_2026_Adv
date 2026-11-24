#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "forestGuidanceLight.h"

//============================================================
// Initialize
//============================================================
bool ForestGuidanceLight::Initialize(vnScene* scene, const XMFLOAT3& position)
{
    if (scene == nullptr)
    {
        return false;
    }

    mPosition = position;

    //========================================================
    // 仮の発光体
    //========================================================
    mLightModel = new ModelBase(L"data/model/primitive/", L"sphere.vnm");


    if (mLightModel == nullptr)
    {
        return false;
    }


    mLightModel->setPosition(position.x, position.y, position.z);

    mLightModel->setScale(0.2f,0.2f, 0.2f );

    //========================================================
    // 発光体なのでPBR Materialとして扱う
    //========================================================
    mLightModel->SetPBREnabled(true);

    //========================================================
    // 赤いEmissive
    // RGB: 赤
    // Intensity: 発光の明るさ
    //========================================================
    mLightModel->SetEmissive(1.0f, 0.02f, 0.01f, 8.0f);

    //========================================================
    // 発光体自身から影は不要
    //========================================================
    mLightModel->setCastShadow(false);

    scene->registerObject(mLightModel);

    return true;
}


//============================================================
// Terminate
//============================================================
void ForestGuidanceLight::Terminate(vnScene* scene)
{
    if (scene != nullptr && mLightModel != nullptr)
    {
        scene->deleteObject(mLightModel);

        mLightModel = nullptr;
    }
}

//============================================================
// Update
//============================================================
void ForestGuidanceLight::Update()
{
    LocalLightManager::AddPointLight(
        mPosition,
        mRange,
        XMFLOAT3(1.0f,0.02f,0.01f ),
        mIntensity,
        -1
    );
}

