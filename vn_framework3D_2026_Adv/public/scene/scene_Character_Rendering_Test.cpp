#include "../../framework.h"
#include "../../framework/vn_environment.h"


//初期化
bool SceneCharacterRenderingTest::initialize()
{

    const bool environmentInitialized = EnvironmentMapManager::Initialize();
    assert(environmentInitialized);
    if (!environmentInitialized) return false;
    EnvironmentMapManager::SetEnvironmentExposureEV(-0.5f);
    const bool environmentLoaded = ChangeEnvironment(EnvironmentType::StudioSmall08);
    assert(environmentLoaded);

    vnCamera::setPosition(0.0f, 0.0f, -5.0f);

    pSKRealHumanModel = new SkeletalModel(L"data/model/RealHumanModel/", L"TestAnimModel.vnm", L"TestAnimModel.bone");
    pSKRealHumanModel->setScale(0.05f, 0.05f, 0.05f);
    pSKRealHumanModel->setRotation(0.0f, 70.0f, 0.0f);
    pSKRealHumanModel->GetModel()->SetPBREnabled(true);
    pSKRealHumanModel->GetModel()->SetMetallic(0.0f);
    pSKRealHumanModel->GetModel()->SetSubsurfaceEnabled(true);
    pSKRealHumanModel->GetModel()->SetSubsurfaceStrength(0.7f);
    pSKRealHumanModel->GetModel()->SetSubsurfaceThickness(1.0f);
    pSKRealHumanModel->GetModel()->SetSubsurfaceColor(1.0f, 0.35f, 0.20f);
    pSKRealHumanModel->GetModel()->SetSubsurfacePower(3.0f, 0);
    pSKRealHumanModel->GetModel()->SetSubsurfaceDistortion(0.20f);

    pSKRealHumanModel->GetModel()->SetEnvironmentReflection(true, 0.02f, 0.03f);
    pSKRealHumanModel->GetModel()->SetORMScale(1.0f, 1.5f, 1.0f);
    pSKRealHumanModel->GetModel()->setCastShadow(true);
    pSKRealHumanModel->GetModel()->setReceiveShadow(true);
    pSKRealHumanModel->GetModel()->SetShadowMobility(ShadowMobility::Dynamic);
    pSKRealHumanModel->GetModel()->setRenderEnable(true);
    const bool idleLoad = pSKRealHumanModel->LoadMotionClip(L"sophiaIdle", L"data/model/RealHumanModel/motion/TestAnimModel_Take 001.mot");
    //const bool walkLoad = pSKRealHumanModel->LoadMotionClip(L"sophiaWalk", L"data/model/RealHumanModel/motion/TestAnimModel_Walk 001.mot");
    assert(idleLoad);
    //assert(walkLoad);
    if (idleLoad)pSKRealHumanModel->SetMotion(L"sophiaIdle", true, true);

    pSKAokunModel = new SkeletalModel(L"data/model/Aokun/Aokun_Dance/", L"Aokun_FunnyDancing.vnm",L"Aokun_FunnyDancing.bone");
    pSKAokunModel->GetModel()->SetPBREnabled(true);
    pSKAokunModel->setScale(0.05f, 0.05f, 0.05f);
    pSKAokunModel->GetModel()->SetMetallic(1.0f);
    pSKAokunModel->setPosition(4.0f, 0.0f, 0.0f);
    bool DanceLoaded =
        pSKAokunModel->LoadMotionClip(L"kannamu", L"data/model/Aokun/Aokun_Dance/motion/Aokun_Gangnam_Groove.mot") &&
        pSKAokunModel->LoadMotionClip(L"FunnyDancing", L"data/model/Aokun/Aokun_Dance/motion/Aokun_FunnyDancing.mot");
    assert(DanceLoaded);
    if (DanceLoaded)pSKAokunModel->SetMotion(L"FunnyDancing");

    pCyclorama = new ModelBase(L"data/model/staticModel/", L"cyclorama.vnm");
    pCyclorama->setScale(0.05f, 0.05f, 0.05f);
    pCyclorama->setRotation(0.0f, 0.0f, 0.0f);
    pCyclorama->SetPBREnabled(true);
    pCyclorama->setDiffuse(0.18f, 0.18f, 0.18f, 1.0f);
    pCyclorama->SetMetallic(0.0f);
    pCyclorama->SetRoughness(0.65f);
    pCyclorama->SetMaterialAO(1.0f);
    pCyclorama->setCastShadow(false);
    pCyclorama->setReceiveShadow(true);
    pCyclorama->SetShadowMobility(ShadowMobility::Static);


    pSSGIRedWall = new ModelBase(L"data/model/primitive/", L"cube.vnm");
    pSSGIRedWall->setScale(12.0f, 12.0f, 1.0f);
    pSSGIRedWall->setRotation(0.0f, 0.0f, 0.0f);
    pSSGIRedWall->setPosition(0.0f, 6.0f, -52.0f);
    pSSGIRedWall->SetPBREnabled(true);
    pSSGIRedWall->setDiffuse(0.80f, 0.03f, 0.03f, 1.0f);
    pSSGIRedWall->SetMetallic(0.0f);
    pSSGIRedWall->SetRoughness(0.75f);
    pSSGIRedWall->SetMaterialAO(1.0f);

    pSSGIBlueWall = new ModelBase(L"data/model/primitive/", L"cube.vnm");
    pSSGIBlueWall->setScale(12.0f, 12.0f, 1.0f);
    pSSGIBlueWall->setRotation(0.0f, 0.0f, 0.0f);
    pSSGIBlueWall->setPosition(0.0f, 6.0f, -27.5f);
    pSSGIBlueWall->SetPBREnabled(true);
    pSSGIBlueWall->setDiffuse(0.03f, 0.06f, 0.80f, 1.0f);
    pSSGIBlueWall->SetMetallic(0.0f);
    pSSGIBlueWall->SetRoughness(0.75f);
    pSSGIBlueWall->SetMaterialAO(1.0f);

    pSSGIFloor = new ModelBase(L"data/model/primitive/", L"plane.vnm");
    pSSGIFloor->setScale(0.125f, 0.1f, 0.125f);
    pSSGIFloor->setRotation(0.0f, 0.0f, 0.0f);
    pSSGIFloor->setPosition(0.0f, 0.0f, -40.0f);
    pSSGIFloor->SetPBREnabled(true);
    pSSGIFloor->setDiffuse(0.75f, 0.75f, 0.75f, 1.0f);
    pSSGIFloor->SetMetallic(0.0f);
    pSSGIFloor->SetRoughness(0.80f);
    pSSGIFloor->SetMaterialAO(1.0f);

    pSSGICube = new ModelBase(L"data/model/primitive/", L"cube.vnm");
    pSSGICube->setScale(1.0f, 1.0f, 1.0f);
    pSSGICube->setPosition(0.0f, 1.5f, -30.0f);
    pSSGICube->setRotation(0.0f, 0.0f, 0.0f);
    pSSGICube->SetPBREnabled(true);
    pSSGICube->setDiffuse(0.75f, 0.75f, 0.75f, 1.0f);
    pSSGICube->SetMetallic(0.0f);
    pSSGICube->SetRoughness(0.70f);
    pSSGICube->SetMaterialAO(1.0f);

    pSSGISphere = new ModelBase(L"data/model/primitive/", L"sphere.vnm");
    pSSGISphere->setScale(1.0f, 1.0f, 1.0f);
    pSSGISphere->setPosition(0.0f, 1.5f, -49.5f);
    pSSGISphere->setRotation(0.0f, 0.0f, 0.0f);
    pSSGISphere->SetPBREnabled(true);
    pSSGISphere->setDiffuse(0.75f, 0.75f, 0.75f, 1.0f);
    pSSGISphere->SetMetallic(0.0f);
    pSSGISphere->SetRoughness(0.45f);
    pSSGISphere->SetMaterialAO(1.0f);

    pSKRealHumanModel->RegisterObject(this);
    pSKAokunModel->RegisterObject(this);
    registerObject(pCyclorama);
    registerObject(pSSGIRedWall);
    registerObject(pSSGIBlueWall);
    registerObject(pSSGIFloor);
    registerObject(pSSGICube);
    registerObject(pSSGISphere);


    ApplyMaterialLookDevSettings();
    ApplyMotionBlurLookDevSettings();
    ApplyVolumetricFogLookDevSettings();
    ApplySSSDiffusionLookDevSettings();
    ApplySSSDebugView();
    ApplySSGILookDevSettings();
    ApplySSGIDebugView();

    PostProcessManager::SetBrightness(mPostLookDev.Brightness);
    PostProcessManager::SetContrast(mPostLookDev.Contrast);
    PostProcessManager::SetSaturation(mPostLookDev.Saturation);
    PostProcessManager::SetVignette(mPostLookDev.VignetteStart, mPostLookDev.VignetteEnd, mPostLookDev.VignetteStrength);
    PostProcessManager::SetChromaticStrength(mPostLookDev.ChromaticStrength);
    PostProcessManager::SetExposure(mPostLookDev.Exposure);
    PostProcessManager::SetBloomIntensity(mPostLookDev.BloomIntensity);
    PostProcessManager::SetGamma(mPostLookDev.Gamma);

    InitializeFreeCamera();

    PostProcessManager::AllDisable();
    //PostProcessManager::Enable(PostProcessManager::LensFlare);
    //PostProcessManager::Enable(PostProcessManager::Bloom);
    //PostProcessManager::Enable(PostProcessManager::MotionBlur);
    //PostProcessManager::Enable(PostProcessManager::VolumetricFog);
    //PostProcessManager::Enable(PostProcessManager::SSSDiffusion);
    //PostProcessManager::Enable(PostProcessManager::SSGI);
    //PostProcessManager::Enable(PostProcessManager::ColorGrading);

    if (!EnableShadowMap(pSKRealHumanModel))return false;

    //============================================================
    // Scene共通ShadowMapを取得
    //============================================================
    ShadowMap* shadowMap = GetShadowMap();
    assert(shadowMap != nullptr);
    shadowMap->SetCascadeUpdateInterval(0, 1);
    shadowMap->SetCascadeUpdateInterval(1, 2);
    shadowMap->SetCascadeUpdateInterval(2, 4);



    prevTime = std::chrono::steady_clock::now();

    return true;
}

//終了
void SceneCharacterRenderingTest::terminate()
{
    EnvironmentMapManager::Terminate();
    deleteObject(pSKAokunModel);
    deleteObject(pCyclorama);
    deleteObject(pSSGIRedWall);
    deleteObject(pSSGIBlueWall);
    deleteObject(pSSGIFloor);
    deleteObject(pSSGICube);
    deleteObject(pSSGISphere);

    if (pSKRealHumanModel != nullptr)
    {
        deleteObject(pSKRealHumanModel );
        pSKRealHumanModel = nullptr;
    }
}

//処理関数
void SceneCharacterRenderingTest::execute()
{
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> delta = now - prevTime;
    prevTime = now;
    float deltaTime = delta.count();
    deltaTime = (std::min)(deltaTime, 0.1f);
    LocalLightManager::BeginFrame();
    SpotLightShadowManager::BeginFrame();
    PointLightShadowManager::BeginFrame();


    //============================================================
    // F1 : Studio Small 08
    //============================================================
    if (vnKeyboard::trg(DIK_F1))
    {
        ChangeEnvironment(EnvironmentType::StudioSmall08);
    }
    //============================================================
    // F2 : White Studio 06
    //============================================================
    if (vnKeyboard::trg(DIK_F2))
    {
        ChangeEnvironment(EnvironmentType::WhiteStudio06);
    }

    if (vnKeyboard::trg(DIK_Q))
    {
        EnvironmentMapManager::AddEnvironmentRotationDegrees(-15.0f);
    }

    if (vnKeyboard::trg(DIK_E))
    {
        EnvironmentMapManager::AddEnvironmentRotationDegrees(15.0f);
    }

    if (vnKeyboard::trg(DIK_R))
    {
        EnvironmentMapManager::SetEnvironmentRotationDegrees(0.0f);
    }

	//============================================================
	// F : Free Camera ON / OFF
	//============================================================
	if (vnKeyboard::trg(DIK_F))
	{
		mFreeCameraEnabled = !mFreeCameraEnabled;
	}

    //============================================================
    // Environment Exposure
    //============================================================
    if (vnKeyboard::trg(DIK_Z))
    {
        EnvironmentMapManager::AddEnvironmentExposureEV(-0.5f);
    }

    if (vnKeyboard::trg(DIK_X))
    {
        EnvironmentMapManager::AddEnvironmentExposureEV(0.5f);
    }

    if (vnKeyboard::trg(DIK_C))
    {
        EnvironmentMapManager::SetEnvironmentExposureEV(0.0f);
    }

    UpdateLookDevInspector();

	UpdateFreeCamera(deltaTime);

    pSKRealHumanModel->execute(deltaTime);
    pSKAokunModel->execute(deltaTime);


    //============================================================
    // LookDev Debug
    //============================================================
    vnFont::print(
        10.0f,
        10.0f,
        L"Environment Rotation : %.1f deg",
        EnvironmentMapManager::GetEnvironmentRotationDegrees()
    );

	vnFont::print(
		10.0f,
		35.0f,
		L"Free Camera : %s",
		mFreeCameraEnabled ?
		L"ON" :
		L"OFF"
	);


	vnFont::print(
		10.0f,
		60.0f,
		L"Camera : %.2f %.2f %.2f",
		XMVectorGetX(mFreeCameraPosition),
		XMVectorGetY(mFreeCameraPosition),
		XMVectorGetZ(mFreeCameraPosition)
	);

    //============================================================
    // Free Camera Speed
    //============================================================
    vnFont::print(
        10.0f,
        85.0f,
        L"Camera Speed : %.2f",
        mFreeCameraMoveSpeed
    );


    //============================================================
    // 操作説明
    //============================================================
    vnFont::print(
        10.0f,
        110.0f,
        L"RMB: Look  WASD: Move  Space/Ctrl: Up/Down"
    );

    vnFont::print(
        10.0f,
        135.0f,
        L"Wheel: Speed  Shift: Fast"
    );

    vnDebugDraw::Grid();
    vnDebugDraw::Axis();


    vnScene::execute();
}

//描画関数
void SceneCharacterRenderingTest::render()
{
    if (mEnvironmentBuildStage != EnvironmentBuildStage::Ready  )
    {
        ProcessEnvironmentBuild();
        Direct3D:: setMainRenderTarget();
        return;
    }   
    vnScene::render();
    EnvironmentMapManager::RenderSkybox();
    Direct3D::setMainRenderTarget();
}

void SceneCharacterRenderingTest::ProcessEnvironmentBuild()
{
    switch (mEnvironmentBuildStage)
    {
        //========================================================
        // Stage 1
        //
        // Equirectangular HDR
        //      ↓
        // TextureCube
        //========================================================
    case EnvironmentBuildStage::HDRToCube:
    {
        const bool generated = EnvironmentMapManager::GenerateEnvironmentCubeFromHDR();


        if (generated)
        {
            OutputDebugStringW(
                L"[LookDev] Environment Stage 1/4 : "
                L"HDR To Cube recorded.\n"
            );


            mEnvironmentBuildStage = EnvironmentBuildStage::BRDFLUT;
        }

        return;
    }


    //========================================================
    // Stage 2
    //
    // Split-Sum BRDF LUT
    //========================================================
    case EnvironmentBuildStage::BRDFLUT:
    {
        const bool generated = EnvironmentMapManager::GenerateBRDFLUT();


        if (generated)
        {
            OutputDebugStringW(
                L"[LookDev] Environment Stage 2/4 : "
                L"BRDF LUT recorded.\n"
            );

            mEnvironmentBuildStage = EnvironmentBuildStage::Prefilter;
        }

        return;
    }


    //========================================================
    // Stage 3
    // Specular IBL用
    // GGX Prefiltered Environment
    //========================================================
    case EnvironmentBuildStage::Prefilter:
    {
        const bool generated =
            EnvironmentMapManager::
            GeneratePrefilteredEnvironment();


        if (generated)
        {
            OutputDebugStringW(
                L"[LookDev] Environment Stage 3/4 : "
                L"Prefilter recorded.\n"
            );

            mEnvironmentBuildStage = EnvironmentBuildStage::Irradiance;
        }

        return;
    }


    //========================================================
    // Stage 4
    //
    // Diffuse IBL用Irradiance
    //========================================================
    case EnvironmentBuildStage::Irradiance:
    {
        const bool generated = EnvironmentMapManager::GenerateIrradianceEnvironment();

        if (generated)
        {
            OutputDebugStringW(
                L"[LookDev] Environment Stage 4/4 : "
                L"Irradiance recorded.\n"
            );
            RefreshEnvironmentDescriptors();
            mEnvironmentBuildStage = EnvironmentBuildStage::Ready;
        }

        return;
    }


    //========================================================
    // 全生成完了
    //========================================================
    case EnvironmentBuildStage::Ready:
    default:
        return;
    }
}

bool SceneCharacterRenderingTest::ChangeEnvironment(EnvironmentType type)
{
    bool loaded = false;

    switch (type)
    {
        //========================================================
        // Studio Small 08
        //========================================================
    case EnvironmentType::StudioSmall08:
        loaded = EnvironmentMapManager::LoadHDRI(L"data/texture/ibl/StudioSmall08/" L"studio_small_08_8k.hdr");
        break;
        //========================================================
        // White Studio 06
        //========================================================
    case EnvironmentType::WhiteStudio06:
        loaded = EnvironmentMapManager::LoadHDRI(L"data/texture/ibl/WhiteStudio06/"   L"white_studio_06_8k.hdr");
        break;
    }
    //========================================================
    // Load成功時のみ現在のEnvironmentを更新
    //========================================================
    if (loaded)
    {
        mEnvironmentType = type;
        mEnvironmentBuildStage = EnvironmentBuildStage::HDRToCube;
    }
    return loaded;
}

void SceneCharacterRenderingTest::RefreshEnvironmentDescriptors()
{
    //============================================================
    // Sophia
    //============================================================
    if (pSKRealHumanModel != nullptr)
    {
        ModelBase* model = pSKRealHumanModel->GetModel();

        if (model != nullptr)
        {
            //====================================================
            // -1:
            // 全Materialを更新
            //====================================================
            model->RefreshEnvironmentMapDescriptor(-1);
        }
    }

    //============================================================
    // Cyclorama
    //============================================================
    if (pCyclorama != nullptr)
    {
        pCyclorama->RefreshEnvironmentMapDescriptor(-1);
    }
}

void SceneCharacterRenderingTest::InitializeFreeCamera()
{
    //============================================================
    // 最初のCamera位置
    //============================================================
    mFreeCameraPosition = XMVectorSet( 0.0f, 1.5f, -4.0f, 0.0f  );
    mFreeCameraYaw = 0.0f;
    mFreeCameraPitch = 0.0f;
    //============================================================
    // vnCameraへ反映
    //============================================================
    vnCamera::setPosition(  &mFreeCameraPosition);
    const XMVECTOR initialForward =   XMVectorSet( 0.0f,   0.0f,  1.0f,     0.0f  );
    const XMVECTOR initialTarget =  mFreeCameraPosition +initialForward;
    vnCamera::setTarget( &initialTarget );
    vnCamera::setUp(0.0f,1.0f, 0.0f );
}

void SceneCharacterRenderingTest::UpdateFreeCamera(float deltaTime)
{
    //========================================================
    // Free Camera無効
    //========================================================
    if (!mFreeCameraEnabled)
    {
        return;
    }

    //========================================================
    // Debug Breakやウィンドウ移動などで
    // DeltaTimeが極端に大きくなった場合に、
    // カメラが一気に飛ばないよう制限する
    //========================================================
    deltaTime = (std::min)(deltaTime, 0.1f);

    //============================================================
    // Mouse Wheel
    //============================================================
    const int wheel = vnMouse::getR();


    if (wheel > 0)
    {
        //========================================================
        // Wheel Up
        //========================================================
        mFreeCameraMoveSpeed *= 1.25f;
    }
    else if (wheel < 0)
    {
        //========================================================
        // Wheel Down
        //========================================================
        mFreeCameraMoveSpeed /= 1.25f;
    }

    //============================================================
    // 移動速度の暴走防止
    //============================================================
    mFreeCameraMoveSpeed =
        std::clamp(
            mFreeCameraMoveSpeed,
            mFreeCameraMinMoveSpeed,
            mFreeCameraMaxMoveSpeed
        );


    //============================================================
    // Mouse Look
    // 右クリック中だけ視点回転する
    //============================================================
    if (vnMouse::onR())
    {
        const float mouseDeltaX = static_cast<float>(vnMouse::getDX());
        const float mouseDeltaY = static_cast<float>(vnMouse::getDY());


        //========================================================
        // Yaw
        //========================================================
        mFreeCameraYaw += mouseDeltaX * mFreeCameraMouseSensitivity;


        //========================================================
        // Pitch
        //========================================================
        mFreeCameraPitch -= mouseDeltaY * mFreeCameraMouseSensitivity;
    }


    //============================================================
    // Pitch Limit
    //============================================================
    const float pitchLimit = XM_PIDIV2 - 0.01f;


    mFreeCameraPitch = std::clamp(mFreeCameraPitch, -pitchLimit, pitchLimit);


    //============================================================
    // Yaw正規化
    //============================================================
    if (mFreeCameraYaw > XM_PI)
    {
        mFreeCameraYaw -= XM_2PI;
    }

    if (mFreeCameraYaw < -XM_PI)
    {
        mFreeCameraYaw += XM_2PI;
    }


    //============================================================
    // Yaw / Pitch
    //      ↓
    // Forward
    //============================================================
    const float cosPitch = cosf(mFreeCameraPitch);


    XMVECTOR forward =
        XMVectorSet(
            cosPitch * sinf(mFreeCameraYaw),
            sinf(mFreeCameraPitch),
            cosPitch * cosf(mFreeCameraYaw),
            0.0f
        );


    forward = XMVector3Normalize(forward);


    //============================================================
    // World Up
    //============================================================
    const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);


    //============================================================
    // Right
    //============================================================
    XMVECTOR right = XMVector3Cross(worldUp, forward);


    const float rightLengthSq = XMVectorGetX(XMVector3LengthSq(right));


    if (rightLengthSq > 0.000001f)
    {
        right = XMVector3Normalize(right);
    }


    //============================================================
    // 現在Frameの移動速度
    //============================================================
    float currentMoveSpeed = mFreeCameraMoveSpeed;


    //============================================================
    // Shift中は高速移動
    //============================================================
    if (vnKeyboard::on(DIK_LSHIFT))
    {
        currentMoveSpeed *= mFreeCameraFastMultiplier;
    }


    const float moveAmount = currentMoveSpeed * deltaTime;

    //============================================================
    // 今Frameの移動量
    //============================================================
    XMVECTOR movement = XMVectorZero();

    //============================================================
    // W : Forward
    //============================================================
    if (vnKeyboard::on(DIK_W))
    {
        movement += forward * moveAmount;
    }

    //============================================================
    // S : Backward
    //============================================================
    if (vnKeyboard::on(DIK_S))
    {
        movement -= forward * moveAmount;
    }

    //============================================================
    // A : Left
    //============================================================
    if (vnKeyboard::on(DIK_A))
    {
        movement -= right * moveAmount;
    }


    //============================================================
    // D : Right
    //============================================================
    if (vnKeyboard::on(DIK_D))
    {
        movement += right * moveAmount;
    }

    //============================================================
    // Space : World Up
    //============================================================
    if (vnKeyboard::on(DIK_SPACE))
    {
        movement += worldUp * moveAmount;
    }

    //============================================================
    // Left Ctrl : World Down
    //============================================================
    if (vnKeyboard::on(DIK_LCONTROL))
    {
        movement -= worldUp * moveAmount;
    }


    //============================================================
    // Position更新
    //============================================================
    mFreeCameraPosition += movement;

    //============================================================
    // LookAt Target
    //============================================================
    const XMVECTOR target = mFreeCameraPosition + forward;

    //============================================================
    // vnCameraへ反映
    //============================================================
    vnCamera::setPosition(&mFreeCameraPosition);
    vnCamera::setTarget(&target);
    vnCamera::setUp(0.0f, 1.0f, 0.0f);
}

void SceneCharacterRenderingTest::UpdateLookDevInspector()
{
    //============================================================
    // F3:
    // Inspector ON / OFF
    //============================================================
    if (vnKeyboard::trg(DIK_F3))
    {
        mLookDevInspectorEnabled = !mLookDevInspectorEnabled;
    }

    if (!mLookDevInspectorEnabled)
    {
        return;
    }


    //============================================================
    // F4:
    // Page切替
    //============================================================
    if (vnKeyboard::trg(DIK_F4))
    {
        int page = static_cast<int>(mLookDevPage);
        ++page;

        if (page >= static_cast<int>(LookDevPage::Count))
        {
            page = 0;
        }

        mLookDevPage = static_cast<LookDevPage>(page);
        // Page変更時は選択位置を先頭へ戻す
        mLookDevSelection = 0;
    }


    //============================================================
    // 現在Pageの処理
    //============================================================
    switch (mLookDevPage)
    {
    case LookDevPage::Material:
        UpdateMaterialLookDev();
        break;
    case LookDevPage::PostEffect:
        UpdatePostEffectLookDev();
        break;
    case LookDevPage::PostParameter:
        UpdatePostParameterLookDev();
        break;
    case LookDevPage::MotionBlur:
        UpdateMotionBlurLookDev();
        break;
    case LookDevPage::VolumetricFog:
        UpdateVolumetricFogLookDev();
        break;
    case LookDevPage::SSSDiffusion:
        UpdateSSSDiffusionLookDev();
        break;
    case LookDevPage::SSGI:
        UpdateSSGILookDev();
        break;
    default:

        break;
    }


    //============================================================
    // Inspector文字表示
    //============================================================
    DrawLookDevInspector();
}

void SceneCharacterRenderingTest::UpdateMaterialLookDev()
{
    //============================================================
    // Material項目数
    //============================================================
    constexpr int itemCount = 17;


    //============================================================
    // ↑ / ↓
    // Parameter選択
    //============================================================
    if (vnKeyboard::trg(DIK_UP))
    {
        --mLookDevSelection;

        if (mLookDevSelection < 0)
        {
            mLookDevSelection = itemCount - 1;
        }
    }


    if (vnKeyboard::trg(DIK_DOWN))
    {
        ++mLookDevSelection;

        if (mLookDevSelection >= itemCount)
        {
            mLookDevSelection = 0;
        }
    }


    //============================================================
    // 数値変更倍率
    //
    // Shift:
    //   大きく変更
    //
    // Ctrl:
    //   細かく変更
    //============================================================
    float multiplier = 1.0f;


    if (vnKeyboard::on(DIK_LSHIFT))
    {
        multiplier = 5.0f;
    }
    else if (vnKeyboard::on(DIK_LCONTROL))
    {
        multiplier = 0.2f;
    }


    //============================================================
    // ← / →
    //============================================================
    float direction = 0.0f;


    if (vnKeyboard::trg(DIK_LEFT))
    {
        direction = -1.0f;
    }


    if (vnKeyboard::trg(DIK_RIGHT))
    {
        direction = 1.0f;
    }


    bool changed = false;


    //============================================================
    // Float Parameter
    //============================================================
    if (direction != 0.0f)
    {
        switch (mLookDevSelection)
        {
            //========================================================
            // Metallic
            //========================================================
        case 0:

            mMaterialLookDev.Metallic +=
                direction *
                0.05f *
                multiplier;

            mMaterialLookDev.Metallic =
                std::clamp(
                    mMaterialLookDev.Metallic,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Roughness
            //========================================================
        case 1:

            mMaterialLookDev.Roughness +=
                direction *
                0.05f *
                multiplier;

            mMaterialLookDev.Roughness =
                std::clamp(
                    mMaterialLookDev.Roughness,
                    0.04f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // AO
            //========================================================
        case 2:

            mMaterialLookDev.AO +=
                direction *
                0.05f *
                multiplier;

            mMaterialLookDev.AO =
                std::clamp(
                    mMaterialLookDev.AO,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Normal Strength
            //========================================================
        case 3:

            mMaterialLookDev.NormalStrength +=
                direction *
                0.1f *
                multiplier;

            mMaterialLookDev.NormalStrength =
                std::clamp(
                    mMaterialLookDev.NormalStrength,
                    0.0f,
                    4.0f
                );

            changed = true;

            break;


        case 4:

            mMaterialLookDev.ORMRoughnessScale +=
                direction *
                0.1f *
                multiplier;

            mMaterialLookDev.ORMRoughnessScale =
                std::clamp(
                    mMaterialLookDev.ORMRoughnessScale,
                    0.0f,
                    4.0f
                );

            changed = true;

            break;


            //========================================================
            // Diffuse IBL
            //========================================================
        case 5:

            mMaterialLookDev.EnvironmentDiffuse +=
                direction *
                0.01f *
                multiplier;

            mMaterialLookDev.EnvironmentDiffuse =
                (std::max)(
                    mMaterialLookDev.EnvironmentDiffuse,
                    0.0f
                    );

            changed = true;

            break;


            //========================================================
            // Specular IBL
            //========================================================
        case 6:

            mMaterialLookDev.EnvironmentSpecular +=
                direction *
                0.01f *
                multiplier;

            mMaterialLookDev.EnvironmentSpecular =
                (std::max)(
                    mMaterialLookDev.EnvironmentSpecular,
                    0.0f
                    );

            changed = true;

            break;


        case 7:

            mMaterialLookDev.SSSStrength +=
                direction *
                0.05f *
                multiplier;

            mMaterialLookDev.SSSStrength =
                std::clamp(
                    mMaterialLookDev.SSSStrength,
                    0.0f,
                    2.0f
                );

            changed = true;

            break;


        case 8:

            mMaterialLookDev.SSSThickness +=
                direction *
                0.05f *
                multiplier;

            mMaterialLookDev.SSSThickness =
                std::clamp(
                    mMaterialLookDev.SSSThickness,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


        case 9:

            mMaterialLookDev.SSSDistortion +=
                direction *
                0.05f *
                multiplier;

            mMaterialLookDev.SSSDistortion =
                std::clamp(
                    mMaterialLookDev.SSSDistortion,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


        case 10:

            mMaterialLookDev.SSSPower +=
                direction *
                0.25f *
                multiplier;

            mMaterialLookDev.SSSPower =
                std::clamp(
                    mMaterialLookDev.SSSPower,
                    0.25f,
                    16.0f
                );

            changed = true;

            break;
        }
    }


    //============================================================
    // Bool Parameter
    // Enterで切替
    //============================================================
    if (vnKeyboard::trg(DIK_RETURN))
    {
        switch (mLookDevSelection)
        {
        case 11:
            mMaterialLookDev.PBR = !mMaterialLookDev.PBR;
            changed = true;
            break;

        case 12:
            mMaterialLookDev.ORM = !mMaterialLookDev.ORM;
            changed = true;
            break;

        case 13:
            mMaterialLookDev.Environment = !mMaterialLookDev.Environment;
            changed = true;
            break;

        case 14:
            mMaterialLookDev.SSS = !mMaterialLookDev.SSS;
            changed = true;
            break;

        case 15:
            mMaterialLookDev.ThicknessMap = !mMaterialLookDev.ThicknessMap;
            changed = true;
            break;

        case 16:
            mMaterialLookDev.SkinMask = !mMaterialLookDev.SkinMask;
            changed = true;
            break;
        }
    }


    //============================================================
    // 設定変更があったFrameだけMaterialへ反映
    //============================================================
    if (changed)
    {
        ApplyMaterialLookDevSettings();
    }
}

void SceneCharacterRenderingTest::UpdatePostEffectLookDev()
{
    //============================================================
    // Inspectorで扱うPostEffect
    //============================================================
    static constexpr PostProcessManager::eEffectMask effects[] =
    {
        PostProcessManager::Bloom,
        PostProcessManager::Gray,
        PostProcessManager::Fxaa,
        PostProcessManager::Vignette,
        PostProcessManager::ColorGrading,
        PostProcessManager::ChromaticAberration,
        PostProcessManager::LensFlare,
        PostProcessManager::MotionBlur,
        PostProcessManager::VolumetricFog,
        PostProcessManager::SSSDiffusion,
        PostProcessManager::SSGI
    };


    constexpr int effectCount = static_cast<int>(_countof(effects));


    if (vnKeyboard::trg(DIK_UP))
    {
        --mLookDevSelection;

        if (mLookDevSelection < 0)
        {
            mLookDevSelection =
                effectCount - 1;
        }
    }


    if (vnKeyboard::trg(DIK_DOWN))
    {
        ++mLookDevSelection;

        if (mLookDevSelection >= effectCount)
        {
            mLookDevSelection = 0;
        }
    }


    //============================================================
    // Enter:
    // Effect ON / OFF
    //============================================================
    if (vnKeyboard::trg(DIK_RETURN))
    {
        TogglePostEffect(effects[mLookDevSelection]);
    }
}

void SceneCharacterRenderingTest::UpdatePostParameterLookDev()
{
    constexpr int itemCount = 10;


    if (vnKeyboard::trg(DIK_UP))
    {
        --mLookDevSelection;

        if (mLookDevSelection < 0)
        {
            mLookDevSelection = itemCount - 1;
        }
    }


    if (vnKeyboard::trg(DIK_DOWN))
    {
        ++mLookDevSelection;

        if (mLookDevSelection >= itemCount)
        {
            mLookDevSelection = 0;
        }
    }


    float multiplier = 1.0f;


    if (vnKeyboard::on(DIK_LSHIFT))
    {
        multiplier = 5.0f;
    }
    else if (vnKeyboard::on(DIK_LCONTROL))
    {
        multiplier = 0.2f;
    }


    float direction = 0.0f;


    if (vnKeyboard::trg(DIK_LEFT))
    {
        direction = -1.0f;
    }

    if (vnKeyboard::trg(DIK_RIGHT))
    {
        direction = 1.0f;
    }


    if (direction == 0.0f)
    {
        return;
    }


    switch (mLookDevSelection)
    {
    case 0:

        mPostLookDev.Brightness +=
            direction *
            0.05f *
            multiplier;

        mPostLookDev.Brightness =
            std::clamp(
                mPostLookDev.Brightness,
                0.0f,
                4.0f
            );

        PostProcessManager::SetBrightness(mPostLookDev.Brightness);

        break;


    case 1:

        mPostLookDev.Contrast +=
            direction *
            0.05f *
            multiplier;

        mPostLookDev.Contrast =
            std::clamp(
                mPostLookDev.Contrast,
                0.0f,
                4.0f
            );

        PostProcessManager::SetContrast(mPostLookDev.Contrast);

        break;


    case 2:

        mPostLookDev.Saturation +=
            direction *
            0.05f *
            multiplier;

        mPostLookDev.Saturation =
            std::clamp(
                mPostLookDev.Saturation,
                0.0f,
                4.0f
            );

        PostProcessManager::SetSaturation(mPostLookDev.Saturation);

        break;


    case 3:

        mPostLookDev.VignetteStart +=
            direction *
            0.02f *
            multiplier;

        mPostLookDev.VignetteStart =
            std::clamp(
                mPostLookDev.VignetteStart,
                0.0f,
                1.0f
            );

        break;


    case 4:

        mPostLookDev.VignetteEnd +=
            direction *
            0.02f *
            multiplier;

        mPostLookDev.VignetteEnd =
            std::clamp(
                mPostLookDev.VignetteEnd,
                0.0f,
                1.5f
            );

        break;


    case 5:

        mPostLookDev.VignetteStrength +=
            direction *
            0.05f *
            multiplier;

        mPostLookDev.VignetteStrength =
            std::clamp(
                mPostLookDev.VignetteStrength,
                0.0f,
                2.0f
            );

        break;


    case 6:

        mPostLookDev.ChromaticStrength +=
            direction *
            0.001f *
            multiplier;

        mPostLookDev.ChromaticStrength =
            std::clamp(
                mPostLookDev.ChromaticStrength,
                0.0f,
                0.1f
            );

        PostProcessManager::SetChromaticStrength(
            mPostLookDev.ChromaticStrength
        );

        break;


    case 7:

        mPostLookDev.Exposure +=
            direction *
            0.1f *
            multiplier;

        mPostLookDev.Exposure =
            std::clamp(
                mPostLookDev.Exposure,
                -8.0f,
                8.0f
            );

        PostProcessManager::SetExposure(
            mPostLookDev.Exposure
        );

        break;


    case 8:

        mPostLookDev.BloomIntensity +=
            direction *
            0.1f *
            multiplier;

        mPostLookDev.BloomIntensity =
            std::clamp(
                mPostLookDev.BloomIntensity,
                0.0f,
                8.0f
            );

        PostProcessManager::SetBloomIntensity(
            mPostLookDev.BloomIntensity
        );

        break;


    case 9:

        mPostLookDev.Gamma +=
            direction *
            0.05f *
            multiplier;

        mPostLookDev.Gamma =
            std::clamp(
                mPostLookDev.Gamma,
                0.5f,
                4.0f
            );

        PostProcessManager::SetGamma(
            mPostLookDev.Gamma
        );

        break;
    }


    //============================================================
    // Vignetteは3Parameterをまとめて更新
    //============================================================
    PostProcessManager::SetVignette(
        mPostLookDev.VignetteStart,
        mPostLookDev.VignetteEnd,
        mPostLookDev.VignetteStrength
    );
}

void SceneCharacterRenderingTest::DrawLookDevInspector()
{
    if (!mLookDevInspectorEnabled)
    {
        return;
    }


    float y = 180.0f;


    //============================================================
    // Header
    //============================================================
    vnFont::print(
        10.0f,
        y,
        L"========== LookDev Inspector =========="
    );

    y += 25.0f;


    //============================================================
    // Page
    //============================================================
    switch (mLookDevPage)
    {
    case LookDevPage::Material:

        vnFont::print(
            10.0f,
            y,
            L"Page : MATERIAL"
        );

        break;


    case LookDevPage::PostEffect:

        vnFont::print(
            10.0f,
            y,
            L"Page : POST EFFECT"
        );

        break;


    case LookDevPage::PostParameter:

        vnFont::print(
            10.0f,
            y,
            L"Page : POST PARAMETER"
        );

        break;


    case LookDevPage::MotionBlur:

        vnFont::print(
            10.0f,
            y,
            L"Page : MOTION BLUR"
        );

        break;


    case LookDevPage::VolumetricFog:

        vnFont::print(
            10.0f,
            y,
            L"Page : VOLUMETRIC FOG"
        );

        break;

    case LookDevPage::SSSDiffusion:

        vnFont::print(
            10.0f,
            y,
            L"Page : SSS DIFFUSION"
        );

        break;

    case LookDevPage::SSGI:

        vnFont::print(
            10.0f,
            y,
            L"Page : SSGI"
        );

        break;
    }


    y += 25.0f;


    vnFont::print(
        10.0f,
        y,
        L"F4:Page  Up/Down:Select  Left/Right:Value  Enter:Toggle"
    );

    y += 25.0f;


    //============================================================
    // Material Page
    //============================================================
    if (mLookDevPage ==
        LookDevPage::Material)
    {
        static const WCHAR* names[] =
        {
            L"Metallic",
            L"Roughness",
            L"AO",
            L"Normal Strength",
            L"ORM Roughness Scale",
            L"IBL Diffuse",
            L"IBL Specular",
            L"SSS Strength",
            L"SSS Thickness",
            L"SSS Distortion",
            L"SSS Power",
            L"PBR Enabled",
            L"ORM Enabled",
            L"Environment Enabled",
            L"SSS Enabled",
            L"Thickness Map",
            L"Skin Mask"
        };


        for (int i = 0; i < _countof(names); ++i)
        {
            vnFont::print(
                10.0f,
                y,
                i == mLookDevSelection ? L"> %s" : L"  %s",
                names[i]
            );


            y += 20.0f;
        }
    }


    //============================================================
    // Post Effect Page
    //============================================================
    if (mLookDevPage == LookDevPage::PostEffect)
    {
        static const WCHAR* names[] =
        {
            L"Bloom",
            L"Gray",
            L"FXAA",
            L"Vignette",
            L"ColorGrading",
            L"Chromatic Aberration",
            L"LensFlare",
            L"MotionBlur",
            L"VolumetricFog",
            L"SSS Diffusion",
            L"SSGI"
        };


        static constexpr PostProcessManager::eEffectMask effects[] =
        {
            PostProcessManager::Bloom,
            PostProcessManager::Gray,
            PostProcessManager::Fxaa,
            PostProcessManager::Vignette,
            PostProcessManager::ColorGrading,
            PostProcessManager::ChromaticAberration,
            PostProcessManager::LensFlare,
            PostProcessManager::MotionBlur,
            PostProcessManager::VolumetricFog,
            PostProcessManager::SSSDiffusion,
            PostProcessManager::SSGI
        };


        for (int i = 0; i < _countof(names); ++i)
        {
            const bool enabled = PostProcessManager::IsEnabled(effects[i]);


            vnFont::print(
                10.0f,
                y,
                i == mLookDevSelection ? L"> %-24s : %s" : L"  %-24s : %s",
                names[i],
                enabled ? L"ON" : L"OFF"
            );


            y += 20.0f;
        }
    }
    //============================================================
    // Motion Blur Page
    //============================================================
    if (mLookDevPage == LookDevPage::MotionBlur)
    {
        //========================================================
        // Motion Blur ON/OFF状態
        //========================================================
        const bool enabled = PostProcessManager::IsEnabled(PostProcessManager::MotionBlur);

        vnFont::print(
            10.0f,
            y,
            L"Motion Blur : %s",
            enabled ? L"ON" : L"OFF"
        );

        y += 25.0f;

        //========================================================
        // Strength
        //========================================================
        vnFont::print(
            10.0f,
            y,
            mLookDevSelection == 0
            ? L"> Strength          : %.3f" : L"  Strength          : %.3f",
            mMotionBlurLookDev.Strength
        );

        y += 20.0f;

        //========================================================
        // Maximum Pixels
        //========================================================
        vnFont::print(
            10.0f,
            y,
            mLookDevSelection == 1
            ? L"> Max Pixels        : %.1f" : L"  Max Pixels        : %.1f",
            mMotionBlurLookDev.MaxPixels
        );

        y += 20.0f;


        //========================================================
        // Sample Count
        //========================================================
        vnFont::print(
            10.0f,
            y,
            mLookDevSelection == 2
            ? L"> Sample Count      : %d" : L"  Sample Count      : %d",
            mMotionBlurLookDev.SampleCount
        );

        y += 20.0f;


        //========================================================
        // Depth Threshold
        //========================================================
        vnFont::print(
            10.0f,
            y,
            mLookDevSelection == 3
            ? L"> Depth Threshold   : %.5f" : L"  Depth Threshold   : %.5f",
            mMotionBlurLookDev.DepthThreshold
        );

        y += 20.0f;


        //========================================================
        // Velocity Threshold
        //========================================================
        vnFont::print(
            10.0f,
            y,
            mLookDevSelection == 4
            ? L"> Velocity Threshold: %.5f" : L"  Velocity Threshold: %.5f",
            mMotionBlurLookDev.VelocityThreshold
        );

        y += 25.0f;

        //========================================================
        // Help
        //========================================================
        vnFont::print(
            10.0f,
            y,
            L"BackSpace : Reset Motion Blur Parameters"
        );
    }
    //============================================================
    // Volumetric Fog Page
    //============================================================
    if (mLookDevPage == LookDevPage::VolumetricFog)
    {
        const bool enabled = PostProcessManager::IsEnabled(PostProcessManager::VolumetricFog);


        vnFont::print(
            10.0f,
            y,
            L"Volumetric Fog : %s",
            enabled ? L"ON" : L"OFF"
        );

        y += 25.0f;


        //============================================================
        // 数値表示用の簡易Lambda
        //============================================================
        auto DrawFloat = [this, &y](int index, const WCHAR* name, float value)
            {
                vnFont::print(
                    10.0f,
                    y,
                    index == mLookDevSelection ? L"> %-26s : %.4f" : L"  %-26s : %.4f",
                    name,
                    value
                );
                y += 20.0f;
            };


        auto DrawInt = [this, &y](int index, const WCHAR* name, int value)
            {
                vnFont::print(
                    10.0f,
                    y,
                    index == mLookDevSelection ? L"> %-26s : %d" : L"  %-26s : %d",
                    name,
                    value
                );
                y += 20.0f;
            };


        auto DrawBool = [this, &y](int index, const WCHAR* name, bool value)
            {
                vnFont::print(
                    10.0f,
                    y,
                    index == mLookDevSelection ? L"> %-26s : %s" : L"  %-26s : %s",
                    name,
                    value ? L"ON" : L"OFF"
                );
                y += 20.0f;
            };


        //============================================================
        // Basic
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Basic ]"
        );

        y += 20.0f;


        DrawFloat(
            0,
            L"Density",
            mVolumetricFogLookDev.Density
        );

        DrawFloat(
            1,
            L"Max Distance",
            mVolumetricFogLookDev.MaxDistance
        );

        DrawFloat(
            2,
            L"Color R",
            mVolumetricFogLookDev.ColorR
        );

        DrawFloat(
            3,
            L"Color G",
            mVolumetricFogLookDev.ColorG
        );

        DrawFloat(
            4,
            L"Color B",
            mVolumetricFogLookDev.ColorB
        );

        DrawFloat(
            5,
            L"Anisotropy",
            mVolumetricFogLookDev.Anisotropy
        );

        DrawFloat(
            6,
            L"Base Height",
            mVolumetricFogLookDev.BaseHeight
        );

        DrawFloat(
            7,
            L"Height Falloff",
            mVolumetricFogLookDev.HeightFalloff
        );

        DrawFloat(
            8,
            L"Scattering Albedo",
            mVolumetricFogLookDev.ScatteringAlbedo
        );


        //============================================================
        // Lighting
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Lighting ]"
        );

        y += 20.0f;


        DrawFloat(
            9,
            L"Directional Strength",
            mVolumetricFogLookDev.DirectionalStrength
        );

        DrawFloat(
            10,
            L"Ambient Strength",
            mVolumetricFogLookDev.AmbientStrength
        );

        DrawFloat(
            11,
            L"Spot Strength",
            mVolumetricFogLookDev.SpotLightStrength
        );

        DrawFloat(
            12,
            L"Point Strength",
            mVolumetricFogLookDev.PointLightStrength
        );


        //============================================================
        // Ray March / Shadow
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Ray March / Shadow ]"
        );

        y += 20.0f;


        DrawInt(
            13,
            L"Sample Count",
            mVolumetricFogLookDev.SampleCount
        );

        DrawFloat(
            14,
            L"Shadow Strength",
            mVolumetricFogLookDev.ShadowStrength
        );

        DrawFloat(
            15,
            L"Shadow Bias",
            mVolumetricFogLookDev.ShadowBias
        );


        //============================================================
        // Temporal / Upsampling
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Temporal / Upsampling ]"
        );

        y += 20.0f;


        DrawBool(
            16,
            L"Temporal",
            mVolumetricFogLookDev.TemporalEnabled
        );

        DrawFloat(
            17,
            L"History Weight",
            mVolumetricFogLookDev.TemporalHistoryWeight
        );

        DrawFloat(
            18,
            L"Temporal Response",
            mVolumetricFogLookDev.TemporalResponse
        );

        DrawBool(
            19,
            L"Depth-aware Upsample",
            mVolumetricFogLookDev.DepthAwareUpsamplingEnabled
        );

        DrawFloat(
            20,
            L"Upsample Depth Falloff",
            mVolumetricFogLookDev.UpsampleDepthFalloff
        );

        DrawBool(
            21,
            L"Blue Noise",
            mVolumetricFogLookDev.BlueNoiseEnabled
        );


        y += 20.0f;


        vnFont::print(
            10.0f,
            y,
            L"Enter: Toggle Bool   BackSpace: Reset"
        );
    }
    //============================================================
    // SSS Diffusion Page
    //============================================================
    if (mLookDevPage == LookDevPage::SSSDiffusion)
    {
        const bool enabled = PostProcessManager::IsEnabled(PostProcessManager::SSSDiffusion);


        vnFont::print(
            10.0f,
            y,
            L"SSS Diffusion : %s",
            enabled ? L"ON" : L"OFF"
        );

        y += 25.0f;


        auto DrawFloat = [this, &y](int index, const WCHAR* name, float value)
            {
                vnFont::print(
                    10.0f,
                    y,
                    index == mLookDevSelection ? L"> %-24s : %.4f" : L"  %-24s : %.4f",
                    name,
                    value
                );

                y += 20.0f;
            };


        auto DrawInt = [this, &y](int index, const WCHAR* name, int value)
            {
                vnFont::print(
                    10.0f,
                    y,
                    index == mLookDevSelection ? L"> %-24s : %d" : L"  %-24s : %d",
                    name,
                    value
                );
                y += 20.0f;
            };


        auto DrawBool = [this, &y](int index, const WCHAR* name, bool value)
            {
                vnFont::print(
                    10.0f,
                    y,
                    index == mLookDevSelection ? L"> %-24s : %s" : L"  %-24s : %s",
                    name,
                    value
                    ? L"ON" : L"OFF"
                );
                y += 20.0f;
            };


        //============================================================
        // Diffusion
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Diffusion ]"
        );

        y += 20.0f;


        DrawFloat(
            0,
            L"Strength",
            mSSSDiffusionLookDev.DiffusionStrength
        );

        DrawInt(
            1,
            L"Blur Radius",
            mSSSDiffusionLookDev.BlurRadius
        );

        DrawFloat(
            2,
            L"Depth Falloff",
            mSSSDiffusionLookDev.DepthFalloff
        );


        //============================================================
        // Scattering Profile
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Scattering Profile ]"
        );

        y += 20.0f;


        DrawFloat(
            3,
            L"Profile R",
            mSSSDiffusionLookDev.ProfileR
        );

        DrawFloat(
            4,
            L"Profile G",
            mSSSDiffusionLookDev.ProfileG
        );

        DrawFloat(
            5,
            L"Profile B",
            mSSSDiffusionLookDev.ProfileB
        );

        DrawFloat(
            6,
            L"Sample Quality",
            mSSSDiffusionLookDev.SampleQuality
        );

        DrawFloat(
            7,
            L"World Radius",
            mSSSDiffusionLookDev.WorldSpaceRadius
        );


        //============================================================
        // Temporal
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Temporal ]"
        );

        y += 20.0f;


        DrawBool(
            8,
            L"Temporal",
            mSSSDiffusionLookDev.TemporalEnabled
        );

        DrawFloat(
            9,
            L"History Weight",
            mSSSDiffusionLookDev.TemporalHistoryWeight
        );

        DrawFloat(
            10,
            L"Depth Threshold",
            mSSSDiffusionLookDev.TemporalDepthThreshold
        );

        DrawFloat(
            11,
            L"Motion Response",
            mSSSDiffusionLookDev.TemporalMotionResponse
        );


        //============================================================
        // Debug
        //============================================================
        static const WCHAR* debugNames[] =
        {
            L"None",
            L"Mask",
            L"Diffusion Source",
            L"Material Profile",
            L"Horizontal Blur",
            L"Vertical Blur",
            L"Temporal Resolved"
        };


        const int debugIndex =
            std::clamp(
                mSSSDiffusionLookDev.DebugView,
                0,
                6
            );


        vnFont::print(
            10.0f,
            y,
            mLookDevSelection == 12 ? L"> %-24s : %s" : L"  %-24s : %s",
            L"Debug View",
            debugNames[debugIndex]
        );

        y += 25.0f;


        vnFont::print(
            10.0f,
            y,
            L"Enter: Toggle Temporal   Left/Right: Value"
        );

        y += 20.0f;


        vnFont::print(
            10.0f,
            y,
            L"BackSpace: Reset SSS Diffusion"
        );
    }
    //============================================================
    // SSGI Page
    //============================================================
    if (mLookDevPage == LookDevPage::SSGI)
    {
        const bool enabled = PostProcessManager::IsEnabled(PostProcessManager::SSGI);


        vnFont::print(
            10.0f,
            y,
            L"SSGI : %s",
            enabled ? L"ON" : L"OFF"
        );

        y += 25.0f;


        auto DrawFloat = [this, &y](int index, const WCHAR* name, float value)
            {
                vnFont::print(
                    10.0f,
                    y,
                    index == mLookDevSelection ? L"> %-27s : %.4f" : L"  %-27s : %.4f",
                    name,
                    value
                );
                y += 20.0f;
            };


        auto DrawInt = [this, &y](int index, const WCHAR* name, int value)
            {
                vnFont::print(
                    10.0f,
                    y,
                    index == mLookDevSelection ? L"> %-27s : %d" : L"  %-27s : %d",
                    name,
                    value
                );
                y += 20.0f;
            };


        auto DrawBool = [this, &y](int index, const WCHAR* name, bool value)
            {
                vnFont::print(
                    10.0f,
                    y,
                    index == mLookDevSelection ? L"> %-27s : %s" : L"  %-27s : %s",
                    name,
                    value ? L"ON" : L"OFF"
                );
                y += 20.0f;
            };


        //============================================================
        // Quality
        //============================================================
        static const WCHAR* qualityNames[] =
        {
            L"Low",
            L"Medium",
            L"High",
            L"Custom"
        };


        const int quality =
            std::clamp(
                mSSGILookDev.QualityPreset,
                0,
                3
            );


        vnFont::print(
            10.0f,
            y,
            mLookDevSelection == 0 ? L"> %-27s : %s" : L"  %-27s : %s",
            L"Quality",
            qualityNames[quality]
        );

        y += 20.0f;


        //============================================================
        // Ray March
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Ray March ]"
        );

        y += 20.0f;


        DrawInt(1, L"Ray Count",
            mSSGILookDev.RayCount);

        DrawInt(2, L"Step Count",
            mSSGILookDev.StepCount);

        DrawFloat(3, L"Max Distance",
            mSSGILookDev.MaxDistance);

        DrawFloat(4, L"Thickness",
            mSSGILookDev.Thickness);

        DrawFloat(5, L"Normal Bias",
            mSSGILookDev.NormalBias);

        DrawFloat(6, L"Intensity",
            mSSGILookDev.Intensity);

        DrawFloat(7, L"Max Radiance",
            mSSGILookDev.MaxRadiance);


        //============================================================
        // Spatial Filter
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Spatial Filter ]"
        );

        y += 20.0f;


        DrawInt(8, L"Filter Radius",
            mSSGILookDev.FilterRadius);

        DrawFloat(9, L"Depth Falloff",
            mSSGILookDev.FilterDepthFalloff);

        DrawFloat(10, L"Normal Power",
            mSSGILookDev.FilterNormalPower);

        DrawFloat(11, L"Confidence Min",
            mSSGILookDev.FilterConfidenceMin);


        //============================================================
        // Temporal
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Temporal ]"
        );

        y += 20.0f;


        DrawBool(12, L"Temporal",
            mSSGILookDev.TemporalEnabled);

        DrawFloat(13, L"History Weight",
            mSSGILookDev.TemporalHistoryWeight);

        DrawFloat(14, L"World Position Threshold",
            mSSGILookDev.TemporalWorldPositionThreshold);

        DrawFloat(15, L"Normal Threshold",
            mSSGILookDev.TemporalNormalThreshold);

        DrawFloat(16, L"Motion Response",
            mSSGILookDev.TemporalMotionResponse);

        DrawFloat(17, L"Clamp Expand",
            mSSGILookDev.TemporalClampExpand);


        //============================================================
        // Upsampling
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Upsampling ]"
        );

        y += 20.0f;


        DrawFloat(18, L"Depth Falloff",
            mSSGILookDev.UpsampleDepthFalloff);

        DrawFloat(19, L"Normal Power",
            mSSGILookDev.UpsampleNormalPower);

        DrawFloat(20, L"Confidence Min",
            mSSGILookDev.UpsampleConfidenceMin);


        //============================================================
        // Multi Bounce
        //============================================================
        vnFont::print(
            10.0f,
            y,
            L"[ Multi Bounce ]"
        );

        y += 20.0f;


        DrawBool(21, L"Multi Bounce",
            mSSGILookDev.MultiBounceEnabled);

        DrawFloat(22, L"Bounce Strength",
            mSSGILookDev.MultiBounceStrength);


        //============================================================
        // Debug
        //============================================================
        static const WCHAR* debugNames[] =
        {
            L"None",
            L"Raw",
            L"Filtered",
            L"Temporal",
            L"Upsampled",
            L"Final Composite"
        };


        const int debug =
            std::clamp(
                mSSGILookDev.DebugView,
                0,
                5
            );


        vnFont::print(
            10.0f,
            y,
            mLookDevSelection == 23 ? L"> %-27s : %s" : L"  %-27s : %s",
            L"Debug View",
            debugNames[debug]
        );

        y += 25.0f;


        vnFont::print(
            10.0f,
            y,
            L"Enter: Toggle Bool   BackSpace: Reset"
        );
    }
}

void SceneCharacterRenderingTest::ApplyMaterialLookDevSettings()
{
    if (pSKRealHumanModel == nullptr)
    {
        return;
    }

    ModelBase* model = pSKRealHumanModel->GetModel();

    if (model == nullptr)
    {
        return;
    }


    constexpr int materialId = 0;

    //============================================================
    // PBR
    //============================================================
    model->SetPBREnabled(mMaterialLookDev.PBR, materialId);
    model->SetMetallic(mMaterialLookDev.Metallic, materialId);
    model->SetRoughness(mMaterialLookDev.Roughness, materialId);
    model->SetMaterialAO(mMaterialLookDev.AO, materialId);
    //============================================================
    // Normal Map
    //============================================================
    model->SetNormalMapStrength(mMaterialLookDev.NormalStrength, materialId);
    //============================================================
    // ORM
    //============================================================
    model->SetORMMapEnabled(mMaterialLookDev.ORM, materialId);
    model->SetORMScale(mMaterialLookDev.ORMAOScale, mMaterialLookDev.ORMRoughnessScale, mMaterialLookDev.ORMMetallicScale, materialId);
    //============================================================
    // IBL
    //============================================================
    model->SetEnvironmentReflection(mMaterialLookDev.Environment, mMaterialLookDev.EnvironmentDiffuse, mMaterialLookDev.EnvironmentSpecular, materialId);
    //============================================================
    // SSS
    //============================================================
    model->SetSubsurfaceEnabled(mMaterialLookDev.SSS, materialId);
    model->SetSubsurfaceStrength(mMaterialLookDev.SSSStrength, materialId);
    model->SetSubsurfaceThickness(mMaterialLookDev.SSSThickness, materialId);
    model->SetSubsurfaceDistortion(mMaterialLookDev.SSSDistortion, materialId);
    model->SetSubsurfacePower(mMaterialLookDev.SSSPower, materialId);

    //============================================================
    // Thickness Map
    //============================================================
    model->SetSubsurfaceThicknessMapEnabled(mMaterialLookDev.ThicknessMap, materialId);

    //============================================================
    // SkinMask
    //============================================================
    model->SetSubsurfaceSkinMaskEnabled(mMaterialLookDev.SkinMask, materialId);
}

void SceneCharacterRenderingTest::ResetMaterialLookDev()
{
    //============================================================
    // Structで定義したDefault値へ戻す
    //============================================================
    mMaterialLookDev = MaterialLookDevSettings{};

    //============================================================
    // Sophiaへ即時反映
    //============================================================
    ApplyMaterialLookDevSettings();
}

void SceneCharacterRenderingTest::ResetPostLookDev()
{
    //============================================================
   // Default値
   //============================================================
    mPostLookDev = PostLookDevSettings{};

    //============================================================
    // PostProcessへ即時反映
    //============================================================
    PostProcessManager::SetBrightness(mPostLookDev.Brightness);
    PostProcessManager::SetContrast(mPostLookDev.Contrast);
    PostProcessManager::SetSaturation(mPostLookDev.Saturation);
    PostProcessManager::SetVignette(
        mPostLookDev.VignetteStart,
        mPostLookDev.VignetteEnd,
        mPostLookDev.VignetteStrength
    );
    PostProcessManager::SetChromaticStrength(mPostLookDev.ChromaticStrength);
    PostProcessManager::SetExposure(mPostLookDev.Exposure);
    PostProcessManager::SetBloomIntensity(mPostLookDev.BloomIntensity);
    PostProcessManager::SetGamma(mPostLookDev.Gamma);
}

void SceneCharacterRenderingTest::TogglePostEffect(PostProcessManager::eEffectMask effect)
{
    if (PostProcessManager::IsEnabled(effect))
    {
        PostProcessManager::Disable(effect);
    }
    else
    {
        PostProcessManager::Enable(effect);

        //========================================================
        // Temporal系Effectは、
        // 再度ONにした瞬間に古いHistoryを使わないようResetする
        //========================================================
        if ((effect & PostProcessManager::SSSDiffusion) != 0)
        {
            PostProcessManager::ResetSSSTemporalHistory();
        }


        if ((effect & PostProcessManager::SSGI) != 0)
        {
            PostProcessManager::ResetSSGITemporalHistory();
        }


        if ((effect & PostProcessManager::VolumetricFog) != 0)
        {
            PostProcessManager::ResetVolumetricFogTemporalHistory();
        }
    }
}

void SceneCharacterRenderingTest::UpdateMotionBlurLookDev()
{
    //============================================================
    // Motion Blur Inspector項目
    //
    // 0 : Strength
    // 1 : Max Pixels
    // 2 : Sample Count
    // 3 : Depth Threshold
    // 4 : Velocity Threshold
    //============================================================
    constexpr int itemCount = 5;


    //============================================================
    // ↑ : 前の項目
    //============================================================
    if (vnKeyboard::trg(DIK_UP))
    {
        --mLookDevSelection;


        if (mLookDevSelection < 0)
        {
            mLookDevSelection =
                itemCount - 1;
        }
    }


    //============================================================
    // ↓ : 次の項目
    //============================================================
    if (vnKeyboard::trg(DIK_DOWN))
    {
        ++mLookDevSelection;


        if (mLookDevSelection >= itemCount)
        {
            mLookDevSelection = 0;
        }
    }


    //============================================================
    // 調整倍率
    //
    // Shift:
    //   大きく変更
    //
    // Ctrl:
    //   細かく変更
    //============================================================
    float multiplier = 1.0f;


    if (vnKeyboard::on(DIK_LSHIFT))
    {
        multiplier = 5.0f;
    }
    else if (vnKeyboard::on(DIK_LCONTROL))
    {
        multiplier = 0.2f;
    }


    //============================================================
    // ← / →
    //============================================================
    float direction = 0.0f;


    if (vnKeyboard::trg(DIK_LEFT))
    {
        direction = -1.0f;
    }


    if (vnKeyboard::trg(DIK_RIGHT))
    {
        direction = 1.0f;
    }


    if (direction == 0.0f)
    {
        //========================================================
        // BackSpace:
        // Motion Blur設定を初期値へ戻す
        //========================================================
        if (vnKeyboard::trg(DIK_BACKSPACE))
        {
            ResetMotionBlurLookDev();
        }

        return;
    }


    //============================================================
    // Parameter変更
    //============================================================
    switch (mLookDevSelection)
    {
        //========================================================
        // Strength
        //========================================================
    case 0:
    {
        mMotionBlurLookDev.Strength +=
            direction * 0.1f * multiplier;


        mMotionBlurLookDev.Strength =
            std::clamp(
                mMotionBlurLookDev.Strength,
                0.0f,
                5.0f
            );

        break;
    }

    //========================================================
    // Max Blur Pixels
    //========================================================
    case 1:
    {
        mMotionBlurLookDev.MaxPixels +=
            direction * 2.0f * multiplier;


        mMotionBlurLookDev.MaxPixels =
            std::clamp(
                mMotionBlurLookDev.MaxPixels,
                0.0f,
                128.0f
            );

        break;
    }


    //========================================================
    // Sample Count
    // intなのでfloat multiplierは使わず、
    // Shift時だけ大きく変更する
    //========================================================
    case 2:
    {
        int step = 1;

        if (vnKeyboard::on(DIK_LSHIFT))
        {
            step = 4;
        }


        if (direction > 0.0f)
        {
            mMotionBlurLookDev.SampleCount += step;
        }
        else
        {
            mMotionBlurLookDev.SampleCount -= step;
        }


        mMotionBlurLookDev.SampleCount =
            std::clamp(
                mMotionBlurLookDev.SampleCount,
                1,
                64
            );

        break;
    }


    //========================================================
    // Depth Threshold
    //========================================================
    case 3:
    {
        mMotionBlurLookDev.DepthThreshold +=
            direction * 0.002f * multiplier;

        mMotionBlurLookDev.DepthThreshold =
            std::clamp(
                mMotionBlurLookDev.DepthThreshold,
                0.0f,
                1.0f
            );

        break;
    }


    //========================================================
    // Velocity Threshold
    //========================================================
    case 4:
    {
        mMotionBlurLookDev.VelocityThreshold +=
            direction * 0.0005f * multiplier;

        mMotionBlurLookDev.VelocityThreshold =
            std::clamp(
                mMotionBlurLookDev.VelocityThreshold,
                0.0f,
                1.0f
            );

        break;
    }
    }

    //============================================================
    // 値を変更したFrameだけGPU側設定へ反映
    //============================================================
    ApplyMotionBlurLookDevSettings();
}

void SceneCharacterRenderingTest::ApplyMotionBlurLookDevSettings()
{
    //============================================================
    // Motion Blur Strength
    //============================================================
    PostProcessManager::SetMotionBlurStrength(mMotionBlurLookDev.Strength);
    //============================================================
    // Maximum Blur Width
    //============================================================
    PostProcessManager::SetMotionBlurMaxPixels(mMotionBlurLookDev.MaxPixels);
    //============================================================
    // Sample Count
    //============================================================
    PostProcessManager::SetMotionBlurSampleCount(mMotionBlurLookDev.SampleCount);
    //============================================================
    // Depth Reject
    //============================================================
    PostProcessManager::SetMotionBlurDepthThreshold(mMotionBlurLookDev.DepthThreshold);
    //============================================================
    // Velocity Reject
    //============================================================
    PostProcessManager::SetMotionBlurVelocityThreshold(mMotionBlurLookDev.VelocityThreshold);
}

void SceneCharacterRenderingTest::ResetMotionBlurLookDev()
{
    //============================================================
    // 初期値へ戻す
    //============================================================
    mMotionBlurLookDev.Strength = 1.0f;
    mMotionBlurLookDev.MaxPixels = 32.0f;
    mMotionBlurLookDev.SampleCount = 8;
    mMotionBlurLookDev.DepthThreshold = 0.01f;
    mMotionBlurLookDev.VelocityThreshold = 0.001f;
    //============================================================
    // PostProcessManagerへ即時反映
    //============================================================
    ApplyMotionBlurLookDevSettings();
}

void SceneCharacterRenderingTest::UpdateVolumetricFogLookDev()
{
    //============================================================
    // Fog Inspector項目数
    //============================================================
    constexpr int itemCount = 22;


    //============================================================
    // ↑ / ↓ : 項目選択
    //============================================================
    if (vnKeyboard::trg(DIK_UP))
    {
        --mLookDevSelection;


        if (mLookDevSelection < 0)
        {
            mLookDevSelection =
                itemCount - 1;
        }
    }


    if (vnKeyboard::trg(DIK_DOWN))
    {
        ++mLookDevSelection;


        if (mLookDevSelection >= itemCount)
        {
            mLookDevSelection = 0;
        }
    }


    //============================================================
    // BackSpace:
    // Fog設定をLookDev初期値へ戻す
    //============================================================
    if (vnKeyboard::trg(DIK_BACKSPACE))
    {
        ResetVolumetricFogLookDev();

        return;
    }


    //============================================================
    // Bool Parameter
    //
    // Enter:
    //   ON / OFF
    //============================================================
    bool changed = false;


    if (vnKeyboard::trg(DIK_RETURN))
    {
        switch (mLookDevSelection)
        {
            //========================================================
            // Temporal
            //========================================================
        case 16:

            mVolumetricFogLookDev.TemporalEnabled = !mVolumetricFogLookDev.TemporalEnabled;

            changed = true;

            break;


            //========================================================
            // Depth-aware Upsampling
            //========================================================
        case 19:

            mVolumetricFogLookDev.DepthAwareUpsamplingEnabled = !mVolumetricFogLookDev.DepthAwareUpsamplingEnabled;

            changed = true;

            break;


            //========================================================
            // Blue Noise
            //========================================================
        case 21:

            mVolumetricFogLookDev.BlueNoiseEnabled = !mVolumetricFogLookDev.BlueNoiseEnabled;

            changed = true;

            break;
        }
    }


    //============================================================
    // Shift / Ctrlによる調整速度
    //============================================================
    float multiplier = 1.0f;


    if (vnKeyboard::on(DIK_LSHIFT))
    {
        multiplier = 5.0f;
    }
    else if (vnKeyboard::on(DIK_LCONTROL))
    {
        multiplier = 0.2f;
    }


    //============================================================
    // ← / →
    //============================================================
    float direction = 0.0f;


    if (vnKeyboard::trg(DIK_LEFT))
    {
        direction = -1.0f;
    }


    if (vnKeyboard::trg(DIK_RIGHT))
    {
        direction = 1.0f;
    }


    //============================================================
    // 数値Parameter
    //============================================================
    if (direction != 0.0f)
    {
        switch (mLookDevSelection)
        {
            //========================================================
            // Density
            //========================================================
        case 0:

            mVolumetricFogLookDev.Density +=
                direction * 0.005f * multiplier;

            mVolumetricFogLookDev.Density =
                std::clamp(
                    mVolumetricFogLookDev.Density,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Maximum Distance
            //========================================================
        case 1:

            mVolumetricFogLookDev.MaxDistance +=
                direction * 5.0f * multiplier;

            mVolumetricFogLookDev.MaxDistance =
                std::clamp(
                    mVolumetricFogLookDev.MaxDistance,
                    1.0f,
                    500.0f
                );

            changed = true;

            break;


            //========================================================
            // Fog Color RGB
            //========================================================
        case 2:

            mVolumetricFogLookDev.ColorR +=
                direction * 0.05f * multiplier;

            mVolumetricFogLookDev.ColorR =
                std::clamp(
                    mVolumetricFogLookDev.ColorR,
                    0.0f,
                    4.0f
                );

            changed = true;

            break;


        case 3:

            mVolumetricFogLookDev.ColorG +=
                direction * 0.05f * multiplier;

            mVolumetricFogLookDev.ColorG =
                std::clamp(
                    mVolumetricFogLookDev.ColorG,
                    0.0f,
                    4.0f
                );

            changed = true;

            break;


        case 4:

            mVolumetricFogLookDev.ColorB +=
                direction * 0.05f * multiplier;

            mVolumetricFogLookDev.ColorB =
                std::clamp(
                    mVolumetricFogLookDev.ColorB,
                    0.0f,
                    4.0f
                );

            changed = true;

            break;


            //========================================================
            // Anisotropy
            // -1に近い: 後方散乱
            // +1に近い: 強い前方散乱
            //========================================================
        case 5:

            mVolumetricFogLookDev.Anisotropy +=
                direction * 0.05f * multiplier;

            mVolumetricFogLookDev.Anisotropy =
                std::clamp(
                    mVolumetricFogLookDev.Anisotropy,
                    -0.95f,
                    0.95f
                );

            changed = true;

            break;


            //========================================================
            // Base Height
            //========================================================
        case 6:

            mVolumetricFogLookDev.BaseHeight +=
                direction * 0.25f * multiplier;

            changed = true;

            break;


            //========================================================
            // Height Falloff
            //========================================================
        case 7:

            mVolumetricFogLookDev.HeightFalloff +=
                direction * 0.05f * multiplier;

            mVolumetricFogLookDev.HeightFalloff =
                std::clamp(
                    mVolumetricFogLookDev.HeightFalloff,
                    0.0f,
                    10.0f
                );

            changed = true;

            break;


            //========================================================
            // Scattering Albedo
            //========================================================
        case 8:

            mVolumetricFogLookDev.ScatteringAlbedo +=
                direction * 0.05f * multiplier;

            mVolumetricFogLookDev.ScatteringAlbedo =
                std::clamp(
                    mVolumetricFogLookDev.ScatteringAlbedo,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Directional
            //========================================================
        case 9:

            mVolumetricFogLookDev.DirectionalStrength +=
                direction * 0.1f * multiplier;

            mVolumetricFogLookDev.DirectionalStrength =
                std::clamp(
                    mVolumetricFogLookDev.DirectionalStrength,
                    0.0f,
                    10.0f
                );

            changed = true;

            break;


            //========================================================
            // Ambient
            //========================================================
        case 10:

            mVolumetricFogLookDev.AmbientStrength +=
                direction * 0.1f * multiplier;

            mVolumetricFogLookDev.AmbientStrength =
                std::clamp(
                    mVolumetricFogLookDev.AmbientStrength,
                    0.0f,
                    10.0f
                );

            changed = true;

            break;


            //========================================================
            // Spot Light
            //========================================================
        case 11:

            mVolumetricFogLookDev.SpotLightStrength +=
                direction * 0.1f * multiplier;

            mVolumetricFogLookDev.SpotLightStrength =
                std::clamp(
                    mVolumetricFogLookDev.SpotLightStrength,
                    0.0f,
                    10.0f
                );

            changed = true;

            break;


            //========================================================
            // Point Light
            //========================================================
        case 12:

            mVolumetricFogLookDev.PointLightStrength +=
                direction * 0.1f * multiplier;

            mVolumetricFogLookDev.PointLightStrength =
                std::clamp(
                    mVolumetricFogLookDev.PointLightStrength,
                    0.0f,
                    10.0f
                );

            changed = true;

            break;


            //========================================================
            // Ray March Sample Count
            //========================================================
        case 13:
        {
            int step = 1;


            if (vnKeyboard::on(DIK_LSHIFT))
            {
                step = 4;
            }


            if (direction > 0.0f)
            {
                mVolumetricFogLookDev.SampleCount += step;
            }
            else
            {
                mVolumetricFogLookDev.SampleCount -= step;
            }


            mVolumetricFogLookDev.SampleCount =
                std::clamp(
                    mVolumetricFogLookDev.SampleCount,
                    4,
                    128
                );

            changed = true;

            break;
        }


        //========================================================
        // Shadow Strength
        //========================================================
        case 14:

            mVolumetricFogLookDev.ShadowStrength +=
                direction * 0.05f * multiplier;

            mVolumetricFogLookDev.ShadowStrength =
                std::clamp(
                    mVolumetricFogLookDev.ShadowStrength,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Shadow Bias
            //========================================================
        case 15:

            mVolumetricFogLookDev.ShadowBias +=
                direction * 0.0005f * multiplier;

            mVolumetricFogLookDev.ShadowBias =
                std::clamp(
                    mVolumetricFogLookDev.ShadowBias,
                    0.0f,
                    0.1f
                );

            changed = true;

            break;


            //========================================================
            // Temporal History Weight
            //========================================================
        case 17:

            mVolumetricFogLookDev.TemporalHistoryWeight +=
                direction * 0.02f * multiplier;

            mVolumetricFogLookDev.TemporalHistoryWeight =
                std::clamp(
                    mVolumetricFogLookDev.TemporalHistoryWeight,
                    0.0f,
                    0.99f
                );

            changed = true;

            break;


            //========================================================
            // Temporal Response
            //========================================================
        case 18:

            mVolumetricFogLookDev.TemporalResponse +=
                direction * 0.1f * multiplier;

            mVolumetricFogLookDev.TemporalResponse =
                std::clamp(
                    mVolumetricFogLookDev.TemporalResponse,
                    0.0f,
                    10.0f
                );

            changed = true;

            break;


            //========================================================
            // Upsample Depth Falloff
            //========================================================
        case 20:

            mVolumetricFogLookDev.UpsampleDepthFalloff +=
                direction * 2.0f * multiplier;

            mVolumetricFogLookDev.UpsampleDepthFalloff =
                std::clamp(
                    mVolumetricFogLookDev.UpsampleDepthFalloff,
                    0.0f,
                    256.0f
                );

            changed = true;

            break;
        }
    }


    if (!changed)
    {
        return;
    }

    //============================================================
    // Fog Passへ即時反映
    //============================================================
    ApplyVolumetricFogLookDevSettings();

    //============================================================
    // Temporal履歴を破棄
    // Fog Parameterを大きく変更した直後に、
    // 古いFog結果が混ざって見えるのを防ぐ
    //============================================================
    PostProcessManager::ResetVolumetricFogTemporalHistory();
}

void SceneCharacterRenderingTest::ApplyVolumetricFogLookDevSettings()
{
    //============================================================
    // Basic
    //============================================================
    PostProcessManager::SetVolumetricFogDensity(mVolumetricFogLookDev.Density);
    PostProcessManager::SetVolumetricFogMaxDistance(mVolumetricFogLookDev.MaxDistance);
    PostProcessManager::SetVolumetricFogColor(
        mVolumetricFogLookDev.ColorR,
        mVolumetricFogLookDev.ColorG,
        mVolumetricFogLookDev.ColorB
    );
    PostProcessManager::SetVolumetricFogAnisotropy(mVolumetricFogLookDev.Anisotropy);
    PostProcessManager::SetVolumetricFogHeight(mVolumetricFogLookDev.BaseHeight, mVolumetricFogLookDev.HeightFalloff);
    PostProcessManager::SetVolumetricFogScatteringAlbedo(mVolumetricFogLookDev.ScatteringAlbedo);
    //============================================================
    // Lighting
    //============================================================
    PostProcessManager::SetVolumetricFogDirectionalStrength(mVolumetricFogLookDev.DirectionalStrength);
    PostProcessManager::SetVolumetricFogAmbientStrength(mVolumetricFogLookDev.AmbientStrength);
    PostProcessManager::SetVolumetricFogSpotLightStrength(mVolumetricFogLookDev.SpotLightStrength);
    PostProcessManager::SetVolumetricFogPointLightStrength(mVolumetricFogLookDev.PointLightStrength);
    //============================================================
    // Ray March
    //============================================================
    PostProcessManager::SetVolumetricFogSampleCount(mVolumetricFogLookDev.SampleCount);
    //============================================================
    // Shadow
    //============================================================
    PostProcessManager::SetVolumetricFogShadowStrength(mVolumetricFogLookDev.ShadowStrength);
    PostProcessManager::SetVolumetricFogShadowBias(mVolumetricFogLookDev.ShadowBias);
    //============================================================
    // Temporal
    //============================================================
    PostProcessManager::SetVolumetricFogTemporalEnabled(mVolumetricFogLookDev.TemporalEnabled);
    PostProcessManager::SetVolumetricFogTemporalHistoryWeight(mVolumetricFogLookDev.TemporalHistoryWeight);
    PostProcessManager::SetVolumetricFogTemporalResponse(mVolumetricFogLookDev.TemporalResponse);
    //============================================================
    // Upsampling
    //============================================================
    PostProcessManager::SetVolumetricFogDepthAwareUpsamplingEnabled(mVolumetricFogLookDev.DepthAwareUpsamplingEnabled);
    PostProcessManager::SetVolumetricFogUpsampleDepthFalloff(mVolumetricFogLookDev.UpsampleDepthFalloff);
    //============================================================
    // Blue Noise
    //============================================================
    PostProcessManager::SetVolumetricFogBlueNoiseEnabled(mVolumetricFogLookDev.BlueNoiseEnabled);
}

void SceneCharacterRenderingTest::ResetVolumetricFogLookDev()
{
    //============================================================
    // LookDev Scene Default
    //============================================================
    mVolumetricFogLookDev = VolumetricFogLookDevSettings{};

    //============================================================
    // Fogへ反映
    //============================================================
    ApplyVolumetricFogLookDevSettings();

    //============================================================
    // 古いTemporal Historyを破棄
    //============================================================
    PostProcessManager:: ResetVolumetricFogTemporalHistory();
}

void SceneCharacterRenderingTest::UpdateSSSDiffusionLookDev()
{
    constexpr int itemCount = 13;


    //============================================================
    // ↑ / ↓
    //============================================================
    if (vnKeyboard::trg(DIK_UP))
    {
        --mLookDevSelection;

        if (mLookDevSelection < 0)
        {
            mLookDevSelection =
                itemCount - 1;
        }
    }


    if (vnKeyboard::trg(DIK_DOWN))
    {
        ++mLookDevSelection;

        if (mLookDevSelection >= itemCount)
        {
            mLookDevSelection = 0;
        }
    }


    //============================================================
    // Reset
    //============================================================
    if (vnKeyboard::trg(DIK_BACKSPACE))
    {
        ResetSSSDiffusionLookDev();

        return;
    }


    bool changed = false;


    //============================================================
    // Enter:
    // Bool Parameter
    //============================================================
    if (vnKeyboard::trg(DIK_RETURN))
    {
        if (mLookDevSelection == 8)
        {
            mSSSDiffusionLookDev.TemporalEnabled =
                !mSSSDiffusionLookDev.TemporalEnabled;

            changed = true;
        }
    }


    //============================================================
    // 調整倍率
    //============================================================
    float multiplier = 1.0f;


    if (vnKeyboard::on(DIK_LSHIFT))
    {
        multiplier = 5.0f;
    }
    else if (vnKeyboard::on(DIK_LCONTROL))
    {
        multiplier = 0.2f;
    }


    //============================================================
    // ← / →
    //============================================================
    float direction = 0.0f;


    if (vnKeyboard::trg(DIK_LEFT))
    {
        direction = -1.0f;
    }


    if (vnKeyboard::trg(DIK_RIGHT))
    {
        direction = 1.0f;
    }


    if (direction != 0.0f)
    {
        switch (mLookDevSelection)
        {
            //========================================================
            // Diffusion Strength
            //========================================================
        case 0:

            mSSSDiffusionLookDev.DiffusionStrength +=
                direction * 0.05f * multiplier;

            mSSSDiffusionLookDev.DiffusionStrength =
                std::clamp(
                    mSSSDiffusionLookDev.DiffusionStrength,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Blur Radius
            //========================================================
        case 1:
        {
            const int step = vnKeyboard::on(DIK_LSHIFT) ? 2 : 1;


            mSSSDiffusionLookDev.BlurRadius +=
                direction > 0.0f ? step : -step;


            mSSSDiffusionLookDev.BlurRadius =
                std::clamp(
                    mSSSDiffusionLookDev.BlurRadius,
                    1,
                    12
                );

            changed = true;

            break;
        }


        //========================================================
        // Depth Falloff
        //========================================================
        case 2:

            mSSSDiffusionLookDev.DepthFalloff +=
                direction * 0.5f * multiplier;

            mSSSDiffusionLookDev.DepthFalloff =
                std::clamp(
                    mSSSDiffusionLookDev.DepthFalloff,
                    0.0f,
                    100.0f
                );

            changed = true;

            break;


            //========================================================
            // Scattering R
            //========================================================
        case 3:

            mSSSDiffusionLookDev.ProfileR +=
                direction * 0.05f * multiplier;

            mSSSDiffusionLookDev.ProfileR =
                std::clamp(
                    mSSSDiffusionLookDev.ProfileR,
                    0.01f,
                    4.0f
                );

            changed = true;

            break;


            //========================================================
            // Scattering G
            //========================================================
        case 4:

            mSSSDiffusionLookDev.ProfileG +=
                direction * 0.05f * multiplier;

            mSSSDiffusionLookDev.ProfileG =
                std::clamp(
                    mSSSDiffusionLookDev.ProfileG,
                    0.01f,
                    4.0f
                );

            changed = true;

            break;


            //========================================================
            // Scattering B
            //========================================================
        case 5:

            mSSSDiffusionLookDev.ProfileB +=
                direction * 0.05f * multiplier;

            mSSSDiffusionLookDev.ProfileB =
                std::clamp(
                    mSSSDiffusionLookDev.ProfileB,
                    0.01f,
                    4.0f
                );

            changed = true;

            break;


            //========================================================
            // Sample Quality
            //========================================================
        case 6:

            mSSSDiffusionLookDev.SampleQuality +=
                direction * 0.05f * multiplier;

            mSSSDiffusionLookDev.SampleQuality =
                std::clamp(
                    mSSSDiffusionLookDev.SampleQuality,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // World-Space Radius
            // Characterは小さい値でもかなり見た目が変わるため、
            // Stepを細かくする
            //========================================================
        case 7:

            mSSSDiffusionLookDev.WorldSpaceRadius +=
                direction * 0.002f * multiplier;

            mSSSDiffusionLookDev.WorldSpaceRadius =
                std::clamp(
                    mSSSDiffusionLookDev.WorldSpaceRadius,
                    0.0f,
                    0.5f
                );

            changed = true;

            break;


            //========================================================
            // Temporal EnabledはEnterで変更
            //========================================================
        case 8:

            break;

            //========================================================
            // Temporal History Weight
            //========================================================
        case 9:

            mSSSDiffusionLookDev.TemporalHistoryWeight +=
                direction * 0.02f * multiplier;

            mSSSDiffusionLookDev.TemporalHistoryWeight =
                std::clamp(
                    mSSSDiffusionLookDev.TemporalHistoryWeight,
                    0.0f,
                    0.98f
                );

            changed = true;

            break;


            //========================================================
            // Temporal Depth Threshold
            //========================================================
        case 10:

            mSSSDiffusionLookDev.TemporalDepthThreshold +=
                direction * 0.005f * multiplier;

            mSSSDiffusionLookDev.TemporalDepthThreshold =
                std::clamp(
                    mSSSDiffusionLookDev.TemporalDepthThreshold,
                    0.0001f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Temporal Motion Response
            //========================================================
        case 11:

            mSSSDiffusionLookDev.TemporalMotionResponse +=
                direction * 2.0f * multiplier;

            mSSSDiffusionLookDev.TemporalMotionResponse =
                std::clamp(
                    mSSSDiffusionLookDev.TemporalMotionResponse,
                    0.0f,
                    200.0f
                );

            changed = true;

            break;


            //========================================================
            // Debug View
            //========================================================
        case 12:

            if (direction > 0.0f)
            {
                ++mSSSDiffusionLookDev.DebugView;
            }
            else
            {
                --mSSSDiffusionLookDev.DebugView;
            }


            if (mSSSDiffusionLookDev.DebugView > 6)
            {
                mSSSDiffusionLookDev.DebugView = 0;
            }


            if (mSSSDiffusionLookDev.DebugView < 0)
            {
                mSSSDiffusionLookDev.DebugView = 6;
            }

            ApplySSSDebugView();

            break;
        }
    }


    //============================================================
    // Parameter変更
    //============================================================
    if (changed)
    {
        ApplySSSDiffusionLookDevSettings();

        //========================================================
        // Parameter変更前のHistoryを混ぜない
        // 特にRadius / Profile / Strength変更時に、
        // 数Frameだけ旧設定が残るのを防止
        //========================================================
        PostProcessManager::ResetSSSTemporalHistory();
    }
}

void SceneCharacterRenderingTest::ApplySSSDiffusionLookDevSettings()
{
    //============================================================
    // Diffusion Strength
    //============================================================
    PostProcessManager::SetSSSDiffusionStrength(mSSSDiffusionLookDev.DiffusionStrength);
    //============================================================
    // Blur Radius
    //============================================================
    PostProcessManager::SetSSSBlurRadius(mSSSDiffusionLookDev.BlurRadius);
    //============================================================
    // Depth-aware Blur
    //============================================================
    PostProcessManager::SetSSSDepthFalloff(mSSSDiffusionLookDev.DepthFalloff);
    //============================================================
    // RGB Scattering Profile
    //============================================================
    PostProcessManager::SetSSSScatteringProfile(
        mSSSDiffusionLookDev.ProfileR,
        mSSSDiffusionLookDev.ProfileG,
        mSSSDiffusionLookDev.ProfileB
    );
    //============================================================
    // Sampling Quality
    //============================================================
    PostProcessManager::SetSSSSampleQuality(mSSSDiffusionLookDev.SampleQuality);
    //============================================================
    // World-Space Radius
    //============================================================
    PostProcessManager::SetSSSWorldSpaceRadius(mSSSDiffusionLookDev.WorldSpaceRadius);
    //============================================================
    // Temporal
    //============================================================
    PostProcessManager::SetSSSTemporalEnabled(mSSSDiffusionLookDev.TemporalEnabled);
    PostProcessManager::SetSSSTemporalHistoryWeight(mSSSDiffusionLookDev.TemporalHistoryWeight);
    PostProcessManager::SetSSSTemporalDepthThreshold(mSSSDiffusionLookDev.TemporalDepthThreshold);
    PostProcessManager::SetSSSTemporalMotionResponse(mSSSDiffusionLookDev.TemporalMotionResponse);
}

void SceneCharacterRenderingTest::ApplySSSDebugView()
{
    using DebugView = SubsurfaceScatteringPass::DebugView;


    switch (mSSSDiffusionLookDev.DebugView)
    {
        //========================================================
        // 通常描画
        //========================================================
    case 0:
        PostProcessManager::SetSSSDebugView(DebugView::None);
        break;


        //========================================================
        // SSS Mask
        // 白: SSSが強く適用される場所
        // 黒: SSSなし
        //========================================================
    case 1:
        PostProcessManager::SetSSSDebugView(DebugView::Mask);
        break;


        //========================================================
        // Blur前のDiffusion Source
        // PBR Diffuse + Transmission SSS
        //========================================================
    case 2:
        PostProcessManager::SetSSSDebugView(DebugView::DiffusionSource);
        break;


        //========================================================
        // MaterialごとのScattering Profile
        //========================================================
    case 3:
        PostProcessManager::SetSSSDebugView(DebugView::MaterialProfile);
        break;


        //========================================================
        // Horizontal Blur結果
        //========================================================
    case 4:
        PostProcessManager::SetSSSDebugView(DebugView::HorizontalBlur);
        break;

        //========================================================
        // Vertical Blur結果
        //========================================================
    case 5:
        PostProcessManager::SetSSSDebugView(DebugView::VerticalBlur);
        break;


        //========================================================
        // Temporal Resolve後
        //========================================================
    case 6:
        PostProcessManager::SetSSSDebugView(DebugView::TemporalResolved);
        break;


    default:

        mSSSDiffusionLookDev.DebugView = 0;

        PostProcessManager::SetSSSDebugView(DebugView::None);

        break;
    }
}

void SceneCharacterRenderingTest::ResetSSSDiffusionLookDev()
{
    //============================================================
    // Struct Defaultへ戻す
    //============================================================
    mSSSDiffusionLookDev = SSSDiffusionLookDevSettings{};

    //============================================================
    //通常表示へ戻す
    //============================================================
    ApplySSSDebugView();

    //============================================================
    // Passへ反映
    //============================================================
    ApplySSSDiffusionLookDevSettings();

    //============================================================
    // Temporal History破棄
    //============================================================
    PostProcessManager::ResetSSSTemporalHistory();
}

void SceneCharacterRenderingTest::UpdateSSGILookDev()
{
    //============================================================
    // SSGI Inspector
    //
    //  0 Quality
    //
    // Ray March
    //  1 Ray Count
    //  2 Step Count
    //  3 Max Distance
    //  4 Thickness
    //  5 Normal Bias
    //  6 Intensity
    //  7 Max Radiance
    //
    // Spatial Filter
    //  8 Filter Radius
    //  9 Filter Depth Falloff
    // 10 Filter Normal Power
    // 11 Filter Confidence Min
    //
    // Temporal
    // 12 Temporal Enabled
    // 13 History Weight
    // 14 World Position Threshold
    // 15 Normal Threshold
    // 16 Motion Response
    // 17 Clamp Expand
    //
    // Upsampling
    // 18 Depth Falloff
    // 19 Normal Power
    // 20 Confidence Min
    //
    // Multi Bounce
    // 21 Multi Bounce Enabled
    // 22 Multi Bounce Strength
    //
    // Debug
    // 23 Debug View
    //============================================================
    constexpr int itemCount = 24;


    //============================================================
    // Selection
    //============================================================
    if (vnKeyboard::trg(DIK_UP))
    {
        --mLookDevSelection;

        if (mLookDevSelection < 0)
        {
            mLookDevSelection = itemCount - 1;
        }
    }


    if (vnKeyboard::trg(DIK_DOWN))
    {
        ++mLookDevSelection;

        if (mLookDevSelection >= itemCount)
        {
            mLookDevSelection = 0;
        }
    }


    //============================================================
    // Reset
    //============================================================
    if (vnKeyboard::trg(DIK_BACKSPACE))
    {
        ResetSSGILookDev();

        return;
    }


    bool changed = false;


    //============================================================
    // Bool Parameter
    //============================================================
    if (vnKeyboard::trg(DIK_RETURN))
    {
        switch (mLookDevSelection)
        {
        case 12:

            mSSGILookDev.TemporalEnabled = !mSSGILookDev.TemporalEnabled;

            changed = true;

            break;


        case 21:

            mSSGILookDev.MultiBounceEnabled = !mSSGILookDev.MultiBounceEnabled;

            changed = true;

            break;
        }
    }


    //============================================================
    // Step倍率
    //============================================================
    float multiplier = 1.0f;


    if (vnKeyboard::on(DIK_LSHIFT))
    {
        multiplier = 5.0f;
    }
    else if (vnKeyboard::on(DIK_LCONTROL))
    {
        multiplier = 0.2f;
    }


    //============================================================
    // Left / Right
    //============================================================
    float direction = 0.0f;


    if (vnKeyboard::trg(DIK_LEFT))
    {
        direction = -1.0f;
    }


    if (vnKeyboard::trg(DIK_RIGHT))
    {
        direction = 1.0f;
    }


    if (direction != 0.0f)
    {
        switch (mLookDevSelection)
        {
            //========================================================
            // Quality Preset
            //========================================================
        case 0:
        {
            if (direction > 0.0f)
            {
                ++mSSGILookDev.QualityPreset;
            }
            else
            {
                --mSSGILookDev.QualityPreset;
            }


            //====================================================
            // Quality選択時はCustomを飛ばす
            // Customは個別Parameter変更時だけ使用する
            //====================================================
            if (mSSGILookDev.QualityPreset > 2)
            {
                mSSGILookDev.QualityPreset = 0;
            }

            if (mSSGILookDev.QualityPreset < 0)
            {
                mSSGILookDev.QualityPreset = 2;
            }


            ApplySSGIQualityPreset();

            return;
        }


        //========================================================
        // Ray Count
        //========================================================
        case 1:
        {
            mSSGILookDev.RayCount +=
                direction > 0.0f ? 1 : -1;

            mSSGILookDev.RayCount =
                std::clamp(
                    mSSGILookDev.RayCount,
                    1,
                    8
                );

            changed = true;

            break;
        }


        //========================================================
        // Step Count
        //========================================================
        case 2:
        {
            const int step = vnKeyboard::on(DIK_LSHIFT) ? 4 : 1;


            mSSGILookDev.StepCount +=
                direction > 0.0f ? step : -step;


            mSSGILookDev.StepCount =
                std::clamp(
                    mSSGILookDev.StepCount,
                    1,
                    32
                );

            changed = true;

            break;
        }


        //========================================================
        // Max Distance
        //========================================================
        case 3:

            mSSGILookDev.MaxDistance +=
                direction * 0.25f * multiplier;

            mSSGILookDev.MaxDistance =
                std::clamp(
                    mSSGILookDev.MaxDistance,
                    0.01f,
                    50.0f
                );

            changed = true;

            break;


            //========================================================
            // Thickness
            //========================================================
        case 4:

            mSSGILookDev.Thickness +=
                direction * 0.01f * multiplier;

            mSSGILookDev.Thickness =
                std::clamp(
                    mSSGILookDev.Thickness,
                    0.001f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Normal Bias
            //========================================================
        case 5:

            mSSGILookDev.NormalBias +=
                direction * 0.005f * multiplier;

            mSSGILookDev.NormalBias =
                std::clamp(
                    mSSGILookDev.NormalBias,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // GI Intensity
            //========================================================
        case 6:

            mSSGILookDev.Intensity +=
                direction * 0.1f * multiplier;

            mSSGILookDev.Intensity =
                std::clamp(
                    mSSGILookDev.Intensity,
                    0.0f,
                    10.0f
                );

            changed = true;

            break;


            //========================================================
            // Maximum Radiance
            //========================================================
        case 7:

            mSSGILookDev.MaxRadiance +=
                direction * 0.5f * multiplier;

            mSSGILookDev.MaxRadiance =
                std::clamp(
                    mSSGILookDev.MaxRadiance,
                    0.1f,
                    64.0f
                );

            changed = true;

            break;


            //========================================================
            // Filter Radius
            //========================================================
        case 8:

            mSSGILookDev.FilterRadius +=
                direction > 0.0f ? 1 : -1;

            mSSGILookDev.FilterRadius =
                std::clamp(
                    mSSGILookDev.FilterRadius,
                    1,
                    8
                );

            changed = true;

            break;


            //========================================================
            // Filter Depth Falloff
            //========================================================
        case 9:

            mSSGILookDev.FilterDepthFalloff +=
                direction * 1.0f * multiplier;

            mSSGILookDev.FilterDepthFalloff =
                (std::max)(
                    mSSGILookDev.FilterDepthFalloff,
                    0.0f
                    );

            changed = true;

            break;


            //========================================================
            // Filter Normal Power
            //========================================================
        case 10:

            mSSGILookDev.FilterNormalPower +=
                direction * 2.0f * multiplier;

            mSSGILookDev.FilterNormalPower =
                (std::max)(
                    mSSGILookDev.FilterNormalPower,
                    1.0f
                    );

            changed = true;

            break;


            //========================================================
            // Filter Confidence
            //========================================================
        case 11:

            mSSGILookDev.FilterConfidenceMin +=
                direction * 0.01f * multiplier;

            mSSGILookDev.FilterConfidenceMin =
                std::clamp(
                    mSSGILookDev.FilterConfidenceMin,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Temporal Enabled
            // Enterで操作
            //========================================================
        case 12:

            break;


            //========================================================
            // History Weight
            //========================================================
        case 13:

            mSSGILookDev.TemporalHistoryWeight +=
                direction * 0.02f * multiplier;

            mSSGILookDev.TemporalHistoryWeight =
                std::clamp(
                    mSSGILookDev.TemporalHistoryWeight,
                    0.0f,
                    0.99f
                );

            changed = true;

            break;


            //========================================================
            // World Position Threshold
            //========================================================
        case 14:

            mSSGILookDev.TemporalWorldPositionThreshold +=
                direction * 0.01f * multiplier;

            mSSGILookDev.TemporalWorldPositionThreshold =
                (std::max)(
                    mSSGILookDev.
                    TemporalWorldPositionThreshold,
                    0.001f
                    );

            changed = true;

            break;


            //========================================================
            // Normal Threshold
            //========================================================
        case 15:

            mSSGILookDev.TemporalNormalThreshold +=
                direction * 0.02f * multiplier;

            mSSGILookDev.TemporalNormalThreshold =
                std::clamp(
                    mSSGILookDev.TemporalNormalThreshold,
                    -1.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Motion Response
            //========================================================
        case 16:

            mSSGILookDev.TemporalMotionResponse +=
                direction * 0.01f * multiplier;

            mSSGILookDev.TemporalMotionResponse =
                (std::max)(
                    mSSGILookDev.TemporalMotionResponse,
                    0.0f
                    );

            changed = true;

            break;


            //========================================================
            // Neighborhood Clamp Expand
            //========================================================
        case 17:

            mSSGILookDev.TemporalClampExpand +=
                direction * 0.01f * multiplier;

            mSSGILookDev.TemporalClampExpand =
                (std::max)(
                    mSSGILookDev.TemporalClampExpand,
                    0.0f
                    );

            changed = true;

            break;


            //========================================================
            // Upsample Depth
            //========================================================
        case 18:

            mSSGILookDev.UpsampleDepthFalloff +=
                direction * 2.0f * multiplier;

            mSSGILookDev.UpsampleDepthFalloff =
                (std::max)(
                    mSSGILookDev.UpsampleDepthFalloff,
                    0.0f
                    );

            changed = true;

            break;


            //========================================================
            // Upsample Normal
            //========================================================
        case 19:

            mSSGILookDev.UpsampleNormalPower +=
                direction * 4.0f * multiplier;

            mSSGILookDev.UpsampleNormalPower =
                (std::max)(
                    mSSGILookDev.UpsampleNormalPower,
                    1.0f
                    );

            changed = true;

            break;


            //========================================================
            // Upsample Confidence
            //========================================================
        case 20:

            mSSGILookDev.UpsampleConfidenceMin +=
                direction * 0.01f * multiplier;

            mSSGILookDev.UpsampleConfidenceMin =
                std::clamp(
                    mSSGILookDev.UpsampleConfidenceMin,
                    0.0f,
                    1.0f
                );

            changed = true;

            break;


            //========================================================
            // Multi Bounce Enabled
            // Enter
            //========================================================
        case 21:

            break;

            //========================================================
            // Multi Bounce Strength
            // Pass側は0 ～ 0.5 Clamp
            //========================================================
        case 22:

            mSSGILookDev.MultiBounceStrength +=
                direction * 0.025f * multiplier;

            mSSGILookDev.MultiBounceStrength =
                std::clamp(
                    mSSGILookDev.MultiBounceStrength,
                    0.0f,
                    0.5f
                );

            changed = true;

            break;


            //========================================================
            // Debug View
            //========================================================
        case 23:

            if (direction > 0.0f)
            {
                ++mSSGILookDev.DebugView;
            }
            else
            {
                --mSSGILookDev.DebugView;
            }


            if (mSSGILookDev.DebugView > 5)
            {
                mSSGILookDev.DebugView = 0;
            }

            if (mSSGILookDev.DebugView < 0)
            {
                mSSGILookDev.DebugView = 5;
            }


            ApplySSGIDebugView();

            return;
        }
    }


    if (!changed)
    {
        return;
    }


    //============================================================
    // Quality Presetから何か1つでも直接調整したら
    // Inspector上ではCustomとする
    //============================================================
    mSSGILookDev.QualityPreset = 3;

    //============================================================
    // Managerへ反映
    //============================================================
    ApplySSGILookDevSettings();

    //============================================================
    // 古いTemporal / Multi Bounce Historyを破棄する
    // 現在のResetTemporalHistory()は、
    // TemporalだけでなくMultiBounceHistoryも無効化する
    //============================================================
    PostProcessManager::ResetSSGITemporalHistory();
}

void SceneCharacterRenderingTest::ApplySSGILookDevSettings()
{
    //============================================================
    // Ray March
    //============================================================
    PostProcessManager::SetSSGIRayCount(mSSGILookDev.RayCount);
    PostProcessManager::SetSSGIStepCount(mSSGILookDev.StepCount);
    PostProcessManager::SetSSGIMaxDistance(mSSGILookDev.MaxDistance);
    PostProcessManager::SetSSGIThickness(mSSGILookDev.Thickness);
    PostProcessManager::SetSSGINormalBias(mSSGILookDev.NormalBias);
    PostProcessManager::SetSSGIIntensity(mSSGILookDev.Intensity);
    PostProcessManager::SetSSGIMaxRadiance(mSSGILookDev.MaxRadiance);
    //============================================================
    // Spatial Filter
    //============================================================
    PostProcessManager::SetSSGIFilterRadius(mSSGILookDev.FilterRadius);
    PostProcessManager::SetSSGIFilterDepthFalloff(mSSGILookDev.FilterDepthFalloff);
    PostProcessManager::SetSSGIFilterNormalPower(mSSGILookDev.FilterNormalPower);
    PostProcessManager::SetSSGIFilterConfidenceMin(mSSGILookDev.FilterConfidenceMin);
    //============================================================
    // Temporal
    //============================================================
    PostProcessManager::SetSSGITemporalEnabled(mSSGILookDev.TemporalEnabled);
    PostProcessManager::SetSSGITemporalHistoryWeight(mSSGILookDev.TemporalHistoryWeight);
    PostProcessManager::SetSSGITemporalWorldPositionThreshold(mSSGILookDev.TemporalWorldPositionThreshold);
    PostProcessManager::SetSSGITemporalNormalThreshold(mSSGILookDev.TemporalNormalThreshold);
    PostProcessManager::SetSSGITemporalMotionResponse(mSSGILookDev.TemporalMotionResponse);
    PostProcessManager::SetSSGITemporalClampExpand(mSSGILookDev.TemporalClampExpand);
    //============================================================
    // Upsampling
    //============================================================
    PostProcessManager::SetSSGIUpsampleDepthFalloff(mSSGILookDev.UpsampleDepthFalloff);
    PostProcessManager::SetSSGIUpsampleNormalPower(mSSGILookDev.UpsampleNormalPower);
    PostProcessManager::SetSSGIUpsampleConfidenceMin(mSSGILookDev.UpsampleConfidenceMin);
    //============================================================
    // Multi Bounce
    //============================================================
    PostProcessManager::SetSSGIMultiBounceEnabled(mSSGILookDev.MultiBounceEnabled);
    PostProcessManager::SetSSGIMultiBounceStrength(mSSGILookDev.MultiBounceStrength);
}

void SceneCharacterRenderingTest::ApplySSGIQualityPreset()
{
    using Quality = SSGIPass::Quality;

    switch (mSSGILookDev.QualityPreset)
    {
        //========================================================
        // Low
        //========================================================
    case 0:
    {
        PostProcessManager::SetSSGIQuality(Quality::Low);
        //====================================================
        // SSGIPass::SetQuality(Low)と同じ値を
        // Inspector側へ同期する
        //====================================================
        mSSGILookDev.RayCount = 2;
        mSSGILookDev.StepCount = 8;
        mSSGILookDev.FilterRadius = 3;
        mSSGILookDev.TemporalHistoryWeight = 0.94f;
        mSSGILookDev.TemporalMotionResponse = 0.08f;
        break;
    }


    //========================================================
    // Medium
    //========================================================
    case 1:
    {
        PostProcessManager::SetSSGIQuality(Quality::Medium);

        mSSGILookDev.RayCount = 4;
        mSSGILookDev.StepCount = 12;
        mSSGILookDev.FilterRadius = 4;
        mSSGILookDev.TemporalHistoryWeight = 0.90f;
        mSSGILookDev.TemporalMotionResponse = 0.05f;
        break;
    }


    //========================================================
    // High
    //========================================================
    case 2:
    {
        PostProcessManager::SetSSGIQuality(Quality::High);

        mSSGILookDev.RayCount = 8;
        mSSGILookDev.StepCount = 20;
        mSSGILookDev.FilterRadius = 5;
        mSSGILookDev.TemporalHistoryWeight = 0.86f;
        mSSGILookDev.TemporalMotionResponse = 0.04f;
        break;
    }


    //========================================================
    // Custom
    // Presetは適用しない
    //========================================================
    case 3:
    default:

        return;
    }


    //============================================================
    // Qualityが変更しないParameterも含め
    // Inspector値をManagerへ同期しておく
    //============================================================
    ApplySSGILookDevSettings();
}

void SceneCharacterRenderingTest::ApplySSGIDebugView()
{
    using DebugView = SSGIPass::DebugView;

    switch (mSSGILookDev.DebugView)
    {
    case 0:

        PostProcessManager::SetSSGIDebugView(DebugView::None);
        break;

    case 1:

        PostProcessManager::SetSSGIDebugView(DebugView::Raw);
        break;

    case 2:

        PostProcessManager::SetSSGIDebugView(DebugView::Filtered);
        break;

    case 3:

        PostProcessManager::SetSSGIDebugView(DebugView::Temporal);
        break;

    case 4:

        PostProcessManager::SetSSGIDebugView(DebugView::Upsampled);
        break;

    case 5:

        PostProcessManager::SetSSGIDebugView(DebugView::FinalComposite);
        break;

    default:

        mSSGILookDev.DebugView = 0;
        PostProcessManager::SetSSGIDebugView(DebugView::None);
        break;
    }
}

void SceneCharacterRenderingTest::ResetSSGILookDev()
{
    //============================================================
    // Default値へ戻す
    //============================================================
    mSSGILookDev = SSGILookDevSettings{};

    //============================================================
    // Medium Qualityを明示
    //============================================================
    ApplySSGIQualityPreset();

    //============================================================
    // 残りのParameterもDefaultへ
    //============================================================
    ApplySSGILookDevSettings();

    //============================================================
    // Debug解除
    //============================================================
    ApplySSGIDebugView();

    //============================================================
    // Temporal / Multi Bounce履歴破棄
    //============================================================
    PostProcessManager::ResetSSGITemporalHistory();
}
