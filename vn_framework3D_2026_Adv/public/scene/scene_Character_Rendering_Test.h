#pragma once

class SceneCharacterRenderingTest : public vnScene
{
public:
	//初期化
	bool initialize();
	//終了
	void terminate();
	//処理
	void execute();
	//描画
	void render();

private:
	enum class EnvironmentType
	{
		StudioSmall08 = 0,
		WhiteStudio06
	};
	enum class EnvironmentBuildStage
	{
		HDRToCube = 0,
		BRDFLUT,
		Prefilter,
		Irradiance,
		Ready
	};


	//============================================================
	// 現在の生成Stage
	//============================================================
	EnvironmentBuildStage mEnvironmentBuildStage = EnvironmentBuildStage::HDRToCube;

	EnvironmentType mEnvironmentType = EnvironmentType::StudioSmall08;


	SkeletalModel* pSKRealHumanModel = nullptr;
	SkeletalModel* pSKAokunModel = nullptr;
	ModelBase* pCyclorama = nullptr;
	ModelBase* pSSGIRedWall = nullptr;
	ModelBase* pSSGIBlueWall = nullptr;
	ModelBase* pSSGIFloor = nullptr;
	ModelBase* pSSGICube = nullptr;
	ModelBase* pSSGISphere = nullptr;

	//フレーム依存をなくすためのタイマー(ゲーム時間計測用)
	std::chrono::steady_clock::time_point prevTime;

private:

	//============================================================
	// Environment生成を1Stageだけ進める
	//============================================================
	void ProcessEnvironmentBuild();

	bool ChangeEnvironment(EnvironmentType type);

	void RefreshEnvironmentDescriptors();

private:

	//============================================================
	// カメラ関連
	//============================================================
	// Free Cameraを使用するか
	bool mFreeCameraEnabled = true;


	//============================================================
	// Camera Position
	//============================================================
	XMVECTOR mFreeCameraPosition = XMVectorZero();


	//============================================================
	// Camera Rotation
	// Yaw: Y軸回転
	// Pitch: 上下回転
	// 単位はRadian
	//============================================================
	float mFreeCameraYaw = 0.0f;
	float mFreeCameraPitch = 0.0f;


	//============================================================
	// Free Camera移動速度
	//============================================================
	// 通常移動速度
	float mFreeCameraMoveSpeed =3.0f;
	// Shift中の倍率
	float mFreeCameraFastMultiplier =4.0f;


	//============================================================
	// Mouse Wheelによる速度変更
	//============================================================
	// 最低速度
	float mFreeCameraMinMoveSpeed =0.1f;
	// 最大速度
	float mFreeCameraMaxMoveSpeed =50.0f;
	//============================================================
	// Mouse Look感度
	//============================================================
	float mFreeCameraMouseSensitivity =0.003f;

private:
	//============================================================
	// Free Camera
	//============================================================
	void InitializeFreeCamera();

	void UpdateFreeCamera(float deltaTime);

private:
	//============================================================
	// LookDev Runtime Inspector
	// Character Rendering Test中に
	// Material / PostProcess Parameterを動的変更する
	//============================================================
	enum class LookDevPage
	{
		Material = 0,
		PostEffect,
		PostParameter,
		MotionBlur,
		VolumetricFog,
		SSSDiffusion,
		SSGI,
		Count
	};


	//============================================================
	// Material LookDev Parameter
	//============================================================
	struct MaterialLookDevSettings
	{
		//========================================================
		// PBR
		//========================================================
		float Metallic = 0.0f;
		float Roughness = 0.5f;
		float AO = 1.0f;

		float NormalStrength = 1.0f;

		//========================================================
		// ORM Scale
		//========================================================
		float ORMAOScale = 1.0f;
		float ORMRoughnessScale = 1.5f;
		float ORMMetallicScale = 1.0f;

		//========================================================
		// IBL
		//========================================================
		float EnvironmentDiffuse = 0.02f;
		float EnvironmentSpecular = 0.03f;

		//========================================================
		// SSS
		//========================================================
		float SSSStrength = 0.7f;
		float SSSThickness = 1.0f;
		float SSSDistortion = 0.20f;
		float SSSPower = 3.0f;

		//========================================================
		// Enable Flags
		//========================================================
		bool PBR = true;
		bool ORM = true;
		bool Environment = true;
		bool SSS = true;
		bool ThicknessMap = true;

		// 現在のSkinMaskは検証中なのでOFF
		bool SkinMask = false;
	};


	//============================================================
	// 基本PostProcess Parameter
	// PostProcessManager::stPostEffectParamと対応
	//============================================================
	struct PostLookDevSettings
	{
		float Brightness = 1.04f;
		float Contrast = 1.18f;
		float Saturation = 1.15f;

		float VignetteStart = 0.38f;
		float VignetteEnd = 0.80f;
		float VignetteStrength = 0.35f;

		float ChromaticStrength = 0.002f;

		float Exposure = 0.0f;
		float BloomIntensity = 0.5f;
		float Gamma = 2.2f;
	};


	bool mLookDevInspectorEnabled = true;

	LookDevPage mLookDevPage = LookDevPage::Material;

	// 現在選択中の項目
	int mLookDevSelection = 0;

	MaterialLookDevSettings mMaterialLookDev;
	PostLookDevSettings mPostLookDev;


	//============================================================
	// LookDev Inspector
	//============================================================
	void UpdateLookDevInspector();

	void UpdateMaterialLookDev();
	void UpdatePostEffectLookDev();
	void UpdatePostParameterLookDev();

	void DrawLookDevInspector();

	void ApplyMaterialLookDevSettings();

	void ResetMaterialLookDev();
	void ResetPostLookDev();


	//============================================================
	// Effect ON/OFF切替
	//============================================================
	void TogglePostEffect(PostProcessManager::eEffectMask effect);

private:
	//============================================================
	// Motion Blur LookDev Settings
	//============================================================
	struct MotionBlurLookDevSettings
	{
		//========================================================
		// Motion Blur全体の強さ
		// 0.0: Blurなし
		// 1.0: 基準値
		//========================================================
		float Strength = 1.0f;

		//========================================================
		// 画面上で許可する最大Blur幅
		// 単位:
		//   Pixel
		//========================================================
		float MaxPixels = 32.0f;

		//========================================================
		// Blur Sample数
		//
		// 多い:
		//   高品質
		//   高負荷
		//========================================================
		int SampleCount = 8;

		//========================================================
		// Depth差が大きいPixelをBlurから除外するための閾値
		// 前景と背景が混ざるのを抑える
		//========================================================
		float DepthThreshold = 0.01f;

		//========================================================
		// 非常に小さいVelocityを無視する閾値
		// 微小なMotion Vectorによる
		// 不要なBlurを防止する
		//========================================================
		float VelocityThreshold = 0.001f;
	};


	MotionBlurLookDevSettings mMotionBlurLookDev;


	//============================================================
	// Motion Blur Inspector
	//============================================================
	void UpdateMotionBlurLookDev();

	void ApplyMotionBlurLookDevSettings();

	void ResetMotionBlurLookDev();

private:
	//============================================================
	// Volumetric Fog LookDev Settings
	//============================================================
	struct VolumetricFogLookDevSettings
	{
		//========================================================
		// Basic
		//========================================================

		// Fog全体の密度
		float Density = 0.02f;

		// Ray Marchする最大距離
		float MaxDistance = 50.0f;

		// Fog Color
		float ColorR = 1.0f;
		float ColorG = 1.0f;
		float ColorB = 1.0f;

		// Henyey-Greenstein異方性
		// 0: 等方散乱
		// +方向: 前方散乱
		//========================================================
		float Anisotropy = 0.5f;

		// Height Fog
		float BaseHeight = 0.0f;
		float HeightFalloff = 0.1f;

		// Fog粒子が光を散乱する割合
		float ScatteringAlbedo = 0.9f;

		//========================================================
		// Lighting
		//========================================================
		float DirectionalStrength = 1.0f;
		float AmbientStrength = 0.2f;
		float SpotLightStrength = 1.0f;
		float PointLightStrength = 1.0f;

		//========================================================
		// Ray March
		//========================================================

		// Sample数
		int SampleCount = 32;


		//========================================================
		// Volumetric Shadow
		//========================================================
		float ShadowStrength = 1.0f;
		float ShadowBias = 0.002f;


		//========================================================
		// Temporal Reprojection
		//========================================================
		bool TemporalEnabled = true;

		float TemporalHistoryWeight = 0.90f;
		float TemporalResponse = 1.0f;


		//========================================================
		// Depth-aware Upsampling
		//========================================================
		bool DepthAwareUpsamplingEnabled = true;

		float UpsampleDepthFalloff = 32.0f;


		//========================================================
		// Blue Noise Jitter
		//========================================================
		bool BlueNoiseEnabled = true;
	};


	VolumetricFogLookDevSettings mVolumetricFogLookDev;


	//============================================================
	// Volumetric Fog Inspector
	//============================================================
	void UpdateVolumetricFogLookDev();

	void ApplyVolumetricFogLookDevSettings();

	void ResetVolumetricFogLookDev();

private:
	struct SSSDiffusionLookDevSettings
	{
		//========================================================
		// Diffusion全体の合成強度
		// 0: Screen-Space Diffusionなし
		// 1: 最大
		//========================================================
		float DiffusionStrength = 0.75f;


		//========================================================
		// 最大Screen-Space Blur Radius
		//========================================================
		int BlurRadius = 12;

		//========================================================
		// Depth差に対するBlur Reject
		// 大きい: Depth境界を強く保持
		// 小さい: より広くBlurが混ざる
		//========================================================
		float DepthFalloff = 5.0f;

		//========================================================
		// RGB別Scattering Profile
		//
		// Skin:
		//   Red   = 広く散乱
		//   Green = 中間
		//   Blue  = 狭く散乱
		//========================================================
		float ProfileR = 1.0f;
		float ProfileG = 0.55f;
		float ProfileB = 0.25f;


		//========================================================
		// Sample Quality
		// LookDev Sceneでは最大品質から比較するため1.0を採用
		//========================================================
		float SampleQuality = 1.0f;


		//========================================================
		// World-Space Diffusion Radius
		// 0: 固定Pixel RadiusへFallback
		// > 0: World-Space基準で散乱幅を決める
		//========================================================
		float WorldSpaceRadius = 0.01f;


		//========================================================
		// Temporal
		//========================================================
		bool TemporalEnabled = true;

		float TemporalHistoryWeight = 0.85f;

		float TemporalDepthThreshold = 0.02f;

		float TemporalMotionResponse = 40.0f;


		//========================================================
		// Debug View
		//
		// 0 None
		// 1 Mask
		// 2 DiffusionSource
		// 3 MaterialProfile
		// 4 HorizontalBlur
		// 5 VerticalBlur
		// 6 TemporalResolved
		//========================================================
		int DebugView = 0;
	};


	SSSDiffusionLookDevSettings mSSSDiffusionLookDev;


	//============================================================
	// SSS Diffusion Inspector
	//============================================================
	void UpdateSSSDiffusionLookDev();

	void ApplySSSDiffusionLookDevSettings();

	void ApplySSSDebugView();

	void ResetSSSDiffusionLookDev();

private:
	//============================================================
	// SSGI LookDev Settings
	//
	// SSGI内部:
	//
	// Scene HDR
	//    ↓
	// GI Surface
	//    ↓
	// Ray March
	//    ↓
	// Spatial Bilateral Filter
	//    ↓
	// Temporal Resolve
	//    ↓
	// Full Resolution Upsampling
	//    ↓
	// Scene Composite
	//============================================================
	struct SSGILookDevSettings
	{
		//========================================================
		// Quality Preset
		//========================================================
		int QualityPreset = 1;


		//========================================================
		// Ray March
		//========================================================

		// PixelあたりのRay本数
		// SSGIPass側Clamp: 1 ～ 8
		int RayCount = 4;

		// 1 RayあたりのMarch回数
		// SSGIPass側Clamp: 1 ～ 32
		int StepCount = 12;

		// World-SpaceでRayを飛ばす最大距離
		float MaxDistance = 3.0f;

		// Depthとの交差判定に使う厚み
		float Thickness = 0.08f;

		// Surface自身へ即Hitするのを防ぐOffset
		float NormalBias = 0.02f;

		// GI全体の強度
		float Intensity = 1.0f;

		// HDR Sceneから取得できる最大Radiance
		float MaxRadiance = 8.0f;


		//========================================================
		// Spatial Bilateral Filter
		//========================================================

		int FilterRadius = 4;

		float FilterDepthFalloff = 8.0f;

		float FilterNormalPower = 32.0f;

		float FilterConfidenceMin = 0.05f;


		//========================================================
		// Temporal Reprojection
		//========================================================

		bool TemporalEnabled = true;

		float TemporalHistoryWeight = 0.90f;

		float TemporalWorldPositionThreshold = 0.08f;

		float TemporalNormalThreshold = 0.85f;

		float TemporalMotionResponse = 0.05f;

		float TemporalClampExpand = 0.05f;


		//========================================================
		// Full Resolution Upsampling
		//========================================================

		float UpsampleDepthFalloff = 32.0f;

		float UpsampleNormalPower = 64.0f;

		float UpsampleConfidenceMin = 0.05f;


		//========================================================
		// Multi Bounce
		//
		// 前Frame GIをHit Radianceへ少量加えて
		// 2 Bounce以降を近似する。
		//========================================================
		bool MultiBounceEnabled = true;

		float MultiBounceStrength = 0.25f;


		//========================================================
		// Debug View
		//
		// 0 None
		// 1 Raw
		// 2 Filtered
		// 3 Temporal
		// 4 Upsampled
		// 5 Final Composite
		//========================================================
		int DebugView = 0;
	};


	SSGILookDevSettings mSSGILookDev;


	//============================================================
	// SSGI Inspector
	//============================================================
	void UpdateSSGILookDev();

	void ApplySSGILookDevSettings();

	void ApplySSGIQualityPreset();

	void ApplySSGIDebugView();

	void ResetSSGILookDev();
};