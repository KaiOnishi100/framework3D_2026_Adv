#include "../../framework.h"
#include "../../framework/vn_environment.h"

//extern stMotion motion_idle;
//extern stMotion motion_walk;

//初期化関数
bool SceneFieldTest::initialize()
{
	//============================================================
	// Environment Map初期化
	//============================================================
	const bool environmentInitialized = EnvironmentMapManager::Initialize();

	assert(environmentInitialized);

	if (!environmentInitialized)
	{
		return false;
	}

	//============================================================
	// DDS CubeMap読み込み
	// 通常の2D DDSではなく、CubeMapとして保存されたDDSを使う
	//============================================================
	const bool cubeMapLoaded = EnvironmentMapManager::LoadCubeMap(L"data/texture/ibl/Empty Space/vz_empty_space_cubemap_ue.dds");

	assert(cubeMapLoaded);

	if (!cubeMapLoaded)
	{
		return false;
	}


	//モデルデータの作成
	pPlayer = new ModelBase(L"data/model/bear/", L"bear.vnm");
	pPlayer->LoadNormalMap(L"data/model/bear/Normal/", L"tex_body_normal.png");
	pPlayer->LoadNormalMap(L"data/model/bear/Normal/", L"tex_face_normal.png");
	pPlayer->LoadNormalMap(L"data/model/bear/Normal/", L"tex_head_normal.png");
	pPlayer->LoadNormalMap(L"data/model/bear/Normal/", L"tex_leather_normal.png");
	pPlayer->LoadNormalMap(L"data/model/bear/Normal/", L"tex_limb_normal.png");
	pPlayer->LoadNormalMap(L"data/model/bear/Normal/", L"tex_part_normal.png");
	pPlayer->LoadORMMap(L"data/model/bear/ORM/", L"tex_body_orm.png");
	pPlayer->LoadORMMap(L"data/model/bear/ORM/", L"tex_face_orm.png");
	pPlayer->LoadORMMap(L"data/model/bear/ORM/", L"tex_head_orm.png");
	pPlayer->LoadORMMap(L"data/model/bear/ORM/", L"tex_leather_orm.png");
	pPlayer->LoadORMMap(L"data/model/bear/ORM/", L"tex_limb_orm.png");
	pPlayer->LoadORMMap(L"data/model/bear/ORM/", L"tex_part_orm.png");
	pPlayer->LoadSubsurfaceThicknessMap(L"data/model/bear/Thickness/", L"tex_body_thickness.png");
	pPlayer->LoadSubsurfaceThicknessMap(L"data/model/bear/Thickness/", L"tex_face_thickness.png");
	pPlayer->LoadSubsurfaceThicknessMap(L"data/model/bear/Thickness/", L"tex_head_thickness.png");
	pPlayer->LoadSubsurfaceThicknessMap(L"data/model/bear/Thickness/", L"tex_leather_thickness.png");
	pPlayer->LoadSubsurfaceThicknessMap(L"data/model/bear/Thickness/", L"tex_limb_thickness.png");
	pPlayer->LoadSubsurfaceThicknessMap(L"data/model/bear/Thickness/", L"tex_part_thickness.png");
	pPlayer->SetPBREnabled(true);
	pPlayer->SetSubsurfaceEnabled(true);
	pPlayer->setRenderEnable(false);

	pPlayer->setRenderFlag(ObjectBase::eRenderFlag::Lighting, true);	//ライティングあり

	pCube = new ModelBase(L"data/model/primitive/cube/", L"Cube.vnm");
	pCube->LoadNormalMap(L"data/model/primitive/cube/", L"primitive_Normal_256.png");
	pCube->LoadORMMap(L"data/model/primitive/cube/", L"primitive_ORM_256.png");
	pCube->LoadSubsurfaceThicknessMap(L"data/model/primitive/cube/", L"primitive_thikness_256.png");
	pCube->SetPBREnabled(true);
	pCube->SetSubsurfaceEnabled(true);
	pCube->setCastShadow(true);
	pCube->setPosition(10.0f, 2.0f, 0.0f);
	pCube->setScale(2.0f, 2.0f, 2.0f);
	pCube->SetMetallic(0.2f);

	pIronCube = new ModelBase(L"data/model/primitive/ironcube/", L"Cube.vnm");
	pIronCube->LoadNormalMap(L"data/model/primitive/ironcube/", L"primitive_ironcube_normal.png");
	pIronCube->LoadORMMap(L"data/model/primitive/ironcube/", L"primitive_ironcube_ORM.png");
	pIronCube->LoadSubsurfaceThicknessMap(L"data/model/primitive/ironcube/", L"primitive_ironcube_thickness.png");
	pIronCube->SetPBREnabled(true);
	pIronCube->SetSubsurfaceEnabled(true);
	pIronCube->setCastShadow(true);
	pIronCube->setReceiveShadow(true);
	pIronCube->SetShadowMobility(ShadowMobility::Static);
	pIronCube->setPosition(24.0f, 5.0f, -10.0f);
	pIronCube->setAmbient(1.0f, 0.0f, 0.0f, 1.0f);
	pIronCube->setScale(10.0f, 10.0f, 1.0f);
	pIronCube->SetMetallic(0.3f);

	pTestModel = new ModelBase(L"data/model/testbox/", L"TestBoxModel.vnm");
	//pTestModel->setDiffuse(1.0f, 0.0f, 0.0f, 1.0f);
	//pTestModel->setAmbient(1.0f, 0.0f, 0.0f, 1.0f);
	pTestModel->setScale(0.01f, 0.01f, 0.01f);
	pTestModel->setRotation(0.0f, 0.0f, 0.0f);
	pTestModel->setPosition(0.0f, 5.0f, 10.0f);
	pTestModel->SetPBREnabled(true);
	pTestModel->setCastShadow(true);
	pTestModel->setReceiveShadow(true);

	//pSKTestModel = new SkeletalModel(L"data/model/Luigi/", L"TestLuigi.vnm", L"TestLuigi.bone");
	pSKTestModel = new SkeletalModel(L"data/model/warrior/", L"TestSKModel.vnm", L"TestSKModel.bone");
	pSKTestModel->setPosition(0.0f, 0.0f, 0.0f);
	pSKTestModel->setScale(0.05f, 0.05f, 0.05f);
	pSKTestModel->GetModel()->setRenderEnable(false);

	pSKTestPEEPERModel = new SkeletalModel(L"data/model/TestPEEPERModel/", L"TestPEEPERModel.vnm", L"TestPEEPERModel.bone");
	pSKTestPEEPERModel->setPosition(-4.0f, 6.0f, -15.0f);
	pSKTestPEEPERModel->setRotation(0.0f,90.0f,0.0f);
	pSKTestPEEPERModel->setScale(0.05f, 0.05f, 0.05f);
	pSKTestPEEPERModel->GetModel()->setReceiveShadow(true);
	pSKTestPEEPERModel->GetModel()->setCastShadow(true);
	pSKTestPEEPERModel->GetModel()->SetSubsurfaceEnabled(true,0);
	pSKTestPEEPERModel->GetModel()->SetSubsurfaceThicknessMapEnabled(true, 0);
	//============================================================
	// SSS全体強度
	//============================================================
	pSKTestPEEPERModel->GetModel()->SetSubsurfaceStrength(3.0f, 0);
	//============================================================
	// Thickness全体倍率
	//============================================================
	pSKTestPEEPERModel->GetModel()->SetSubsurfaceThickness(1.0f,0);
	//============================================================
	// SSS Color
	//============================================================
	pSKTestPEEPERModel->GetModel()->SetSubsurfaceColor(1.0f, 0.30f, 0.12f, 0);
	//============================================================
	// Transmission Power
	//============================================================
	pSKTestPEEPERModel->GetModel()->SetSubsurfacePower(2.0f,0);
	//============================================================
	// Transmission方向の歪み
	//============================================================
	pSKTestPEEPERModel->GetModel()->SetSubsurfaceDistortion(0.25f,0);
	//============================================================
	// RGBごとのScreen-Space散乱幅
	//============================================================
	pSKTestPEEPERModel->GetModel()->SetSubsurfaceScatteringProfile(
		1.5f,   // Red
		0.8f,   // Green
		0.35f,  // Blue
		0);
	//============================================================
	// World-Space散乱半径
	//============================================================
	pSKTestPEEPERModel->GetModel()->SetSubsurfaceWorldSpaceRadius(0.03f,0);

	pSKAnimTestModel = new SkeletalModel(L"data/model/testAnimModel/", L"TestAnimModel.vnm", L"TestAnimModel.bone");
	pSKAnimTestModel->setScale(0.01f, 0.01f, 0.01f);

	pSKAnimTestModel->SetMotionLoop(true);
	pSKAnimTestModel->SetMotionPlaySpeed(1.0f);
	pSKAnimTestModel->PlayMotion(true);
	pSKAnimTestModel->GetModel()->SetPBREnabled(true);
	pSKAnimTestModel->GetModel()->setCastShadow(true);
	pSKAnimTestModel->GetModel()->setReceiveShadow(false);
	pSKAnimTestModel->setPosition(-10.0f, 0.0f, 5.0f);
	const bool idleLoaded = pSKAnimTestModel->LoadMotionClip(L"Idle", L"data/model/testAnimModel/motion/TestAnimModel_Idle.mot");
	const bool walkLoaded = pSKAnimTestModel->LoadMotionClip(L"Walk", L"data/model/testAnimModel/motion/TestAnimModel_Walk.mot");
	const bool runLoaded = pSKAnimTestModel->LoadMotionClip(L"Run", L"data/model/testAnimModel/motion/TestAnimModel_Run.mot");
	const bool punchLoaded = pSKAnimTestModel->LoadMotionClip(L"Punch", L"data/model/testAnimModel/motion/TestAnimModel_Punch.mot");
	const bool swordLoaded = pSKAnimTestModel->LoadMotionClip(L"SwordSlash", L"data/model/testAnimModel/motion/TestAnimModel_SwordSlash.mot");
	pSKAnimController = new SkeletalAnimationController(pSKAnimTestModel);
	const bool animationControllerInitialized = pSKAnimController->Initialize();
	assert(animationControllerInitialized);
	assert(idleLoaded);
	assert(walkLoaded);
	assert(runLoaded);
	assert(punchLoaded);
	assert(swordLoaded);
	//============================================================
	// Combo 1
	//============================================================
	const bool combo1Added =
		pSKAnimController->AddComboStep(
			L"Punch",
			// Attackへ入るBlend
			0.08f,
			// 次入力受付開始
			0.35f,
			// 次入力受付終了
			0.85f,
			// 次Animationへ移る位置
			0.70f
		);

	// Combo 2
	const bool combo2Added = pSKAnimController->AddComboStep(L"SwordSlash", 0.08f, 0.30f, 0.85f, 0.70f);
	// Combo 3
	const bool combo3Added =pSKAnimController->AddComboStep(L"Punch",0.08f,0.35f,0.85f,0.75f);


	assert(combo1Added);
	assert(combo2Added);
	assert(combo3Added);
	if (idleLoaded && walkLoaded && runLoaded && punchLoaded && swordLoaded)
	{
		pSKAnimTestModel->SetMotion(L"Idle", true, true);
	}

	pSKRealHumanModel = new SkeletalModel(L"data/model/RealHumanModel/", L"TestAnimModel.vnm", L"TestAnimModel.bone");
	pSKRealHumanModel->setScale(0.05f, 0.05f, 0.05f);
	pSKRealHumanModel->GetModel()->SetMetallic(0.0f);
	pSKRealHumanModel->GetModel()->SetSubsurfaceEnabled(true);
	//pSKRealHumanModel->GetModel()->SetEnvironmentReflection(false, 1.0f, 0);
	const bool idleLoad = pSKRealHumanModel->LoadMotionClip(L"sophiaIdle", L"data/model/RealHumanModel/motion/TestAnimModel_Take 001.mot");
	assert(idleLoad);

	if (idleLoad)
	{
		pSKRealHumanModel->SetMotion(L"sophiaIdle", true, true);
	}

	pSphere = new ModelBase(L"data/model/bear/", L"bear.vnm");

	pGround = new ModelBase(L"data/model/testMap/", L"ground.vnm");
	pGround->setRenderFlag(ObjectBase::eRenderFlag::Lighting, true);	//ライティングあり
	pGround->LoadNormalMap(L"data/model/testMap/", L"ground_Normal.png", 0);
	pGround->LoadORMMap(L"data/model/testMap/", L"groundORM.png", 0);
	pGround->LoadSubsurfaceThicknessMap(L"data/model/testMap/", L"ground_thickness.png",0);
	pGround->SetNormalMapStrength(0.2f, 0);
	pGround->SetNormalMapFlipY(false, 0);// 緑方向がおかしい場合だけtrue

	//pGround->LoadORMMap(L"data/texture/", L"ground_ORM.png",0);
	//pGround->SetORMScale(
	//	1.0f, // AO
	//	1.0f, // Roughness
	//	1.0f, // Metallic
	//	0
	//);

	pGround->SetPBREnabled(true, 0);
	pGround->SetSubsurfaceThicknessMapEnabled(true, 0);
	pGround->SetMetallic(0.0f, 0);
	pGround->SetRoughness(0.7f, 0);
	pGround->SetEnvironmentReflection(true, 1.0f, 0);
	pGround->SetSubsurfaceEnabled(false);
	pGround->SetSubsurfaceStrength(2.0f);
	pGround->SetSubsurfaceThickness(0.8f);
	pGround->SetSubsurfaceDistortion(0.25f);
	pGround->SetSubsurfaceColor(1.0f, 0.20f, 0.08f);
	pGround->SetSubsurfacePower(4.0f);
	pGround->SetSubsurfaceScatteringProfile(1.0f, 0.55f, 0.25f, 0);
	pGround->SetSubsurfaceWorldSpaceRadius(0.01f,0);
	pGround->SetSubsurfaceEnabled(true);
	pGround->SetSubsurfaceStrength(1.0f);

	pNPC = new vnCharacter(L"data/model/bear_part/", L"bear_part.bone");

	pDebugFan = new ModelBase(L"data/model/testModel/", L"debug_fan.vnm");
	pDebugFan->setRenderFlag(ObjectBase::eRenderFlag::Transparent, true);

	//適当な位置に初期配置
	pGround->setPosition(5.0f, 0.0f, 5.0f);
	pGround->setRotationY(1.0f);

	pNPC->setPosition(20.0f, 0.0f, 5.0f);

	pNPCBoss = new Boss(L"data/model/bear/", L"bear.vnm");
	pNPCBoss->setRotationY(0.0f);
	pNPCBoss->setPosition(20.0f, 0.0f, 5.0f);
	mEffectManager.AddEffect(std::make_unique<BreathEffect>(
		pNPCBoss,
		BreathEffect::Type::FireYellow,
		30.0f,
		XMConvertToRadians(35.0f),
		1.0f));


	pSphere->setScale(4.f, 4.f, 4.f);
	pSphere->setPosition(20.0, 0.0f, 0.0f);


	pCrown = new ModelBase(L"data/model/testModel/", L"crown.vnm");
	//階層構造の設定
	pCrown->setParent(pPlayer);
	//位置調整(親からの)
	pCrown->setPositionY(1.3f);
	pCrown->setScale(0.2f, 0.2f, 0.2f);
	pCrown->setRenderEnable(false);

	//フェンスの配置
	for (int i = 0; i < FENCE_NUM; i++)
	{
		//フェンスモデルを作成
		pFence[i] = new ModelBase(L"data/model/testModel/", L"fence.vnm");
		//配置角度
		float degree = (float)i * 360.0f / (float)FENCE_NUM;
		//Degree -> Radian変換
		float radian = degree * 3.141592f / 180.0f;
		//極座標 -> 直交座標
		//※角度(rotY)と配置(posXZ)が一致するようにx=sin,z=cosにする
		float x = sinf(radian) * FENCE_RADIUS;
		float z = cosf(radian) * FENCE_RADIUS;

		//フェンスに姿勢情報を設定
		pFence[i]->setPosition(x, 0.0f, z);
		pFence[i]->setRotationY(radian);
		//フェンスをシーンに登録
		registerObject(pFence[i]);
		pFence[i]->setCastShadow(true);
		pFence[i]->SetShadowMobility(ShadowMobility::Static);
	}

	//ビルボードの作成(幅、高さ、画像パス)
	pBillboard = new vnBillboard(1.0f, 1.0f, L"data/image/icon_exc.png");
	pBillboard->setPositionY(3.5f);

	pDebugFan->setParent(pNPC);
	pBillboard->setParent(pNPC);
	//エミッターの作成
//設定構造体
	Emitter::stEmitterDesc desc;
	swprintf_s(desc.Texture, L"%s", L"data/image/particle/particle002.png");
	desc.Color.Max = XMVectorSet(0.9f, 0.5f, 0.4f, 1.0f);
	pEmitter = new Emitter(&desc);


	swprintf_s(desc.Texture, L"%s", L"data/image/particle/particle004.png");
	desc.Color.Max = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
	pEmitterNPC = new Emitter(&desc);

	pEmitter->setParent(pPlayer);
	pEmitterNPC->setParent(pNPC);
	pEmitter->setEmit(false);
	pEmitterNPC->setEmit(false);

	//オブジェクトの登録(不透明→半透明)
	registerObject(pGround);
	registerObject(pSphere);

	pNPC->registerObject();		//オブジェクト内に含まれるパーツもすべて登録するためクラスに用意された関数を呼び出す

	registerObject(pPlayer);
	registerObject(pCube);
	registerObject(pIronCube);
	registerObject(pTestModel);
	registerObject(pNPCBoss);
	registerObject(pCrown);
	//半透明のオブジェクトは不透明オブジェクトの後に描画
	registerObject(pDebugFan);
	registerObject(pBillboard);
	registerObject(pEmitter);
	registerObject(pEmitterNPC);
	pSKTestModel->RegisterObject(this);
	pSKTestModel->BindPose();
	pSKTestPEEPERModel->RegisterObject(this);
	pSKTestPEEPERModel->BindPose();
	pSKAnimTestModel->RegisterObject(this);
	pSKRealHumanModel->RegisterObject(this);

	//三人称視点プレイヤーカメラの初期値
	radius = 10.0f;
	theta = 0.0f;
	phi = 0.0f;

	//モーションファイルの読み込み
	pMotionIdle = vnCharacter::loadMotionFile(L"data/model/bear_part/motion/motion_idle.mot");
	pMotionWalk = vnCharacter::loadMotionFile(L"data/model/bear_part/motion/motion_walk.mot");

	//初期モーション
	pNPC->setMotion(pMotionIdle);

	PostProcessManager::AllDisable();
	//PostProcessManager::Enable(PostProcessManager::LensFlare);
	//PostProcessManager::Enable(PostProcessManager::Bloom);
	//PostProcessManager::Enable(PostProcessManager::MotionBlur);
	//PostProcessManager::Enable(PostProcessManager::VolumetricFog);
	//PostProcessManager::Enable(PostProcessManager::SSSDiffusion);
	//PostProcessManager::Enable(PostProcessManager::SSGI);
	//PostProcessManager::Enable(PostProcessManager::ColorGrading);
	//PostProcessManager::Enable(PostProcessManager::Vignette);
	//PostProcessManager::Enable(PostProcessManager::ChromaticAberration);

	PostProcessManager::SetContrast(1.0f);
	PostProcessManager::SetSaturation(1.0f);
	PostProcessManager::SetBrightness(1.0f);
	PostProcessManager::SetVignetteStrength(0.80f);
	PostProcessManager::SetChromaticStrength(0.012f);
	PostProcessManager::SetExposure(0.0f);
	PostProcessManager::SetBloomIntensity(0.5f);
	PostProcessManager::SetGamma(2.2f);
	PostProcessManager::SetLensFlareEdgeFade(0.1f);
	PostProcessManager::SetLensFlareAngularFade(0.15f, 0.8f);

	PostProcessManager::SetMotionBlurStrength(0.0f);
	PostProcessManager::SetMotionBlurMaxPixels(24.0f);
	PostProcessManager::SetMotionBlurSampleCount(8);
	PostProcessManager::SetMotionBlurDepthThreshold(0.0025f);
	PostProcessManager::SetMotionBlurVelocityThreshold(0.015f);

	//============================================================
	// 基本Fog
	//============================================================
	PostProcessManager::SetVolumetricFogDensity(0.02f);
	//PostProcessManager::SetVolumetricFogDensity(0.02f);
	PostProcessManager::SetVolumetricFogMaxDistance(80.0f);
	PostProcessManager::SetVolumetricFogColor(1.0f,1.0f,1.0f);
	PostProcessManager::SetVolumetricFogAnisotropy(0.45f);
	PostProcessManager::SetVolumetricFogScatteringAlbedo(0.90f);
	//============================================================
	// Lighting
	//============================================================
	PostProcessManager::SetVolumetricFogDirectionalStrength(0.25f);
	PostProcessManager::SetVolumetricFogAmbientStrength(0.08f);
	PostProcessManager::SetVolumetricFogSpotLightStrength(0.5f);
	PostProcessManager::SetVolumetricFogPointLightStrength(0.1f);
	//============================================================
	// Quality
	//============================================================
	PostProcessManager::SetVolumetricFogSampleCount(32);
	PostProcessManager::SetVolumetricFogTemporalHistoryWeight(0.90f);
	PostProcessManager::SetVolumetricFogTemporalResponse(2.0f);
	PostProcessManager::SetVolumetricFogUpsampleDepthFalloff(2.0f);
	PostProcessManager::SetVolumetricFogBlueNoiseEnabled(true);;

	//============================================================
	// SSS画面空間拡散
	//============================================================
	PostProcessManager::SetSSSDiffusionStrength(0.75f);
	PostProcessManager::SetSSSDepthFalloff(1.5f);	
	PostProcessManager::SetSSSScatteringProfile(1.5f, 0.8f, 0.35f);// Skin Profile
	//============================================================
	// SSS
	//============================================================
	PostProcessManager::SetSSSBlurRadius(12);
	PostProcessManager::SetSSSDepthFalloff(5.0f);
	PostProcessManager::SetSSSTemporalHistoryWeight(0.85f);
	PostProcessManager::SetSSSTemporalEnabled(false);

	//============================================================
	// SSGI有効化
	//============================================================
	PostProcessManager::Enable(PostProcessManager::SSGI);
	//============================================================
	// 動作確認中はRay本数を多めにする
	//============================================================
	PostProcessManager::SetSSGIRayCount(8);
	//============================================================
	// 1RayあたりのMarch回数
	//============================================================
	PostProcessManager::SetSSGIStepCount(16);
	//============================================================
	// 最大GI探索距離
	//
	// World Scaleによって要調整
	//============================================================
	PostProcessManager::SetSSGIMaxDistance(3.0f);
	//============================================================
	// Depth交差判定の厚み
	//============================================================
	PostProcessManager::SetSSGIThickness(0.08f);
	//============================================================
	// Self Intersection対策
	//============================================================
	PostProcessManager::SetSSGINormalBias(0.02f);
	//============================================================
	// GI強度
	//============================================================
	PostProcessManager::SetSSGIIntensity(1.0f);

	PostProcessManager::SetSSGIFilterRadius(4);
	PostProcessManager::SetSSGIFilterDepthFalloff(2.0f);
	PostProcessManager::SetSSGIFilterNormalPower(8.0f);
	PostProcessManager::SetSSGIFilterConfidenceMin(0.05f);

	PostProcessManager::SetSSGITemporalEnabled(true);
	PostProcessManager::SetSSGITemporalHistoryWeight(0.90f);
	PostProcessManager::SetSSGITemporalWorldPositionThreshold(0.08f);
	PostProcessManager::SetSSGITemporalNormalThreshold(0.85f);
	PostProcessManager::SetSSGITemporalMotionResponse(0.05f);
	PostProcessManager::SetSSGITemporalClampExpand(0.05f);
	PostProcessManager::SetSSGIUpsampleDepthFalloff(32.0f);
	PostProcessManager::SetSSGIUpsampleNormalPower(64.0f);
	PostProcessManager::SetSSGIUpsampleConfidenceMin(0.05f);

	PostProcessManager::SetSSGIQuality(SSGIPass::Quality::High);
	PostProcessManager::SetSSGIMaxRadiance(8.0f);
	PostProcessManager::SetSSGIDebugView(SSGIPass::DebugView::None);

	//============================================================
	// Multi Bounce
	//============================================================
	PostProcessManager::SetSSGIMultiBounceEnabled(true);
	PostProcessManager::SetSSGIMultiBounceStrength(0.20f);

	//============================================================
	// このSceneではCascade Shadow Mapを使用する
	//============================================================
	if (!EnableShadowMap(pPlayer))
	{
		return false;
	}

	//============================================================
	// Scene共通ShadowMapを取得
	//============================================================
	ShadowMap* shadowMap = GetShadowMap();

	assert(shadowMap != nullptr);

	//============================================================
	// Cascade更新頻度
	//
	// Cascade 0:
	//   毎フレーム
	//
	// Cascade 1:
	//   毎フレーム
	//
	// Cascade 2:
	//   2フレームに1回
	//============================================================
	shadowMap->SetCascadeUpdateInterval(0, 1);

	shadowMap->SetCascadeUpdateInterval(1, 1);

	shadowMap->SetCascadeUpdateInterval(2, 2);

	//mDebugSprite = new vnSprite(
	//	1700.0f,
	//	220.0f,
	//	400.0f,
	//	400.0f,
	//	L"data/texture/white.png" //空のテクスチャ
	//);

	//============================================================
	// ShadowMap用SRV設定
	//
	// ShadowMap Resource:
	//   R32_TYPELESS
	//
	// SRVとして読む形式:
	//   R32_FLOAT
	//============================================================
	//D3D12_SHADER_RESOURCE_VIEW_DESC shadowSrvDesc = {};
	//shadowSrvDesc.Format =DXGI_FORMAT_R32_FLOAT;
	//shadowSrvDesc.ViewDimension =D3D12_SRV_DIMENSION_TEXTURE2D;
	//shadowSrvDesc.Texture2D.MostDetailedMip = 0;
	//shadowSrvDesc.Texture2D.MipLevels = 1;
	//shadowSrvDesc.Texture2D.PlaneSlice = 0;
	//shadowSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	//============================================================
	// R32_FLOATは赤チャンネル1成分だけのTexture。
	//
	// 通常のマッピングだと、
	//   R = Depth
	//   G = 0
	//   B = 0
	//
	// となり赤色表示になる可能性がある。
	//
	// そこで、R成分をRGBすべてへ割り当てて、
	// 深度をグレースケール表示する。
	//============================================================
	//shadowSrvDesc.Shader4ComponentMapping =
	//	D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
	//		D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
	//		D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
	//		D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
	//		D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1
	//	);

	//============================================================
	// SpriteのSRVをShadowMap Resourceへ向ける
	//============================================================
	//mShadowMapDebugSprite->setExternalTextureView(
	//	shadowMap->GetResource(0, ShadowMapLayer::Dynamic),
	//	shadowSrvDesc
	//);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format =DXGI_FORMAT_R16G16_FLOAT;
	srvDesc.Shader4ComponentMapping =D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension =D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip =0;
	srvDesc.Texture2D.MipLevels =1;
	srvDesc.Texture2D.PlaneSlice =0;
	srvDesc.Texture2D.ResourceMinLODClamp =0.0f;

	//mDebugSprite->setExternalTextureView(EnvironmentMapManager::GetBRDFLUTResource(), srvDesc);

	////============================================================
	//// 他のUIより前面へ表示
	////============================================================
	//mDebugSprite->setRenderPriority(
	//	10000
	//);

	////============================================================
	//// vnSceneの2Dオブジェクトとして登録
	////============================================================
	//vnScene::registerObject(
	//	mDebugSprite
	//);

	pGround->setCastShadow(false);
	pGround->setReceiveShadow(true);

	//============================================================
	// デバッグ表示
	//============================================================
	pDebugFan->setCastShadow(false);

	//============================================================
	// 巨大モデル
	//============================================================
	pSphere->setCastShadow(true);
	pSphere->SetShadowMobility(ShadowMobility::Static);
	pSphere->setReceiveShadow(true);
	pNPC->setCastShadow(false);
	pNPC->setReceiveShadow(false);
	pCrown->setCastShadow(false);

	pEmitter->setCastShadow(false);
	pEmitterNPC->setCastShadow(false);
	pBillboard->setCastShadow(false);

	//本体に実体はない
	pPlayer->setCastShadow(true);
	pPlayer->setReceiveShadow(true);
	pPlayer->SetShadowMobility(ShadowMobility::Dynamic);
	
	prevTime = std::chrono::steady_clock::now();

	return true;
}


//終了関数
void SceneFieldTest::terminate()
{
	mEffectManager.Clear();
	deleteObject(pPlayer);
	deleteObject(pCube);
	deleteObject(pIronCube);
	deleteObject(pTestModel);
	for (int i = 0; i < pNPC->getPartsNum(); i++)
	{
		deleteObject(pNPC->getParts(i));
	}
	deleteObject(pNPC);
	deleteObject(pNPCBoss);
	deleteObject(pSphere);
	deleteObject(pGround);
	deleteObject(pCrown);
	for (int i = 0; i < FENCE_NUM; i++)
	{
		deleteObject(pFence[i]);
	}
	deleteObject(pDebugFan);
	deleteObject(pBillboard);
	deleteObject(pEmitter);
	deleteObject(pEmitterNPC);
	//deleteObject(mDebugSprite);
	EnvironmentMapManager::Terminate();
	delete[] pMotionIdle;
	delete[] pMotionWalk;

}

//処理関数
void SceneFieldTest::execute()
{
	auto now = std::chrono::steady_clock::now();
	std::chrono::duration<float> delta = now - prevTime;
	prevTime = now;
	float daltaTime = delta.count();


	LocalLightManager::BeginFrame();
	SpotLightShadowManager::BeginFrame();
	PointLightShadowManager::BeginFrame();
	//プレイヤーの移動量
	bool input = false;
	XMVECTOR vMove = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	//プレイヤーの移動
	if (vnKeyboard::on(DIK_W))
	{
		vMove = XMVectorSetZ(vMove, 0.1f);
		input = true;
	}
	if (vnKeyboard::on(DIK_S))
	{
		vMove = XMVectorSetZ(vMove, -0.1f);
		input = true;
	}
	if (vnKeyboard::on(DIK_A))
	{
		vMove = XMVectorSetX(vMove, -0.1f);
		input = true;
	}
	if (vnKeyboard::on(DIK_D))
	{
		vMove = XMVectorSetX(vMove, 0.1f);
		input = true;
	}

	if (vnKeyboard::on(DIK_SPACE))
	{
		vMove = XMVectorSetY(vMove, 0.1f);
		input = true;
	}

	if (vnKeyboard::on(DIK_C))
	{
		vMove = XMVectorSetY(vMove, -0.1f);
		input = true;
	}

	const bool animationMoving = input;


	//============================================================
	// Shiftを押しながら移動している場合はRun
	//============================================================
	const bool animationRunning = animationMoving && vnKeyboard::on(DIK_LSHIFT);


	if (vnKeyboard::trg(DIK_Z))
	{
		pSKAnimController->RequestComboAttack();
	}

	//if (vnKeyboard::trg(DIK_1))
	//{
	//	pSKAnimTestModel->BlendToMotion(
	//		L"Idle",
	//		0.25f,
	//		true,
	//		true
	//	);
	//}


	//if (vnKeyboard::trg(DIK_2))
	//{
	//	pSKAnimTestModel->BlendToMotion(
	//		L"Walk",
	//		0.20f,
	//		true,
	//		true
	//	);
	//}


	//if (vnKeyboard::trg(DIK_3))
	//{
	//	pSKAnimTestModel->BlendToMotion(
	//		L"Run",
	//		0.15f,
	//		true,
	//		true
	//	);
	//}


	//if (vnKeyboard::trg(DIK_4))
	//{
	//	pSKAnimTestModel->PlayOneShot(
	//		L"Punch",
	//		L"Idle",
	//		0.10f,
	//		0.15f,
	//		true
	//	);
	//}


	//if (vnKeyboard::trg(DIK_5))
	//{
	//	pSKAnimTestModel->PlayOneShot(
	//		L"SwordSlash",
	//		L"Idle",
	//		0.08f,
	//		0.18f,
	//		true
	//	);
	//}

	pSKAnimController->Update(
		animationMoving,
		animationRunning
	);


	pSKAnimTestModel->execute(daltaTime);
	pSKRealHumanModel->execute(daltaTime);

	//プレイヤーの移動ベクトルをカメラの視点に応じて回転
	XMMATRIX rotate = XMMatrixRotationY(theta);
	//移動ベクトルに回転マトリクスを適用
	vMove = XMVector3TransformNormal(vMove, rotate);

	//プレイヤーの移動
	pPlayer->addPosition(&vMove);
	//pSKAnimTestModel->addPosition(&vMove);

	//プレイヤーを進行方向(vMove)に向ける
	if (input == true)	//入力があるときのみ実行
	{
		float moveX = XMVectorGetX(vMove);	//ベクトルのx成分
		float moveZ = XMVectorGetZ(vMove);	//ベクトルのz成分
		float rotY = atan2f(moveX, moveZ);	//目的の角度

		//現在の角度(source)
		//目的の角度(destination)
		XMMATRIX /*mSrc, mDst, */mAns;
		XMVECTOR qSrc, qDst, qAns;

		//オイラー角をマトリクスに変換
		//mSrc = XMMatrixRotationY(pPlayer->getRotationY());
		//mDst = XMMatrixRotationY(rotY);

		//マトリクスをクオータニオンに変換
		//qSrc = XMQuaternionRotationMatrix(mSrc);
		//qDst = XMQuaternionRotationMatrix(mDst);

		//オイラー角→クオータニオンに変換する関数
		qSrc = XMQuaternionRotationRollPitchYaw(0.0f, pPlayer->getRotationY(), 0.0f);
		qDst = XMQuaternionRotationRollPitchYaw(0.0f, rotY, 0.0f);

		//球面線形補間で中間の姿勢を計算
		qAns = XMQuaternionSlerp(qSrc, qDst, 0.1f);

		//クオータニオンからマトリクスに変換
		mAns = XMMatrixRotationQuaternion(qAns);

		//マトリクスからY軸回転値を計算
		float ry = atan2f(-XMVectorGetZ(mAns.r[0]), XMVectorGetZ(mAns.r[2]));

		pPlayer->setRotationY(ry);

		pEmitter->setEmit(true);
	}
	else
	{
		pEmitter->setEmit(false);
	}


	//カメラの回転
	if (vnKeyboard::on(DIK_UP))
	{
		phi += 1.0f * 3.141592f / 180.0f;
	}
	if (vnKeyboard::on(DIK_DOWN))
	{
		phi -= 1.0f * 3.141592f / 180.0f;
	}
	if (vnKeyboard::on(DIK_LEFT))
	{
		theta += 1.0f * 3.141592f / 180.0f;
	}
	if (vnKeyboard::on(DIK_RIGHT))
	{
		theta -= 1.0f * 3.141592f / 180.0f;
	}

	XMVECTOR camPos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR camTrg = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR camBias = XMVectorSet(0.0f, 1.5f, 0.0f, 0.0f);

	//カメラの座標の計算
	//回転していない基準となるベクトル
	XMVECTOR eye = XMVectorSet(0.0f, 0.0f, -radius, 0.0f);
	//回転マトリクスを作成
	XMMATRIX camRotate = XMMatrixRotationRollPitchYaw(phi, theta, 0.0f);
	//基準となるベクトルを回転させる
	eye = XMVector3TransformNormal(eye, camRotate);

	//float px = radius * cosf(theta) * cos(phi);
	//float py = radius * sinf(phi);
	//float pz = radius * sinf(theta) * cos(phi);

	//回転の中心をプレイヤーにする
	//XMVECTOR v = XMVectorSet(px, py, pz, 0.0f);
	camPos = XMVectorAdd(eye, *pPlayer->getPosition());
	//camPos = XMVectorAdd(eye, *pSKAnimTestModel->getPosition());
	//float s = XMVectorGetZ(camPos) * -1.0f;
	//camPos = XMVectorSet(XMVectorGetX(camPos), XMVectorGetY(camPos), s, 0.0f);
	
	camPos = XMVectorAdd(camPos, camBias);

	//カメラの位置を極座標(r,θ,φ)で表した座標にする
	vnCamera::setPosition(&camPos);

	//カメラの注視点をプレイヤーに合わせる
	camTrg = XMVectorAdd(*pPlayer->getPosition(), camBias);
	//camTrg = XMVectorAdd(eye, *pSKAnimTestModel->getPosition());
	vnCamera::setTarget(&camTrg);


	//vnCamera::setPosition(&camTrg);
	//vnCamera::setTarget(&camPos);

	//XMVECTOR, XMMATRIX
	// SIMD : Single Instructure Multi Decimal



	//・プレイヤーの原点からの距離を調べる
	//プレイヤーの座標 : pPlayer->getPosition()
	//ベクトルの長さを調べる
	XMVECTOR v = *pPlayer->getPosition();
	XMVECTOR vLength = XMVector3Length(v);
	float length = XMVectorGetX(vLength);
	//・距離(length)とフェンスの半径(FENCE_RADIUS)を比較
	if (length > FIELD_RADIUS)
	{
		//・プレイヤーがフェンスの外にいる場合
		//・プレイヤーの原点からの距離をフェンスの半径になる位置を計算
		//ベクトルの長さを自分で設定する
		//正規化:ベクトルの長さを1にする
		XMVECTOR nv = XMVector3Normalize(v);
		//設定したい長さの数値を掛ける
		nv = XMVectorScale(nv, FIELD_RADIUS);
		//→プレイヤーの位置を設定
		pPlayer->setPosition(&nv);
	}


	//NPCの認識範囲にプレイヤーが入っているか調べる

	//①NPCとプレイヤーの距離が10ｍ以内

	//NPCからプレイヤーへ向かうベクトルを作る
	XMVECTOR vNpcToPlayer =
		*pPlayer->getPosition() - *pNPC->getPosition();
	//NPCからプレイヤーへ向かうベクトルの長さを求める
	XMVECTOR vDist = XMVector3Length(vNpcToPlayer);
	float dist = XMVectorGetX(vDist);

	//②NPCの前方向からプレイヤーの角度が±45度以内
	// 
	//XMVECTOR vFront = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	//XMMATRIX mNPC = XMMatrixRotationY(pNPC->getRotationY());
	//vFront = XMVector3TransformNormal(vFront, mNPC);
	XMMATRIX mNpc = *pNPC->getWorld();	//ワールドマトリクスを取得
	XMVECTOR vFront = mNpc.r[2];	//前方向を表すベクトル

	//2本のベクトルの内積を計算
	//2本のベクトルの長さが1のとき、
	//内積値はcosθと一致する

	//正規化を行い、ベクトルの長さを1にする
	vNpcToPlayer = XMVector3Normalize(vNpcToPlayer);
	//vFront = XMVector3Normalize(vFront);
	//内積値(cosθ)を求める
	XMVECTOR vDot = XMVector3Dot(vNpcToPlayer, vFront);
	float dot = XMVectorGetX(vDot);
	//float型変数のノイズ除去
	if (dot > 1.0f)
	{
		dot = 1.0f;
	}
	else if (dot < -1.0f)
	{
		dot = -1.0f;
	}
	//θを求める
	float angle = acosf(dot);	//dot = 1.00000012(float型の精度のノイズ)
	//-nan(ind)
	if (dist < 10.0f && angle < 45.0f * 3.141592f / 180.0f)
	{
		//vnFont::print(250.0f, 100.0f, L"認識範囲に入っている");

		//プレイヤーを追いかける
		vNpcToPlayer = XMVector3Normalize(vNpcToPlayer);
		vNpcToPlayer = XMVectorScale(vNpcToPlayer, 0.05f);
		pNPC->addPosition(&vNpcToPlayer);
		//プレイヤーの方を向く
		float x = XMVectorGetX(vNpcToPlayer);
		float z = XMVectorGetZ(vNpcToPlayer);
		float npcY = atan2f(x, z);
		pNPC->setRotationY(npcY);

		pNPC->setMotion(pMotionWalk);

		pBillboard->setRenderEnable(true);
		pEmitterNPC->setEmit(true);
	}
	else
	{
		pNPC->setMotion(pMotionIdle);
		pBillboard->setRenderEnable(false);
		pEmitterNPC->setEmit(false);
	}

	//============================================================
	// Playerの現在位置を取得
	//============================================================
	XMFLOAT3 pointLightPosition = {};

	XMStoreFloat3(
		&pointLightPosition,
		*pSKTestPEEPERModel->getPosition()
		/**pPlayer->getPosition()*/
	);
	//============================================================
	// 地面付近ではなくPlayerの胴体付近へ配置
	//============================================================
	pointLightPosition.y +=	2.5f;

	//============================================================
	// 通常LightingとShadow Cameraで共通使用するRange
	//
	// PointLightShadowManagerとLocalLightManagerへ
	// 必ず同じ値を渡す。
	//============================================================
	const float pointLightRange = 12.0f;

	//============================================================
	// 先にPoint Shadowを登録
	//
	// 戻り値:
	//
	//   0～MaxShadowLights - 1
	//     Point Shadow Index
	//
	//   -1
	//     登録失敗
	//============================================================
	const int pointShadowIndex =
		PointLightShadowManager::AddLight(
			pointLightPosition,
			pointLightRange,
			1.0f,
			0.0002f,
			0.0010f
		);

	//============================================================
	// 通常PBR描画用Point Lightを登録
	// PointLightShadowManagerから返されたShadow Indexを
	// 最後の引数へ渡す
	// ここでPoint Lightを登録するのは1回だけ
	//============================================================
	const bool pointLightAdded =
		LocalLightManager::AddPointLight(
			pointLightPosition,
			pointLightRange,
			// Light Color
			XMFLOAT3(
				0.3f,
				0.3f,
				0.3f
			),
			// HDR Intensity
			100.0f,
			// Point Shadow Index
			pointShadowIndex
		);

	assert(pointLightAdded);

	const XMFLOAT3 torchLightPosition =
	{
		10.0f,
		3.0f,
		5.0f
	};

	const float torchLightRange = 12.0f;

	const int torchShadowIndex =
		PointLightShadowManager::AddLight(
			torchLightPosition,
			torchLightRange,
			1.0f,
			0.00002f,
			0.00010f,

			//====================================================
			// Lightが固定されているため、
			// 初回だけShadow Mapを生成して再利用する
			//====================================================
			PointLightShadowManager::
			UpdateMode::OnDemand
		);

	LocalLightManager::AddPointLight(
		torchLightPosition,
		torchLightRange,
		XMFLOAT3(
			0.3f,
			0.3f,
			0.3f
		),
		100.0f,
		torchShadowIndex
	);

	//============================================================
	// Spot Light 0
	//============================================================
	{
		const XMFLOAT3 position =
		{
			-4.0f,
			6.0f,
			2.0f
		};

		const XMFLOAT3 direction =
		{
			0.0f,
			0.0f,
			-0.5f
		};

		const float range =
			30.0f;

		const float outerAngle =
			28.0f;

		//========================================================
		// Shadow Texture ArrayのSlice番号を取得
		//========================================================
		const int shadowIndex =
			SpotLightShadowManager::AddLight(
				position,
				direction,
				range,
				outerAngle,
				0.75f,
				// Minimum Bias
				0.0002f,
				// Slope Bias
				0.0015f
			);

		//========================================================
		// 通常PBR用Spot Light登録
		//========================================================
		LocalLightManager::AddSpotLight(
			position,
			direction,
			range,

			XMFLOAT3(
				0.4f,
				0.4f,
				0.4f
			),

			10.0f,
			15.0f,
			outerAngle,
			shadowIndex
		);
	}

	//============================================================
	// Spot Light 1
	//============================================================
	{
		const XMFLOAT3 position =
		{
			4.0f,
			6.0f,
			2.0f
		};

		const XMFLOAT3 direction =
		{
			0.0f,
			-1.0f,
			0.0f
		};

		const float range = 30.0f;

		const float outerAngle = 28.0f;

		const int shadowIndex =
			SpotLightShadowManager::AddLight(
				position,
				direction,
				range,
				outerAngle,
				1.75f,
				// Minimum Bias
				0.00035f,
				// Slope Bias
				0.0025f
			);

		LocalLightManager::AddSpotLight(
			position,
			direction,
			range,

			XMFLOAT3(
				0.4f,
				0.4f,
				0.4f
			),

			10.0f,
			15.0f,
			outerAngle,
			shadowIndex
		);
	}

	LocalLightManager::Upload();

	PostProcessManager::SetDeltaTime(daltaTime);
	//デバッグ表示
	vnFont::setColor(0xff000000);
	vnFont::print(10.0f, 10.0f, L"PlayerPos : %.3f, %.3f, %.3f",
		pPlayer->getPositionX(),
		pPlayer->getPositionY(),
		pPlayer->getPositionZ());
	vnFont::print(10.0f, 30.0f, L"CrownLocalPos : %.3f, %.3f, %.3f",
		pCrown->getPositionX(),
		pCrown->getPositionY(),
		pCrown->getPositionZ());

	XMVECTOR wld;
	pCrown->getWorldPosition(&wld);
	vnFont::print(10.0f, 50.0f, L"CrownWorldPos : %.3f, %.3f, %.3f",
		XMVectorGetX(wld),
		XMVectorGetY(wld),
		XMVectorGetZ(wld));


	vnFont::print(10.0f, 70.0f, L"Player RotY : %.3f",
		pPlayer->getRotationY() * 180.0f / 3.1415192f);
	vnFont::print(10.0f, 90.0f, L"radius : %.3f", radius);
	vnFont::print(10.0f, 110.0f, L"theta : %.3f", theta * 180.0f / 3.1415192f);
	vnFont::print(10.0f, 130.0f, L"phi : %.3f", phi * 180.0f / 3.1415192f);
	//vnFont::print(10.0f, 150.0f,
	//	L"プレイヤーの原点からの距離 : %.3f", length);

	//vnFont::print(10.0f, 190.0f,
	//	L"NPCとプレイヤーの距離 : %.3f", dist);
	//vnFont::print(10.0f, 210.0f,
	//	L"NPCとプレイヤーの角度 : %.3f", angle * 180.0f / 3.141592f);
	vnFont::setColor(0xff000000);

	vnDebugDraw::Grid();
	vnDebugDraw::Axis();


	//============================================================
	// Game Effect更新
	//============================================================
	mEffectManager.Update(daltaTime);

	vnScene::execute();
}

//描画関数
void SceneFieldTest::render()
{
	EnvironmentMapManager::GenerateBRDFLUT();
	EnvironmentMapManager::GeneratePrefilteredEnvironment();
	EnvironmentMapManager::GenerateIrradianceEnvironment();
	vnScene::render();
	EnvironmentMapManager::RenderSkybox();  
	Direct3D::setMainRenderTarget();
	mEffectManager.RenderFullResolution();
	Direct3D::TransitionMainDepthToSRV();
	HalfResolutionEffectPass::Begin();
	mEffectManager.RenderHalfResolutionAdditive();
	HalfResolutionEffectPass::EndAndComposite();
}

void SceneFieldTest::DrawCascadeFrustum(const ShadowMap& shadowMap, UINT cascadeIndex)
{
	XMVECTOR corner[8] = {};

	for (UINT i = 0; i < 8; ++i)
	{
		corner[i] =
			shadowMap.GetCascadeFrustumCorner(
			cascadeIndex,
			i
			);
	}

	//============================================================
	// Near平面
	//============================================================
	vnDebugDraw::Line(&corner[0], &corner[1]);
	vnDebugDraw::Line(&corner[1], &corner[3]);
	vnDebugDraw::Line(&corner[3], &corner[2]);
	vnDebugDraw::Line(&corner[2], &corner[0]);

	//============================================================
	// Far平面
	//============================================================
	vnDebugDraw::Line(&corner[4], &corner[5]);
	vnDebugDraw::Line(&corner[5], &corner[7]);
	vnDebugDraw::Line(&corner[7], &corner[6]);
	vnDebugDraw::Line(&corner[6], &corner[4]);

	//============================================================
	// NearとFarを接続
	//============================================================
	vnDebugDraw::Line(&corner[0], &corner[4]);
	vnDebugDraw::Line(&corner[1], &corner[5]);
	vnDebugDraw::Line(&corner[2], &corner[6]);
	vnDebugDraw::Line(&corner[3], &corner[7]);
}
