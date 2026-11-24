#include "../framework.h"
#include "vn_environment.h"
#include "environmentMapManager.h"

//============================================================
// Static Member
//============================================================
bool EnvironmentMapManager::sInitialized =false;
bool EnvironmentMapManager::sCubeMapLoaded =false;
ID3D12Resource* EnvironmentMapManager::sEnvironmentCube =nullptr;
ID3D12DescriptorHeap* EnvironmentMapManager::sSRVHeap =nullptr;
ID3D12RootSignature* EnvironmentMapManager::sRootSignature =nullptr;
ID3D12PipelineState* EnvironmentMapManager::sPipelineState =nullptr;
ID3D12Resource* EnvironmentMapManager::sVertexBuffer =nullptr;
D3D12_VERTEX_BUFFER_VIEW EnvironmentMapManager::sVertexBufferView ={};
ID3D12Resource* EnvironmentMapManager::sConstantBuffer =nullptr;
D3D12_RESOURCE_STATES EnvironmentMapManager::sEnvironmentResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
EnvironmentMapManager::SkyboxConstantBuffer* EnvironmentMapManager::sMappedConstantBuffer = nullptr;

ID3D12Resource*EnvironmentMapManager::sPrefilteredEnvironmentCube =nullptr;
ID3D12DescriptorHeap*EnvironmentMapManager::sPrefilterRTVHeap =nullptr;
UINT EnvironmentMapManager::sPrefilterRTVDescriptorSize =0;
ID3D12RootSignature*EnvironmentMapManager::sPrefilterRootSignature =nullptr;
ID3D12PipelineState*EnvironmentMapManager::sPrefilterPipelineState =nullptr;
bool EnvironmentMapManager::sPrefilterNeedsGeneration =false;
bool EnvironmentMapManager::sPrefilterReady =false;
D3D12_RESOURCE_STATES EnvironmentMapManager::sPrefilterResourceState =D3D12_RESOURCE_STATE_RENDER_TARGET;

ID3D12Resource*EnvironmentMapManager::sIrradianceEnvironmentCube =nullptr;
ID3D12DescriptorHeap*EnvironmentMapManager::sIrradianceRTVHeap =nullptr;
UINT EnvironmentMapManager::sIrradianceRTVDescriptorSize =0;
ID3D12PipelineState*EnvironmentMapManager::sIrradiancePipelineState =nullptr;
bool EnvironmentMapManager::sIrradianceNeedsGeneration =false;
bool EnvironmentMapManager::sIrradianceReady =false;
D3D12_RESOURCE_STATES EnvironmentMapManager::sIrradianceResourceState =D3D12_RESOURCE_STATE_RENDER_TARGET;

ID3D12Resource*EnvironmentMapManager::sBRDFLUT =nullptr;
ID3D12DescriptorHeap*EnvironmentMapManager::sBRDFLUTRTVHeap =nullptr;
ID3D12RootSignature*EnvironmentMapManager::sBRDFLUTRootSignature =nullptr;
ID3D12PipelineState*EnvironmentMapManager::sBRDFLUTPipelineState =nullptr;
bool EnvironmentMapManager::sBRDFLUTNeedsGeneration =false;
bool EnvironmentMapManager::sBRDFLUTReady =false;
D3D12_RESOURCE_STATES EnvironmentMapManager::sBRDFLUTResourceState =D3D12_RESOURCE_STATE_RENDER_TARGET;

ID3D12Resource* EnvironmentMapManager::sHDREquirectangularTexture = nullptr;
ID3D12RootSignature* EnvironmentMapManager::sHDRToCubeRootSignature = nullptr;
ID3D12PipelineState* EnvironmentMapManager::sHDRToCubePipelineState = nullptr;
ID3D12DescriptorHeap* EnvironmentMapManager::sEnvironmentRTVHeap = nullptr;
UINT EnvironmentMapManager::sEnvironmentRTVDescriptorSize = 0;
bool EnvironmentMapManager::sHDRToCubeNeedsGeneration = false;

float EnvironmentMapManager::sEnvironmentRotationRadians = 0.0f;
float EnvironmentMapManager::sEnvironmentExposureEV = 0.0f;
//============================================================
// Skybox頂点
//============================================================
namespace
{
	struct SkyboxVertex
	{
		float x;
		float y;
		float z;
	};
}

//============================================================
// 初期化
//============================================================
bool EnvironmentMapManager::Initialize()
{
	if (sInitialized)
	{
		return true;
	}

	if (!CreateRootSignature())
	{
		Terminate();
		return false;
	}

	if (!CreatePipelineState())
	{
		Terminate();
		return false;
	}

	if (!CreateVertexBuffer())
	{
		Terminate();
		return false;
	}

	if (!CreateConstantBuffer())
	{
		Terminate();
		return false;
	}

	if (!CreateDescriptorHeap())
	{
		Terminate();
		return false;
	}
	if (!CreatePrefilterRootSignature())
	{
		Terminate();
		return false;
	}

	if (!CreatePrefilterPipelineState())
	{
		Terminate();
		return false;
	}

	if (!CreateIrradiancePipelineState())
	{
		Terminate();
		return false;
	}

	if (!CreateBRDFLUTRootSignature())
	{
		Terminate();

		return false;
	}

	if (!CreateBRDFLUTPipelineState())
	{
		Terminate();

		return false;
	}

	if (!CreateBRDFLUTResources())
	{
		Terminate();

		return false;
	}

	if (!CreateHDRToCubeRootSignature())
	{
		Terminate();

		return false;
	}

	if (!CreateHDRToCubePipelineState())
	{
		Terminate();

		return false;
	}
	sInitialized = true;

	return true;
}

//============================================================
// 解放
//============================================================
void EnvironmentMapManager::Terminate()
{
	sCubeMapLoaded = false;
	sInitialized = false;

	if (sConstantBuffer != nullptr)
	{
		if (sMappedConstantBuffer != nullptr)
		{
			sConstantBuffer->Unmap(0, nullptr);
			sMappedConstantBuffer = nullptr;
		}
	}

	SAFE_RELEASE(sConstantBuffer);
	SAFE_RELEASE(sVertexBuffer);
	SAFE_RELEASE(sEnvironmentCube);
	SAFE_RELEASE(sSRVHeap);
	SAFE_RELEASE(sPipelineState);
	SAFE_RELEASE(sRootSignature);
	SAFE_RELEASE(sPrefilteredEnvironmentCube);
	SAFE_RELEASE(sPrefilterRTVHeap);
	SAFE_RELEASE(sPrefilterPipelineState);
	SAFE_RELEASE(sPrefilterRootSignature);
	SAFE_RELEASE(sIrradianceEnvironmentCube);
	SAFE_RELEASE(sIrradianceRTVHeap);
	SAFE_RELEASE(sIrradiancePipelineState);
	SAFE_RELEASE(sBRDFLUT);
	SAFE_RELEASE(sBRDFLUTRTVHeap);
	SAFE_RELEASE(sBRDFLUTPipelineState);
	SAFE_RELEASE(sBRDFLUTRootSignature);
	SAFE_RELEASE(sHDREquirectangularTexture);
	SAFE_RELEASE(sHDRToCubePipelineState);
	SAFE_RELEASE(sHDRToCubeRootSignature);
	SAFE_RELEASE(sEnvironmentRTVHeap);
	sEnvironmentResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	sBRDFLUTNeedsGeneration = false;
	sBRDFLUTReady = false;
	sIrradianceNeedsGeneration = false;
	sIrradianceReady = false;
	sIrradianceRTVDescriptorSize = 0;
	sPrefilterReady = false;
	sPrefilterNeedsGeneration = false;
	sPrefilterRTVDescriptorSize = 0;
	sEnvironmentRTVDescriptorSize = 0;
	sEnvironmentExposureEV = 0.0f;
	sEnvironmentRotationRadians = 0.0f;
	sHDRToCubeNeedsGeneration = false;
	sVertexBufferView = {};
}

//============================================================
// Root Signature
//
// [0] b0 : Skybox ViewProjection
// [1] t0 : Environment CubeMap
//============================================================
bool EnvironmentMapManager::CreateRootSignature()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	//============================================================
	// t0用Descriptor Range
	// t1用Prefiltered Environment CubeMap
	//============================================================
	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0;
	srvRange.RegisterSpace = 0;
	srvRange.OffsetInDescriptorsFromTableStart = 0;

	//============================================================
	// Root Parameter
	//============================================================
	D3D12_ROOT_PARAMETER rootParameters[2] = {};

	//============================================================
	// [0] b0 : ViewProjection
	//============================================================
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	//============================================================
	// [1] t0 : TextureCube
	//============================================================
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//============================================================
	// Static Sampler s0
	//============================================================
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.ShaderRegister = 0;
	samplerDesc.RegisterSpace = 0;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//============================================================
	// Root Signature全体
	//============================================================
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);

	if (FAILED(hr))
	{
		if (errorBlob != nullptr)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
		}

		SAFE_RELEASE(signatureBlob);
		SAFE_RELEASE(errorBlob);

		assert(false);

		return false;
	}

	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&sRootSignature)
	);

	SAFE_RELEASE(signatureBlob);
	SAFE_RELEASE(errorBlob);

	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	sRootSignature->SetName(L"EnvironmentMap Skybox RootSignature");

	return true;
}

//============================================================
// Pipeline State
//============================================================
bool EnvironmentMapManager::CreatePipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || sRootSignature == nullptr)
	{
		return false;
	}

	//============================================================
	// 頂点要素
	//============================================================
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = sRootSignature;
	psoDesc.InputLayout =
	{
		inputLayout,_countof(inputLayout)
	};

	//============================================================
	// Shader
	//============================================================
	psoDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_Skybox)->getCode();
	psoDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_Skybox)->getLength();
	psoDesc.PS.pShaderBytecode = Shader::getPShader(Shader::ePixelShader::PS_Skybox)->getCode();
	psoDesc.PS.BytecodeLength = Shader::getPShader(Shader::ePixelShader::PS_Skybox)->getLength();

	//============================================================
	// Rasterizer
	// ※Skyboxは立方体の内側から見るため
	// ※最初はCullを無効にして確実に表示する
	//============================================================
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
	psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;
	psoDesc.RasterizerState.MultisampleEnable = FALSE;
	psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
	psoDesc.RasterizerState.ForcedSampleCount = 0;
	psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//============================================================
	// Blendなし
	//============================================================
	psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
	psoDesc.BlendState.IndependentBlendEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// Depth
	// Vertex ShaderでzをFar Planeへ固定している
	// LESS_EQUAL: Depth Clear値1.0と同値でも描画可能
	// DepthWriteMask ZERO: SkyboxはDepthを書き換えない
	//============================================================
	psoDesc.DepthStencilState.DepthEnable = TRUE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	//============================================================
	// Output
	// SkyboxはTone Mapping前のHDR Sceneへ描画する
	//============================================================
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = RenderFormat::SceneHDR;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	const HRESULT hr = device->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(&sPipelineState)
	);

	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	sPipelineState->SetName(L"EnvironmentMap Skybox HDR PipelineState");

	return true;
}

//============================================================
// Cube Vertex Buffer
//============================================================
bool EnvironmentMapManager::CreateVertexBuffer()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	//============================================================
	// 立方体36頂点
	// CullMode NONEなので面の頂点順には依存しない
	//============================================================
	static const SkyboxVertex vertices[] =
	{
		// Front
		{-1.0f, -1.0f, -1.0f},
		{-1.0f,  1.0f, -1.0f},
		{ 1.0f,  1.0f, -1.0f},
		{-1.0f, -1.0f, -1.0f},
		{ 1.0f,  1.0f, -1.0f},
		{ 1.0f, -1.0f, -1.0f},

		// Back
		{-1.0f, -1.0f,  1.0f},
		{ 1.0f,  1.0f,  1.0f},
		{-1.0f,  1.0f,  1.0f},
		{-1.0f, -1.0f,  1.0f},
		{ 1.0f, -1.0f,  1.0f},
		{ 1.0f,  1.0f,  1.0f},

		// Left
		{-1.0f, -1.0f,  1.0f},
		{-1.0f,  1.0f,  1.0f},
		{-1.0f,  1.0f, -1.0f},
		{-1.0f, -1.0f,  1.0f},
		{-1.0f,  1.0f, -1.0f},
		{-1.0f, -1.0f, -1.0f},

		// Right
		{ 1.0f, -1.0f, -1.0f},
		{ 1.0f,  1.0f, -1.0f},
		{ 1.0f,  1.0f,  1.0f},
		{ 1.0f, -1.0f, -1.0f},
		{ 1.0f,  1.0f,  1.0f},
		{ 1.0f, -1.0f,  1.0f},

		// Top
		{-1.0f,  1.0f, -1.0f},
		{-1.0f,  1.0f,  1.0f},
		{ 1.0f,  1.0f,  1.0f},
		{-1.0f,  1.0f, -1.0f},
		{ 1.0f,  1.0f,  1.0f},
		{ 1.0f,  1.0f, -1.0f},

		// Bottom
		{-1.0f, -1.0f,  1.0f},
		{-1.0f, -1.0f, -1.0f},
		{ 1.0f, -1.0f, -1.0f},
		{-1.0f, -1.0f,  1.0f},
		{ 1.0f, -1.0f, -1.0f},
		{ 1.0f, -1.0f,  1.0f}
	};

	const UINT vertexBufferSize = static_cast<UINT>(sizeof(vertices));

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = vertexBufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	const HRESULT createResult = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sVertexBuffer)
	);

	if (FAILED(createResult))
	{
		assert(false);
		return false;
	}

	sVertexBuffer->SetName(L"EnvironmentMap Skybox VertexBuffer");

	void* mappedMemory = nullptr;

	D3D12_RANGE readRange =
	{
		0,
		0
	};

	const HRESULT mapResult =
		sVertexBuffer->Map(
			0,
			&readRange,
			&mappedMemory
		);

	if (FAILED(mapResult) || mappedMemory == nullptr)
	{
		assert(false);
		return false;
	}

	memcpy(mappedMemory, vertices, sizeof(vertices));

	sVertexBuffer->Unmap(0, nullptr);
	sVertexBufferView.BufferLocation = sVertexBuffer->GetGPUVirtualAddress();
	sVertexBufferView.StrideInBytes = sizeof(SkyboxVertex);
	sVertexBufferView.SizeInBytes = vertexBufferSize;

	return true;
}

//============================================================
// Constant Buffer
//============================================================
bool EnvironmentMapManager::CreateConstantBuffer()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	const UINT bufferSize = (sizeof(SkyboxConstantBuffer) + 255u) & ~255u;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = bufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	const HRESULT createResult =
		device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&sConstantBuffer)
		);

	if (FAILED(createResult))
	{
		assert(false);
		return false;
	}

	sConstantBuffer->SetName(L"EnvironmentMap Skybox ConstantBuffer");

	const HRESULT mapResult =
		sConstantBuffer->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&sMappedConstantBuffer)
		);

	if (FAILED(mapResult) || sMappedConstantBuffer == nullptr)
	{
		assert(false);
		return false;
	}

	XMStoreFloat4x4(
		&sMappedConstantBuffer->ViewProjection,
		XMMatrixIdentity()
	);

	sMappedConstantBuffer->EnvironmentSceneParam =
	{
		0.0f,
		1.0f,
		0.0f,
		0.0f
	};
	return true;
}

//============================================================
// CubeMap用SRV Heap
//============================================================
bool EnvironmentMapManager::CreateDescriptorHeap()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = (UINT)EnvironmentSRVIndex::Count;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;

	const HRESULT hr = device->CreateDescriptorHeap(
		&heapDesc,
		IID_PPV_ARGS(&sSRVHeap)
	);

	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	sSRVHeap->SetName(L"EnvironmentMap CubeMap SRV Heap");

	return true;
}

//============================================================
// DDS CubeMap読み込み
//============================================================
bool EnvironmentMapManager::LoadCubeMap(const wchar_t* filePath)
{
	if (!sInitialized || filePath == nullptr || sSRVHeap == nullptr)
	{
		return false;
	}

	sHDRToCubeNeedsGeneration =false;

	SAFE_RELEASE(sHDREquirectangularTexture);

	SAFE_RELEASE(sEnvironmentRTVHeap);

	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	TexMetadata metadata = {};
	ScratchImage scratchImage = {};

	const HRESULT loadResult = LoadFromDDSFile(
		filePath,
		DDS_FLAGS_NONE,
		&metadata,
		scratchImage
	);

	if (FAILED(loadResult))
	{
		OutputDebugStringW(L"[EnvironmentMap] Failed to load DDS file.\n");
		return false;
	}

	//============================================================
	// 通常の2D DDSを誤って渡していないか確認
	//============================================================
	if (!metadata.IsCubemap())
	{
		OutputDebugStringW(L"[EnvironmentMap] DDS is not a CubeMap.\n");
		return false;
	}

	//============================================================
	// CubeMap 1個:
	//   ArraySize = 6
	//============================================================
	if (metadata.arraySize != 6)
	{
		OutputDebugStringW(L"[EnvironmentMap] CubeMap Array is not supported yet.\n");
		return false;
	}

	//============================================================
	// 新規Resource
	// CPU WriteBack Heapを使用する
	//============================================================
	ID3D12Resource* newCubeMap = nullptr;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapProperties.CreationNodeMask = 0;
	heapProperties.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = metadata.width;
	resourceDesc.Height = static_cast<UINT>(metadata.height);
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	const HRESULT createResult = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&newCubeMap)
	);

	if (FAILED(createResult) || newCubeMap == nullptr)
	{
		OutputDebugStringW(L"[EnvironmentMap] Failed to create CubeMap Resource.\n");
		assert(false);
		return false;
	}

	newCubeMap->SetName(L"EnvironmentMap Cube Texture");

	//============================================================
	// 全Face・全MipをResourceへ書き込む
	// Subresource番号: mipLevel + arraySlice * mipCount
	//============================================================
	for (size_t arraySlice = 0; arraySlice < metadata.arraySize; ++arraySlice)
	{
		for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel)
		{
			const Image* image = scratchImage.GetImage(
				mipLevel,
				arraySlice,
				0
			);

			if (image == nullptr)
			{
				SAFE_RELEASE(newCubeMap);
				return false;
			}

			const UINT subresourceIndex =
				static_cast<UINT>(
					mipLevel +
					arraySlice *
					metadata.mipLevels
					);

			const HRESULT writeResult =
				newCubeMap->WriteToSubresource(
					subresourceIndex,
					nullptr,
					image->pixels,
					static_cast<UINT>(image->rowPitch),
					static_cast<UINT>(image->slicePitch)
				);

			if (FAILED(writeResult))
			{
				OutputDebugStringW(L"[EnvironmentMap] WriteToSubresource failed.\n");
				SAFE_RELEASE(newCubeMap);
				assert(false);
				return false;
			}
		}
	}

	//============================================================
	// 読み込み成功後に古いCubeMapと交換
	//============================================================
	SAFE_RELEASE(sEnvironmentCube);

	sEnvironmentCube = newCubeMap;

	newCubeMap = nullptr;

	sEnvironmentResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	//============================================================
	// TextureCube SRV作成
	//============================================================
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = static_cast<UINT>(metadata.mipLevels);
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(
		sEnvironmentCube,
		&srvDesc,
		sSRVHeap->GetCPUDescriptorHandleForHeapStart()
	);

	sCubeMapLoaded = true;

	//============================================================
	// 元CubeMapの読み込みが完了したため、
	// Prefiltered CubeMap用Resourceを作成する
	//============================================================
	if (!CreatePrefilterResources())
	{
		return false;
	}

	sPrefilterNeedsGeneration = true;
	sPrefilterReady = false;

	if (!CreateIrradianceResources())
	{
		return false;
	}

	sIrradianceNeedsGeneration = true;
	sIrradianceReady = false;

	return true;
}

bool EnvironmentMapManager::LoadHDRI(const wchar_t* filePath)
{
	if (!sInitialized || filePath == nullptr || sSRVHeap == nullptr)
	{
		return false;
	}

	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	//========================================================
	// HDR File Load
	//
	// DirectXTex:
	//
	// .hdr
	//   ↓
	// R32G32B32A32_FLOAT
	//
	// Linear HDRとして読み込まれる
	//========================================================
	TexMetadata sourceMetadata = {};

	ScratchImage sourceImage = {};


	HRESULT hr =
		LoadFromHDRFile(
			filePath,
			&sourceMetadata,
			sourceImage
		);


	if (FAILED(hr))
	{
		OutputDebugStringW(
			L"[EnvironmentMap] "
			L"LoadFromHDRFile failed.\n"
		);

		return false;
	}


	//========================================================
	// Equirectangularは通常の2D Textureである必要がある
	//========================================================
	if (sourceMetadata.dimension != TEX_DIMENSION_TEXTURE2D || sourceMetadata.arraySize != 1)
	{
		OutputDebugStringW(
			L"[EnvironmentMap] "
			L"HDR must be 2D Equirectangular Texture.\n"
		);

		return false;
	}


	//========================================================
	// 2:1か確認
	//========================================================
	const float aspect =
		static_cast<float>(sourceMetadata.width) / static_cast<float>(sourceMetadata.height);


	if (fabsf(aspect - 2.0f) > 0.05f)
	{
		OutputDebugStringW(
			L"[EnvironmentMap] "
			L"Warning: HDR aspect is not approximately 2:1.\n"
		);
	}


	//========================================================
	// GPUへ8KのR32_FLOATをそのまま持つと非常に重い
	// 8K → 4Kへ縮小する
	//========================================================
	ScratchImage resizedImage = {};


	hr = Resize(
		sourceImage.GetImages(),
		sourceImage.GetImageCount(),
		sourceImage.GetMetadata(),
		HDRSourceWidth,
		HDRSourceHeight,
		TEX_FILTER_DEFAULT,
		resizedImage
	);


	if (FAILED(hr))
	{
		OutputDebugStringW(
			L"[EnvironmentMap] "
			L"HDR Resize failed.\n"
		);

		return false;
	}


	//========================================================
	// R32G32B32A32_FLOAT
	//       ↓
	// R16G16B16A16_FLOAT
	//
	// HDRレンジを維持しつつメモリを半分にする
	//========================================================
	ScratchImage convertedImage = {};


	hr = Convert(
		resizedImage.GetImages(),
		resizedImage.GetImageCount(),
		resizedImage.GetMetadata(),
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		TEX_FILTER_DEFAULT,
		TEX_THRESHOLD_DEFAULT,

		convertedImage
	);


	if (FAILED(hr))
	{
		OutputDebugStringW(
			L"[EnvironmentMap] "
			L"HDR FP16 conversion failed.\n"
		);

		return false;
	}


	const Image* hdrImage =
		convertedImage.GetImage(
			0,
			0,
			0
		);


	if (hdrImage == nullptr)
	{
		return false;
	}


	//========================================================
	// 古いHDR Sourceを破棄
	//========================================================
	SAFE_RELEASE(sHDREquirectangularTexture);


	//========================================================
	// Texture2D Resource
	//========================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;


	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = HDRSourceWidth;
	resourceDesc.Height = HDRSourceHeight;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&sHDREquirectangularTexture)
	);


	if (FAILED(hr) || sHDREquirectangularTexture == nullptr)
	{
		OutputDebugStringW(
			L"[EnvironmentMap] "
			L"Failed to create HDR Texture2D.\n"
		);

		return false;
	}


	sHDREquirectangularTexture->SetName(L"HDR Equirectangular Texture");


	//========================================================
	// CPU → Texture
	//========================================================
	hr = sHDREquirectangularTexture->WriteToSubresource(
		0,
		nullptr,
		hdrImage->pixels,
		static_cast<UINT>(hdrImage->rowPitch),
		static_cast<UINT>(hdrImage->slicePitch)
	);


	if (FAILED(hr))
	{
		OutputDebugStringW(
			L"[EnvironmentMap] "
			L"HDR WriteToSubresource failed.\n"
		);

		SAFE_RELEASE(sHDREquirectangularTexture);

		return false;
	}


	//========================================================
	// Internal HeapのHDR位置へTexture2D SRV
	//========================================================
	const UINT descriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	D3D12_CPU_DESCRIPTOR_HANDLE hdrHandle =
		sSRVHeap->GetCPUDescriptorHandleForHeapStart();


	hdrHandle.ptr +=
		static_cast<SIZE_T>(static_cast<UINT>(EnvironmentSRVIndex::HDREquirectangular)) *
		static_cast<SIZE_T>(descriptorSize);


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;


	device->CreateShaderResourceView(
		sHDREquirectangularTexture,
		&srvDesc,
		hdrHandle
	);


	//========================================================
	// CubeMap Output Resource作成
	//========================================================
	if (!CreateHDRToCubeResources())
	{
		return false;
	}


	//========================================================
	// この後にModelを生成した場合でも
	// Material Descriptorから参照できる
	//========================================================
	if (!CreatePrefilterResources())
	{
		return false;
	}


	if (!CreateIrradianceResources())
	{
		return false;
	}


	//========================================================
	// GPU変換待ち
	//========================================================
	sHDRToCubeNeedsGeneration = true;
	sCubeMapLoaded = false;
	sPrefilterNeedsGeneration = true;
	sPrefilterReady = false;
	sIrradianceNeedsGeneration = true;
	sIrradianceReady = false;


	OutputDebugStringW(
		L"[EnvironmentMap] "
		L"HDR Equirectangular loaded.\n"
	);


	return true;
}

bool EnvironmentMapManager::GenerateEnvironmentCubeFromHDR()
{
	//========================================================
	// 生成不要
	//========================================================
	if (!sHDRToCubeNeedsGeneration)
	{
		return sCubeMapLoaded;
	}


	if (sHDREquirectangularTexture == nullptr ||
		sEnvironmentCube == nullptr ||
		sEnvironmentRTVHeap == nullptr ||
		sHDRToCubeRootSignature == nullptr ||
		sHDRToCubePipelineState == nullptr ||
		sVertexBuffer == nullptr)
	{
		return false;
	}


	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	ID3D12Device* device = Direct3D::getDevice();


	if (commandList == nullptr || device == nullptr)
	{
		return false;
	}

	if (sEnvironmentResourceState != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = sEnvironmentCube;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		//========================================================
		// 現在State
		//========================================================
		barrier.Transition.StateBefore = sEnvironmentResourceState;

		//========================================================
		// HDR → Cubeの描画先へ戻す
		//========================================================
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		commandList->ResourceBarrier(
			1,
			&barrier
		);


		//========================================================
		// CPU側State Tracker更新
		//========================================================
		sEnvironmentResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	//========================================================
	// Cube Face Direction
	//========================================================
	const XMVECTOR faceDirections[6] =
	{
		XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f,-1.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f,-1.0f, 0.0f)
	};


	const XMVECTOR faceUpDirections[6] =
	{
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f,  1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f)
	};


	//========================================================
	// 90度Perspective
	//========================================================
	const XMMATRIX projection =
		XMMatrixPerspectiveFovLH(
			XM_PIDIV2,
			1.0f,
			0.1f,
			10.0f
		);


	//========================================================
	// Viewport
	//========================================================
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(EnvironmentCubeSize);
	viewport.Height = static_cast<float>(EnvironmentCubeSize);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(EnvironmentCubeSize),
		static_cast<LONG>(EnvironmentCubeSize)
	};


	commandList->RSSetViewports(
		1,
		&viewport
	);


	commandList->RSSetScissorRects(
		1,
		&scissor
	);


	//========================================================
	// Pipeline
	//========================================================
	commandList->SetGraphicsRootSignature(sHDRToCubeRootSignature);
	commandList->SetPipelineState(sHDRToCubePipelineState);


	//========================================================
	// HDR Source SRV
	//========================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		sSRVHeap
	};


	commandList->SetDescriptorHeaps(
		1,
		heaps
	);


	const UINT descriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	D3D12_GPU_DESCRIPTOR_HANDLE hdrHandle =
		sSRVHeap->GetGPUDescriptorHandleForHeapStart();


	hdrHandle.ptr +=
		static_cast<UINT64>(static_cast<UINT>(EnvironmentSRVIndex::HDREquirectangular)) *
		static_cast<UINT64>(descriptorSize);


	//========================================================
	// Root[1] = HDR t0
	//========================================================
	commandList->SetGraphicsRootDescriptorTable(
		2,
		hdrHandle
	);


	//========================================================
	// Cube Vertex
	//========================================================
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	commandList->IASetVertexBuffers(
		0,
		1,
		&sVertexBufferView
	);

	//============================================================
	// HDR → Cube Parameters
	// x : Environment Y Rotation [radian]
	// y : Environment Exposure Scale
	//============================================================
	const XMFLOAT4 hdrToCubeParam =
	{
		0.0f,   // Rotationなし
		1.0f,   // Exposureなし
		0.0f,
		0.0f
	};


	//============================================================
	// Root[1] = b1
	// Pixel ShaderへEnvironment Rotationを渡す
	//============================================================
	commandList->SetGraphicsRoot32BitConstants(
		1,
		4,
		&hdrToCubeParam,
		0
	);

	//========================================================
	// 6 Face
	//========================================================
	for (UINT faceIndex = 0; faceIndex < 6; ++faceIndex)
	{
		//====================================================
		// View
		//====================================================
		const XMMATRIX view =
			XMMatrixLookAtLH(
				XMVectorZero(),
				faceDirections[faceIndex],
				faceUpDirections[faceIndex]
			);


		const XMMATRIX viewProjection =
			XMMatrixMultiply(
				view,
				projection
			);


		XMFLOAT4X4 storedVP = {};


		XMStoreFloat4x4(
			&storedVP,
			XMMatrixTranspose(viewProjection)
		);


		//====================================================
		// Root[0] = Capture ViewProjection
		//====================================================
		commandList->SetGraphicsRoot32BitConstants(
			0,
			16,
			&storedVP,
			0
		);


		//====================================================
		// Face RTV
		//====================================================
		D3D12_CPU_DESCRIPTOR_HANDLE rtv =
			sEnvironmentRTVHeap->GetCPUDescriptorHandleForHeapStart();


		rtv.ptr +=
			static_cast<SIZE_T>(faceIndex) *
			static_cast<SIZE_T>(sEnvironmentRTVDescriptorSize);


		const float clearColor[4] =
		{
			0.0f,
			0.0f,
			0.0f,
			1.0f
		};


		commandList->OMSetRenderTargets(
			1,
			&rtv,
			FALSE,
			nullptr
		);


		commandList->ClearRenderTargetView(
			rtv,
			clearColor,
			0,
			nullptr
		);


		commandList->DrawInstanced(36, 1, 0, 0);
	}


	//========================================================
	// RenderTarget
	//      ↓
	// PixelShaderResource
	//========================================================
	if (sEnvironmentResourceState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = sEnvironmentCube;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		//========================================================
		// 現在はHDRを書き込んだ直後なので
		// RENDER_TARGET
		//========================================================
		barrier.Transition.StateBefore = sEnvironmentResourceState;
		//========================================================
		// IBL生成 / Skyboxから読む
		//========================================================
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


		commandList->ResourceBarrier(
			1,
			&barrier
		);

		//========================================================
		// State更新
		//========================================================
		sEnvironmentResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	//========================================================
	// Manager Heap [0]へTextureCube SRV
	//========================================================
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = RenderFormat::SceneHDR;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;


	device->CreateShaderResourceView(
		sEnvironmentCube,
		&srvDesc,
		sSRVHeap->GetCPUDescriptorHandleForHeapStart()
	);


	//========================================================
	// Complete
	//========================================================
	sHDRToCubeNeedsGeneration = false;

	sCubeMapLoaded = true;


	//========================================================
	// 通常Scene Render Targetへ戻す
	//========================================================
	Direct3D::setMainRenderTarget();


	OutputDebugStringW(
		L"[EnvironmentMap] "
		L"HDR -> CubeMap Complete.\n"
	);


	return true;
}

//============================================================
// Skybox描画
//============================================================
void EnvironmentMapManager::RenderSkybox()
{
	if (!sInitialized ||
		!sCubeMapLoaded ||
		sEnvironmentCube == nullptr ||
		sPipelineState == nullptr ||
		sRootSignature == nullptr ||
		sMappedConstantBuffer == nullptr)
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return;
	}

	//============================================================
	// カメラ行列
	//============================================================
	const XMMATRIX cameraView = *vnCamera::getView();

	const XMMATRIX cameraViewProjection = *vnCamera::getScreen();

	//============================================================
	// Projection行列を復元
	// cameraViewProjection = View * Projection
	// Projection = inverse(View) * ViewProjection
	//============================================================
	const XMMATRIX projection =
		XMMatrixMultiply(
			XMMatrixInverse(nullptr, cameraView),
			cameraViewProjection
		);

	//============================================================
	// View行列から平行移動を除去
	// カメラが移動してもSkyboxの中心から離れないようにする
	// 回転だけは残すため、視点を回すと背景も変化する
	//============================================================
	XMMATRIX skyboxView = cameraView;

	skyboxView.r[3] =
		XMVectorSet(
			0.0f,
			0.0f,
			0.0f,
			1.0f
		);

	const XMMATRIX skyboxViewProjection =
		XMMatrixMultiply(
			skyboxView,
			projection
		);

	//============================================================
	// ※HLSL側でmul(vector, matrix)を使用するため転置して格納
	//============================================================
	XMStoreFloat4x4(
		&sMappedConstantBuffer->ViewProjection,
		XMMatrixTranspose(skyboxViewProjection)
	);

	sMappedConstantBuffer->EnvironmentSceneParam =
	{
		GetEnvironmentRotationRadians(),
		GetEnvironmentExposureScale(),
		0.0f,
		0.0f
	};
	//============================================================
	// Viewport
	//============================================================
	commandList->RSSetViewports(
		1,
		vnCamera::getViewport()
	);

	D3D12_RECT scissorRect =
	{
		0,
		0,
		static_cast<LONG>(vnMainFrame::screenWidth),
		static_cast<LONG>(vnMainFrame::screenHeight)
	};

	commandList->RSSetScissorRects(
		1,
		&scissorRect
	);

	//============================================================
	// Pipeline
	//============================================================
	commandList->SetGraphicsRootSignature(sRootSignature);
	commandList->SetPipelineState(sPipelineState);

	//============================================================
	// b0 : Skybox ViewProjection
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		0,
		sConstantBuffer->GetGPUVirtualAddress()
	);

	//============================================================
	// t0 : Environment CubeMap
	//============================================================
	ID3D12DescriptorHeap* descriptorHeaps[] =
	{
		sSRVHeap
	};

	commandList->SetDescriptorHeaps(
		1,
		descriptorHeaps
	);

	commandList->SetGraphicsRootDescriptorTable(
		1,
		sSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);

	//============================================================
	// Cube描画
	//============================================================
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(
		0,
		1,
		&sVertexBufferView
	);

	commandList->DrawInstanced(36, 1, 0, 0);
}

//============================================================
// Getter
//============================================================
ID3D12Resource*EnvironmentMapManager::GetEnvironmentResource()
{
	return sEnvironmentCube;
}

ID3D12DescriptorHeap* EnvironmentMapManager::GetEnvironmentSRVHeap()
{
	return sSRVHeap;
}

D3D12_GPU_DESCRIPTOR_HANDLE EnvironmentMapManager::GetEnvironmentSRVGPU()
{
	if (sSRVHeap == nullptr)
	{
		return {};
	}

	return sSRVHeap->GetGPUDescriptorHandleForHeapStart();
}

void EnvironmentMapManager::WriteEnvironmentSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle)
{
	ID3D12Device* device = Direct3D::getDevice();

	assert(device != nullptr);

	if (device == nullptr)
	{
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	//============================================================
	// 実際のEnvironment CubeMapが存在する場合
	//============================================================
	if (sEnvironmentCube != nullptr)
	{
		const D3D12_RESOURCE_DESC resourceDesc = sEnvironmentCube->GetDesc();
		srvDesc.Format = resourceDesc.Format;
		srvDesc.TextureCube.MipLevels = resourceDesc.MipLevels;
		device->CreateShaderResourceView(
			sEnvironmentCube,
			&srvDesc,
			destinationHandle
		);

		return;
	}

	//============================================================
	// CubeMapがまだ読み込まれていない場合
	// 未初期化Descriptorを残さず、
	// Null TextureCube Descriptorを作成する
	// Null SRVをSampleすると0が返るため、
	// Environment未設定でも安全に描画できる
	//============================================================
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	srvDesc.TextureCube.MipLevels = 1;

	device->CreateShaderResourceView(
		nullptr,
		&srvDesc,
		destinationHandle
	);
}

//============================================================
// Environment CubeMapのMip数
//============================================================
UINT EnvironmentMapManager::GetEnvironmentMipCount()
{
	if (sEnvironmentCube == nullptr)
	{
		return 1;
	}

	const D3D12_RESOURCE_DESC resourceDesc = sEnvironmentCube->GetDesc();

	return(std::max)(static_cast<UINT>(resourceDesc.MipLevels), 1u);
}

void EnvironmentMapManager::SetEnvironmentRotationDegrees(float degrees)
{
	//========================================================
	// 360度以内へ正規化
	//========================================================
	float normalizedDegrees = fmodf(degrees, 360.0f);

	if (normalizedDegrees < 0.0f)
	{
		normalizedDegrees += 360.0f;
	}
	const float newRadians =XMConvertToRadians(normalizedDegrees);

	if (fabsf(newRadians -sEnvironmentRotationRadians) <0.000001f)
	{
		return;
	}

	//========================================================
	// ShaderではRadianを使用する
	//========================================================
	sEnvironmentRotationRadians = newRadians;
	//========================================================
	// HDRIがすでにロードされている場合、
	// CubeMapを再生成する
	// ※ハングする可能性が高いため再生成中止
	//========================================================
	//if (sHDREquirectangularTexture != nullptr)
	//{
	//	sHDRToCubeNeedsGeneration = true;
	//	sPrefilterNeedsGeneration = true;
	//	sPrefilterReady = false;
	//	sIrradianceNeedsGeneration = true;
	//	sIrradianceReady = false;
	//}
}

void EnvironmentMapManager::AddEnvironmentRotationDegrees(float degrees)
{
	const float currentDegrees = GetEnvironmentRotationDegrees();
	SetEnvironmentRotationDegrees(currentDegrees + degrees);
}

float EnvironmentMapManager::GetEnvironmentRotationDegrees()
{
	return XMConvertToDegrees(sEnvironmentRotationRadians);
}

void EnvironmentMapManager::SetEnvironmentExposureEV(float exposureEV)
{
	const float newExposureEV = std::clamp(exposureEV, -16.0f, 16.0f);

	//========================================================
	// 実質的に変化していなければ
	// Environmentを再生成しない
	//========================================================
	if (fabsf(newExposureEV - sEnvironmentExposureEV) < 0.000001f)
	{
		return;
	}


	//========================================================
	// 新しいExposureを保存
	//========================================================
	sEnvironmentExposureEV = newExposureEV;

	//========================================================
	// HDR Equirectangularから生成している場合のみ
	// Environmentを作り直す
	// ※ハングする可能性が高いため再生成中止
	//========================================================
	//if (sHDREquirectangularTexture != nullptr)
	//{
	//	//====================================================
	//	// 元Environment Cube
	//	//====================================================
	//	sHDRToCubeNeedsGeneration =
	//		true;


	//	//====================================================
	//	// 新しいExposureのEnvironmentから
	//	// Specular Prefilterを作り直す
	//	//====================================================
	//	sPrefilterNeedsGeneration = true;
	//	sPrefilterReady = false;
	//	//====================================================
	//	// Diffuse Irradianceも作り直す
	//	//====================================================
	//	sIrradianceNeedsGeneration = true;
	//	sIrradianceReady = false;
	//	//====================================================
	//	// 再生成が終わるまではCubeMap更新待ち状態
	//	//====================================================
	//	sCubeMapLoaded = false;
	//}
}

void EnvironmentMapManager::AddEnvironmentExposureEV(float exposureEV)
{
	SetEnvironmentExposureEV(sEnvironmentExposureEV +exposureEV);
}

float EnvironmentMapManager::GetEnvironmentExposureEV()
{
	return sEnvironmentExposureEV;
}

float EnvironmentMapManager::GetEnvironmentExposureScale()
{
	return exp2f(sEnvironmentExposureEV);
}

float EnvironmentMapManager::GetEnvironmentRotationRadians()
{
	return  sEnvironmentRotationRadians;
}

bool EnvironmentMapManager::GeneratePrefilteredEnvironment()
{
	if (!sInitialized ||
		!sCubeMapLoaded ||
		!sPrefilterNeedsGeneration)
	{
		return sPrefilterReady;
	}

	if (sEnvironmentCube == nullptr ||
		sPrefilteredEnvironmentCube == nullptr ||
		sPrefilterRTVHeap == nullptr ||
		sPrefilterRootSignature == nullptr ||
		sPrefilterPipelineState == nullptr ||
		sVertexBuffer == nullptr)
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return false;
	}

	//============================================================
	// 再生成時はShader ResourceからRender Targetへ戻す
	//============================================================
	if (sPrefilterResourceState != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = sPrefilteredEnvironmentCube;
		barrier.Transition.StateBefore = sPrefilterResourceState;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		commandList->ResourceBarrier(
			1,
			&barrier
		);

		sPrefilterResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	//============================================================
	// CubeMap Face方向
	//============================================================
	const XMVECTOR faceDirections[6] =
	{
		XMVectorSet(1.0f,  0.0f,  0.0f, 0.0f), // +X
		XMVectorSet(-1.0f,  0.0f,  0.0f, 0.0f), // -X
		XMVectorSet(0.0f,  1.0f,  0.0f, 0.0f), // +Y
		XMVectorSet(0.0f, -1.0f,  0.0f, 0.0f), // -Y
		XMVectorSet(0.0f,  0.0f,  1.0f, 0.0f), // +Z
		XMVectorSet(0.0f,  0.0f, -1.0f, 0.0f)  // -Z
	};

	const XMVECTOR faceUpDirections[6] =
	{
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f), // +X
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f), // -X
		XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), // +Y
		XMVectorSet(0.0f, 0.0f,  1.0f, 0.0f), // -Y
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f), // +Z
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f)  // -Z
	};

	const XMMATRIX projection =
		XMMatrixPerspectiveFovLH(
			XM_PIDIV2,
			1.0f,
			0.1f,
			10.0f
		);

	commandList->SetGraphicsRootSignature(sPrefilterRootSignature);
	commandList->SetPipelineState(sPrefilterPipelineState);

	ID3D12DescriptorHeap* descriptorHeaps[] =
	{
		sSRVHeap
	};

	commandList->SetDescriptorHeaps(
		1,
		descriptorHeaps
	);

	//============================================================
	// RootParameter[2] t0
	//
	// Manager Heapの先頭は元Environment CubeMap。
	//============================================================
	commandList->SetGraphicsRootDescriptorTable(
		2,
		sSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);

	commandList->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);

	commandList->IASetVertexBuffers(
		0,
		1,
		&sVertexBufferView
	);

	const D3D12_RESOURCE_DESC sourceDesc = sEnvironmentCube->GetDesc();

	const float sourceResolution = static_cast<float>(sourceDesc.Width);

	const float sourceMaximumMip =
		static_cast<float>(sourceDesc.MipLevels > 0 ? sourceDesc.MipLevels - 1 : 0);

	//============================================================
	// 各Mipを生成
	//============================================================
	for (UINT mipLevel = 0; mipLevel < PrefilterMipCount; ++mipLevel)
	{
		const UINT mipSize = (std::max)(PrefilterSize >> mipLevel, 1u);

		const float roughness = PrefilterMipCount > 1 ? static_cast<float>(mipLevel) / static_cast<float>(PrefilterMipCount - 1) : 0.0f;

		D3D12_VIEWPORT viewport = {};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(mipSize);
		viewport.Height = static_cast<float>(mipSize);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D12_RECT scissorRect =
		{
			0,
			0,
			static_cast<LONG>(mipSize),
			static_cast<LONG>(mipSize)
		};

		commandList->RSSetViewports(
			1,
			&viewport
		);

		commandList->RSSetScissorRects(
			1,
			&scissorRect
		);

		//========================================================
		// b1 PrefilterParam
		//========================================================
		const XMFLOAT4 prefilterParam =
		{
			roughness,
			sourceResolution,
			sourceMaximumMip,
			128.0f
		};

		commandList->SetGraphicsRoot32BitConstants(
			1,
			4,
			&prefilterParam,
			0
		);

		//========================================================
		// 6Faceを描画
		//========================================================
		for (UINT faceIndex = 0; faceIndex < 6; ++faceIndex)
		{
			const XMMATRIX view =
				XMMatrixLookAtLH(
					XMVectorZero(),
					faceDirections[faceIndex],
					faceUpDirections[faceIndex]
				);

			const XMMATRIX viewProjection =
				XMMatrixMultiply(
					view,
					projection
				);

			XMFLOAT4X4 storedViewProjection = {};

			XMStoreFloat4x4(
				&storedViewProjection,
				XMMatrixTranspose(viewProjection)
			);

			//====================================================
			// b0 CaptureViewProjection
			//====================================================
			commandList->SetGraphicsRoot32BitConstants(
				0,
				16,
				&storedViewProjection,
				0
			);

			const UINT descriptorIndex =
				mipLevel * 6 + faceIndex;

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = sPrefilterRTVHeap->GetCPUDescriptorHandleForHeapStart();

			rtvHandle.ptr +=
				static_cast<SIZE_T>(descriptorIndex) *
				static_cast<SIZE_T>(sPrefilterRTVDescriptorSize);

			const float clearColor[4] =
			{
				0.0f,
				0.0f,
				0.0f,
				1.0f
			};

			commandList->OMSetRenderTargets(
				1,
				&rtvHandle,
				FALSE,
				nullptr
			);

			commandList->ClearRenderTargetView(
				rtvHandle,
				clearColor,
				0,
				nullptr
			);

			commandList->DrawInstanced(36, 1, 0, 0);
		}
	}

	//============================================================
	// 生成後はPixel Shaderから読める状態へ変更
	//============================================================
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = sPrefilteredEnvironmentCube;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(
		1,
		&barrier
	);

	sPrefilterResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	sPrefilterNeedsGeneration = false;
	sPrefilterReady = true;

	//============================================================
	// Prefilter生成で変更した状態を、
	// 通常3D描画状態へ戻す。
	//============================================================
	Direct3D::setMainRenderTarget();

	return true;
}

void EnvironmentMapManager::WritePrefilteredEnvironmentSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle)
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	if (sPrefilteredEnvironmentCube != nullptr)
	{
		const D3D12_RESOURCE_DESC resourceDesc = sPrefilteredEnvironmentCube->GetDesc();
		srvDesc.Format = resourceDesc.Format;
		srvDesc.TextureCube.MipLevels = resourceDesc.MipLevels;

		device->CreateShaderResourceView(
			sPrefilteredEnvironmentCube,
			&srvDesc,
			destinationHandle
		);

		return;
	}

	//============================================================
	// 未生成時はNull TextureCube SRV
	//============================================================
	srvDesc.Format = RenderFormat::SceneHDR;
	srvDesc.TextureCube.MipLevels = 1;

	device->CreateShaderResourceView(
		nullptr,
		&srvDesc,
		destinationHandle
	);
}

UINT EnvironmentMapManager::GetPrefilteredMipCount()
{
	if (sPrefilteredEnvironmentCube == nullptr)
	{
		return 1;
	}

	return(std::max)(static_cast<UINT>(sPrefilteredEnvironmentCube->GetDesc().MipLevels), 1u);
}

ID3D12Resource* EnvironmentMapManager::GetPrefilteredEnvironmentResource()
{
	return sPrefilteredEnvironmentCube;
}

bool EnvironmentMapManager::CreatePrefilterRootSignature()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	//============================================================
	// 元Environment CubeMap用SRV
	//============================================================
	D3D12_DESCRIPTOR_RANGE environmentRange = {};

	environmentRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	environmentRange.NumDescriptors = 1;
	environmentRange.BaseShaderRegister = 0;
	environmentRange.RegisterSpace = 0;
	environmentRange.OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};

	//============================================================
	// [0] b0 : Capture ViewProjection
	// 4×4行列なので32bit値16個
	//============================================================
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[0].Constants.ShaderRegister = 0;
	rootParameters[0].Constants.RegisterSpace = 0;
	rootParameters[0].Constants.Num32BitValues = 16;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	//============================================================
	// [1] b1 : PrefilterParam
	// float4なので32bit値4個
	//============================================================
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[1].Constants.ShaderRegister = 1;
	rootParameters[1].Constants.RegisterSpace = 0;
	rootParameters[1].Constants.Num32BitValues = 4;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//============================================================
	// [2] t0 : 元Environment CubeMap
	//============================================================
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &environmentRange;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//============================================================
	// Static Sampler
	//============================================================
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.ShaderRegister = 0;
	samplerDesc.RegisterSpace = 0;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = _countof(rootParameters);
	rootDesc.pParameters = rootParameters;
	rootDesc.NumStaticSamplers = 1;
	rootDesc.pStaticSamplers = &samplerDesc;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* signatureBlob = nullptr;

	ID3DBlob* errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);

	if (FAILED(hr))
	{
		if (errorBlob != nullptr)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}

		SAFE_RELEASE(signatureBlob);
		SAFE_RELEASE(errorBlob);

		assert(false);

		return false;
	}

	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&sPrefilterRootSignature)
	);

	SAFE_RELEASE(signatureBlob);
	SAFE_RELEASE(errorBlob);

	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	sPrefilterRootSignature->SetName(L"Environment Prefilter RootSignature");

	return true;
}

bool EnvironmentMapManager::CreatePrefilterPipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || sPrefilterRootSignature == nullptr)
	{
		return false;
	}

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.pRootSignature = sPrefilterRootSignature;

	psoDesc.InputLayout =
	{
		inputLayout,_countof(inputLayout)
	};

	psoDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_PrefilterEnvironment)->getCode();
	psoDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_PrefilterEnvironment)->getLength();
	psoDesc.PS.pShaderBytecode = Shader::getPShader(Shader::ePixelShader::PS_PrefilterEnvironment)->getCode();
	psoDesc.PS.BytecodeLength = Shader::getPShader(Shader::ePixelShader::PS_PrefilterEnvironment)->getLength();

	//============================================================
	// Cubeの内側・外側を気にせず描画する
	//============================================================
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;

	//============================================================
	// Blendなし
	//============================================================
	psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// Prefilter生成ではDepth不要
	//============================================================
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = RenderFormat::SceneHDR;
	psoDesc.SampleDesc.Count = 1;

	const HRESULT hr = device->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(&sPrefilterPipelineState)
	);

	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	sPrefilterPipelineState->SetName(L"Environment Prefilter PipelineState");

	return true;
}

bool EnvironmentMapManager::CreatePrefilterResources()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	SAFE_RELEASE(sPrefilteredEnvironmentCube);

	SAFE_RELEASE(sPrefilterRTVHeap);

	sPrefilterReady = false;

	//============================================================
	// HDR CubeMap Resource
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = PrefilterSize;
	resourceDesc.Height = PrefilterSize;

	//============================================================
	// CubeMapなのでArraySizeは6
	//============================================================
	resourceDesc.DepthOrArraySize = 6;
	resourceDesc.MipLevels = PrefilterMipCount;
	resourceDesc.Format = RenderFormat::SceneHDR;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = RenderFormat::SceneHDR;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 1.0f;

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&sPrefilteredEnvironmentCube)
	);

	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	sPrefilteredEnvironmentCube->SetName(L"Environment Prefiltered CubeMap");

	sPrefilterResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	//============================================================
	// RTV Heap
	// 6 Face × PrefilterMipCount
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 6 * PrefilterMipCount;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = device->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(&sPrefilterRTVHeap)
	);

	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	sPrefilterRTVHeap->SetName(L"Environment Prefilter RTV Heap");

	sPrefilterRTVDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//============================================================
	// 各Mip・各FaceのRTVを作成
	//============================================================
	for (UINT mipLevel = 0; mipLevel < PrefilterMipCount; ++mipLevel)
	{
		for (UINT faceIndex = 0; faceIndex < 6; ++faceIndex)
		{
			const UINT descriptorIndex =
				mipLevel * 6 + faceIndex;

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = sPrefilterRTVHeap->GetCPUDescriptorHandleForHeapStart();

			rtvHandle.ptr +=
				static_cast<SIZE_T>(descriptorIndex) *
				static_cast<SIZE_T>(sPrefilterRTVDescriptorSize);

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = RenderFormat::SceneHDR;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = mipLevel;
			rtvDesc.Texture2DArray.FirstArraySlice = faceIndex;
			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.PlaneSlice = 0;

			device->CreateRenderTargetView(
				sPrefilteredEnvironmentCube,
				&rtvDesc,
				rtvHandle
			);
		}
	}

	//============================================================
	// Manager Heap [1]へTextureCube SRVを作成
	//============================================================
	const UINT srvDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE prefilteredSRVHandle = sSRVHeap->GetCPUDescriptorHandleForHeapStart();

	prefilteredSRVHandle.ptr += 
		static_cast<SIZE_T>(static_cast<UINT>(EnvironmentSRVIndex::PrefilteredEnvironment)) *
		static_cast<SIZE_T>(srvDescriptorSize); 

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = RenderFormat::SceneHDR;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = PrefilterMipCount;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(
		sPrefilteredEnvironmentCube,
		&srvDesc,
		prefilteredSRVHandle
	);

	sPrefilterNeedsGeneration = true;

	return true;
}

bool EnvironmentMapManager::GenerateIrradianceEnvironment()
{
	if (!sInitialized ||!sCubeMapLoaded ||!sIrradianceNeedsGeneration)
	{
		return sIrradianceReady;
	}

	if (sEnvironmentCube == nullptr ||
		sIrradianceEnvironmentCube == nullptr ||
		sIrradianceRTVHeap == nullptr ||
		sPrefilterRootSignature == nullptr ||
		sIrradiancePipelineState == nullptr ||
		sVertexBuffer == nullptr)
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return false;
	}

	//============================================================
	// 再生成時はRenderTarget状態へ戻す
	//============================================================
	if (sIrradianceResourceState != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = sIrradianceEnvironmentCube;
		barrier.Transition.StateBefore = sIrradianceResourceState;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		commandList->ResourceBarrier(
			1,
			&barrier
		);

		sIrradianceResourceState =D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	//============================================================
	// CubeMapの各Face方向
	//============================================================
	const XMVECTOR faceDirections[6] =
	{
		XMVectorSet(1.0f,  0.0f,  0.0f, 0.0f),
		XMVectorSet(-1.0f,  0.0f,  0.0f, 0.0f),
		XMVectorSet(0.0f,  1.0f,  0.0f, 0.0f),
		XMVectorSet(0.0f, -1.0f,  0.0f, 0.0f),
		XMVectorSet(0.0f,  0.0f,  1.0f, 0.0f),
		XMVectorSet(0.0f,  0.0f, -1.0f, 0.0f)
	};

	const XMVECTOR faceUpDirections[6] =
	{
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f,  1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f)
	};

	const XMMATRIX projection =
		XMMatrixPerspectiveFovLH(
			XM_PIDIV2,
			1.0f,
			0.1f,
			10.0f
		);

	//============================================================
	// Irradiance生成用Pipeline
	//============================================================
	commandList->SetGraphicsRootSignature(sPrefilterRootSignature);
	commandList->SetPipelineState(sIrradiancePipelineState);

	//============================================================
	// 元Environment CubeMapを含むHeap
	//============================================================
	ID3D12DescriptorHeap* descriptorHeaps[] =
	{
		sSRVHeap
	};

	commandList->SetDescriptorHeaps(
		1,
		descriptorHeaps
	);

	//============================================================
	// RootParameter[2] = t0
	// Heap先頭の元Environment CubeMapを設定
	//============================================================
	commandList->SetGraphicsRootDescriptorTable(
		2,
		sSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(
		0,
		1,
		&sVertexBufferView
	);

	//============================================================
	// Irradiance CubeMap解像度
	//============================================================
	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(IrradianceSize);
	viewport.Height = static_cast<float>(IrradianceSize);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissorRect =
	{
		0,
		0,
		static_cast<LONG>(IrradianceSize),
		static_cast<LONG>(IrradianceSize)
	};

	commandList->RSSetViewports(
		1,
		&viewport
	);

	commandList->RSSetScissorRects(
		1,
		&scissorRect
	);

	const D3D12_RESOURCE_DESC sourceDesc = sEnvironmentCube->GetDesc();

	//============================================================
	// b1 : IrradianceParam
	// 128サンプルで開始する
	// ノイズが目立つ場合は256へ増やす
	//============================================================
	const XMFLOAT4 irradianceParam =
	{
		128.0f,
		static_cast<float>(sourceDesc.Width),
		static_cast<float>(sourceDesc.MipLevels > 0 ? sourceDesc.MipLevels - 1 : 0),
		0.0f
	};

	commandList->SetGraphicsRoot32BitConstants(
		1,
		4,
		&irradianceParam,
		0
	);

	//============================================================
	// 6面を描画
	//============================================================
	for (UINT faceIndex = 0; faceIndex < 6; ++faceIndex)
	{
		const XMMATRIX view =
			XMMatrixLookAtLH(
				XMVectorZero(),
				faceDirections[faceIndex],
				faceUpDirections[faceIndex]
			);

		const XMMATRIX viewProjection =
			XMMatrixMultiply(
				view,
				projection
			);

		XMFLOAT4X4 storedViewProjection = {};

		XMStoreFloat4x4(
			&storedViewProjection,
			XMMatrixTranspose(viewProjection)
		);

		//========================================================
		// RootParameter[0] = b0
		//========================================================
		commandList->SetGraphicsRoot32BitConstants(
			0,
			16,
			&storedViewProjection,
			0
		);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = sIrradianceRTVHeap->GetCPUDescriptorHandleForHeapStart();

		rtvHandle.ptr +=
			static_cast<SIZE_T>(faceIndex) *
			static_cast<SIZE_T>(sIrradianceRTVDescriptorSize);

		const float clearColor[4] =
		{
			0.0f,
			0.0f,
			0.0f,
			1.0f
		};

		commandList->OMSetRenderTargets(
			1,
			&rtvHandle,
			FALSE,
			nullptr
		);

		commandList->ClearRenderTargetView(
			rtvHandle,
			clearColor,
			0,
			nullptr
		);

		commandList->DrawInstanced(36, 1, 0, 0);
	}

	//============================================================
	// Pixel Shaderから参照可能な状態へ変更
	//============================================================
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = sIrradianceEnvironmentCube;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(
		1,
		&barrier
	);

	sIrradianceResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	sIrradianceNeedsGeneration = false;
	sIrradianceReady = true;

	//============================================================
	// 通常3D描画状態へ復帰
	//============================================================
	Direct3D::setMainRenderTarget();

	return true;
}

void EnvironmentMapManager::WriteIrradianceEnvironmentSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle)
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	if (sIrradianceEnvironmentCube != nullptr)
	{
		const D3D12_RESOURCE_DESC resourceDesc = sIrradianceEnvironmentCube->GetDesc();

		srvDesc.Format = resourceDesc.Format;
		srvDesc.TextureCube.MipLevels = resourceDesc.MipLevels;

		device->CreateShaderResourceView(
			sIrradianceEnvironmentCube,
			&srvDesc,
			destinationHandle
		);

		return;
	}

	//============================================================
	// 未作成時はNull TextureCube
	//============================================================
	srvDesc.Format = RenderFormat::SceneHDR;

	srvDesc.TextureCube.MipLevels = 1;

	device->CreateShaderResourceView(
		nullptr,
		&srvDesc,
		destinationHandle
	);
}

ID3D12Resource* EnvironmentMapManager::GetIrradianceEnvironmentResource()
{
	return sIrradianceEnvironmentCube;;
}

bool EnvironmentMapManager::CreateIrradiancePipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || sPrefilterRootSignature == nullptr)
	{
		return false;
	}

	//============================================================
	// Prefilter用Cube Vertex Bufferと同じ頂点構造
	//============================================================
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.pRootSignature = sPrefilterRootSignature;

	psoDesc.InputLayout =
	{
		inputLayout,_countof(inputLayout)
	};

	//============================================================
	// Vertex ShaderはPrefilter用を再利用
	//============================================================
	psoDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_PrefilterEnvironment)->getCode();
	psoDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_PrefilterEnvironment)->getLength();

	//============================================================
	// Pixel ShaderだけIrradiance用
	//============================================================
	psoDesc.PS.pShaderBytecode = Shader::getPShader(Shader::ePixelShader::PS_IrradianceEnvironment)->getCode();
	psoDesc.PS.BytecodeLength = Shader::getPShader(Shader::ePixelShader::PS_IrradianceEnvironment)->getLength();

	//============================================================
	// Cubeの向きに依存しないようCullなし
	//============================================================
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;
	//============================================================
	// Blendなし
	//============================================================
	psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	//============================================================
	// Depth不要
	//============================================================
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = RenderFormat::SceneHDR;
	psoDesc.SampleDesc.Count = 1;

	const HRESULT hr = device->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(&sIrradiancePipelineState)
	);

	if (FAILED(hr) || sIrradiancePipelineState == nullptr)
	{
		OutputDebugStringW(L"[EnvironmentMap] Failed to create Irradiance PSO.\n");

		assert(false);

		return false;
	}

	sIrradiancePipelineState->SetName(L"Environment Irradiance PipelineState");

	return true;
}

bool EnvironmentMapManager::CreateIrradianceResources()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || sSRVHeap == nullptr)
	{
		return false;
	}

	SAFE_RELEASE(sIrradianceEnvironmentCube);

	SAFE_RELEASE(sIrradianceRTVHeap);

	sIrradianceReady = false;

	//============================================================
	// 32×32、6面、Mip 1のHDR CubeMap
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = IrradianceSize;
	resourceDesc.Height = IrradianceSize;
	resourceDesc.DepthOrArraySize = 6;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = RenderFormat::SceneHDR;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = RenderFormat::SceneHDR;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 1.0f;

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&sIrradianceEnvironmentCube)
	);

	if (FAILED(hr) || sIrradianceEnvironmentCube == nullptr)
	{
		OutputDebugStringW(L"[EnvironmentMap] Failed to create Irradiance CubeMap.\n");

		assert(false);

		return false;
	}

	sIrradianceEnvironmentCube->SetName(L"Environment Irradiance CubeMap");

	sIrradianceResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	//============================================================
	// 6面分のRTV Heap
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 6;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = device->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(&sIrradianceRTVHeap)
	);

	if (FAILED(hr) || sIrradianceRTVHeap == nullptr)
	{
		OutputDebugStringW(L"[EnvironmentMap] Failed to create Irradiance RTV Heap.\n");

		assert(false);

		return false;
	}

	sIrradianceRTVHeap->SetName(L"Environment Irradiance RTV Heap");

	sIrradianceRTVDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//============================================================
	// 各Face用RTV作成
	//============================================================
	for (UINT faceIndex = 0; faceIndex < 6; ++faceIndex)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = sIrradianceRTVHeap->GetCPUDescriptorHandleForHeapStart();

		rtvHandle.ptr +=
			static_cast<SIZE_T>(faceIndex) *
			static_cast<SIZE_T>(sIrradianceRTVDescriptorSize);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = RenderFormat::SceneHDR;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.FirstArraySlice = faceIndex;
		rtvDesc.Texture2DArray.ArraySize = 1;
		rtvDesc.Texture2DArray.PlaneSlice = 0;

		device->CreateRenderTargetView(
			sIrradianceEnvironmentCube,
			&rtvDesc,
			rtvHandle
		);
	}

	//============================================================
	// Manager Heap [2]へTextureCube SRV作成
	//============================================================
	const UINT descriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE irradianceHandle = sSRVHeap->GetCPUDescriptorHandleForHeapStart();

	irradianceHandle.ptr +=
		static_cast<SIZE_T>(static_cast<UINT>(EnvironmentSRVIndex::IrradianceEnvironment)) *
		static_cast<SIZE_T>(descriptorSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = RenderFormat::SceneHDR;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(
		sIrradianceEnvironmentCube,
		&srvDesc,
		irradianceHandle
	);

	sIrradianceNeedsGeneration = true;

	return true;
}

bool EnvironmentMapManager::GenerateBRDFLUT()
{
	if (!sInitialized || !sBRDFLUTNeedsGeneration)
	{
		return sBRDFLUTReady;
	}

	if (sBRDFLUT == nullptr ||
		sBRDFLUTRTVHeap == nullptr ||
		sBRDFLUTRootSignature == nullptr ||
		sBRDFLUTPipelineState == nullptr)
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return false;
	}

	//============================================================
	// 再生成時はRenderTargetへ戻す
	//============================================================
	if (sBRDFLUTResourceState != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = sBRDFLUT;
		barrier.Transition.StateBefore = sBRDFLUTResourceState;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		commandList->ResourceBarrier(
			1,
			&barrier
		);

		sBRDFLUTResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	//============================================================
	// Viewport
	//============================================================
	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(BRDFLUTSize);
	viewport.Height = static_cast<float>(BRDFLUTSize);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissorRect =
	{
		0,
		0,
		static_cast<LONG>(BRDFLUTSize),
		static_cast<LONG>(BRDFLUTSize)
	};

	commandList->RSSetViewports(
		1,
		&viewport
	);

	commandList->RSSetScissorRects(
		1,
		&scissorRect
	);

	//============================================================
	// Pipeline
	//============================================================
	commandList->SetGraphicsRootSignature(sBRDFLUTRootSignature);
	commandList->SetPipelineState(sBRDFLUTPipelineState);

	//============================================================
	// b0 : BRDFParam
	//============================================================
	const XMFLOAT4 brdfParam =
	{
		256.0f,
		0.0f,
		0.0f,
		0.0f
	};

	commandList->SetGraphicsRoot32BitConstants(
		0,
		4,
		&brdfParam,
		0
	);

	const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = sBRDFLUTRTVHeap->GetCPUDescriptorHandleForHeapStart();

	const float clearColor[4] =
	{
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	commandList->OMSetRenderTargets(
		1,
		&rtvHandle,
		FALSE,
		nullptr
	);

	commandList->ClearRenderTargetView(
		rtvHandle,
		clearColor,
		0,
		nullptr
	);

	//============================================================
	// Fullscreen Triangle描画
	// ※Vertex Bufferは設定しない
	//============================================================
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->DrawInstanced(
		3,
		1,
		0,
		0
	);

	//============================================================
	// Shader Resourceへ遷移
	//============================================================
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = sBRDFLUT;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(
		1,
		&barrier
	);

	sBRDFLUTResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	sBRDFLUTNeedsGeneration = false;
	sBRDFLUTReady = true;

	//============================================================
	// 通常3D描画状態へ復帰
	//============================================================
	Direct3D::setMainRenderTarget();

	return true;
}

void EnvironmentMapManager::WriteBRDFLUTSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle)
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	//============================================================
	// Resource作成済み
	//============================================================
	if (sBRDFLUT != nullptr)
	{
		device->CreateShaderResourceView(
			sBRDFLUT,
			&srvDesc,
			destinationHandle
		);

		return;
	}

	//============================================================
	// 未作成時はNull Texture2D SRV
	//============================================================
	device->CreateShaderResourceView(
		nullptr,
		&srvDesc,
		destinationHandle
	);
}

ID3D12Resource* EnvironmentMapManager::GetBRDFLUTResource()
{
	return sBRDFLUT;
}

bool EnvironmentMapManager::CreateBRDFLUTRootSignature()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	D3D12_ROOT_PARAMETER rootParameter = {};

	//============================================================
	// b0 : float4 BRDFParam
	// Root Constantsとして4個の32bit値を渡す
	//============================================================
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameter.Constants.ShaderRegister = 0;
	rootParameter.Constants.RegisterSpace = 0;
	rootParameter.Constants.Num32BitValues = 4;
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = 1;
	rootDesc.pParameters = &rootParameter;
	rootDesc.NumStaticSamplers = 0;
	rootDesc.pStaticSamplers = nullptr;

	//============================================================
	// SV_VertexIDから描画するためInput Layoutは使わない
	//============================================================
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);

	if (FAILED(hr))
	{
		if (errorBlob != nullptr)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}

		SAFE_RELEASE(signatureBlob);

		SAFE_RELEASE(errorBlob);

		assert(false);

		return false;
	}

	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&sBRDFLUTRootSignature)
	);

	SAFE_RELEASE(signatureBlob);

	SAFE_RELEASE(errorBlob);

	if (FAILED(hr) || sBRDFLUTRootSignature == nullptr)
	{
		assert(false);

		return false;
	}

	sBRDFLUTRootSignature->SetName(L"Environment BRDF LUT RootSignature");

	return true;
}

bool EnvironmentMapManager::CreateBRDFLUTPipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || sBRDFLUTRootSignature == nullptr)
	{
		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = sBRDFLUTRootSignature;

	//============================================================
	// SV_VertexIDを使うためInput Layoutなし
	//============================================================
	psoDesc.InputLayout =
	{
		nullptr,
		0
	};

	psoDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_BRDFLUT)->getCode();
	psoDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_BRDFLUT)->getLength();
	psoDesc.PS.pShaderBytecode = Shader::getPShader(Shader::ePixelShader::PS_BRDFLUT)->getCode();
	psoDesc.PS.BytecodeLength = Shader::getPShader(Shader::ePixelShader::PS_BRDFLUT)->getLength();

	//============================================================
	// Rasterizer
	//============================================================
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;

	//============================================================
	// Blendなし
	//============================================================
	psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// Depthなし
	//============================================================
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;

	//============================================================
	// BRDF LUTはRGの2チャンネルだけ使用
	//============================================================
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
	psoDesc.SampleDesc.Count = 1;

	const HRESULT hr = device->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(&sBRDFLUTPipelineState)
	);

	if (FAILED(hr) || sBRDFLUTPipelineState == nullptr)
	{
		OutputDebugStringW(L"[EnvironmentMap] Failed to create BRDF LUT PSO.\n");
		assert(false);
		return false;
	}

	sBRDFLUTPipelineState->SetName(L"Environment BRDF LUT PipelineState");

	return true;
}

bool EnvironmentMapManager::CreateBRDFLUTResources()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || sSRVHeap == nullptr)
	{
		return false;
	}

	SAFE_RELEASE(sBRDFLUT);
	SAFE_RELEASE(sBRDFLUTRTVHeap);

	sBRDFLUTReady = false;

	//============================================================
	// 256×256 R16G16_FLOAT
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = BRDFLUTSize;
	resourceDesc.Height = BRDFLUTSize;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_R16G16_FLOAT;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 1.0f;

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&sBRDFLUT)
	);

	if (FAILED(hr) || sBRDFLUT == nullptr)
	{
		OutputDebugStringW(L"[EnvironmentMap] Failed to create BRDF LUT resource.\n");
		assert(false);
		return false;
	}

	sBRDFLUT->SetName(L"Environment BRDF LUT");

	sBRDFLUTResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	//============================================================
	// RTV Heap
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 1;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = device->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(&sBRDFLUTRTVHeap)
	);

	if (FAILED(hr) || sBRDFLUTRTVHeap == nullptr)
	{
		assert(false);

		return false;
	}

	sBRDFLUTRTVHeap->SetName(L"Environment BRDF LUT RTV Heap");

	//============================================================
	// RTV作成
	//============================================================
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	device->CreateRenderTargetView(
		sBRDFLUT,
		&rtvDesc,
		sBRDFLUTRTVHeap->GetCPUDescriptorHandleForHeapStart()
	);

	//============================================================
	// Manager Heap [3]へSRV作成
	//============================================================
	const UINT descriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = sSRVHeap->GetCPUDescriptorHandleForHeapStart();

	srvHandle.ptr +=
		static_cast<SIZE_T>(static_cast<UINT>(EnvironmentSRVIndex::BRDFLUT)) *
		static_cast<SIZE_T>(descriptorSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(
		sBRDFLUT,
		&srvDesc,
		srvHandle
	);

	sBRDFLUTNeedsGeneration = true;

	return true;
}

bool EnvironmentMapManager::CreateHDRToCubeRootSignature()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//========================================================
	// t0
	//========================================================
	D3D12_DESCRIPTOR_RANGE hdrRange = {};
	hdrRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	hdrRange.NumDescriptors = 1;
	hdrRange.BaseShaderRegister = 0;
	hdrRange.RegisterSpace = 0;
	hdrRange.OffsetInDescriptorsFromTableStart = 0;


	//========================================================
	// Root Parameters
	//========================================================
	D3D12_ROOT_PARAMETER rootParameters[3] = {};


	//========================================================
	// [0] b0
	// VS_PrefilterEnvironmentの
	// CaptureViewProjection
	//========================================================
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[0].Constants.ShaderRegister = 0;
	rootParameters[0].Constants.RegisterSpace = 0;
	rootParameters[0].Constants.Num32BitValues = 16;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	//============================================================
	// [1] b1
	// HDRToCubeParam:
	// x = Environment Rotation Radians
	// yzw = unused
	//============================================================
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[1].Constants.ShaderRegister = 1;
	rootParameters[1].Constants.RegisterSpace = 0;
	rootParameters[1].Constants.Num32BitValues = 4;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//========================================================
	// [1] t0
	// Equirectangular HDR
	//========================================================
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &hdrRange;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


	//========================================================
	// HDR Sampler
	// U = Wrap 360度の継ぎ目
	// V = Clamp 天頂 / 地面
	//========================================================
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 1;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


	//========================================================
	// Root Signature
	//========================================================
	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};

	rootDesc.NumParameters = _countof(rootParameters);
	rootDesc.pParameters = rootParameters;
	rootDesc.NumStaticSamplers = 1;
	rootDesc.pStaticSamplers = &sampler;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;


	const HRESULT serializeResult = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);


	if (FAILED(serializeResult))
	{
		if (errorBlob != nullptr)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
		}


		SAFE_RELEASE(signatureBlob);

		SAFE_RELEASE(errorBlob);


		return false;
	}


	const HRESULT createResult = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&sHDRToCubeRootSignature)
	);


	SAFE_RELEASE(signatureBlob);

	SAFE_RELEASE(errorBlob);


	if (FAILED(createResult))
	{
		return false;
	}


	sHDRToCubeRootSignature->SetName(L"HDR To Cube RootSignature");

	return true;
}

bool EnvironmentMapManager::CreateHDRToCubePipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr || sHDRToCubeRootSignature == nullptr)
	{
		return false;
	}


	//========================================================
	// Cube Vertex
	//========================================================
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};


	desc.pRootSignature = sHDRToCubeRootSignature;


	desc.InputLayout =
	{
		inputLayout,
		_countof(inputLayout)
	};


	//========================================================
	// VSはPrefilterのものを再利用
	//========================================================
	auto* vertexShader =Shader::getVShader(Shader::eVertexShader::VS_PrefilterEnvironment);
	auto* pixelShader =Shader::getPShader(Shader::ePixelShader::PS_HDRToCube);


	if (vertexShader == nullptr || pixelShader == nullptr)
	{
		return false;
	}


	desc.VS =
	{
		vertexShader->getCode(),
		static_cast<SIZE_T>(vertexShader->getLength())
	};


	desc.PS =
	{
		pixelShader->getCode(),
		static_cast<SIZE_T>(pixelShader->getLength())
	};


	//========================================================
	// Rasterizer
	//========================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;

	//========================================================
	// Blendなし
	//========================================================
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


	//========================================================
	// Depth不要
	//========================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//========================================================
	// HDR CubeMapへ出力
	//========================================================
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.SampleDesc.Count = 1;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&sHDRToCubePipelineState)
	);


	if (FAILED(hr) || sHDRToCubePipelineState == nullptr)
	{
		OutputDebugStringW(
			L"[EnvironmentMap] "
			L"Failed HDR-To-Cube PSO.\n"
		);

		return false;
	}

	sHDRToCubePipelineState->SetName(L"HDR To Cube PipelineState");

	return true;
}

bool EnvironmentMapManager::CreateHDRToCubeResources()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	SAFE_RELEASE(sEnvironmentCube);

	SAFE_RELEASE(sEnvironmentRTVHeap);


	//========================================================
	// CubeMap Resource
	// 1024 × 1024
	// 6 Faces
	// FP16 HDR
	//========================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;


	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = EnvironmentCubeSize;
	resourceDesc.Height = EnvironmentCubeSize;
	resourceDesc.DepthOrArraySize = 6;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = RenderFormat::SceneHDR;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = RenderFormat::SceneHDR;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 1.0f;


	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&sEnvironmentCube)
	);


	if (FAILED(hr) || sEnvironmentCube == nullptr)
	{
		return false;
	}


	sEnvironmentCube->SetName(L"HDR Generated Environment CubeMap");

	sEnvironmentResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	//========================================================
	// 6 Face RTV Heap
	//========================================================
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NumDescriptors = 6;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = device->CreateDescriptorHeap(
		&heapDesc,
		IID_PPV_ARGS(&sEnvironmentRTVHeap)
	);


	if (FAILED(hr) || sEnvironmentRTVHeap == nullptr)
	{
		return false;
	}


	sEnvironmentRTVDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


	//========================================================
	// Face RTV
	//========================================================
	for (UINT faceIndex = 0; faceIndex < 6; ++faceIndex)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle =
			sEnvironmentRTVHeap->GetCPUDescriptorHandleForHeapStart();


		handle.ptr +=
			static_cast<SIZE_T>(faceIndex) *
			static_cast<SIZE_T>(sEnvironmentRTVDescriptorSize);


		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = RenderFormat::SceneHDR;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.FirstArraySlice = faceIndex;
		rtvDesc.Texture2DArray.ArraySize = 1;
		rtvDesc.Texture2DArray.PlaneSlice = 0;


		device->CreateRenderTargetView(
			sEnvironmentCube,
			&rtvDesc,
			handle
		);
	}

	return true;
}
