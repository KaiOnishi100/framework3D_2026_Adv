#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "halfResolutionEffectPass.h"

UINT HalfResolutionEffectPass::sFullWidth = 0u;
UINT HalfResolutionEffectPass::sFullHeight = 0u;
UINT HalfResolutionEffectPass::sHalfWidth = 0u;
UINT HalfResolutionEffectPass::sHalfHeight = 0u;
ID3D12Resource* HalfResolutionEffectPass::sColorResource = nullptr;
ID3D12Resource* HalfResolutionEffectPass::sDepthResource = nullptr;
ID3D12DescriptorHeap* HalfResolutionEffectPass::sRTVHeap = nullptr;
ID3D12DescriptorHeap* HalfResolutionEffectPass::sDSVHeap = nullptr;
ID3D12DescriptorHeap* HalfResolutionEffectPass::sSRVHeap = nullptr;
UINT HalfResolutionEffectPass::sSRVDescriptorSize = 0u;
ID3D12PipelineState* HalfResolutionEffectPass::sDepthDownsamplePSO = nullptr;
ID3D12PipelineState* HalfResolutionEffectPass::sCompositePSO = nullptr;
D3D12_RESOURCE_STATES HalfResolutionEffectPass::sColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

bool HalfResolutionEffectPass::Initialize(UINT fullWidth, UINT fullHeight)
{
	sFullWidth = (std::max)(fullWidth, 1u);
	sFullHeight = (std::max)(fullHeight, 1u);

	//============================================================
	// 1/2 Resolution
	// ※奇数解像度にも対応するため+1してから/2
	//============================================================
	sHalfWidth = (std::max)(1u, (sFullWidth + 1u) / 2u);
	sHalfHeight = (std::max)(1u, (sFullHeight + 1u) / 2u);


	if (!CreateResources())
	{
		Terminate();
		return false;
	}


	if (!CreateDepthDownsamplePSO())
	{
		Terminate();
		return false;
	}


	if (!CreateCompositePSO())
	{
		Terminate();
		return false;
	}


	return true;
}

void HalfResolutionEffectPass::Terminate()
{
	//============================================================
	// PSO
	//============================================================
	SAFE_RELEASE(sCompositePSO);
	SAFE_RELEASE(sDepthDownsamplePSO);

	//============================================================
	// Descriptor Heap
	//============================================================
	SAFE_RELEASE(sSRVHeap);
	SAFE_RELEASE(sDSVHeap);
	SAFE_RELEASE(sRTVHeap);

	//============================================================
	// Resource
	//============================================================
	SAFE_RELEASE(sDepthResource);
	SAFE_RELEASE(sColorResource);


	//============================================================
	// State Reset
	//============================================================
	sFullWidth =0u;
	sFullHeight =0u;
	sHalfWidth =0u;
	sHalfHeight =0u;
	sSRVDescriptorSize =0u;
	sColorState =D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void HalfResolutionEffectPass::Begin()
{
	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	Direct3D::TransitionMainDepthToSRV();

	//============================================================
	// Full → Half Depth
	//============================================================
	DownsampleDepth();


	//============================================================
	// Half HDRをRenderTargetへ
	//============================================================
	TransitionColor(D3D12_RESOURCE_STATE_RENDER_TARGET);


	const D3D12_CPU_DESCRIPTOR_HANDLE rtv =sRTVHeap->GetCPUDescriptorHandleForHeapStart();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv =sDSVHeap->GetCPUDescriptorHandleForHeapStart();


	//============================================================
	// Half HDRを黒でClear
	// ※Additive Effectなので背景は0
	//============================================================
	const float clearColor[4] =
	{
		0.0f,
		0.0f,
		0.0f,
		0.0f
	};


	commandList->ClearRenderTargetView(
		rtv,
		clearColor,
		0u,
		nullptr
	);


	//============================================================
	// Half HDR + Half Depth
	//============================================================
	commandList->OMSetRenderTargets(
		1u,
		&rtv,
		FALSE,
		&dsv
	);


	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(sHalfWidth),
		static_cast<float>(sHalfHeight),
		0.0f,
		1.0f
	};


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(sHalfWidth),
		static_cast<LONG>(sHalfHeight)
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
	// Skybox / Depth Downsampleで変化したRoot Signatureを
	// 通常3D用へ戻す
	//============================================================
	Direct3D::SetMain3DRootState();
}

void HalfResolutionEffectPass::EndAndComposite()
{
	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr ||
		sColorResource == nullptr ||
		sSRVHeap == nullptr ||
		sCompositePSO == nullptr)
	{
		return;
	}


	//============================================================
	// Half HDR
	// RenderTarget
	//      ↓
	// PixelShaderResource
	// Composite Shaderから読み取れる状態へ変更
	//============================================================
	TransitionColor(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


	//============================================================
	// Main HDR RTV
	// IMPORTANT:
	// getScreenSRVCPUHandle()ではない
	// pScreenResource用RTVであるhPPRTVを取得する
	//============================================================
	const D3D12_CPU_DESCRIPTOR_HANDLE mainRTV = Direct3D::getScreenRTVCPUHandle();


	//============================================================
	// Main HDRを出力先へ
	// DepthはCompositeでは使用しない
	//============================================================
	commandList->OMSetRenderTargets(
		1u,
		&mainRTV,
		FALSE,
		nullptr
	);


	//============================================================
	// Full Resolution Viewportへ戻す
	//============================================================
	D3D12_VIEWPORT viewport = {};

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(sFullWidth);
	viewport.Height = static_cast<float>(sFullHeight);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;


	D3D12_RECT scissor = {};

	scissor.left = 0;
	scissor.top = 0;
	scissor.right = static_cast<LONG>(sFullWidth);
	scissor.bottom = static_cast<LONG>(sFullHeight);


	commandList->RSSetViewports(
		1u,
		&viewport
	);

	commandList->RSSetScissorRects(
		1u,
		&scissor
	);


	//============================================================
	// Screen用Root Signature
	// Root 0: Descriptor Table
	//   t0 = Main Depth
	//   t1 = Half HDR
	//============================================================
	commandList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());


	//============================================================
	// Composite PSO
	//============================================================
	commandList->SetPipelineState(sCompositePSO);


	//============================================================
	// Descriptor Heap
	//============================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		sSRVHeap
	};

	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	commandList->SetGraphicsRootDescriptorTable(
		0u,
		sSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);


	//============================================================
	// Full Screen Quad
	//============================================================
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	commandList->IASetVertexBuffers(
		0u,
		1u,
		Direct3D::getScreenVBView()
	);


	//============================================================
	// Main HDR += Half Effect
	// ※Additive BlendはComposite PSO側で行う
	//============================================================
	commandList->DrawInstanced(4u, 1u, 0u, 0u);
}

bool HalfResolutionEffectPass::CreateResources()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// Heap Properties
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};

	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


	//============================================================
	// Half HDR Color
	//============================================================
	{
		D3D12_RESOURCE_DESC desc = {};

		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = sHalfWidth;
		desc.Height = sHalfHeight;
		desc.DepthOrArraySize = 1u;
		desc.MipLevels = 1u;
		desc.Format = RenderFormat::SceneHDR;
		desc.SampleDesc.Count = 1u;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = RenderFormat::SceneHDR;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 0.0f;


		const HRESULT hr = device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(&sColorResource)
		);


		if (FAILED(hr))
		{
			return false;
		}


		sColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}


	//============================================================
	// Half Depth
	// Resource: R32_TYPELESS
	// DSV: D32_FLOAT
	//============================================================
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = sHalfWidth;
		desc.Height = sHalfHeight;
		desc.DepthOrArraySize = 1u;
		desc.MipLevels = 1u;
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.SampleDesc.Count = 1u;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;


		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil.Depth = 1.0f;


		const HRESULT hr = device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS(&sDepthResource)
		);


		if (FAILED(hr))
		{
			return false;
		}
	}

	//============================================================
	// RTV Heap
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors = 1u;


		if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&sRTVHeap))))
		{
			return false;
		}


		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = RenderFormat::SceneHDR;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


		device->CreateRenderTargetView(
			sColorResource,
			&rtvDesc,
			sRTVHeap->GetCPUDescriptorHandleForHeapStart()
		);
	}

	//============================================================
	// DSV Heap
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.NumDescriptors = 1u;


		if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&sDSVHeap))))
		{
			return false;
		}


		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;


		device->CreateDepthStencilView(
			sDepthResource,
			&dsvDesc,
			sDSVHeap->GetCPUDescriptorHandleForHeapStart()
		);
	}

	//============================================================
	// SRV Heap
	// Descriptor 0: Main Depth
	// Descriptor 1: Half HDR
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 2u;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


		if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&sSRVHeap))))
		{
			return false;
		}


		sSRVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


		D3D12_CPU_DESCRIPTOR_HANDLE handle = sSRVHeap->GetCPUDescriptorHandleForHeapStart();


		//========================================================
		// [0] Main Depth
		//========================================================
		D3D12_SHADER_RESOURCE_VIEW_DESC depthSRV = {};
		depthSRV.Format = DXGI_FORMAT_R32_FLOAT;
		depthSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		depthSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		depthSRV.Texture2D.MipLevels = 1u;


		device->CreateShaderResourceView(
			Direct3D::getDepthStencilResource(),
			&depthSRV,
			handle
		);


		//========================================================
		// [1] Half HDR
		//========================================================
		handle.ptr += sSRVDescriptorSize;


		D3D12_SHADER_RESOURCE_VIEW_DESC colorSRV = {};
		colorSRV.Format = RenderFormat::SceneHDR;
		colorSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		colorSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		colorSRV.Texture2D.MipLevels = 1u;

		device->CreateShaderResourceView(
			sColorResource,
			&colorSRV,
			handle
		);
	}

	return true;
}

bool HalfResolutionEffectPass::CreateDepthDownsamplePSO()
{
	ID3D12Device* device = Direct3D::getDevice();

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
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


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	desc.InputLayout =
	{
		inputLayout,
		_countof(inputLayout)
	};

	//============================================================
	// Screen Root Signature
	//
	// Root 0 = t0
	//============================================================
	desc.pRootSignature = Direct3D::getRootSignatureScr();

	auto* vertexShader = Shader::getVShader(Shader::eVertexShader::VS_Copy);
	auto* pixelShader = Shader::getPShader(Shader::ePixelShader::PS_EffectDepthDownsample);


	desc.VS.pShaderBytecode = vertexShader->getCode();
	desc.VS.BytecodeLength = vertexShader->getLength();
	desc.PS.pShaderBytecode = pixelShader->getCode();
	desc.PS.BytecodeLength = pixelShader->getLength();


	//============================================================
	// Rasterizer
	//============================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;

	//============================================================
	// DepthへShaderから書く
	//============================================================
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	//============================================================
	// Pixel ShaderのSV_Depthをそのまま保存する。
	//============================================================
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//============================================================
	// Color出力なし
	//============================================================
	desc.NumRenderTargets = 0u;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1u;


	return SUCCEEDED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&sDepthDownsamplePSO)));
}

bool HalfResolutionEffectPass::CreateCompositePSO()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// VS_Copy用Input Layout
	//============================================================
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
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


	//============================================================
	// Shader取得
	//============================================================
	auto* vertexShader = Shader::getVShader(Shader::eVertexShader::VS_Copy);
	auto* pixelShader = Shader::getPShader(Shader::ePixelShader::PS_EffectHalfComposite);


	if (vertexShader == nullptr || pixelShader == nullptr)
	{
		return false;
	}


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};


	//============================================================
	// Input Layout
	//============================================================
	desc.InputLayout =
	{
		inputLayout,
		_countof(inputLayout)
	};


	//============================================================
	// Screen Root Signature
	// t0: Main Depth
	// t1: Half HDR
	//============================================================
	desc.pRootSignature = Direct3D::getRootSignatureScr();

	//============================================================
	// Shader
	//============================================================
	desc.VS.pShaderBytecode = vertexShader->getCode();
	desc.VS.BytecodeLength = vertexShader->getLength();
	desc.PS.pShaderBytecode = pixelShader->getCode();
	desc.PS.BytecodeLength = pixelShader->getLength();


	//============================================================
	// Rasterizer
	//============================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.FrontCounterClockwise = FALSE;
	desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.RasterizerState.MultisampleEnable = FALSE;
	desc.RasterizerState.AntialiasedLineEnable = FALSE;
	desc.RasterizerState.ForcedSampleCount = 0;
	desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//============================================================
	// Additive Composite
	// Final: MainHDR = MainHDR + HalfEffect
	//============================================================
	desc.BlendState.AlphaToCoverageEnable = FALSE;
	desc.BlendState.IndependentBlendEnable = FALSE;

	D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;
	blend.LogicOpEnable = FALSE;


	//============================================================
	// RGB
	// Source      = Half Effect
	// Destination = Main HDR
	// Src * 1 + Dest * 1
	//============================================================
	blend.SrcBlend = D3D12_BLEND_ONE;
	blend.DestBlend = D3D12_BLEND_ONE;
	blend.BlendOp = D3D12_BLEND_OP_ADD;


	//============================================================
	// Alpha
	// Main HDR側Alphaを維持する
	//============================================================
	blend.SrcBlendAlpha = D3D12_BLEND_ZERO;
	blend.DestBlendAlpha = D3D12_BLEND_ONE;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// CompositeではDepth不要
	//============================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.DepthStencilState.StencilEnable = FALSE;

	//============================================================
	// Primitive
	//============================================================
	desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


	//============================================================
	// Output
	// Composite先はBackBufferではなく
	// ToneMapping前のHDR Scene
	//============================================================
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;


	//============================================================
	// Sample
	//============================================================
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;
	desc.SampleDesc.Quality = 0u;


	//============================================================
	// PSO作成
	//============================================================
	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&sCompositePSO)
	);


	if (FAILED(hr))
	{
		OutputDebugStringW(
			L"[HalfResolutionEffectPass] "
			L"CreateCompositePSO failed.\n"
		);

		assert(false);

		return false;
	}

	sCompositePSO->SetName(L"Half Resolution Effect Composite PSO");

	return true;
}

void HalfResolutionEffectPass::DownsampleDepth()
{
	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	//============================================================
	// Half Resolution Viewport
	//============================================================
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(sHalfWidth);
	viewport.Height = static_cast<float>(sHalfHeight);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(sHalfWidth),
		static_cast<LONG>(sHalfHeight)
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
	// Half DSVのみbind
	//============================================================
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = sDSVHeap->GetCPUDescriptorHandleForHeapStart();


	commandList->OMSetRenderTargets(
		0u,
		nullptr,
		FALSE,
		&dsv
	);


	//============================================================
	// Depth初期化
	//============================================================
	commandList->ClearDepthStencilView(
		dsv,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0u,
		0u,
		nullptr
	);


	commandList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());
	commandList->SetPipelineState(sDepthDownsamplePSO);


	ID3D12DescriptorHeap* heaps[] =
	{
		sSRVHeap
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	//============================================================
	// Descriptor 0 = Main Depth
	//============================================================
	commandList->SetGraphicsRootDescriptorTable(
		0u,
		sSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);


	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	commandList->IASetVertexBuffers(
		0u,
		1u,
		Direct3D::getScreenVBView()
	);


	commandList->DrawInstanced(4u, 1u, 0u, 0u);
}

void HalfResolutionEffectPass::TransitionColor(D3D12_RESOURCE_STATES afterState)
{
	if (sColorResource == nullptr || sColorState == afterState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = sColorResource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = sColorState;
	barrier.Transition.StateAfter = afterState;

	Direct3D::getCommandList()->ResourceBarrier(
		1u,
		&barrier
	);

	sColorState = afterState;
}
