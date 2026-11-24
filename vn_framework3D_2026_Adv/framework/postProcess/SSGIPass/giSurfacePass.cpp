#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "giSurfacePass.h"


bool GISurfacePass::Initialize(UINT width, UINT height)
{
	mWidth =(std::max)(width,1u);
	mHeight =(std::max)(height,1u);


	if (!CreateResource())
	{
		Terminate();
		return false;
	}


	if (!CreatePipelineStates())
	{
		Terminate();
		return false;
	}


	return true;
}

void GISurfacePass::Terminate()
{
	//============================================================
	// Pipeline
	//============================================================
	mRigidPSO.Reset();

	mSkinPSO.Reset();


	//============================================================
	// Descriptor Heap
	//============================================================
	mSRVHeap.Reset();

	mRTVHeap.Reset();


	//============================================================
	// Resource
	//============================================================
	mSurfaceResource.Reset();
	mMaterialResource.Reset();

	mSurfaceState =	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	mMaterialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	mMaterialRTV = {};

	mWidth = 0u;
	mHeight = 0u;
}

void GISurfacePass::Generate()
{
	if (!mSurfaceResource ||
		!mRTVHeap ||
		!mRigidPSO ||
		!mSkinPSO)
	{
		return;
	}


	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	if (commandList == nullptr)
	{
		return;
	}


	//============================================================
	// Main DepthをDSVとして使用
	//============================================================
	Direct3D::TransitionMainDepthToDepthWrite();


	//============================================================
	// GI Surface MRTをRenderTargetへ
	//============================================================
	Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
	TransitionMaterial(D3D12_RESOURCE_STATE_RENDER_TARGET);

	//============================================================
	// Clear
	//============================================================

	//============================================================
	// Normal / Roughness
	//============================================================
	const float normalClear[4] =
	{
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};


	commandList->ClearRenderTargetView(
		mSurfaceRTV,
		normalClear,
		0u,
		nullptr
	);


	//============================================================
	// BaseColor / Metallic
	//============================================================
	const float materialClear[4] =
	{
		0.0f,
		0.0f,
		0.0f,
		0.0f
	};


	commandList->ClearRenderTargetView(
		mMaterialRTV,
		materialClear,
		0u,
		nullptr
	);



	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = Direct3D::getDepthStencilView();

	//============================================================
	// GI Surface MRT
	//
	// [0] Normal + Roughness
	// [1] BaseColor + Metallic
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2] =
	{
		mSurfaceRTV,
		mMaterialRTV
	};


	commandList->OMSetRenderTargets(
		2u,
		rtvs,
		FALSE,
		&dsv
	);

	//============================================================
	// Full Resolution
	//============================================================
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(mWidth);
	viewport.Height = static_cast<float>(mHeight);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(mWidth),
		static_cast<LONG>(mHeight)
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
	// Main 3D Root State
	//============================================================
	Direct3D::SetMain3DRootState();


	//============================================================
	// Scene Geometry再描画
	//============================================================
	vnScene::renderGISurface(
		mRigidPSO.Get(),
		mSkinPSO.Get()
	);


	//============================================================
	// SSGI Shaderから読める状態へ
	//============================================================
	Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	TransitionMaterial(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//============================================================
	// Main RenderTargetへ戻す
	//============================================================
	Direct3D::setMainRenderTarget();
}

bool GISurfacePass::CreateResource()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// Default Heap
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


	//============================================================
	// 共通Texture Format
	//
	// Surface:
	//   RGB = Normal
	//   A   = Roughness
	//
	// Material:
	//   RGB = BaseColor
	//   A   = Metallic
	//============================================================
	constexpr DXGI_FORMAT giSurfaceFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;


	//============================================================
	// Surface Resource
	//============================================================
	D3D12_RESOURCE_DESC surfaceDesc = {};
	surfaceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	surfaceDesc.Width = mWidth;
	surfaceDesc.Height = mHeight;
	surfaceDesc.DepthOrArraySize = 1u;
	surfaceDesc.MipLevels = 1u;
	surfaceDesc.Format = giSurfaceFormat;
	surfaceDesc.SampleDesc.Count = 1u;
	surfaceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	surfaceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


	//============================================================
	// Surface Clear
	//
	// Normal = 0
	// Roughness = 1
	//============================================================
	D3D12_CLEAR_VALUE surfaceClearValue = {};
	surfaceClearValue.Format = giSurfaceFormat;
	surfaceClearValue.Color[0] = 0.0f;
	surfaceClearValue.Color[1] = 0.0f;
	surfaceClearValue.Color[2] = 0.0f;
	surfaceClearValue.Color[3] = 1.0f;


	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&surfaceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&surfaceClearValue,
		IID_PPV_ARGS(mSurfaceResource.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr) || !mSurfaceResource)
	{
		return false;
	}


	mSurfaceResource->SetName(L"GI Surface Normal Roughness");


	mSurfaceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


	//============================================================
	// Material Resource
	//============================================================
	D3D12_RESOURCE_DESC materialDesc = surfaceDesc;

	D3D12_CLEAR_VALUE materialClearValue = {};
	materialClearValue.Format = giSurfaceFormat;
	materialClearValue.Color[0] = 0.0f;
	materialClearValue.Color[1] = 0.0f;
	materialClearValue.Color[2] = 0.0f;
	materialClearValue.Color[3] = 0.0f;


	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&materialDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&materialClearValue,
		IID_PPV_ARGS(mMaterialResource.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr) || !mMaterialResource)
	{
		return false;
	}


	mMaterialResource->SetName(L"GI Surface BaseColor Metallic");


	mMaterialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


	//============================================================
	// RTV Heap
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 2u;


	hr = device->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(mRTVHeap.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr) || !mRTVHeap)
	{
		return false;
	}


	const UINT rtvDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
		mRTVHeap->GetCPUDescriptorHandleForHeapStart();


	//============================================================
	// RTV[0]
	// Normal + Roughness
	//============================================================
	mSurfaceRTV = rtvHandle;


	D3D12_RENDER_TARGET_VIEW_DESC surfaceRTVDesc = {};
	surfaceRTVDesc.Format = giSurfaceFormat;
	surfaceRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


	device->CreateRenderTargetView(
		mSurfaceResource.Get(),
		&surfaceRTVDesc,
		mSurfaceRTV
	);


	rtvHandle.ptr += static_cast<SIZE_T>(rtvDescriptorSize);


	//============================================================
	// RTV[1]
	// BaseColor + Metallic
	//============================================================
	mMaterialRTV = rtvHandle;


	D3D12_RENDER_TARGET_VIEW_DESC materialRTVDesc = {};
	materialRTVDesc.Format = giSurfaceFormat;
	materialRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


	device->CreateRenderTargetView(
		mMaterialResource.Get(),
		&materialRTVDesc,
		mMaterialRTV
	);


	//============================================================
	// SRV Heap
	// GI SurfaceをDebug表示などで使用する場合
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = 2u;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


	hr = device->CreateDescriptorHeap(
		&srvHeapDesc,
		IID_PPV_ARGS(mSRVHeap.ReleaseAndGetAddressOf())
	);


	if (FAILED(hr) || !mSRVHeap)
	{
		return false;
	}


	const UINT srvDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = giSurfaceFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0u;
	srvDesc.Texture2D.MipLevels = 1u;


	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
		mSRVHeap->GetCPUDescriptorHandleForHeapStart();


	//============================================================
	// SRV[0]
	// Normal + Roughness
	//============================================================
	device->CreateShaderResourceView(
		mSurfaceResource.Get(),
		&srvDesc,
		srvHandle
	);


	//============================================================
	// SRV[1]
	// BaseColor + Metallic
	//============================================================
	srvHandle.ptr += static_cast<SIZE_T>(srvDescriptorSize);


	device->CreateShaderResourceView(
		mMaterialResource.Get(),
		&srvDesc,
		srvHandle
	);


	return true;
}

//============================================================
// Rigid / SkinのGI Surface PSOを生成
//============================================================
bool GISurfacePass::CreatePipelineStates()
{
	//============================================================
	// Rigid
	//============================================================
	if (!CreatePipelineState(false, mRigidPSO))
	{
		return false;
	}


	//============================================================
	// Skin
	//============================================================
	if (!CreatePipelineState(true, mSkinPSO))
	{
		return false;
	}


	//============================================================
	// Debug Name
	//============================================================
	if (mRigidPSO)
	{
		mRigidPSO->SetName(L"GI Surface Rigid PSO");
	}


	if (mSkinPSO)
	{
		mSkinPSO->SetName(L"GI Surface Skin PSO");
	}


	return true;
}

bool GISurfacePass::CreatePipelineState(bool skinned, ComPtr<ID3D12PipelineState>& outputPSO)
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// Input Layout
	//============================================================
	D3D12_INPUT_ELEMENT_DESC rigidLayout[] =
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
			"NORMAL",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
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
		},

		{
			"TANGENT",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};


	//============================================================
	// Skin側Input Layout
	//============================================================
	D3D12_INPUT_ELEMENT_DESC skinLayout[] =
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
			"NORMAL",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
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
		},

		{
			"TANGENT",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		{
			"SKIN_INDEX",
			0,
			DXGI_FORMAT_R16G16B16A16_UINT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		{
			"SKIN_WEIGHT",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};


	auto* vertexShader =
		Shader::getVShader(
			skinned ? Shader::eVertexShader::VS_3D_Skin : Shader::eVertexShader::VS_3D
		);


	auto* pixelShader =
		Shader::getPShader(
			Shader::ePixelShader::PS_GISurface
		);

	if (vertexShader == nullptr || pixelShader == nullptr)
	{
		return false;
	}


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	desc.pRootSignature = Direct3D::getRootSignature();


	if (skinned)
	{
		desc.InputLayout =
		{
			skinLayout,
			_countof(skinLayout)
		};
	}
	else
	{
		desc.InputLayout =
		{
			rigidLayout,
			_countof(rigidLayout)
		};
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

	//============================================================
	// Rasterizer
	//============================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;

	//============================================================
	// Depth
	//
	// Main Sceneですでに描かれた最前面Pixelだけを取得する
	//============================================================
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
	desc.DepthStencilState.StencilEnable = FALSE;

	//============================================================
	// Blendなし
	//============================================================
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// GI Surface Buffer
	//============================================================
	desc.NumRenderTargets = 2u;
	desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;// Normal + Roughness
	desc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;// BaseColor + Metallic
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[1].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(outputPSO.ReleaseAndGetAddressOf())
	);


	return SUCCEEDED(hr);
}

void GISurfacePass::Transition(D3D12_RESOURCE_STATES newState)
{
	if (!mSurfaceResource || mSurfaceState == newState)
	{
		return;
	}


	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	if (commandList == nullptr)
	{
		return;
	}


	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = mSurfaceResource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = mSurfaceState;
	barrier.Transition.StateAfter = newState;

	commandList->ResourceBarrier(
		1u,
		&barrier
	);

	mSurfaceState = newState;
}

void GISurfacePass::TransitionMaterial(D3D12_RESOURCE_STATES newState)
{
	if (!mMaterialResource || mMaterialState == newState)
	{
		return;
	}


	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	if (commandList == nullptr)
	{
		return;
	}


	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = mMaterialResource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = mMaterialState;
	barrier.Transition.StateAfter = newState;

	commandList->ResourceBarrier(
		1u,
		&barrier
	);

	mMaterialState = newState;
}
