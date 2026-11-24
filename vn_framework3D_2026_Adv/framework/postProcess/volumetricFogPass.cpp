#include "../../framework.h"
#include "../vn_environment.h"

#include "volumetricFogPass.h"

//============================================================
// 初期化
//============================================================
bool VolumetricFogPass::Initialize(UINT fullWidth, UINT fullHeight)
{
	if (mInitialized)
	{
		return true;
	}


	//============================================================
	// Full Resolution
	//============================================================
	mFullWidth = (std::max)(fullWidth, 1u);
	mFullHeight = (std::max)(fullHeight, 1u);


	//============================================================
	// Half Resolution
	//============================================================
	mHalfWidth = (mFullWidth + 1u) / 2u;
	mHalfHeight = (mFullHeight + 1u) / 2u;

	mBlueNoiseAvailable = LoadBlueNoiseTexture(L"data/texture/noise/BlueNoiseTexture_128x128.png");


	if (!CreateFogResource())
	{
		Terminate();
		return false;
	}

	if (!CreateDescriptorHeaps())
	{
		Terminate();
		return false;
	}

	if (!CreateConstantBuffer())
	{
		Terminate();
		return false;
	}

	if (!CreateRaymarchPipeline())
	{
		Terminate();
		return false;
	}

	if (!CreateCompositePipeline())
	{
		Terminate();
		return false;
	}

	if (!CreateShadowRaymarchRootSignature())
	{
		Terminate();
		return false;
	}

	if (!CreateShadowRaymarchDescriptorHeap())
	{
		Terminate();
		return false;
	}

	if (!CreateShadowRaymarchPipeline())
	{
		Terminate();
		return false;
	}

	if (!CreateTemporalResources())
	{
		Terminate();
		return false;
	}

	if (!CreateTemporalDescriptorHeap())
	{
		Terminate();
		return false;
	}

	if (!CreateTemporalRootSignature())
	{
		Terminate();
		return false;
	}

	if (!CreateTemporalConstantBuffer())
	{
		Terminate();
		return false;
	}

	if (!CreateTemporalPipeline())
	{
		Terminate();
		return false;
	}

	if (!CreateDepthAwareCompositeRootSignature())
	{
		Terminate();
		return false;
	}

	if (!CreateDepthAwareCompositeDescriptorHeap())
	{
		Terminate();
		return false;
	}

	if (!CreateDepthAwareCompositeConstantBuffer())
	{
		Terminate();
		return false;
	}

	if (!CreateDepthAwareCompositePipeline())
	{
		Terminate();
		return false;
	}

	mInitialized = true;

	return true;
}

void VolumetricFogPass::Terminate()
{
	if (mConstantBuffer != nullptr && mMappedBuffer != nullptr)
	{
		mConstantBuffer->Unmap(
			0u,
			nullptr
		);

		mMappedBuffer = nullptr;
	}
	if (mTemporalConstantBuffer && mTemporalMappedBuffer != nullptr)
	{
		mTemporalConstantBuffer->Unmap(
				0u,
				nullptr
			);

		mTemporalMappedBuffer = nullptr;
	}
	if (mUpsampleConstantBuffer && mUpsampleMappedBuffer != nullptr)
	{
		mUpsampleConstantBuffer->Unmap(
				0u,
				nullptr
			);

		mUpsampleMappedBuffer = nullptr;
	}

	SAFE_RELEASE(mFogResource);
	SAFE_RELEASE(mRTVHeap);
	SAFE_RELEASE(mRaymarchSRVHeap);
	SAFE_RELEASE(mCompositeSRVHeap);
	SAFE_RELEASE(mRaymarchPSO);
	SAFE_RELEASE(mCompositePSO);
	SAFE_RELEASE(mConstantBuffer);
	mShadowRaymarchPSO.Reset();
	mShadowRaymarchSRVHeap.Reset();
	mShadowRaymarchRootSignature.Reset();
	mTemporalPSO.Reset();
	mTemporalRootSignature.Reset();
	mTemporalSRVHeap.Reset();
	mTemporalRTVHeap.Reset();
	mTemporalConstantBuffer.Reset();

	for (UINT historyIndex = 0u; historyIndex < 2u; ++historyIndex)
	{
		mTemporalHistoryResources[historyIndex].Reset();
	}

	mDepthAwareCompositePSO.Reset();
	mDepthAwareCompositeRootSignature.Reset();
	mDepthAwareCompositeSRVHeap.Reset();
	mUpsampleConstantBuffer.Reset();
	mBlueNoiseTexture.Reset();
	mBlueNoiseAvailable = false;

	mInitialized = false;
	mTemporalHistoryValid = false;
	mFrameIndex = 0u;
	mDescriptorSize = 0u;
}

void VolumetricFogPass::ApplyToRT(
	ID3D12Resource* inputSceneResource,
	ID3D12Resource* depthResource,
	D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
	UINT fullWidth,
	UINT fullHeight
)
{
	if (!mInitialized ||
		inputSceneResource == nullptr ||
		depthResource == nullptr)
	{
		return;
	}


	//============================================================
	// Camera / Light / Fog Parameter更新
	//============================================================
	UpdateConstantBuffer();

	if (ShadowMapManager::IsValid())
	{
		// 有効なCSMがあれば、影付きのVolumetric Fogを描画する
		RaymarchShadowed(depthResource);
	}
	else
	{
		// CSMがないシーンでも影なしのRaymarchでFogを更新する
		Raymarch(depthResource);
	}

	if (mTemporalEnabled)
	{
		ResolveTemporal(depthResource);
	}

	//============================================================
	// SceneへComposite
	//============================================================
	//Composite(
	//	inputSceneResource,
	//	outputRTV,
	//	fullWidth,
	//	fullHeight
	//);

	CompositeDepthAware(
		inputSceneResource,
		depthResource,
		outputRTV,
		fullWidth,
		fullHeight
	);

	//============================================================
	// Jitter用Frame Index
	//============================================================
	mFrameIndex = (mFrameIndex + 1u) & 1023u;
}

void VolumetricFogPass::ResetTemporalHistory()
{
	//============================================================
	// Historyを次Frameで使用しない
	//============================================================
	mTemporalHistoryValid = false;

	//============================================================
	// PingPongを初期位置へ
	//============================================================
	mTemporalReadIndex = 0u;
	mTemporalWriteIndex = 1u;

	//============================================================
	// Previous Cameraを現在値へ
	//============================================================
	mPreviousViewProjection = *vnCamera::getScreen();

	//============================================================
	// Jitter SequenceもReset
	//============================================================
	mFrameIndex = 0u;
}

//============================================================
// Fog Resource作成
//============================================================
bool VolumetricFogPass::CreateFogResource()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// HDR Fog Texture
	// RGB: ScatteringはHDR値になる
	// A: Transmittance 0～1
	//============================================================
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = mHalfWidth;
	desc.Height = mHalfHeight;
	desc.DepthOrArraySize = 1u;
	desc.MipLevels = 1u;
	desc.Format = RenderFormat::SceneHDR;
	desc.SampleDesc.Count = 1u;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type =D3D12_HEAP_TYPE_DEFAULT;


	//============================================================
	// Fogなし:
	// Scattering    = 0
	// Transmittance = 1
	//============================================================
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = RenderFormat::SceneHDR;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 1.0f;


	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(&mFogResource)
	);


	if (FAILED(hr))
	{
		return false;
	}


	mFogResource->SetName(L"Volumetric Fog Half Resolution");


	mFogState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


	//============================================================
	// RTV Heap
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NumDescriptors = 1u;


	hr = device->CreateDescriptorHeap(
		&heapDesc,
		IID_PPV_ARGS(&mRTVHeap)
	);


	if (FAILED(hr))
	{
		return false;
	}


	mFogRTV = mRTVHeap->GetCPUDescriptorHandleForHeapStart();


	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = RenderFormat::SceneHDR;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	device->CreateRenderTargetView(
		mFogResource,
		&rtvDesc,
		mFogRTV
	);


	return true;
}

//============================================================
// SRV Heap作成
//============================================================
bool VolumetricFogPass::CreateDescriptorHeaps()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}

	mDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	//============================================================
	// 共通Heap Desc
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 2u;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


	//============================================================
	// Ray March Heap
	// [0] Depth
	// [1] Dummy
	//============================================================
	HRESULT hr = device->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(&mRaymarchSRVHeap)
	);


	if (FAILED(hr))
	{
		return false;
	}


	//============================================================
	// Composite Heap
	// [0] Scene
	// [1] Fog
	//============================================================
	hr = device->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(&mCompositeSRVHeap)
	);


	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

bool VolumetricFogPass::CreateConstantBuffer()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// CBV 256byte Alignment
	//============================================================
	const UINT size = (sizeof(VolumetricFogBuffer) + 255u) & ~255u;


	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size;
	desc.Height = 1u;
	desc.DepthOrArraySize = 1u;
	desc.MipLevels = 1u;
	desc.SampleDesc.Count = 1u;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mConstantBuffer)
	);


	if (FAILED(hr))
	{
		return false;
	}


	hr = mConstantBuffer->Map(
		0u,
		nullptr,
		reinterpret_cast<void**>(&mMappedBuffer)
	);


	return SUCCEEDED(hr) && mMappedBuffer != nullptr;
}

bool VolumetricFogPass::CreateRaymarchPipeline()
{
	ID3D12Device* device = Direct3D::getDevice();


	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};


	auto* vs = Shader::getVShader(Shader::eVertexShader::VS_Copy);
	auto* ps = Shader::getPShader(Shader::ePixelShader::PS_VolumetricFog);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	desc.pRootSignature = Direct3D::getRootSignatureScr();

	desc.InputLayout =
	{
		layout,
		_countof(layout)
	};


	desc.VS =
	{
		vs->getCode(),
		static_cast<SIZE_T>(vs->getLength())
	};


	desc.PS =
	{
		ps->getCode(),
		static_cast<SIZE_T>(ps->getLength())
	};


	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// DepthはTextureとして読むのでHardware Depth Testは不要
	//============================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	return SUCCEEDED(device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mRaymarchPSO))
	);
}

bool VolumetricFogPass::CreateCompositePipeline()
{
	ID3D12Device* device = Direct3D::getDevice();

	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};


	auto* vs = Shader::getVShader(Shader::eVertexShader::VS_Copy);
	auto* ps = Shader::getPShader(Shader::ePixelShader::PS_VolumetricFogComposite);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	desc.pRootSignature = Direct3D::getRootSignatureScr();

	desc.InputLayout =
	{
		layout,
		_countof(layout)
	};


	desc.VS =
	{
		vs->getCode(),
		static_cast<SIZE_T>(vs->getLength())
	};


	desc.PS =
	{
		ps->getCode(),
		static_cast<SIZE_T>(ps->getLength())
	};


	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// DepthはTextureとして読むのでHardware Depth Testは不要
	//============================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	return SUCCEEDED(device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mCompositePSO))
	);
}

bool VolumetricFogPass::CreateShadowRaymarchRootSignature()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// Volumetric Fogで使用するSRV
	//
	// t0      : Main Depth
	// t1～t3  : Static Directional CSM
	// t4～t6  : Dynamic Directional CSM
	// t7      : Local Light StructuredBuffer
	// t8      : Spot Light Shadow Texture2DArray
	// t9      : Point Light Shadow Texture2DArray
	//============================================================
	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 11u;
	srvRange.BaseShaderRegister = 0u;
	srvRange.RegisterSpace = 0u;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	//============================================================
	// Root Parameter
	// [0] t0～t6
	// [1] b0
	//============================================================
	D3D12_ROOT_PARAMETER rootParameters[2] = {};

	//============================================================
	// Root[0] SRV Table
	//============================================================
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;


	//============================================================
	// Root[1] b0 VolumetricFogBuffer
	//============================================================
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0u;
	rootParameters[1].Descriptor.RegisterSpace = 0u;

	//============================================================
	// Sampler 0
	//
	// Main Depth用Linear Clamp
	//============================================================
	D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
	samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].ShaderRegister = 0u;
	samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplers[0].MinLOD = 0.0f;
	samplers[0].MaxLOD = D3D12_FLOAT32_MAX;

	//============================================================
	// Sampler 1
	// Shadow Map比較用Sampler
	// SampleCmpLevelZero()から使用する
	//============================================================
	samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	samplers[1].ShaderRegister = 1u;
	samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplers[1].MinLOD = 0.0f;
	samplers[1].MaxLOD = D3D12_FLOAT32_MAX;


	//============================================================
	// Root Signature
	//============================================================
	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = _countof(rootParameters);
	rootDesc.pParameters = rootParameters;
	rootDesc.NumStaticSamplers = _countof(samplers);
	rootDesc.pStaticSamplers = samplers;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	//============================================================
	// ID3DBlobもCOM ObjectなのでComPtrを使用
	//============================================================
	ComPtr<ID3DBlob>signatureBlob;

	ComPtr<ID3DBlob>errorBlob;


	HRESULT hr = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		signatureBlob.GetAddressOf(),
		errorBlob.GetAddressOf()
	);


	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}
		return false;
	}


	//============================================================
	// Root Signature生成
	//============================================================
	hr = device->CreateRootSignature(
		0u,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(mShadowRaymarchRootSignature.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}

	mShadowRaymarchRootSignature->SetName(L"VolumetricFog Shadow Raymarch RootSignature");

	return true;
}

bool VolumetricFogPass::CreateShadowRaymarchDescriptorHeap()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// t0 ～ t8
	//
	// t0 : Main Depth
	//
	// t1～t3 : Static Directional CSM
	// t4～t6 : Dynamic Directional CSM
	//
	// t7 : Local Light StructuredBuffer
	// t8 : Spot Light Shadow Texture2DArray
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type =D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 11u;
	desc.Flags =D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


	const HRESULT hr = device->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(mShadowRaymarchSRVHeap.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	mShadowRaymarchSRVHeap->SetName(L"VolumetricFog Shadow Raymarch SRV Heap");


	return true;
}

bool VolumetricFogPass::CreateShadowRaymarchPipeline()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr || !mShadowRaymarchRootSignature)
	{
		return false;
	}


	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};


	auto* vs = Shader::getVShader(Shader::eVertexShader::VS_Copy);
	auto* ps = Shader::getPShader(Shader::ePixelShader::PS_VolumetricFogShadowed);


	if (vs == nullptr || ps == nullptr)
	{
		return false;
	}


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};


	//============================================================
	// 専用Root Signature
	//============================================================
	desc.pRootSignature = mShadowRaymarchRootSignature.Get();


	desc.InputLayout =
	{
		layout,
		_countof(layout)
	};


	desc.VS =
	{
		vs->getCode(),
		static_cast<SIZE_T>(vs->getLength())
	};


	desc.PS =
	{
		ps->getCode(),
		static_cast<SIZE_T>(ps->getLength())
	};


	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(mShadowRaymarchPSO.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	mShadowRaymarchPSO->SetName(L"VolumetricFog Shadow Raymarch PSO");

	return true;
}

//============================================================
// Constant Buffer更新
//============================================================
void VolumetricFogPass::UpdateConstantBuffer()
{
	if (mMappedBuffer == nullptr)
	{
		return;
	}


	//============================================================
	// Current ViewProjection
	//============================================================
	const XMMATRIX viewProjection = *vnCamera::getScreen();


	//============================================================
	// World Position復元用逆行列
	//============================================================
	const XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, viewProjection);


	XMStoreFloat4x4(&mMappedBuffer->InverseViewProjection,
		XMMatrixTranspose(inverseViewProjection));


	//============================================================
	// Camera Position
	//============================================================
	XMFLOAT4 cameraPosition = {};

	XMStoreFloat4(
		&cameraPosition,
		*vnCamera::getPosition()
	);


	mMappedBuffer->CameraPositionMaxDistance =
		XMFLOAT4(
			cameraPosition.x,
			cameraPosition.y,
			cameraPosition.z,
			mMaxDistance
		);


	//============================================================
	// Fog
	//============================================================
	mMappedBuffer->FogColorDensity =
		XMFLOAT4(
			mFogColor.x,
			mFogColor.y,
			mFogColor.z,
			mDensity
		);


	//============================================================
	// Directional Light Direction
	//============================================================
	XMVECTOR lightDirection =XMVector3Normalize(*Light::getILightDir());


	XMFLOAT4 lightDir = {};

	XMStoreFloat4(
		&lightDir,
		lightDirection
	);


	mMappedBuffer->LightDirectionAnisotropy =
		XMFLOAT4(
			lightDir.x,
			lightDir.y,
			lightDir.z,
			mAnisotropy
		);


	//============================================================
	// Directional Light Color
	//============================================================
	XMFLOAT4 lightColor = {};

	XMStoreFloat4(
		&lightColor,
		*Light::getLightColor()
	);


	mMappedBuffer->LightColorStrength =
		XMFLOAT4(
			lightColor.x,
			lightColor.y,
			lightColor.z,
			mDirectionalStrength
		);


	//============================================================
	// Medium Parameter
	//============================================================
	mMappedBuffer->FogParameters =
		XMFLOAT4(
			mScatteringAlbedo,
			mAmbientStrength,
			mBaseHeight,
			mHeightFalloff
		);

	const bool useBlueNoise = mBlueNoiseEnabled && mBlueNoiseAvailable;

	mMappedBuffer->RaymarchParameters =
		XMFLOAT4(
			static_cast<float>(mSampleCount),
			static_cast<float>(mFrameIndex),
			useBlueNoise ? 1.0f : 0.0f,
			0.0f
		);

	//============================================================
	// Camera View Matrix
	//============================================================
	XMStoreFloat4x4(
		&mMappedBuffer->View,
		XMMatrixTranspose(*vnCamera::getView())
	);


	//============================================================
	// CSM
	//============================================================
	if (ShadowMapManager::IsValid())
	{
		for (UINT cascadeIndex = 0u;cascadeIndex < SHADOW_CASCADE_COUNT;++cascadeIndex)
		{
			//========================================================
			// World → Light Clip
			//========================================================
			XMStoreFloat4x4(
				&mMappedBuffer->LightViewProjection[cascadeIndex],
				XMMatrixTranspose(ShadowMapManager::GetLightViewProj(cascadeIndex))
			);
		}


		//============================================================
		// Cascade終了距離
		//============================================================
		mMappedBuffer->CascadeSplits =
			XMFLOAT4(
				ShadowMapManager::GetCascadeSplitDepth(0),
				ShadowMapManager::GetCascadeSplitDepth(1),
				ShadowMapManager::GetCascadeSplitDepth(2),
				0.0f
			);


		//============================================================
		// Cascade Blend開始位置
		//============================================================
		mMappedBuffer->CascadeBlendStarts =
			XMFLOAT4(
				ShadowMapManager::GetCascadeBlendStartDepth(0),
				ShadowMapManager::GetCascadeBlendStartDepth(1),
				ShadowMapManager::GetCascadeBlendStartDepth(2),
				0.0f
			);


		mMappedBuffer->ShadowParameters =
			XMFLOAT4(
				1.0f,
				mShadowStrength,
				mShadowBias,
				0.0f
			);
	}
	else
	{
		//============================================================
		// ShadowMapなし
		//============================================================
		for (UINT cascadeIndex = 0u; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			XMStoreFloat4x4(
				&mMappedBuffer->LightViewProjection[cascadeIndex],
				XMMatrixIdentity()
			);
		}


		mMappedBuffer->CascadeSplits =
			XMFLOAT4(
				1000000.0f,
				1000000.0f,
				1000000.0f,
				0.0f
			);


		mMappedBuffer->CascadeBlendStarts = mMappedBuffer->CascadeSplits;

		mMappedBuffer->ShadowParameters =
			XMFLOAT4(
				0.0f,
				0.0f,
				mShadowBias,
				0.0f
			);
	}

	//============================================================
	// Local Light情報
	//============================================================
	
	//============================================================
	// 現在登録されているLocal Light数
	// Point + Spotを合わせた数
	// Shader側でSpotだけ選別する
	//============================================================
	const UINT localLightCount =LocalLightManager::GetLightCount();

	//============================================================
	// Spot Shadowが現在利用可能か
	//============================================================
	const bool spotShadowActive =SpotLightShadowManager::IsActive();


	//============================================================
	// Volumetric Local Light Parameter
	//============================================================
	mMappedBuffer->LocalLightParameters =
		XMFLOAT4(
			static_cast<float>(localLightCount),// x : Local Light数
			mSpotLightStrength,// y : Spot Volumetric強度
			spotShadowActive ? 1.0f : 0.0f,// z : Spot Shadow有効
			0.0f// w : unused
		);


	//============================================================
	// 現在登録されているSpot Shadow数
	//============================================================
	const UINT spotShadowCount =SpotLightShadowManager::GetLightCount();


	//============================================================
	// 最大Slot数までConstant Bufferを埋める
	//============================================================
	for (UINT shadowIndex = 0u;shadowIndex <SpotLightShadowManager::MaxShadowLights;++shadowIndex)
	{
		//========================================================
		// このSlotに有効なSpot Shadowがある
		//========================================================
		if (shadowIndex < spotShadowCount)
		{
			//====================================================
			// World → Spot Light Clip
			//====================================================
			XMStoreFloat4x4(
				&mMappedBuffer->SpotLightViewProjection[shadowIndex],
				XMMatrixTranspose(SpotLightShadowManager::GetLightViewProjection(shadowIndex))
			);


			//====================================================
			// Spot Shadow個別設定
			//====================================================
			mMappedBuffer->SpotShadowParameters[shadowIndex] =
				SpotLightShadowManager::GetShadowParameters(shadowIndex);
		}
		else
		{
			//====================================================
			// 未使用Slot
			//====================================================
			XMStoreFloat4x4(&mMappedBuffer->SpotLightViewProjection[shadowIndex], XMMatrixIdentity());


			mMappedBuffer->SpotShadowParameters[shadowIndex] =
				XMFLOAT4(	
					0.0f, // Disable
					1.0f, // PCF Radius
					0.0003f, // Minimum Bias
					0.0020f  // Slope Bias
				);
		}
	}

	//============================================================
	// Point Light Volumetric / Shadow
	//============================================================
	//============================================================
	// Point Shadowが現在有効か
	//============================================================
	const bool pointShadowActive = PointLightShadowManager::IsActive();


	//============================================================
	// Point Volumetric Parameter
	//
	// x : Point Light Volumetric Strength
	// y : Point Shadow Enable
	//============================================================
	mMappedBuffer->PointLightParameters =
		XMFLOAT4(
			mPointLightStrength,
			pointShadowActive ? 1.0f : 0.0f,
			0.0f,
			0.0f
		);


	//============================================================
	// 現在登録されているPoint Shadow Light数
	//============================================================
	const UINT pointShadowLightCount = PointLightShadowManager::GetLightCount();


	//============================================================
	// 最大2灯すべて処理
	//============================================================
	for (UINT shadowIndex = 0u;shadowIndex <PointLightShadowManager::MaxShadowLights;++shadowIndex)
	{
		//========================================================
		// このShadow Slotが有効か
		//========================================================
		const bool shadowEnabled = shadowIndex < pointShadowLightCount;


		if (shadowEnabled)
		{
			//====================================================
			// Point Light 1灯につき6面
			//====================================================
			for (UINT faceIndex = 0u;faceIndex <PointLightShadowManager::FaceCount;++faceIndex)
			{
				//================================================
				// 一次元配列Index
				// Light 0: 0～5
				// Light 1: 6～11
				//================================================
				const UINT matrixIndex =
					shadowIndex *
					PointLightShadowManager::FaceCount +
					faceIndex;


				//================================================
				// HLSL:
				// mul(float4(position,1), matrix)
				// なのでTransposeして送る
				//================================================
				XMStoreFloat4x4(
					&mMappedBuffer->PointLightViewProjection[matrixIndex],
					XMMatrixTranspose(PointLightShadowManager::GetLightViewProjection(shadowIndex, faceIndex))
				);
			}


			//====================================================
			// Point Shadow設定
			//====================================================
			mMappedBuffer->PointShadowParameters[shadowIndex] =
				PointLightShadowManager::GetShadowParameters(shadowIndex);
		}
		else
		{
			//====================================================
			// 未使用Slot
			//====================================================
			for (UINT faceIndex = 0u;faceIndex <PointLightShadowManager::FaceCount;++faceIndex)
			{
				const UINT matrixIndex =
					shadowIndex *
					PointLightShadowManager::FaceCount +
					faceIndex;


				XMStoreFloat4x4(
					&mMappedBuffer->PointLightViewProjection[matrixIndex],
					XMMatrixIdentity()
				);
			}


			mMappedBuffer->PointShadowParameters[shadowIndex] =
				XMFLOAT4(
					0.0f,      // Disable
					1.0f,      // PCF Radius
					0.0003f,   // Minimum Bias
					0.0020f    // Slope Bias
				);
		}
	}
}

void VolumetricFogPass::RaymarchShadowed(ID3D12Resource* depthResource)
{
	if (depthResource == nullptr ||
		!mShadowRaymarchSRVHeap ||
		!mShadowRaymarchRootSignature ||
		!mShadowRaymarchPSO)
	{
		return;
	}

	if (!ShadowMapManager::IsValid())
	{
		return;
	}

	ID3D12Device* device = Direct3D::getDevice();

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	//============================================================
	// Main DepthをSRVへ
	//============================================================
	Direct3D::TransitionMainDepthToSRV();


	//============================================================
	// Heap先頭
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE heapStart = mShadowRaymarchSRVHeap->GetCPUDescriptorHandleForHeapStart();


	//============================================================
	// [0] t0 Main Depth
	//============================================================
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1u;


		device->CreateShaderResourceView(
			depthResource,
			&srvDesc,
			heapStart
		);
	}


	//============================================================
	// [1～3] Static CSM
	//============================================================
	{
		D3D12_CPU_DESCRIPTOR_HANDLE destination = heapStart;

		destination.ptr += static_cast<SIZE_T>(mDescriptorSize);


		const D3D12_CPU_DESCRIPTOR_HANDLE source =
			ShadowMapManager::GetSRVCopyCPUHandle(
				0,
				ShadowMapLayer::Static
			);


		device->CopyDescriptorsSimple(
			SHADOW_CASCADE_COUNT,
			destination,
			source,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}


	//============================================================
	// [4～6] Dynamic CSM
	//============================================================
	{
		D3D12_CPU_DESCRIPTOR_HANDLE destination = heapStart;

		destination.ptr += static_cast<SIZE_T>(mDescriptorSize) * 4u;


		const D3D12_CPU_DESCRIPTOR_HANDLE source =
			ShadowMapManager::GetSRVCopyCPUHandle(
				0,
				ShadowMapLayer::Dynamic
			);


		device->CopyDescriptorsSimple(
			SHADOW_CASCADE_COUNT,
			destination,
			source,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}

	//============================================================
	// [7] t7 : Local Light StructuredBuffer
	//============================================================
	{
		D3D12_CPU_DESCRIPTOR_HANDLE destination = heapStart;

		destination.ptr += static_cast<SIZE_T>(mDescriptorSize) * 7u;

		//========================================================
		// LocalLightManagerが保持しているUpload Bufferから
		// StructuredBuffer SRVを作成する
		//========================================================
		LocalLightManager::WriteSRV(destination);
	}


	//============================================================
	// [8] t8 : Spot Light Shadow Texture2DArray
	//============================================================
	{
		D3D12_CPU_DESCRIPTOR_HANDLE destination = heapStart;

		destination.ptr += static_cast<SIZE_T>(mDescriptorSize) * 8u;


		//========================================================
		// SpotLightShadowManagerが保持している
		// Texture2DArray Shadow MapのSRVを作成する
		//========================================================
		SpotLightShadowManager::WriteSRV(destination);
	}


	//============================================================
	// [9] t9 : Point Light Shadow Texture2DArray
	//
	// Slice:
	//
	// Point Light 0
	//   0 : +X
	//   1 : -X
	//   2 : +Y
	//   3 : -Y
	//   4 : +Z
	//   5 : -Z
	//
	// Point Light 1
	//   6～11
	//============================================================
	{
		D3D12_CPU_DESCRIPTOR_HANDLE destination = heapStart;

		destination.ptr += static_cast<SIZE_T>(mDescriptorSize) * 9u;

		PointLightShadowManager::WriteSRV(destination);
	}

	//============================================================
	// [10] t10 : Blue Noise
	//============================================================
	{
		D3D12_CPU_DESCRIPTOR_HANDLE destination = heapStart;

		destination.ptr += static_cast<SIZE_T>(mDescriptorSize) * 10u;


		if (mBlueNoiseAvailable && mBlueNoiseTexture)
		{
			//========================================================
			// Blue Noise Texture用SRV
			//========================================================
			const D3D12_RESOURCE_DESC resourceDesc = mBlueNoiseTexture->GetDesc();


			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = resourceDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MostDetailedMip = 0u;
			srvDesc.Texture2D.MipLevels = 1u;
			srvDesc.Texture2D.PlaneSlice = 0u;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;


			device->CreateShaderResourceView(
				mBlueNoiseTexture.Get(),
				&srvDesc,
				destination
			);
		}
		else
		{
			//========================================================
			// Blue Noiseが存在しなくても、
			// Descriptor自体は必ず有効にする
			// Shader側ではRaymarchParameters.z == 0なら
			// このTextureをSampleしない
			//========================================================
			device->CreateShaderResourceView(
				Direct3D::getWhiteTexture(),
				Direct3D::getWhiteTextueViewDesc(),
				destination
			);
		}
	}

	//============================================================
	// Fog Texture → RTV
	//============================================================
	TransitionFog(D3D12_RESOURCE_STATE_RENDER_TARGET);


	const float clearColor[] =
	{
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};


	commandList->ClearRenderTargetView(
		mFogRTV,
		clearColor,
		0u,
		nullptr
	);


	commandList->OMSetRenderTargets(
		1u,
		&mFogRTV,
		FALSE,
		nullptr
	);


	//============================================================
	// Half Resolution
	//============================================================
	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(mHalfWidth),
		static_cast<float>(mHalfHeight),
		0.0f,
		1.0f
	};


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(mHalfWidth),
		static_cast<LONG>(mHalfHeight)
	};


	commandList->RSSetViewports(
		1u,
		&viewport
	);

	commandList->RSSetScissorRects(
		1u,
		&scissor
	);


	//============================================================
	// 専用Root Signature
	//============================================================
	commandList->SetGraphicsRootSignature(mShadowRaymarchRootSignature.Get());


	//============================================================
	// 専用PSO
	//============================================================
	commandList->SetPipelineState(mShadowRaymarchPSO.Get());


	//============================================================
	// Descriptor Heap
	//============================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		mShadowRaymarchSRVHeap.Get()
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	commandList->SetGraphicsRootDescriptorTable(
		0u,
		mShadowRaymarchSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);


	//============================================================
	// b0
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		1u,
		mConstantBuffer->GetGPUVirtualAddress()
	);


	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	commandList->IASetVertexBuffers(
		0u,
		1u,
		Direct3D::getScreenVBView()
	);


	commandList->DrawInstanced(4u, 1u, 0u, 0u);


	//============================================================
	// CompositeからFog Textureを読む
	//============================================================
	TransitionFog(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void VolumetricFogPass::TransitionFog(D3D12_RESOURCE_STATES newState)
{
	if (mFogResource == nullptr || mFogState == newState)
	{
		return;
	}


	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = mFogResource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = mFogState;
	barrier.Transition.StateAfter = newState;


	Direct3D::getCommandList()->ResourceBarrier(
		1u,
		&barrier
	);

	mFogState = newState;
}

void VolumetricFogPass::Raymarch(ID3D12Resource* depthResource)
{
	ID3D12Device* device = Direct3D::getDevice();

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	//============================================================
	// Main DepthをShaderから読む
	//============================================================
	Direct3D::TransitionMainDepthToSRV();

	//============================================================
	// Raymarch Heap
	// [0] t0 = Main Depth
	// [1] t1 = Dummy Main Depth
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE handle = mRaymarchSRVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1u;


	//============================================================
	// t0
	//============================================================
	device->CreateShaderResourceView(
		depthResource,
		&srvDesc,
		handle
	);


	//============================================================
	// t1 Dummy
	//============================================================
	handle.ptr += static_cast<SIZE_T>(mDescriptorSize);


	device->CreateShaderResourceView(
		depthResource,
		&srvDesc,
		handle
	);


	//============================================================
	// Fog Texture → RTV
	//============================================================
	TransitionFog(D3D12_RESOURCE_STATE_RENDER_TARGET);


	const float clearColor[] =
	{
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};


	commandList->ClearRenderTargetView(
		mFogRTV,
		clearColor,
		0u,
		nullptr
	);


	commandList->OMSetRenderTargets(
		1u,
		&mFogRTV,
		FALSE,
		nullptr
	);


	//============================================================
	// Half Resolution
	//============================================================
	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(mHalfWidth),
		static_cast<float>(mHalfHeight),
		0.0f,
		1.0f
	};


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(mHalfWidth),
		static_cast<LONG>(mHalfHeight)
	};


	commandList->RSSetViewports(
		1u,
		&viewport
	);

	commandList->RSSetScissorRects(
		1u,
		&scissor
	);


	commandList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());


	commandList->SetPipelineState(mRaymarchPSO);


	ID3D12DescriptorHeap* heaps[] =
	{
		mRaymarchSRVHeap
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	//============================================================
	// t0 / t1
	//============================================================
	commandList->SetGraphicsRootDescriptorTable(
		0u,
		mRaymarchSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);


	//============================================================
	// b0 VolumetricFogBuffer
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		1u,
		mConstantBuffer->GetGPUVirtualAddress()
	);


	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	commandList->IASetVertexBuffers(
		0u,
		1u,
		Direct3D::getScreenVBView()
	);

	commandList->DrawInstanced(4u, 1u, 0u, 0u);

	//============================================================
	// Compositeから読む
	//============================================================
	TransitionFog(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void VolumetricFogPass::Composite(ID3D12Resource* inputSceneResource,
	D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
	UINT fullWidth,
	UINT fullHeight)
{
	ID3D12Device* device = Direct3D::getDevice();

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	//============================================================
	// [0] Scene HDR
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE handle = mCompositeSRVHeap->GetCPUDescriptorHandleForHeapStart();


	D3D12_SHADER_RESOURCE_VIEW_DESC sceneSRV = {};
	sceneSRV.Format = RenderFormat::SceneHDR;
	sceneSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sceneSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sceneSRV.Texture2D.MipLevels = 1u;


	device->CreateShaderResourceView(
		inputSceneResource,
		&sceneSRV,
		handle
	);


	//============================================================
	// [1] Half Resolution Fog
	//============================================================
	handle.ptr += static_cast<SIZE_T>(mDescriptorSize);


	D3D12_SHADER_RESOURCE_VIEW_DESC fogSRV = {};
	fogSRV.Format = RenderFormat::SceneHDR;
	fogSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	fogSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	fogSRV.Texture2D.MipLevels = 1u;


	ID3D12Resource* resolvedFogResource = GetResolvedFogResource();

	device->CreateShaderResourceView(
		resolvedFogResource,
		&fogSRV,
		handle
	);


	//============================================================
	// Full Resolution Output
	//============================================================
	commandList->OMSetRenderTargets(
		1u,
		&outputRTV,
		FALSE,
		nullptr
	);


	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(fullWidth),
		static_cast<float>(fullHeight),
		0.0f,
		1.0f
	};


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(fullWidth),
		static_cast<LONG>(fullHeight)
	};


	commandList->RSSetViewports(
		1u,
		&viewport
	);

	commandList->RSSetScissorRects(
		1u,
		&scissor
	);


	commandList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());

	commandList->SetPipelineState(mCompositePSO);


	ID3D12DescriptorHeap* heaps[] =
	{
		mCompositeSRVHeap
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	commandList->SetGraphicsRootDescriptorTable(
		0u,
		mCompositeSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);


	//============================================================
	// Composite ShaderではCBを使わなくても
	// Screen Root Signatureのb0へ有効Addressを設定しておく
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		1u,
		mConstantBuffer->GetGPUVirtualAddress()
	);


	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	commandList->IASetVertexBuffers(
		0u,
		1u,
		Direct3D::getScreenVBView()
	);


	commandList->DrawInstanced(4u, 1u, 0u, 0u);
}

bool VolumetricFogPass::CreateTemporalResources()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// History Texture
	// Volumetric Fog本体と同じHalf Resolution HDR
	// RGB: In-Scattering
	// A: Transmittance
	//============================================================
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = mHalfWidth;
	resourceDesc.Height = mHalfHeight;
	resourceDesc.DepthOrArraySize = 1u;
	resourceDesc.MipLevels = 1u;
	resourceDesc.Format = RenderFormat::SceneHDR;
	resourceDesc.SampleDesc.Count = 1u;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


	//============================================================
	// Fogなし状態
	// RGB = 0
	// A   = 1
	//============================================================
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = RenderFormat::SceneHDR;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 1.0f;


	for (UINT historyIndex = 0u; historyIndex < 2u; ++historyIndex)
	{
		const HRESULT hr = device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			//================================================
			// Temporal Shaderから読む状態で開始
			//================================================
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(mTemporalHistoryResources[historyIndex].ReleaseAndGetAddressOf())
		);


		if (FAILED(hr))
		{
			return false;
		}


		mTemporalHistoryStates[historyIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}


	mTemporalHistoryResources[0]->SetName(L"VolumetricFog Temporal History 0");
	mTemporalHistoryResources[1]->SetName(L"VolumetricFog Temporal History 1");


	//============================================================
	// RTV Heap
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 2u;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;


	HRESULT hr = device->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(mTemporalRTVHeap.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	const UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mTemporalRTVHeap->GetCPUDescriptorHandleForHeapStart();


	for (UINT historyIndex = 0u; historyIndex < 2u; ++historyIndex)
	{
		mTemporalRTV[historyIndex] = rtvHandle;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = RenderFormat::SceneHDR;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


		device->CreateRenderTargetView(
			mTemporalHistoryResources[historyIndex].Get(),
			&rtvDesc,
			rtvHandle
		);

		rtvHandle.ptr += static_cast<SIZE_T>(rtvDescriptorSize);
	}


	return true;
}

bool VolumetricFogPass::CreateTemporalDescriptorHeap()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 3u;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


	const HRESULT hr = device->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(mTemporalSRVHeap.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	mTemporalSRVHeap->SetName(L"VolumetricFog Temporal SRV Heap");

	return true;
}

bool VolumetricFogPass::CreateTemporalRootSignature()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 3u;
	srvRange.BaseShaderRegister = 0u;
	srvRange.RegisterSpace = 0u;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	D3D12_ROOT_PARAMETER rootParameters[2] = {};

	//============================================================
	// Root[0] t0～t2
	//============================================================
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;


	//============================================================
	// Root[1] b0
	//============================================================
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0u;


	//============================================================
	// Sampler
	//============================================================
	D3D12_STATIC_SAMPLER_DESC samplers[2] = {};

	//============================================================
	// s0 Linear
	// Fog / History用
	//============================================================
	samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].ShaderRegister = 0u;
	samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplers[0].MinLOD = 0.0f;
	samplers[0].MaxLOD = D3D12_FLOAT32_MAX;


	//============================================================
	// s1 Point
	// Depth用
	//============================================================
	samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].ShaderRegister = 1u;
	samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplers[1].MinLOD = 0.0f;
	samplers[1].MaxLOD = D3D12_FLOAT32_MAX;


	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = _countof(rootParameters);
	rootDesc.pParameters = rootParameters;
	rootDesc.NumStaticSamplers = _countof(samplers);
	rootDesc.pStaticSamplers = samplers;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;


	HRESULT hr = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		signatureBlob.GetAddressOf(),
		errorBlob.GetAddressOf()
	);


	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}

		return false;
	}


	hr = device->CreateRootSignature(
		0u,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(mTemporalRootSignature.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	mTemporalRootSignature->SetName(L"VolumetricFog Temporal RootSignature");

	return true;
}

bool VolumetricFogPass::CreateTemporalConstantBuffer()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	const UINT bufferSize = (sizeof(TemporalFogBuffer) + 255u) & ~255u;


	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;


	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = bufferSize;
	resourceDesc.Height = 1u;
	resourceDesc.DepthOrArraySize = 1u;
	resourceDesc.MipLevels = 1u;
	resourceDesc.SampleDesc.Count = 1u;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mTemporalConstantBuffer.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	hr = mTemporalConstantBuffer->Map(
		0u,
		nullptr,
		reinterpret_cast<void**>(&mTemporalMappedBuffer)
	);


	return SUCCEEDED(hr) && mTemporalMappedBuffer != nullptr;
}

bool VolumetricFogPass::CreateTemporalPipeline()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || !mTemporalRootSignature)
	{
		return false;
	}


	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};


	auto* vs = Shader::getVShader(Shader::eVertexShader::VS_Copy);
	auto* ps = Shader::getPShader(Shader::ePixelShader::PS_VolumetricFogTemporal);


	if (vs == nullptr || ps == nullptr)
	{
		return false;
	}


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	desc.pRootSignature = mTemporalRootSignature.Get();


	desc.InputLayout =
	{
		layout,
		_countof(layout)
	};


	desc.VS =
	{
		vs->getCode(),
		static_cast<SIZE_T>(vs->getLength())
	};


	desc.PS =
	{
		ps->getCode(),
		static_cast<SIZE_T>(ps->getLength())
	};

	//============================================================
	// Full Screen
	//============================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;

	//============================================================
	// Shader側で完全な色を出力するのでBlendなし
	//============================================================
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// PostProcessなのでDepth Testなし
	//============================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	//============================================================
	// Half Resolution HDR
	//============================================================
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(mTemporalPSO.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}

	mTemporalPSO->SetName(L"VolumetricFog Temporal PSO");

	return true;
}

void VolumetricFogPass::ResolveTemporal(ID3D12Resource* depthResource)
{
	if (!mTemporalEnabled ||
		depthResource == nullptr ||
		mFogResource == nullptr ||
		!mTemporalHistoryResources[mTemporalReadIndex] ||
		!mTemporalHistoryResources[mTemporalWriteIndex] ||
		!mTemporalSRVHeap ||
		!mTemporalRootSignature ||
		!mTemporalPSO ||
		!mTemporalConstantBuffer)
	{
		return;
	}


	ID3D12Device* device = Direct3D::getDevice();

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	if (device == nullptr ||
		commandList == nullptr)
	{
		return;
	}


	//============================================================
	// Current DepthはSRV状態
	//============================================================
	Direct3D::TransitionMainDepthToSRV();


	//============================================================
	// Current FogはRaymarch終了後SRV状態
	//============================================================
	TransitionFog(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


	//============================================================
	// Previous History → SRV
	//============================================================
	TransitionTemporalHistory(mTemporalReadIndex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


	//============================================================
	// Write History → RTV
	//============================================================
	TransitionTemporalHistory(mTemporalWriteIndex, D3D12_RESOURCE_STATE_RENDER_TARGET);


	//============================================================
	// SRV Descriptor作成
	//
	// [0] Current Fog
	// [1] Previous History
	// [2] Main Depth
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE handle = mTemporalSRVHeap->GetCPUDescriptorHandleForHeapStart();


	//============================================================
	// [0] Current Fog
	//============================================================
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = RenderFormat::SceneHDR;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1u;


		device->CreateShaderResourceView(
			mFogResource,
			&srvDesc,
			handle
		);
	}


	handle.ptr += static_cast<SIZE_T>(mDescriptorSize);


	//============================================================
	// [1] Previous History
	//============================================================
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = RenderFormat::SceneHDR;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1u;


		device->CreateShaderResourceView(
			mTemporalHistoryResources[mTemporalReadIndex].Get(),
			&srvDesc,
			handle);
	}


	handle.ptr += static_cast<SIZE_T>(mDescriptorSize);


	//============================================================
	// [2] Main Depth
	//============================================================
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1u;


		device->CreateShaderResourceView(
			depthResource,
			&srvDesc,
			handle
		);
	}


	//============================================================
	// Temporal CB更新
	//============================================================
	UpdateTemporalConstantBuffer();


	//============================================================
	// Write Historyへ描画
	//============================================================
	const D3D12_CPU_DESCRIPTOR_HANDLE outputRTV = mTemporalRTV[mTemporalWriteIndex];

	commandList->OMSetRenderTargets(
		1u,
		&outputRTV,
		FALSE,
		nullptr
	);


	//============================================================
	// Half Resolution
	//============================================================
	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(mHalfWidth),
		static_cast<float>(mHalfHeight),
		0.0f,
		1.0f
	};


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(mHalfWidth),
		static_cast<LONG>(mHalfHeight)
	};


	commandList->RSSetViewports(
		1u,
		&viewport
	);

	commandList->RSSetScissorRects(
		1u,
		&scissor
	);


	//============================================================
	// Temporal Root Signature / PSO
	//============================================================
	commandList->SetGraphicsRootSignature(mTemporalRootSignature.Get());
	commandList->SetPipelineState(mTemporalPSO.Get());


	ID3D12DescriptorHeap* heaps[] =
	{
		mTemporalSRVHeap.Get()
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	commandList->SetGraphicsRootDescriptorTable(
		0u,
		mTemporalSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);


	commandList->SetGraphicsRootConstantBufferView(
		1u,
		mTemporalConstantBuffer->GetGPUVirtualAddress()
	);


	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	commandList->IASetVertexBuffers(
		0u,
		1u,
		Direct3D::getScreenVBView()
	);


	commandList->DrawInstanced(4u, 1u, 0u, 0u);


	//============================================================
	// 今書いたHistoryをCompositeから読めるようにする
	//============================================================
	TransitionTemporalHistory(mTemporalWriteIndex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


	//============================================================
	// Write → Read Swap
	//============================================================
	std::swap(mTemporalReadIndex, mTemporalWriteIndex);


	//============================================================
	// これ以降History利用可能
	//============================================================
	mTemporalHistoryValid = true;

	//============================================================
	// 今FrameのCamera VPを次Frame用Previousへ保存
	// IMPORTANT:
	// Temporal Resolveが全部終わった後に更新する
	//============================================================
	mPreviousViewProjection = *vnCamera::getScreen();
}

void VolumetricFogPass::UpdateTemporalConstantBuffer()
{
	if (mTemporalMappedBuffer == nullptr)
	{
		return;
	}


	const XMMATRIX currentViewProjection = *vnCamera::getScreen();


	//============================================================
	// 初回FrameはPrevious = Current
	//============================================================
	if (!mTemporalHistoryValid)
	{
		mPreviousViewProjection = currentViewProjection;
	}


	const XMMATRIX inverseCurrentViewProjection =
		XMMatrixInverse(
			nullptr,
			currentViewProjection
		);


	//============================================================
	// Current Inverse VP
	//============================================================
	XMStoreFloat4x4(
		&mTemporalMappedBuffer->InverseCurrentViewProjection,
		XMMatrixTranspose(inverseCurrentViewProjection)
	);


	//============================================================
	// Previous VP
	//============================================================
	XMStoreFloat4x4(
		&mTemporalMappedBuffer->PreviousViewProjection,
		XMMatrixTranspose(mPreviousViewProjection)
	);


	//============================================================
	// Camera
	//============================================================
	XMFLOAT4 cameraPosition = {};


	XMStoreFloat4(
		&cameraPosition,
		*vnCamera::getPosition()
	);


	mTemporalMappedBuffer->CameraPositionMaxDistance =
		XMFLOAT4(
			cameraPosition.x,
			cameraPosition.y,
			cameraPosition.z,
			mMaxDistance
		);


	//============================================================
	// Temporal Parameter
	//============================================================
	mTemporalMappedBuffer->TemporalParameters =
		XMFLOAT4(
			mTemporalHistoryWeight,
			mTemporalResponse,
			mTemporalHistoryValid ? 1.0f : 0.0f,
			0.0f
		);


	//============================================================
	// Half Resolution
	//============================================================
	const float width = static_cast<float>((std::max)(mHalfWidth, 1u));
	const float height = static_cast<float>((std::max)(mHalfHeight, 1u));


	mTemporalMappedBuffer->ScreenParameters =
		XMFLOAT4(
			width,
			height,
			1.0f / width,
			1.0f / height
		);
}

void VolumetricFogPass::TransitionTemporalHistory(UINT index, D3D12_RESOURCE_STATES newState)
{
	if (index >= 2u || !mTemporalHistoryResources[index] || mTemporalHistoryStates[index] == newState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = mTemporalHistoryResources[index].Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = mTemporalHistoryStates[index];
	barrier.Transition.StateAfter = newState;


	Direct3D::getCommandList()->ResourceBarrier(
		1u,
		&barrier
	);

	mTemporalHistoryStates[index] = newState;
}

bool VolumetricFogPass::CreateDepthAwareCompositeRootSignature()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}

	//============================================================
	// t0～t2
	// t0 : Scene HDR
	// t1 : Temporal Resolved Fog
	// t2 : Main Depth
	//============================================================
	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 3u;
	srvRange.BaseShaderRegister = 0u;
	srvRange.RegisterSpace = 0u;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	//============================================================
	// Root Parameters
	//============================================================
	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	//============================================================
	// Root[0] : SRV Table
	//============================================================
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;


	//============================================================
	// Root[1] : b0
	//============================================================
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0u;
	rootParameters[1].Descriptor.RegisterSpace = 0u;


	//============================================================
	// Sampler
	// s0: Scene用Linear
	// s1: Fog / Depth用Point
	// Fogは自前で4tap補間するためPointで読む
	//============================================================
	D3D12_STATIC_SAMPLER_DESC samplers[2] = {};


	//============================================================
	// s0 Linear Clamp
	//============================================================
	samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].ShaderRegister = 0u;
	samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplers[0].MinLOD = 0.0f;
	samplers[0].MaxLOD = D3D12_FLOAT32_MAX;

	//============================================================
	// s1 Point Clamp
	//============================================================
	samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].ShaderRegister = 1u;
	samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplers[1].MinLOD = 0.0f;
	samplers[1].MaxLOD = D3D12_FLOAT32_MAX;


	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = _countof(rootParameters);
	rootDesc.pParameters = rootParameters;
	rootDesc.NumStaticSamplers = _countof(samplers);
	rootDesc.pStaticSamplers = samplers;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;


	HRESULT hr = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		signatureBlob.GetAddressOf(),
		errorBlob.GetAddressOf()
	);


	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}

		return false;
	}


	hr = device->CreateRootSignature(
		0u,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(mDepthAwareCompositeRootSignature.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	mDepthAwareCompositeRootSignature->SetName(L"VolumetricFog DepthAware Composite RootSignature");

	return true;
}

bool VolumetricFogPass::CreateDepthAwareCompositeDescriptorHeap()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 3u;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


	const HRESULT hr = device->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(mDepthAwareCompositeSRVHeap.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	mDepthAwareCompositeSRVHeap->SetName(L"VolumetricFog DepthAware Composite SRV Heap");


	return true;
}

bool VolumetricFogPass::CreateDepthAwareCompositeConstantBuffer()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	const UINT bufferSize = (sizeof(VolumetricUpsampleBuffer) + 255u) & ~255u;


	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type =D3D12_HEAP_TYPE_UPLOAD;


	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension =D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width =bufferSize;
	resourceDesc.Height =1u;
	resourceDesc.DepthOrArraySize =1u;
	resourceDesc.MipLevels =1u;
	resourceDesc.SampleDesc.Count =1u;
	resourceDesc.Layout =D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mUpsampleConstantBuffer.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	hr = mUpsampleConstantBuffer->Map(
		0u,
		nullptr,
		reinterpret_cast<void**>(&mUpsampleMappedBuffer)
	);


	return SUCCEEDED(hr) && mUpsampleMappedBuffer != nullptr;
}

bool VolumetricFogPass::CreateDepthAwareCompositePipeline()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || !mDepthAwareCompositeRootSignature)
	{
		return false;
	}


	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};


	auto* vs = Shader::getVShader(Shader::eVertexShader::VS_Copy);
	auto* ps = Shader::getPShader(Shader::ePixelShader::PS_VolumetricFogUpsample);


	if (vs == nullptr || ps == nullptr)
	{
		return false;
	}


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};


	desc.pRootSignature = mDepthAwareCompositeRootSignature.Get();

	desc.InputLayout =
	{
		layout,
		_countof(layout)
	};


	desc.VS =
	{
		vs->getCode(),
		static_cast<SIZE_T>(vs->getLength())
	};


	desc.PS =
	{
		ps->getCode(),
		static_cast<SIZE_T>(ps->getLength())
	};

	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(mDepthAwareCompositePSO.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		return false;
	}


	mDepthAwareCompositePSO->SetName(L"VolumetricFog DepthAware Composite PSO");

	return true;
}

void VolumetricFogPass::UpdateUpsampleConstantBuffer()
{
	if (mUpsampleMappedBuffer == nullptr)
	{
		return;
	}


	//============================================================
	// Current ViewProjection
	//============================================================
	const XMMATRIX viewProjection = *vnCamera::getScreen();


	const XMMATRIX inverseViewProjection =
		XMMatrixInverse(
			nullptr,
			viewProjection
		);


	//============================================================
	// HLSLはmul(vector, matrix)
	//============================================================
	XMStoreFloat4x4(
		&mUpsampleMappedBuffer->InverseViewProjection,
		XMMatrixTranspose(inverseViewProjection)
	);


	//============================================================
	// Camera Position
	//============================================================
	XMFLOAT4 cameraPosition = {};


	XMStoreFloat4(
		&cameraPosition,
		*vnCamera::getPosition()
	);


	mUpsampleMappedBuffer->CameraPositionMaxDistance =
		XMFLOAT4(
			cameraPosition.x,
			cameraPosition.y,
			cameraPosition.z,
			mMaxDistance
		);


	//============================================================
	// Upsample設定
	//============================================================
	mUpsampleMappedBuffer->UpsampleParameters =
		XMFLOAT4(
			mUpsampleDepthFalloff,// x : World Depth Falloff
			0.999999f,// y : Sky判定
			mDepthAwareUpsamplingEnabled ? 1.0f : 0.0f,// z : Enable
			0.0f
		);
}

void VolumetricFogPass::CompositeDepthAware(ID3D12Resource* inputSceneResource,
	ID3D12Resource* depthResource,
	D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
	UINT fullWidth,
	UINT fullHeight)
{
	if (inputSceneResource == nullptr ||
		depthResource == nullptr ||
		!mDepthAwareCompositeRootSignature ||
		!mDepthAwareCompositePSO ||
		!mDepthAwareCompositeSRVHeap ||
		!mUpsampleConstantBuffer)
	{
		return;
	}


	ID3D12Device* device = Direct3D::getDevice();

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	if (device == nullptr || commandList == nullptr)
	{
		return;
	}


	//============================================================
	// Main DepthをPixel Shaderから読む
	//============================================================
	Direct3D::TransitionMainDepthToSRV();


	//============================================================
	// Temporal後のFog Resource
	//============================================================
	ID3D12Resource* resolvedFogResource = GetResolvedFogResource();


	if (resolvedFogResource == nullptr)
	{
		return;
	}


	//============================================================
	// Descriptor Heap
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE handle = mDepthAwareCompositeSRVHeap->GetCPUDescriptorHandleForHeapStart();


	//============================================================
	// [0] t0 Scene HDR
	//============================================================
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = RenderFormat::SceneHDR;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1u;


		device->CreateShaderResourceView(
			inputSceneResource,
			&srvDesc,
			handle
		);
	}


	//============================================================
	// [1] t1 Resolved Fog
	//============================================================
	handle.ptr += static_cast<SIZE_T>(mDescriptorSize);


	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = RenderFormat::SceneHDR;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1u;


		device->CreateShaderResourceView(
			resolvedFogResource,
			&srvDesc,
			handle
		);
	}


	//============================================================
	// [2] t2 Main Depth
	//============================================================
	handle.ptr += static_cast<SIZE_T>(mDescriptorSize);


	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1u;


		device->CreateShaderResourceView(
			depthResource,
			&srvDesc,
			handle
		);
	}


	//============================================================
	// Constant Buffer
	//============================================================
	UpdateUpsampleConstantBuffer();


	//============================================================
	// Full Resolution Output
	//============================================================
	commandList->OMSetRenderTargets(
		1u,
		&outputRTV,
		FALSE,
		nullptr
	);


	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(fullWidth),
		static_cast<float>(fullHeight),
		0.0f,
		1.0f
	};


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(fullWidth),
		static_cast<LONG>(fullHeight)
	};


	commandList->RSSetViewports(
		1u,
		&viewport
	);

	commandList->RSSetScissorRects(
		1u,
		&scissor
	);


	//============================================================
	// Root / PSO
	//============================================================
	commandList->SetGraphicsRootSignature(mDepthAwareCompositeRootSignature.Get());
	commandList->SetPipelineState(mDepthAwareCompositePSO.Get());


	ID3D12DescriptorHeap* heaps[] =
	{
		mDepthAwareCompositeSRVHeap.Get()
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	commandList->SetGraphicsRootDescriptorTable(
		0u,
		mDepthAwareCompositeSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);


	commandList->SetGraphicsRootConstantBufferView(
		1u,
		mUpsampleConstantBuffer->GetGPUVirtualAddress()
	);


	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	commandList->IASetVertexBuffers(
		0u,
		1u,
		Direct3D::getScreenVBView()
	);


	commandList->DrawInstanced(4u, 1u, 0u, 0u);
}

bool VolumetricFogPass::LoadBlueNoiseTexture(const wchar_t* filePath)
{
	if (filePath == nullptr)
	{
		return false;
	}


	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// DirectXTexで読み込み
	//============================================================
	TexMetadata metadata = {};

	ScratchImage scratchImage = {};


	HRESULT hr = LoadFromWICFile(
		filePath,
		WIC_FLAGS_NONE,
		&metadata,
		scratchImage
	);


	if (FAILED(hr))
	{
		OutputDebugStringW(
			L"[VolumetricFogPass] "
			L"Failed to load Blue Noise texture.\n"
		);

		return false;
	}


	const Image* image = scratchImage.GetImage(
		0,
		0,
		0
	);


	if (image == nullptr)
	{
		return false;
	}


	//============================================================
	// SRGB → Linear
	// Blue Noiseの値をそのまま0～1として使用したいので
	// Gamma変換されないFormatへ変更する
	//============================================================
	const DXGI_FORMAT linearFormat = RenderFormat::ConvertToLinearFormat(metadata.format);


	if (linearFormat == DXGI_FORMAT_UNKNOWN)
	{
		OutputDebugStringW(
			L"[VolumetricFogPass] "
			L"Unsupported Blue Noise texture format.\n"
		);

		return false;
	}


	//============================================================
	// Texture Resource
	// WRITE_BACK + L0 Heapを使用する
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type =D3D12_HEAP_TYPE_CUSTOM;
	heapProperties.CPUPageProperty =D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProperties.MemoryPoolPreference =D3D12_MEMORY_POOL_L0;


	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension =D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width =metadata.width;
	resourceDesc.Height =static_cast<UINT>(metadata.height	);
	resourceDesc.DepthOrArraySize =1u;
	resourceDesc.MipLevels =1u;
	resourceDesc.Format =linearFormat;
	resourceDesc.SampleDesc.Count =1u;
	resourceDesc.Layout =D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags =D3D12_RESOURCE_FLAG_NONE;


	//============================================================
	// ComPtrへ生成
	//============================================================
	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(mBlueNoiseTexture.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr))
	{
		mBlueNoiseAvailable =false;
		return false;
	}


	mBlueNoiseTexture->SetName(L"VolumetricFog Blue Noise");


	//============================================================
	// Pixel Data転送
	//============================================================
	hr = mBlueNoiseTexture->WriteToSubresource(
		0u,
		nullptr,
		image->pixels,
		static_cast<UINT>(image->rowPitch),
		static_cast<UINT>(image->slicePitch)
	);


	if (FAILED(hr))
	{
		mBlueNoiseTexture.Reset();
		mBlueNoiseAvailable =false;
		return false;
	}

	mBlueNoiseAvailable =true;

	return true;
}

void VolumetricFogPass::SetDensity(float density)
{
	mDensity = (std::max)(density, 0.0f);
}

void VolumetricFogPass::SetMaxDistance(float distance)
{
	mMaxDistance = (std::max)(distance, 0.01f);
}


void VolumetricFogPass::SetColor(float r,float g,float b)
{
	mFogColor = XMFLOAT3((std::max)(r, 0.0f), (std::max)(g, 0.0f), (std::max)(b, 0.0f));
}


void VolumetricFogPass::SetDirectionalStrength(float strength)
{
	mDirectionalStrength = (std::max)(strength, 0.0f);
}

void VolumetricFogPass::SetAmbientStrength(float strength)
{
	mAmbientStrength = (std::max)(strength, 0.0f);
}

void VolumetricFogPass::SetScatteringAlbedo(float albedo)
{
	mScatteringAlbedo = (std::clamp)(albedo, 0.0f, 1.0f);
}

void VolumetricFogPass::SetAnisotropy(float anisotropy)
{
	mAnisotropy = (std::clamp)(anisotropy, -0.90f, 0.90f);
}

void VolumetricFogPass::SetHeightFog(float baseHeight,float heightFalloff)
{
	mBaseHeight = baseHeight;
	mHeightFalloff = (std::max)(heightFalloff, 0.0f);
}

void VolumetricFogPass::SetSampleCount(int count)
{
	mSampleCount = (std::clamp)(count, 4, 64);
}

void VolumetricFogPass::SetShadowStrength(float strength)
{
	mShadowStrength = (std::clamp)(strength, 0.0f, 1.0f);	
}

void VolumetricFogPass::SetShadowBias(float bias)
{
	mShadowBias = (std::max)(bias, 0.0f);
}

void VolumetricFogPass::SetSpotLightStrength(float strength)
{
	mSpotLightStrength =(std::max)(strength,0.0f);
}

void VolumetricFogPass::SetPointLightStrength(float strength)
{
	mPointLightStrength = (std::max)(strength, 0.0f);
}

void VolumetricFogPass::SetTemporalEnabled(bool enabled)
{
	//============================================================
	// 状態が変わった場合は古いHistoryを捨てる
	//============================================================
	if (mTemporalEnabled != enabled)
	{
		mTemporalEnabled =enabled;
		ResetTemporalHistory();
	}
}

void VolumetricFogPass::SetTemporalHistoryWeight(float weight)
{
	mTemporalHistoryWeight = (std::clamp)(weight, 0.0f, 0.99f);
}

void VolumetricFogPass::SetTemporalResponse(float response)
{
	mTemporalResponse = (std::max)(response, 0.0f);
}

void VolumetricFogPass::SetDepthAwareUpsamplingEnabled(bool enabled)
{
	mDepthAwareUpsamplingEnabled = enabled;
}

void VolumetricFogPass::SetUpsampleDepthFalloff(float falloff)
{
	mUpsampleDepthFalloff = (std::max)(falloff, 0.0f);
}

void VolumetricFogPass::SetBlueNoiseEnabled(bool enabled)
{
	mBlueNoiseEnabled = enabled;
	//============================================================
	// Jitter Patternが突然変わるので、
	// 古いTemporal Historyを一旦破棄する
	//============================================================
	ResetTemporalHistory();
}

ID3D12Resource* VolumetricFogPass::GetResolvedFogResource() const
{
	//============================================================
	// Temporalが有効かつHistory生成済み
	//============================================================
	if (mTemporalEnabled &&
		mTemporalHistoryValid &&
		mTemporalHistoryResources[mTemporalReadIndex])
	{
		return mTemporalHistoryResources[mTemporalReadIndex].Get();
	}
	//============================================================
	// Fallback
	//============================================================
	return mFogResource;
}
