#pragma once

//============================================================
// PostEffectParam
//
// ポストプロセス共通の定数バッファ用パラメータ
//
//============================================================
struct stPostEffectParam
{
	//============================================================
	// ColorGrading
	// x : brightness
	// y : contrast
	// z : saturation
	// w : unused
	//============================================================
	float brightness;
	float contrast;
	float saturation;
	float colorGradingPadding;

	//============================================================
	// Vignette
	// x : vignetteStart
	// y : vignetteEnd
	// z : vignetteStrength
	// w : unused
	//============================================================
	float vignetteStart;
	float vignetteEnd;
	float vignetteStrength;
	float vignettePadding;

	//============================================================
	// Chromatic Aberration
	// x : chromaticStrength
	// yzw : unused
	//============================================================
	float chromaticStrength;
	float chromaticPadding[3];

	//============================================================
	// Tone Mapping
	//
	// x : exposure
	// y : bloomIntensity
	// z : gamma
	// w : bloomEnabled
	//============================================================
	float exposure;
	float bloomIntensity;
	float gamma;
	float bloomEnabled;
};

class PostProcessManager
{
public:
	enum eEffectMask
	{
		None = 0,
		Bloom = 1 << 0,
		Gray = 1 << 1,
		Fxaa = 1 << 2,
		Vignette = 1 << 3,
		ColorGrading = 1 << 4, //色補正
		ChromaticAberration = 1 << 5,// 色収差
		LensFlare = 1 << 6,
		MotionBlur = 1 << 7,// Motion Blur
		VolumetricFog = 1 << 8,// Volumetric Fog
		SSSDiffusion = 1 << 9,// Screen Space SSS Diffusion
		SSGI = 1 << 10 // Screen-Space Global Illumination
	};
public:
	static bool Initialize();
	static void Terminate();

	//ポストプロセス全体を実行
	static void Apply();

	//エフェクトON
	static void Enable(eEffectMask mask);

	//エフェクトOFF
	static void Disable(eEffectMask mask);

	//全てのエフェクトをOFF
	static void AllDisable();

	//エフェクトの実行状態を確認
	static bool IsEnabled(eEffectMask mask);

	//============================================================
	// PostEffectParam 操作用
	//============================================================
	static void SetBrightness(float value);
	static void SetContrast(float value);
	static void SetSaturation(float value);

	static void SetVignette(float start, float end, float strength);
	static void SetVignetteStrength(float strength);

	static void SetChromaticStrength(float strength);

	static void SetExposure(float exposure);

	static void SetBloomIntensity(float intensity);

	static void SetGamma(float gamma);

	static void SetDeltaTime(float deltaTime);

	static void SetLensFlareEdgeFade(float width);
	static void SetLensFlareAngularFade(float start,float end);

	static void SetMotionBlurStrength(float strength);
	static void SetMotionBlurMaxPixels(float pixels);
	static void SetMotionBlurSampleCount(int count);
	static void SetMotionBlurDepthThreshold(float threshold);
	static void SetMotionBlurVelocityThreshold(float threshold);

	//------------------------------------------------------------
	// 基本的なFog設定
	//------------------------------------------------------------
	// Fog全体の密度
	static void SetVolumetricFogDensity(float density);

	// Ray Marchする最大距離
	static void SetVolumetricFogMaxDistance(float distance);

	// Fog自体の色
	static void SetVolumetricFogColor(
		float r,
		float g,
		float b
	);

	// Henyey-Greenstein Phase Functionの異方性
	// 0.0: 等方散乱
	// 0.5: 前方散乱
	// 0.8: 強いGod Ray向け
	static void SetVolumetricFogAnisotropy(float anisotropy);

	// Height Fog
	// baseHeight: Fogの基準高度
	// heightFalloff: 高さ方向の減衰率
	static void SetVolumetricFogHeight(
		float baseHeight,
		float heightFalloff
	);

	// Fog内部で散乱する光の割合
	static void SetVolumetricFogScatteringAlbedo(float albedo);


	//------------------------------------------------------------
	// Lighting
	//------------------------------------------------------------

	// Directional Light
	static void SetVolumetricFogDirectionalStrength(float strength);

	// Ambient
	static void SetVolumetricFogAmbientStrength(float strength);

	// Spot Light
	static void SetVolumetricFogSpotLightStrength(float strength);

	// Point Light
	static void SetVolumetricFogPointLightStrength(float strength);


	//------------------------------------------------------------
	// Ray March
	//------------------------------------------------------------

	// Ray March Sample数
	// 高い: 高品質 / 高負荷
	// 低い: BlueNoise + Temporal前提で高速化可能
	static void SetVolumetricFogSampleCount(int count);


	//------------------------------------------------------------
	// Shadow
	//------------------------------------------------------------

	// Volumetric Shadow全体の強さ
	static void SetVolumetricFogShadowStrength(float strength);

	// Volumetric Shadow Bias
	static void SetVolumetricFogShadowBias(float bias);


	//------------------------------------------------------------
	// Temporal Reprojection
	//------------------------------------------------------------

	// Temporal Reprojection ON/OFF
	static void SetVolumetricFogTemporalEnabled(bool enabled);

	// 前Frame Historyの使用率
	static void SetVolumetricFogTemporalHistoryWeight(float weight);

	// History Reject感度
	static void SetVolumetricFogTemporalResponse(float response);

	// Camera Cut / Teleport / Scene変更などで
	// 古いHistoryを破棄する
	static void ResetVolumetricFogTemporalHistory();

	static int GetVolumetricFogSampleCount() { return sVolumetricFogPass.GetSampleCount(); }

	//------------------------------------------------------------
	// Depth-aware Upsampling
	//------------------------------------------------------------

	// Bilateral Upsampling ON/OFF
	static void SetVolumetricFogDepthAwareUpsamplingEnabled(bool enabled);
	// Depth差に対するWeight減衰
	static void SetVolumetricFogUpsampleDepthFalloff(float falloff);

	//------------------------------------------------------------
	// Blue Noise
	//------------------------------------------------------------
	// Ray March JitterをBlue Noiseにするか
	static void SetVolumetricFogBlueNoiseEnabled(bool enabled);


	//============================================================
	// Screen-Space SSS
	//============================================================
	static void SetSSSDiffusionStrength(float strength);

	static void SetSSSBlurRadius(int radius);

	static void SetSSSDepthFalloff(float falloff);

	static void SetSSSScatteringProfile(float red,float green,float blue);

	static void SetSSSSampleQuality(float quality);

	//============================================================
	// SSS World-Space Diffusion Radius
	// World座標単位
	//============================================================
	static void SetSSSWorldSpaceRadius(float radius);

	//============================================================
	// Temporal SSS
	//============================================================
	static void SetSSSTemporalEnabled(bool enabled);

	static void SetSSSTemporalHistoryWeight(float weight);

	static void SetSSSTemporalDepthThreshold(float threshold);

	static void SetSSSTemporalMotionResponse(float response);

	static void ResetSSSTemporalHistory();

	//============================================================
	// SSS Debug View
	//============================================================
	static void SetSSSDebugView(SubsurfaceScatteringPass::DebugView view);

	//============================================================
	// Screen-Space Global Illumination
	//============================================================
	static SSGIPass::Quality GetSSGIQuality() { return sSSGIPass.GetQuality(); }

	//------------------------------------------------------------
	// Ray本数
	// 多い: ノイズが減る GPU負荷が増える
	//------------------------------------------------------------
	static void SetSSGIRayCount(int count);

	//------------------------------------------------------------
	// 1 RayあたりのMarch Step数
	// 多い: Hit精度が上がる GPU負荷が増える
	//------------------------------------------------------------
	static void SetSSGIStepCount(int count);

	//------------------------------------------------------------
	// GIを探索する最大World-Space距離
	//------------------------------------------------------------
	static void SetSSGIMaxDistance(float distance);

	//------------------------------------------------------------
	// Screen-Space RayとDepthの交差許容厚み
	// 小さすぎる: RayがSurfaceをすり抜けやすい	
	// 大きすぎる: 本来HitしないSurfaceへ誤Hitしやすい
	//------------------------------------------------------------
	static void SetSSGIThickness(float thickness);

	//------------------------------------------------------------
	// Ray開始位置をNormal方向へ押し出す量
	//
	// Self Intersection対策
	//------------------------------------------------------------
	static void SetSSGINormalBias(float bias);

	//------------------------------------------------------------
	// GI全体の強さ
	//------------------------------------------------------------
	static void SetSSGIIntensity(float intensity);

	//============================================================
	// SSGI Bilateral Spatial Filter
	//============================================================
	static void SetSSGIFilterRadius(int radius);

	static void SetSSGIFilterDepthFalloff(float falloff);

	static void SetSSGIFilterNormalPower(float power);

	static void SetSSGIFilterConfidenceMin(float value);

	//============================================================
	// SSGI Temporal Reprojection
	//============================================================
	static void SetSSGITemporalEnabled(bool enabled);

	static void SetSSGITemporalHistoryWeight(float weight);

	static void SetSSGITemporalWorldPositionThreshold(float threshold);

	static void SetSSGITemporalNormalThreshold(float threshold);

	static void SetSSGITemporalMotionResponse(float response);

	static void SetSSGITemporalClampExpand(float expand);

	static void ResetSSGITemporalHistory();

	//============================================================
	// SSGI Depth / Normal-aware Upsampling
	//============================================================
	static void SetSSGIUpsampleDepthFalloff(float falloff);

	static void SetSSGIUpsampleNormalPower(float power);

	static void SetSSGIUpsampleConfidenceMin(float value);

	//============================================================
	// SSGI Debug View
	//============================================================
	static void SetSSGIDebugView(SSGIPass::DebugView view);

	//============================================================
	// SSGI Quality
	//============================================================
	static void SetSSGIQuality(SSGIPass::Quality quality);

	//============================================================
	// SSGI HDR Radiance Clamp
	//============================================================
	static void SetSSGIMaxRadiance(float value);

	//============================================================
	// SSGI Multi Bounce
	//============================================================
	static void SetSSGIMultiBounceEnabled(bool enabled);

	static void SetSSGIMultiBounceStrength(float strength);

	//============================================================
	// 前フレームCamera ViewProjection取得
	//============================================================
	static const XMMATRIX& GetPreviousViewProjection();

	//============================================================
	// 各Passから b0 にセットするためのGPU仮想アドレス取得
	//============================================================
	static D3D12_GPU_VIRTUAL_ADDRESS GetPostEffectParamAddress();

	//============================================================
	// MotionVectorPass取得
	//============================================================
	static MotionVectorPass& GetMotionVectorPass();

private:
	static void ApplyCopy();

	//============================================================
	// 任意のSRVをBackBufferへコピーする
	//
	// inputSRVHeap : 入力テクスチャのSRVが入っているヒープ
	// inputSRV     : 実際に読むSRVのGPUハンドル
	//============================================================
	static void CopyToBackBuffer(
		ID3D12DescriptorHeap* inputSRVHeap,
		D3D12_GPU_DESCRIPTOR_HANDLE inputSRV
	);

	//============================================================
	// 定数バッファ作成・更新
	//============================================================
	static bool CreatePostEffectConstantBuffer();
	static void UpdatePostEffectConstantBuffer();

	static bool CreateMotionVectorDebugPSO();
	static bool CreateMotionVectorDebugBuffer();
	static void RenderMotionVectorDebug();


private:
	static int sMask;
	static BloomPass sBloomPass;
	static VignettePass sVignettePass;
	static ColorGradingPass sColorGradingPass;
	static FxaaPass sFxaaPass;
	static PingPongPostRT sPingPongRT;
	static ChromaticAberrationPass sChromaticAberrationPass;
	static ToneMappingPass sToneMappingPass;
	static LensFlarePass sLensFlarePass;
	static MotionVectorPass sMotionVectorPass;
	static MotionBlurPass sMotionBlurPass;
	static VolumetricFogPass sVolumetricFogPass;
	static SubsurfaceScatteringPass sSubsurfaceScatteringPass;
	static SSGIPass sSSGIPass;
	//============================================================
	// PostEffectParam 定数バッファ
	//============================================================
	static ID3D12Resource* sPostEffectCB;
	static stPostEffectParam* sMappedPostEffectParam;
	static stPostEffectParam sPostEffectParam;

	static ID3D12PipelineState* sMotionVectorDebugPSO;
	static ID3D12Resource* sMotionVectorDebugCB;
	static XMFLOAT4* sMotionVectorDebugMapped;
	static bool sMotionVectorDebugEnabled;
};