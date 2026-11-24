#pragma once

#include "../shadow/shadowMap.h"

//============================================================
// VolumetricFogPass
// Half ResolutionでView RayをRay Marchし
// 空気中の光の散乱を計算する
// Fog Texture:
//   RGB : In-Scattering
//   A   : Transmittance
// Composite:
//   FinalColor = SceneColor * Transmittance + InScattering
//============================================================
class VolumetricFogPass
{
public:

	//============================================================
	// 初期化
	//============================================================
	bool Initialize(UINT fullWidth, UINT fullHeight);


	//============================================================
	// 終了
	//============================================================
	void Terminate();


	//============================================================
	// Volumetric Fog適用
	// inputSceneResource: 現在のHDR Scene
	// depthResource: Main Depth
	// outputRTV: PingPongPostRT Write側
	//============================================================
	void ApplyToRT(
		ID3D12Resource* inputSceneResource,
		ID3D12Resource* depthResource,
		D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
		UINT fullWidth,
		UINT fullHeight
	);

	//============================================================
	// Camera Cut / Teleport / Scene切替時に呼ぶ
	//============================================================
	void ResetTemporalHistory();

	int GetSampleCount() const { return mSampleCount; }

	//============================================================
	// Fog Density
	//============================================================
	void SetDensity(float density);


	//============================================================
	// Ray March最大距離
	//============================================================
	void SetMaxDistance(float distance);


	//============================================================
	// Fog Color
	//============================================================
	void SetColor(
		float r,
		float g,
		float b
	);


	//============================================================
	// Directional Lightから受ける散乱強度
	//============================================================
	void SetDirectionalStrength(float strength);


	//============================================================
	// 環境散乱
	//============================================================
	void SetAmbientStrength(float strength);

	//============================================================
	// Scattering Albedo
	// Extinctionした光のうち、
	// どれくらいを散乱光として扱うか
	// 0.0: ほぼ吸収
	// 1.0: ほぼ散乱
	// 通常は0.8～0.98程度
	//============================================================
	void SetScatteringAlbedo(float albedo);

	//============================================================
	// Henyey-Greenstein Anisotropy
	//  0.0 : 等方散乱
	//  0.5 : Forward Scattering
	//  0.8 : 強い光芒
	// -0.9 ～ 0.9程度にClampする
	//============================================================
	void SetAnisotropy(float anisotropy);


	//============================================================
	// Height Fog設定
	// heightFalloff = 0: 高さによる密度変化なし
	// > 0: 高い場所ほどFogが薄くなる
	//============================================================
	void SetHeightFog(float baseHeight, float heightFalloff);


	//============================================================
	// Ray March Sample数
	//============================================================
	void SetSampleCount(int count);

	void SetShadowStrength(float strength);

	void SetShadowBias(float bias);

	//============================================================
	// Spot Volumetric Light強度
	//============================================================
	void SetSpotLightStrength(float strength);

	//============================================================
	// Point Light Volumetric Strength
	//============================================================
	void SetPointLightStrength(float strength);

	void SetTemporalEnabled(bool enabled);

	void SetTemporalHistoryWeight(float weight);

	void SetTemporalResponse(float response);

	void SetDepthAwareUpsamplingEnabled(bool enabled);

	void SetUpsampleDepthFalloff(float falloff);

	void SetBlueNoiseEnabled(bool enabled);

	ID3D12Resource* GetResolvedFogResource() const;


private:

	//============================================================
	// GPU Constant Buffer
	//============================================================
	struct alignas(16) VolumetricFogBuffer
	{
		//========================================================
		// Current ViewProjectionの逆行列
		// DepthからWorld Positionを復元する
		//========================================================
		XMFLOAT4X4 InverseViewProjection;


		//========================================================
		// xyz : Camera Position
		// w   : Max Distance
		//========================================================
		XMFLOAT4 CameraPositionMaxDistance;


		//========================================================
		// xyz : Fog Color
		// w   : Density
		//========================================================
		XMFLOAT4 FogColorDensity;


		//========================================================
		// xyz : Pixel → Directional Light方向
		// w   : Anisotropy
		//========================================================
		XMFLOAT4 LightDirectionAnisotropy;


		//========================================================
		// xyz : Directional Light Color
		// w   : Directional Strength
		//========================================================
		XMFLOAT4 LightColorStrength;


		//========================================================
		// x : Scattering Albedo
		// y : Ambient Strength
		// z : Base Height
		// w : Height Falloff
		//========================================================
		XMFLOAT4 FogParameters;


		//========================================================
		// x : Sample Count
		// y : Frame Index
		// zw: unused
		//========================================================
		XMFLOAT4 RaymarchParameters;

		//========================================================
		// Volumetric Shadow用 View Matrix
		// Ray March中のWorld PositionをView空間へ変換し
		// CSMのCascadeを選択するために使用する
		//========================================================
		XMFLOAT4X4 View;


		//========================================================
		// Directional Light CSM
		// World Position
		//     ↓
		// Light Clip Position
		// へ変換する
		//========================================================
		XMFLOAT4X4 LightViewProjection[SHADOW_CASCADE_COUNT];


		//========================================================
		// xyz: Cascade 0 / 1 / 2 の終了距離
		//========================================================
		XMFLOAT4 CascadeSplits;


		//========================================================
		// xyz: Cascade 0 / 1 / 2 のBlend開始距離
		//========================================================
		XMFLOAT4 CascadeBlendStarts;


		//========================================================
		// Volumetric Shadow Parameter
		// x : Shadow有効
		// y : Shadow Strength
		// z : Base Shadow Bias
		// w : 未使用
		//========================================================
		XMFLOAT4 ShadowParameters;

		//========================================================
		// Local Light / Spot Volumetric設定
		// x: 現在登録されているLocal Light数
		// y: Spot LightのVolumetric強度倍率
		// z: Spot Shadowを使用するか
		// w: 未使用
		//========================================================
		XMFLOAT4 LocalLightParameters;


		//========================================================
		// Spot Shadow ViewProjection
		// LocalLightGPU.Parameters.wから復元したShadow Indexで
		// 対応する行列を選択する
		//========================================================
		XMFLOAT4X4 SpotLightViewProjection[SpotLightShadowManager::MaxShadowLights];


		//========================================================
		// Spot Shadow個別Parameter
		// x : Enable
		// y : PCF Radius
		// z : Minimum Bias
		// w : Slope Bias
		//========================================================
		XMFLOAT4 SpotShadowParameters[SpotLightShadowManager::MaxShadowLights];

		//============================================================
		// Point Light Volumetric設定
		// x : Point Light Volumetric Strength
		// y : Point Shadow有効
		// z : 未使用
		// w : 未使用
		//============================================================
		XMFLOAT4 PointLightParameters;


		//============================================================
		// Point Light Shadow ViewProjection
		// 1灯: 6 Face
		// Index: shadowIndex * 6 + faceIndex
		//============================================================
		XMFLOAT4X4 PointLightViewProjection[PointLightShadowManager::TotalSliceCount];


		//============================================================
		// Point Shadow Parameter
		// x : Enable
		// y : PCF Radius
		// z : Minimum Bias
		// w : Slope Bias
		//============================================================
		XMFLOAT4 PointShadowParameters[PointLightShadowManager::MaxShadowLights];
	};

	//============================================================
	// Temporal Reprojection用 Constant Buffer
	//============================================================
	struct alignas(16) TemporalFogBuffer
	{
		//========================================================
		// Current ViewProjectionの逆行列
		// Current DepthからWorld Positionを復元する
		//========================================================
		XMFLOAT4X4 InverseCurrentViewProjection;


		//========================================================
		// Previous ViewProjection
		// Current World Positionを前Frameへ投影する
		//========================================================
		XMFLOAT4X4 PreviousViewProjection;


		//========================================================
		// xyz : Current Camera Position
		// w   : Fog Max Distance
		// Sky PixelのReprojectionにも使用する
		//========================================================
		XMFLOAT4 CameraPositionMaxDistance;


		//========================================================
		// x : History Weight
		// y : History Response
		// z : History Valid
		// w : unused
		//========================================================
		XMFLOAT4 TemporalParameters;


		//========================================================
		// x : Half Width
		// y : Half Height
		// z : 1 / Half Width
		// w : 1 / Half Height
		// Neighborhood Clampで使用する
		//========================================================
		XMFLOAT4 ScreenParameters;
	};

	//============================================================
	// Depth-aware Upsampling専用Constant Buffer
	//============================================================
	struct alignas(16) VolumetricUpsampleBuffer
	{
		//========================================================
		// Depth → World Position復元用
		//========================================================
		XMFLOAT4X4 InverseViewProjection;


		//========================================================
		// xyz : Camera Position
		// w   : Fog Max Distance
		//========================================================
		XMFLOAT4 CameraPositionMaxDistance;


		//========================================================
		// x : Depth Falloff
		// y : Sky Depth Threshold
		// z : Depth-aware Enable
		// w : unused
		//========================================================
		XMFLOAT4 UpsampleParameters;
	};

	//============================================================
	// Shadow付きVolumetric Fog専用Root Signature
	//
	// t0 : Main Depth
	//
	// t1 : Static Cascade 0
	// t2 : Static Cascade 1
	// t3 : Static Cascade 2
	//
	// t4 : Dynamic Cascade 0
	// t5 : Dynamic Cascade 1
	// t6 : Dynamic Cascade 2
	//
	// b0 : VolumetricFogBuffer
	//============================================================
	ComPtr<ID3D12RootSignature> mShadowRaymarchRootSignature;

	//============================================================
	// Shadow付きRay March PSO
	//============================================================
	ComPtr<ID3D12PipelineState> mShadowRaymarchPSO;

	//============================================================
	// Shadow付きRay March Descriptor Heap
	//============================================================
	ComPtr<ID3D12DescriptorHeap> mShadowRaymarchSRVHeap;

	//============================================================
	// Depth-aware Composite専用Root Signature
	//
	// t0 : HDR Scene
	// t1 : Resolved Half Resolution Fog
	// t2 : Main Depth
	//
	// b0 : VolumetricUpsampleBuffer
	//============================================================
	ComPtr<ID3D12RootSignature> mDepthAwareCompositeRootSignature;


	//============================================================
	// Depth-aware Composite PSO
	//============================================================
	ComPtr<ID3D12PipelineState> mDepthAwareCompositePSO;


	//============================================================
	// Descriptor Heap
	// [0] t0 Scene
	// [1] t1 Fog
	// [2] t2 Depth
	//============================================================
	ComPtr<ID3D12DescriptorHeap> mDepthAwareCompositeSRVHeap;


	//============================================================
	// Upsample Constant Buffer
	//============================================================
	ComPtr<ID3D12Resource> mUpsampleConstantBuffer;

	//============================================================
	// Blue Noise Texture
	// Ray March開始位置のJitterに使用する
	// 64x64程度の小さいTextureを画面全体へTileする
	//============================================================
	ComPtr<ID3D12Resource> mBlueNoiseTexture;

private:

	bool CreateFogResource();

	bool CreateDescriptorHeaps();

	bool CreateConstantBuffer();

	bool CreateRaymarchPipeline();

	bool CreateCompositePipeline();

	bool CreateShadowRaymarchRootSignature();

	bool CreateShadowRaymarchDescriptorHeap();

	bool CreateShadowRaymarchPipeline();

	void UpdateConstantBuffer();


	//============================================================
	// Fog TextureのState変更
	//============================================================
	void TransitionFog(D3D12_RESOURCE_STATES newState);


	//============================================================
	// Half Resolution Ray March
	//============================================================
	void Raymarch(ID3D12Resource* depthResource);
	void RaymarchShadowed(ID3D12Resource* depthResource);

	//============================================================
	// Full Resolution Composite
	//============================================================
	void Composite(
		ID3D12Resource* inputSceneResource,
		D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
		UINT fullWidth,
		UINT fullHeight
	);

	bool CreateTemporalResources();

	bool CreateTemporalDescriptorHeap();

	bool CreateTemporalRootSignature();

	bool CreateTemporalConstantBuffer();

	bool CreateTemporalPipeline();


	void ResolveTemporal(ID3D12Resource* depthResource);

	void UpdateTemporalConstantBuffer();

	void TransitionTemporalHistory(UINT index, D3D12_RESOURCE_STATES newState);


	bool CreateDepthAwareCompositeRootSignature();

	bool CreateDepthAwareCompositeDescriptorHeap();

	bool CreateDepthAwareCompositeConstantBuffer();

	bool CreateDepthAwareCompositePipeline();


	void UpdateUpsampleConstantBuffer();


	void CompositeDepthAware(
		ID3D12Resource* inputSceneResource,
		ID3D12Resource* depthResource,
		D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
		UINT fullWidth,
		UINT fullHeight
	);

	//============================================================
	// Blue Noise読み込み
	//============================================================
	bool LoadBlueNoiseTexture(const wchar_t* filePath);

private:

	//============================================================
	// Full Resolution
	//============================================================
	UINT mFullWidth = 0u;
	UINT mFullHeight = 0u;


	//============================================================
	// Half Resolution
	//============================================================
	UINT mHalfWidth = 0u;
	UINT mHalfHeight = 0u;


	//============================================================
	// Fog Render Target
	// RGB: Scattering
	// A: Transmittance
	//============================================================
	ID3D12Resource* mFogResource = nullptr;


	ID3D12DescriptorHeap* mRTVHeap = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE mFogRTV = {};


	//============================================================
	// Ray March Heap
	// [0] t0 = Main Depth
	// [1] t1 = Dummy Depth
	// 既存Screen Root Signatureがt0～t1の
	// 2Descriptorを要求するため2個確保
	//============================================================
	ID3D12DescriptorHeap* mRaymarchSRVHeap = nullptr;


	//============================================================
	// Composite Heap
	// [0] t0 = Scene HDR
	// [1] t1 = Fog Texture
	//============================================================
	ID3D12DescriptorHeap* mCompositeSRVHeap = nullptr;


	UINT mDescriptorSize = 0u;


	ID3D12PipelineState* mRaymarchPSO = nullptr;

	ID3D12PipelineState* mCompositePSO = nullptr;


	//============================================================
	// Constant Buffer
	//============================================================
	ID3D12Resource* mConstantBuffer = nullptr;

	VolumetricFogBuffer* mMappedBuffer = nullptr;

	D3D12_RESOURCE_STATES mFogState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


	//============================================================
	// Fog Parameter
	//============================================================
	float mDensity = 0.035f;
	float mMaxDistance = 80.0f;

	XMFLOAT3 mFogColor = XMFLOAT3(0.72f, 0.78f, 0.85f);

	float mDirectionalStrength = 2.0f;
	float mAmbientStrength = 0.15f;
	float mAnisotropy = 0.45f;
	float mScatteringAlbedo = 0.90f;
	float mBaseHeight = 0.0f;
	float mHeightFalloff = 0.0f;
	float mShadowStrength = 1.0f;// Volumetric Shadowの強さ
	float mShadowBias = 0.0003f;// Volume Sample用Shadow Bias
	float mSpotLightStrength = 0.02f;// Spot LightがFogへ与える強さ
	float mPointLightStrength = 0.02f;// Point LightのVolumetric Lighting倍率

	int mSampleCount = 32;


	UINT mFrameIndex = 0u;


	bool mInitialized = false;
	bool mBlueNoiseAvailable = false;// Blue Noiseが正常に読み込めたか
	bool mBlueNoiseEnabled = true;// Blue Noizeを使用するかどうか
private:

	//============================================================
	// Temporal Fog History
	// 0 / 1をPingPongする
	// HistoryRead: 前FrameのAccumulated Fog
	// HistoryWrite: 今FrameのTemporal Resolve結果
	//============================================================
	ComPtr<ID3D12Resource> mTemporalHistoryResources[2];


	//============================================================
	// Temporal History用RTV Heap
	// Descriptor 0 = History 0
	// Descriptor 1 = History 1
	//============================================================
	ComPtr<ID3D12DescriptorHeap> mTemporalRTVHeap;


	D3D12_CPU_DESCRIPTOR_HANDLE mTemporalRTV[2] = {};


	//============================================================
	// Temporal Pass入力用Descriptor Heap
	//
	// [0] t0 = Current Fog
	// [1] t1 = Previous History
	// [2] t2 = Current Main Depth
	//============================================================
	ComPtr<ID3D12DescriptorHeap> mTemporalSRVHeap;

	//============================================================
	// Temporal専用Root Signature
	//============================================================
	ComPtr<ID3D12RootSignature> mTemporalRootSignature;


	//============================================================
	// Temporal PSO
	//============================================================
	ComPtr<ID3D12PipelineState> mTemporalPSO;


	//============================================================
	// Temporal Constant Buffer
	//============================================================
	ComPtr<ID3D12Resource> mTemporalConstantBuffer;


	TemporalFogBuffer* mTemporalMappedBuffer = nullptr;


	//============================================================
	// History Resource State
	//============================================================
	D3D12_RESOURCE_STATES mTemporalHistoryStates[2] =
	{
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	};


	//============================================================
	// History PingPong Index
	//============================================================
	UINT mTemporalReadIndex = 0u;
	UINT mTemporalWriteIndex = 1u;

	//============================================================
	// Historyが利用可能か
	// Scene開始直後 / Camera Cut直後はfalse
	//============================================================
	bool mTemporalHistoryValid = false;

	//============================================================
	// Volumetric Fog用Previous Camera VP
	//============================================================
	XMMATRIX mPreviousViewProjection = XMMatrixIdentity();

	//============================================================
	// Temporal有効
	//============================================================
	bool mTemporalEnabled = true;

	//============================================================
	// History Blend率
	//============================================================
	float mTemporalHistoryWeight = 0.90f;

	//============================================================
	// History Reject感度
	//============================================================
	float mTemporalResponse = 2.0f;



	VolumetricUpsampleBuffer* mUpsampleMappedBuffer = nullptr;

	//============================================================
	// Depth-aware Upsampling設定
	// World Space距離差に対する減衰
	// 大きい: 境界を厳しく分離
	// 小さい: 滑らかだが境界が滲みやすい
	//============================================================
	float mUpsampleDepthFalloff = 2.0f;

	bool mDepthAwareUpsamplingEnabled = true;

};