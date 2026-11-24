#pragma once

//============================================================
// Environment Map Manager
//
// 現段階の役割:
//
//   ・DDS CubeMapの読み込み
//   ・TextureCube SRVの作成
//   ・Skybox用Resourceの管理
//   ・Skyboxの描画
//   ・Diffuse Irradiance Map
//   ・Prefiltered Environment Map
//   ・BRDF LUT
//============================================================
class EnvironmentMapManager final
{
public:

	//============================================================
	// Skybox描画Resourceを初期化
	// CubeMapの読み込みはLoadCubeMap()で別に行う
	//============================================================
	static bool Initialize();

	//============================================================
	// 全Resourceを解放
	//============================================================
	static void Terminate();

	//============================================================
	// DDS形式のCubeMapを読み込む
	// ※通常の2D DDSではなく TextureCubeとして保存されたDDSが必要
	//============================================================
	static bool LoadCubeMap(const wchar_t* filePath);

	//============================================================
	// Equirectangular HDRをEnvironmentとして読み込む
	//============================================================
	static bool LoadHDRI(const wchar_t* filePath);

	//============================================================
	// 読み込んだ2D HDR
	//      ↓
	// Environment CubeMap
	// GPUで6面へ変換する
	//============================================================
	static bool GenerateEnvironmentCubeFromHDR();

	//============================================================
	// Skybox描画
	// 呼び出し位置:
	//   3D Scene描画後
	//   PostProcess実行前
	//============================================================
	static void RenderSkybox();

	//============================================================
	// PBR Shaderへ接続するためのGetter
	//============================================================
	static ID3D12Resource* GetEnvironmentResource();

	static ID3D12DescriptorHeap* GetEnvironmentSRVHeap();

	static D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentSRVGPU();

	//============================================================
	// 指定されたDescriptor位置へEnvironment CubeMapのSRVを作成
	// CubeMapが未ロードの場合: Null TextureCube Descriptorを作成する
	// CubeMapがロード済みの場合: 実際のEnvironment Resourceを登録する
	//============================================================
	static void WriteEnvironmentSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle);

	//============================================================
	// Environment CubeMapのMip数を取得
	// Roughnessから参照Mipを決める際に使用する
	//============================================================
	static UINT GetEnvironmentMipCount();

	//============================================================
	// Rotation API関数
	//============================================================
	//============================================================
	// Environment Rotation
	// HDRIをY軸方向へ回転する
	//============================================================
	static void SetEnvironmentRotationDegrees(float degrees);

	//============================================================
	// 現在角度へ加算
	// Example: AddEnvironmentRotationDegrees(15.0f);
	//============================================================
	static void AddEnvironmentRotationDegrees(float degrees);

	//============================================================
	// 現在のEnvironment角度取得
	// 戻り値: Degree
	//============================================================
	static float GetEnvironmentRotationDegrees();

	//============================================================
	// Environment Exposure用関数
	// HDRIそのものの明るさをEV単位で変更する
	//============================================================
	static void SetEnvironmentExposureEV(float exposureEV);

	//============================================================
	// 現在値へEVを加算
	//============================================================
	static void AddEnvironmentExposureEV(float exposureEV);

	//============================================================
	// 現在のExposure取得
	// 戻り値: EV
	//============================================================
	static float GetEnvironmentExposureEV();

	//============================================================
	// 現在EVをLinear倍率へ変換した値を取得
	// EV =  0 → 1.0
	// EV = +1 → 2.0
	// EV = -1 → 0.5
	//============================================================
	static float GetEnvironmentExposureScale();

	//============================================================
	// 現在のEnvironment RotationをRadianで取得
	//============================================================
	static float GetEnvironmentRotationRadians();


private:

	static float sEnvironmentRotationRadians;

private:

	//============================================================
	// Skybox用Constant Buffer
	//============================================================
	struct SkyboxConstantBuffer
	{
		XMFLOAT4X4 ViewProjection;
		//========================================================
		// x : Environment Rotation [radian]
		// y : Exposure Scale
		// z,w : unused
		//========================================================
		XMFLOAT4 EnvironmentSceneParam;
	};

	//============================================================
	// EnvironmentMapManager内部SRV Heapの配置
	//============================================================
	enum class EnvironmentSRVIndex : UINT
	{
		// 元のEnvironment CubeMap
		Environment = 0,

		// GGX Prefilter済みCubeMap
		PrefilteredEnvironment,

		// Diffuse Irradiance CubeMap
		IrradianceEnvironment,

		// Split-Sum BRDF LUT
		BRDFLUT,

		// Equirectangular HDR Source
		HDREquirectangular,

		// Descriptor総数
		Count
	};

	//============================================================
	// 初期化内部処理
	//============================================================
	static bool CreateRootSignature();

	static bool CreatePipelineState();

	static bool CreateVertexBuffer();

	static bool CreateConstantBuffer();

	static bool CreateDescriptorHeap();

private:

	static bool sInitialized;

	static bool sCubeMapLoaded;

	//============================================================
	// CubeMap
	//============================================================
	static ID3D12Resource* sEnvironmentCube;

	static ID3D12DescriptorHeap* sSRVHeap;

	//============================================================
	// Skybox Pipeline
	//============================================================
	static ID3D12RootSignature* sRootSignature;

	static ID3D12PipelineState* sPipelineState;

	//============================================================
	// Cube Vertex Buffer
	//============================================================
	static ID3D12Resource* sVertexBuffer;

	static D3D12_VERTEX_BUFFER_VIEW sVertexBufferView;

	//============================================================
	// ViewProjection Constant Buffer
	//============================================================
	static ID3D12Resource* sConstantBuffer;

	//============================================================
	// Environment Cube Resource State
	//============================================================
	static D3D12_RESOURCE_STATES sEnvironmentResourceState;

	static SkyboxConstantBuffer* sMappedConstantBuffer;

public:
	//============================================================
	// Prefiltered Environment Mapを生成する
	// ※CommandListが記録可能な状態で呼び出す必要がある
	//============================================================
	static bool GeneratePrefilteredEnvironment();

	//============================================================
	// Material DescriptorへPrefiltered CubeMap SRVを書き込む
	//============================================================
	static void WritePrefilteredEnvironmentSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle);

	//============================================================
	// Prefiltered CubeMapのMip数
	//============================================================
	static UINT GetPrefilteredMipCount();

	//============================================================
	// Prefiltered Resource取得
	//============================================================
	static ID3D12Resource* GetPrefilteredEnvironmentResource();

private:

	static bool CreatePrefilterRootSignature();

	static bool CreatePrefilterPipelineState();

	static bool CreatePrefilterResources();

private:

	//============================================================
	// Prefilter設定
	//============================================================
	static constexpr UINT PrefilterSize = 256;
	static constexpr UINT PrefilterMipCount = 5;

	//============================================================
	// Prefiltered Environment CubeMap
	//============================================================
	static ID3D12Resource* sPrefilteredEnvironmentCube;

	//============================================================
	// 各Face・各Mip用RTV
	// Descriptor数: 6 Face × 5 Mip = 30
	//============================================================
	static ID3D12DescriptorHeap* sPrefilterRTVHeap;

	static UINT sPrefilterRTVDescriptorSize;

	//============================================================
	// Prefilter生成用Pipeline
	//============================================================
	static ID3D12RootSignature* sPrefilterRootSignature;

	static ID3D12PipelineState* sPrefilterPipelineState;

	//============================================================
	// 再生成が必要か
	//============================================================
	static bool sPrefilterNeedsGeneration;

	static bool sPrefilterReady;

	//============================================================
	// Resource State
	//============================================================
	static D3D12_RESOURCE_STATES sPrefilterResourceState;

public:

	//============================================================
	// Diffuse IBL用Irradiance CubeMapを生成
	//============================================================
	static bool GenerateIrradianceEnvironment();

	//============================================================
	// 指定DescriptorへIrradiance CubeMapのSRVを作成
	//============================================================
	static void WriteIrradianceEnvironmentSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle);

	//============================================================
	// Irradiance Resource取得
	//============================================================
	static ID3D12Resource* GetIrradianceEnvironmentResource();

private:

	//============================================================
	// Irradiance用PSO作成
	// Root SignatureはPrefilter用を再利用する
	//============================================================
	static bool CreateIrradiancePipelineState();

	//============================================================
	// Irradiance CubeMap Resource作成
	//============================================================
	static bool CreateIrradianceResources();

private:

	//============================================================
	// IrradianceSize
	//============================================================
	static constexpr UINT IrradianceSize = 32;

	//============================================================
	// Irradiance CubeMap Resource
	//============================================================
	static ID3D12Resource* sIrradianceEnvironmentCube;

	//============================================================
	// 6面分のRTV Heap
	//============================================================
	static ID3D12DescriptorHeap* sIrradianceRTVHeap;

	static UINT sIrradianceRTVDescriptorSize;

	//============================================================
	// Irradiance生成用PSO
	//============================================================
	static ID3D12PipelineState* sIrradiancePipelineState;

	//============================================================
	// 生成状態
	//============================================================
	static bool sIrradianceNeedsGeneration;

	static bool sIrradianceReady;

	static D3D12_RESOURCE_STATES sIrradianceResourceState;

public:

	//============================================================
	// BRDF LUT生成
	//============================================================
	static bool GenerateBRDFLUT();

	//============================================================
	// Material DescriptorへBRDF LUTのSRVを作成
	//============================================================
	static void WriteBRDFLUTSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle);

	//============================================================
	// BRDF LUT Resource取得
	//============================================================
	static ID3D12Resource* GetBRDFLUTResource();

private:

	//============================================================
	// BRDF LUT用Root Signature
	//============================================================
	static bool CreateBRDFLUTRootSignature();

	//============================================================
	// BRDF LUT用PSO
	//============================================================
	static bool CreateBRDFLUTPipelineState();

	//============================================================
	// BRDF LUT Resource作成
	//============================================================
	static bool CreateBRDFLUTResources();

private:

	//============================================================
	// BRDF LUT解像度
	//============================================================
	static constexpr UINT BRDFLUTSize = 256;

	//============================================================
	// BRDF LUT Resource
	//============================================================
	static ID3D12Resource* sBRDFLUT;

	//============================================================
	// BRDF LUT RTV
	//============================================================
	static ID3D12DescriptorHeap* sBRDFLUTRTVHeap;

	//============================================================
	// BRDF LUT生成Pipeline
	//============================================================
	static ID3D12RootSignature* sBRDFLUTRootSignature;

	static ID3D12PipelineState* sBRDFLUTPipelineState;

	//============================================================
	// 生成状態
	//============================================================
	static bool sBRDFLUTNeedsGeneration;

	static bool sBRDFLUTReady;

	static D3D12_RESOURCE_STATES sBRDFLUTResourceState;

private:
	//============================================================
	// HDR→Cube内部関数
	//============================================================

	//============================================================
	// HDR → Cube変換Pipeline
	//============================================================
	static bool CreateHDRToCubeRootSignature();

	static bool CreateHDRToCubePipelineState();

	//============================================================
	// HDR用Environment Cube 
	// 6 Face分のRTVも作成する
	//============================================================
	static bool CreateHDRToCubeResources();

private:

	//============================================================
	// HDR関連メンバ変数
	//============================================================

	//============================================================
	// Environment Cube解像度
	//============================================================
	static constexpr UINT EnvironmentCubeSize = 1024;

	//============================================================
	// GPUへ保持するHDR Source解像度
	// EquirectangularではCube Face 1辺に対して
	// Width ≒ FaceSize × 4
	// Height ≒ FaceSize × 2
	//============================================================
	static constexpr UINT HDRSourceWidth = EnvironmentCubeSize * 4;

	static constexpr UINT HDRSourceHeight = EnvironmentCubeSize * 2;


	//============================================================
	// Equirectangular HDR Texture2D
	//============================================================
	static ID3D12Resource* sHDREquirectangularTexture;


	//============================================================
	// HDR → Cube Pipeline
	//============================================================
	static ID3D12RootSignature* sHDRToCubeRootSignature;

	static ID3D12PipelineState* sHDRToCubePipelineState;


	//============================================================
	// Environment Cube 6面用RTV
	//============================================================
	static ID3D12DescriptorHeap* sEnvironmentRTVHeap;

	static UINT sEnvironmentRTVDescriptorSize;


	//============================================================
	// HDR→Cube生成待ち
	//============================================================
	static bool sHDRToCubeNeedsGeneration;

	//============================================================
	// HDRI Exposure
	// EV単位で保持する
	//============================================================
	static float sEnvironmentExposureEV;

};