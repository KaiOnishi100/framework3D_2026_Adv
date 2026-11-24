#include "../../framework.h"
#include "../vn_environment.h"

#include "lensFlarePass.h"

//============================================================
// 初期化
//============================================================
bool LensFlarePass::Initialize()
{
	if (mInitialized)
	{
		return true;
	}

	if (!CreateRootSignature())
	{
		Terminate();
		return false;
	}

	if (!CreateDescriptorHeap())
	{
		Terminate();
		return false;
	}

	if (!CreateVisibilityHistoryResources())
	{
		Terminate();
		return false;
	}

	if (!CreateConstantBuffer())
	{
		Terminate();
		return false;
	}

	if (!CreateOcclusionPipelineState())
	{
		Terminate();
		return false;
	}

	if (!CreatePipelineState())
	{
		Terminate();
		return false;
	}

	mInitialized = true;
	mFadeInSpeed = 6.0f;
	mFadeOutSpeed = 12.0f;

	return true;
}

//============================================================
// 終了
//============================================================
void LensFlarePass::Terminate()
{
	mInitialized = false;

	if (mConstantBuffer != nullptr && mMappedBuffer != nullptr)
	{
		mConstantBuffer->Unmap(
			0,
			nullptr
		);

		mMappedBuffer = nullptr;
	}

	SAFE_RELEASE(mConstantBuffer);
	SAFE_RELEASE(mPipelineState);
	SAFE_RELEASE(mSRVHeap);
	SAFE_RELEASE(mRootSignature);
	SAFE_RELEASE(mOcclusionPipelineState);
	SAFE_RELEASE(mVisibilityHistory[0]);
	SAFE_RELEASE(mVisibilityHistory[1]);
	SAFE_RELEASE(mVisibilityRTVHeap);

	mVisibilityHistoryValid = false;
	mVisibilityReadIndex = 0u;
	mVisibilityWriteIndex = 1u;
}

//============================================================
// Lens Flare Constant Buffer作成
//============================================================
bool LensFlarePass::CreateConstantBuffer()
{
	ID3D12Device* device =Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	const UINT bufferSize = (sizeof(LensFlareBuffer) + 255u) & ~255u;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type =D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CreationNodeMask =1u;
	heapProperties.VisibleNodeMask =1u;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension =D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width =bufferSize;
	resourceDesc.Height =1u;
	resourceDesc.DepthOrArraySize =1u;
	resourceDesc.MipLevels = 1u;
	resourceDesc.SampleDesc.Count =1u;
	resourceDesc.Layout =D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
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

	if (FAILED(hr) ||mMappedBuffer ==nullptr)
	{
		return false;
	}

	//============================================================
	// 初期値
	//============================================================
	mMappedBuffer->LightScreenParam =
		XMFLOAT4(
			0.5f,
			0.5f,
			0.0f,
			mIntensity
		);

	mMappedBuffer->FlareColor =
		XMFLOAT4(
			mColor.x,
			mColor.y,
			mColor.z,
			1.0f
		);

	mMappedBuffer->FlareShapeParam =
		XMFLOAT4(
			0.065f, // Core Radius
			0.16f,  // Halo Radius
			0.40f,  // Ghost
			0.35f   // Streak
		);

	mMappedBuffer->ScreenParam =
		XMFLOAT4(
			static_cast<float>(vnMainFrame::screenWidth),
			static_cast<float>(vnMainFrame::screenHeight),
			1.0f / static_cast<float>(vnMainFrame::screenWidth),
			1.0f /static_cast<float>(vnMainFrame::screenHeight));

	mMappedBuffer->FadeParam =
		XMFLOAT4(
			0.10f, // Screen Edge Fade Width
			0.15f, // Angular Fade Start
			0.85f, // Angular Fade End
			0.0f
		);

	return true;
}

bool LensFlarePass::CreateRootSignature()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	//============================================================
	// t0: HDR Scene
	// t1: Main Depth
	// t2: Lens Flare Temporal Visibility
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
	// RootParameter 0
	// Descriptor Table:
	//   t0
	//   t1
	//============================================================
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


	//============================================================
	// RootParameter 1
	// b0
	//============================================================
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].Descriptor.ShaderRegister = 0u;
	rootParameters[1].Descriptor.RegisterSpace = 0u;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


	//============================================================
	// Linear Clamp Sampler
	//============================================================
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 1u;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0u;
	sampler.RegisterSpace = 0u;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = _countof(rootParameters);
	rootDesc.pParameters = rootParameters;
	rootDesc.NumStaticSamplers = 1u;
	rootDesc.pStaticSamplers = &sampler;
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
			OutputDebugStringA(
				static_cast<const char*>(
					errorBlob->GetBufferPointer()
					)
			);
		}

		SAFE_RELEASE(errorBlob);
		SAFE_RELEASE(signatureBlob);

		return false;
	}


	hr = device->CreateRootSignature(
		0u,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature));


	SAFE_RELEASE(errorBlob);
	SAFE_RELEASE(signatureBlob);


	if (FAILED(hr))
	{
		return false;
	}


	mRootSignature->SetName(L"Lens Flare Root Signature");

	return true;
}

bool LensFlarePass::CreateDescriptorHeap()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 3u;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	HRESULT hr = device->CreateDescriptorHeap(
		&heapDesc,
		IID_PPV_ARGS(&mSRVHeap)
	);


	if (FAILED(hr))
	{
		return false;
	}


	mSRVDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	//============================================================
	// Descriptor 1へMain Depth SRVを作成
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE depthHandle = mSRVHeap->GetCPUDescriptorHandleForHeapStart();

	depthHandle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC depthSRVDesc = {};

	//============================================================
	// R32_TYPELESS ResourceをR32_FLOATとして読む
	//============================================================
	depthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	depthSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	depthSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthSRVDesc.Texture2D.MostDetailedMip = 0u;
	depthSRVDesc.Texture2D.MipLevels = 1u;
	depthSRVDesc.Texture2D.PlaneSlice = 0u;
	depthSRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(
		Direct3D::getDepthStencilResource(),
		&depthSRVDesc,
		depthHandle
	);


	return true;
}

bool LensFlarePass::CreateVisibilityHistoryResources()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// RTV Heap
	// History 0 / History 1
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 2u;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(&mVisibilityRTVHeap)
	);


	if (FAILED(hr))
	{
		return false;
	}


	const UINT rtvDescriptorSize = 
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


	//============================================================
	// Visibility Texture
	// 1x1
	// R16_FLOAT
	//============================================================
	for (UINT historyIndex = 0u; historyIndex < 2u; ++historyIndex)
	{
		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = 1u;
		resourceDesc.Height = 1u;
		resourceDesc.DepthOrArraySize = 1u;
		resourceDesc.MipLevels = 1u;
		resourceDesc.Format = DXGI_FORMAT_R16_FLOAT;
		resourceDesc.SampleDesc.Count = 1u;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


		//========================================================
		// Clear値
		// 初期状態では1.0を指定している
		// HistoryValid=false時にはShaderが履歴を使用しない
		//========================================================
		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = DXGI_FORMAT_R16_FLOAT;
		clearValue.Color[0] = 1.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 1.0f;


		hr = device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(&mVisibilityHistory[historyIndex]));


		if (FAILED(hr))
		{
			return false;
		}


		mVisibilityStates[historyIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


		//========================================================
		// RTV作成
		//========================================================
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mVisibilityRTVHeap->GetCPUDescriptorHandleForHeapStart();

		rtvHandle.ptr +=
			static_cast<SIZE_T>(historyIndex) *
			static_cast<SIZE_T>(rtvDescriptorSize);


		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R16_FLOAT;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0u;


		device->CreateRenderTargetView(
			mVisibilityHistory[historyIndex],
			&rtvDesc,
			rtvHandle
		);
	}


	mVisibilityReadIndex = 0u;

	mVisibilityWriteIndex = 1u;

	mVisibilityHistoryValid = false;

	return true;
}

bool LensFlarePass::CreateOcclusionPipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
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


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	desc.InputLayout =
	{
		layout,_countof(layout)
	};


	//============================================================
	// Lens Flareと同じRoot Signature
	//
	// t0 = Scene
	// t1 = Depth
	// t2 = History
	// b0 = Parameters
	//============================================================
	desc.pRootSignature = mRootSignature;


	auto* vertexShader =
		Shader::getVShader(
			Shader::eVertexShader::VS_Copy
		);

	auto* pixelShader =
		Shader::getPShader(
			Shader::ePixelShader::
			PS_LensFlareOcclusion
		);


	desc.VS.pShaderBytecode =
		vertexShader->getCode();

	desc.VS.BytecodeLength =
		vertexShader->getLength();


	desc.PS.pShaderBytecode =
		pixelShader->getCode();

	desc.PS.BytecodeLength =
		pixelShader->getLength();


	//============================================================
	// Rasterizer
	//============================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;

	//============================================================
	// Blendなし
	//============================================================
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// Depth不要
	//============================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//============================================================
	// Occlusion TextureはR16_FLOAT
	//============================================================
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = DXGI_FORMAT_R16_FLOAT;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1u;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mOcclusionPipelineState)
	);


	return SUCCEEDED(hr);
}

void LensFlarePass::TransitionVisibilityResource(UINT index, D3D12_RESOURCE_STATES afterState)
{
	if (index >= 2u)
	{
		return;
	}


	ID3D12Resource* resource = mVisibilityHistory[index];


	if (resource == nullptr)
	{
		return;
	}


	const D3D12_RESOURCE_STATES beforeState = mVisibilityStates[index];


	if (beforeState == afterState)
	{
		return;
	}


	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = beforeState;
	barrier.Transition.StateAfter = afterState;


	commandList->ResourceBarrier(
		1u,
		&barrier
	);

	mVisibilityStates[index] = afterState;
}

void LensFlarePass::RenderOcclusionVisibility()
{
	if (mOcclusionPipelineState == nullptr ||
		mVisibilityRTVHeap == nullptr)
	{
		return;
	}


	ID3D12Device* device = Direct3D::getDevice();

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	//============================================================
	// Temporal ParametersをGPUへ
	//============================================================
	mMappedBuffer->TemporalParam =
		XMFLOAT4(
			mDeltaTime,
			mFadeInSpeed,
			mFadeOutSpeed,
			mVisibilityHistoryValid ? 1.0f : 0.0f
		);


	//============================================================
	// t2へ前フレームHistoryのSRVを作成
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE historySRVHandle = mSRVHeap->GetCPUDescriptorHandleForHeapStart();


	historySRVHandle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize) * 2u;


	D3D12_SHADER_RESOURCE_VIEW_DESC historySRVDesc = {};
	historySRVDesc.Format = DXGI_FORMAT_R16_FLOAT;
	historySRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	historySRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	historySRVDesc.Texture2D.MostDetailedMip = 0u;
	historySRVDesc.Texture2D.MipLevels = 1u;


	device->CreateShaderResourceView(
		mVisibilityHistory[mVisibilityReadIndex],
		&historySRVDesc,
		historySRVHandle
	);


	//============================================================
	// Write側だけRender Targetへ
	//============================================================
	TransitionVisibilityResource(
		mVisibilityWriteIndex,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);


	//============================================================
	// Write側RTV
	//============================================================
	const UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


	D3D12_CPU_DESCRIPTOR_HANDLE visibilityRTV = mVisibilityRTVHeap->GetCPUDescriptorHandleForHeapStart();


	visibilityRTV.ptr +=
		static_cast<SIZE_T>(mVisibilityWriteIndex) *
		static_cast<SIZE_T>(rtvDescriptorSize);


	//============================================================
	// 1x1 Viewport
	//============================================================
	D3D12_VIEWPORT viewport = {};
	viewport.Width = 1.0f;
	viewport.Height = 1.0f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;


	D3D12_RECT scissor =
	{
		0,
		0,
		1,
		1
	};


	commandList->RSSetViewports(
		1u,
		&viewport
	);

	commandList->RSSetScissorRects(
		1u,
		&scissor
	);


	commandList->OMSetRenderTargets(
		1u,
		&visibilityRTV,
		FALSE,
		nullptr
	);

	commandList->SetGraphicsRootSignature(mRootSignature);
	commandList->SetPipelineState(mOcclusionPipelineState);


	ID3D12DescriptorHeap* heaps[] =
	{
		mSRVHeap
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	//============================================================
	// t0～t2
	// Occlusion Shaderではt1とt2を使用する
	//============================================================
	commandList->SetGraphicsRootDescriptorTable(
		0u,
		mSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);


	commandList->SetGraphicsRootConstantBufferView(
		1u,
		mConstantBuffer->GetGPUVirtualAddress()
	);


	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	commandList->IASetVertexBuffers(
		0u,
		1u,
		Direct3D::
		getScreenVBView()
	);


	commandList->DrawInstanced(
		4u,
		1u,
		0u,
		0u
	);


	//============================================================
	// 書き込んだHistoryをShader Resourceへ戻す
	//============================================================
	TransitionVisibilityResource(
		mVisibilityWriteIndex,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);


	//============================================================
	// Read / Writeを交換
	//============================================================
	const UINT oldReadIndex = mVisibilityReadIndex;

	mVisibilityReadIndex = mVisibilityWriteIndex;

	mVisibilityWriteIndex = oldReadIndex;


	//============================================================
	// 次フレームからHistory使用可能
	//============================================================
	mVisibilityHistoryValid = true;
}

//============================================================
// Directional Light位置更新
//============================================================
void LensFlarePass::UpdateDirectionalLight()
{
	if (!mInitialized || mMappedBuffer == nullptr)
	{
		return;
	}

	//============================================================
	// Camera位置
	//============================================================
	XMVECTOR cameraPosition = *vnCamera::getPosition();

	//============================================================
	// Directional Lightの逆方向
	//============================================================
	XMVECTOR lightDirection =
		XMVector3Normalize(*Light::getILightDir());

	//============================================================
	// Cameraから十分遠い位置に仮想的な太陽を置く
	//============================================================
	XMVECTOR virtualSunPosition =
		XMVectorAdd(
			cameraPosition,
			XMVectorScale(lightDirection, 10000.0f
			)
		);

	virtualSunPosition =
		XMVectorSetW(
			virtualSunPosition,
			1.0f
		);

	//============================================================
	// World → Clip
	//============================================================
	const XMMATRIX viewProjection = *vnCamera::getScreen();

	const XMVECTOR clipPosition =
		XMVector4Transform(
			virtualSunPosition,
			viewProjection
		);

	const float clipW =
		XMVectorGetW(clipPosition);

	//============================================================
	// Camera後方なら非表示
	//============================================================
	if (clipW <= 0.0001f)
	{
		mMappedBuffer->LightScreenParam.z = 0.0f;
		return;
	}

	//============================================================
	// Perspective Divide
	//============================================================
	const float ndcX = XMVectorGetX(clipPosition) / clipW;
	const float ndcY = XMVectorGetY(clipPosition) / clipW;

	//============================================================
	// NDC → UV
	// DirectX:
	// NDC X : -1 ～ 1
	// NDC Y : -1 ～ 1
	// Texture:
	// UV X : 0 ～ 1
	// UV Y : 0 ～ 1
	//============================================================
	const float screenU = ndcX * 0.5f + 0.5f;
	const float screenV = -ndcY * 0.5f + 0.5f;

	//============================================================
	// 少し画面外まで許容する
	// 完全に画面外へ出た瞬間に消えるのではなく
	// 端から自然に抜けるようにする
	//============================================================
	const bool insideScreenArea =
		screenU >= -0.15f &&
		screenU <= 1.15f &&
		screenV >= -0.15f &&
		screenV <= 1.15f;

	mMappedBuffer->LightScreenParam =
		XMFLOAT4(
			screenU,
			screenV,
			insideScreenArea ? 1.0f : 0.0f, 
			mIntensity
		);
}

void LensFlarePass::ApplyToRT(
	ID3D12Resource* inputResource,
	D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
	UINT width,
	UINT height)
{

	if (!mInitialized ||
		mPipelineState == nullptr ||
		mConstantBuffer == nullptr ||
		mRootSignature == nullptr ||
		mSRVHeap == nullptr)
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
	// Directional LightのScreen位置更新
	//============================================================
	UpdateDirectionalLight();


	//============================================================
	// Screen Size
	//============================================================
	mMappedBuffer->ScreenParam =
		XMFLOAT4(
			static_cast<float>(width),
			static_cast<float>(height),
			1.0f / static_cast<float>((std::max)(width, 1u)),
			1.0f / static_cast<float>((std::max)(height, 1u))
		);


	//============================================================
	// Descriptor 0へ現在のHDR Scene SRVを直接作成
	// CopyDescriptorsSimple()を使用せず
	//   ID3D12Resource*
	//          ↓
	//   LensFlare専用SRV
	// を直接作る
	// 
	// これにより、外部Descriptor HeapのCPU Handleに
	// 依存しなくなる
	//============================================================
	if (inputResource == nullptr)
	{
		OutputDebugStringW(L"[LensFlarePass] inputResource is null.\n");
		assert(false);
		return;
	}


	//============================================================
	// Lens Flare専用HeapのDescriptor 0
	// t0 HDR Scene
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE sceneDestination = mSRVHeap->GetCPUDescriptorHandleForHeapStart();


	//============================================================
	// 現在のHDR Scene Resource情報を確認
	//============================================================
	const D3D12_RESOURCE_DESC inputResourceDesc = inputResource->GetDesc();


	//============================================================
	// Lens Flareへ渡すScene SRV
	//============================================================
	D3D12_SHADER_RESOURCE_VIEW_DESC sceneSRVDesc = {};
	sceneSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sceneSRVDesc.Format = inputResourceDesc.Format;
	sceneSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sceneSRVDesc.Texture2D.MostDetailedMip = 0u;
	sceneSRVDesc.Texture2D.MipLevels = 1u;
	sceneSRVDesc.Texture2D.PlaneSlice = 0u;
	sceneSRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;


	//============================================================
	// Descriptor 0へHDR Scene SRVを作成
	//============================================================
	device->CreateShaderResourceView(
		inputResource,
		&sceneSRVDesc,
		sceneDestination
	);

	//============================================================
	// Temporal Occlusionを更新する
	//============================================================
	RenderOcclusionVisibility();

	//============================================================
	// t2を最新Temporal Visibilityへ変更
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE visibilitySRVHandle = mSRVHeap->GetCPUDescriptorHandleForHeapStart();

	visibilitySRVHandle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize) * 2u;

	D3D12_SHADER_RESOURCE_VIEW_DESC visibilitySRVDesc = {};
	visibilitySRVDesc.Format = DXGI_FORMAT_R16_FLOAT;
	visibilitySRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	visibilitySRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	visibilitySRVDesc.Texture2D.MostDetailedMip = 0u;
	visibilitySRVDesc.Texture2D.MipLevels = 1u;

	device->CreateShaderResourceView(
		mVisibilityHistory[mVisibilityReadIndex],
		&visibilitySRVDesc,
		visibilitySRVHandle
	);

	//============================================================
	// Viewport
	//============================================================
	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;


	D3D12_RECT scissor = {};
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = static_cast<LONG>(width);
	scissor.bottom = static_cast<LONG>(height);


	commandList->RSSetViewports(
		1u,
		&viewport
	);

	commandList->RSSetScissorRects(
		1u,
		&scissor
	);


	commandList->OMSetRenderTargets(
		1u,
		&outputRTV,
		FALSE,
		nullptr
	);


	//============================================================
	// Lens Flare専用Pipeline
	//============================================================
	commandList->SetGraphicsRootSignature(mRootSignature);
	commandList->SetPipelineState(mPipelineState);


	//============================================================
	// Lens Flare専用Heap
	//============================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		mSRVHeap
	};

	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	//============================================================
	// Root 0
	// t0 = Scene
	// t1 = Depth
	//============================================================
	commandList->SetGraphicsRootDescriptorTable(
		0u,
		mSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);


	//============================================================
	// Root 1
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
}

void LensFlarePass::SetIntensity(float intensity)
{
	mIntensity = (std::max)(intensity, 0.0f);
}

void LensFlarePass::SetColor(const XMFLOAT3& color)
{
	mColor = color;
	if (mMappedBuffer != nullptr)
	{
		mMappedBuffer->FlareColor =
			XMFLOAT4(
				color.x,
				color.y,
				color.z,
				1.0f
			);
	}
}

void LensFlarePass::SetDeltaTime(float deltaTime)
{
	mDeltaTime = (std::clamp)(deltaTime, 0.0f, 0.1f);
}

void LensFlarePass::SetEdgeFadeWidth(float width)
{
	if (mMappedBuffer == nullptr)
	{
		return;
	}
	//============================================================
	// 0～0.5に制限
	// 0.5以上だと画面中央までFade領域になる
	//============================================================
	const float safeWidth = (std::clamp)(width, 0.0f, 0.5f);

	mMappedBuffer->FadeParam.x = safeWidth;
}

void LensFlarePass::SetAngularFade(float start, float end)
{
	if (mMappedBuffer == nullptr)
	{
		return;
	}

	const float safeStart = (std::max)(start, 0.0f);

	//============================================================
	// Endは必ずStartより大きくする
	//============================================================
	const float safeEnd = (std::max)(end, safeStart + 0.0001f);

	mMappedBuffer->FadeParam.y = safeStart;
	mMappedBuffer->FadeParam.z = safeEnd;
}

bool LensFlarePass::CreatePipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		assert(false);
		return false;
	}


	//============================================================
	// Vertex Shader確認
	//============================================================
	Shader::ShaderObject* vertexShader =
		Shader::getVShader(Shader::eVertexShader::VS_Copy);

	if (vertexShader == nullptr ||
		vertexShader->getCode() == nullptr ||
		vertexShader->getLength() == 0)
	{
		OutputDebugStringW(
			L"[LensFlarePass] VS_Copy is invalid.\n"
		);

		assert(false);
		return false;
	}

	//============================================================
	// Pixel Shader確認
	//============================================================
	Shader::ShaderObject* pixelShader =
		Shader::getPShader(Shader::ePixelShader::PS_LensFlare);

	if (pixelShader == nullptr ||
		pixelShader->getCode() == nullptr ||
		pixelShader->getLength() == 0)
	{
		OutputDebugStringW(L"[LensFlarePass] PS_LensFlare is invalid.\n");

		assert(false);
		return false;
	}

	//============================================================
	// Screen Vertex Input Layout
	//============================================================
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	//============================================================
	// Input Layout
	//============================================================
	desc.InputLayout =
	{
		layout,
		_countof(layout)
	};

	//============================================================
	// Root Signature
	//============================================================
	desc.pRootSignature = mRootSignature;

	//============================================================
	// Vertex Shader
	//============================================================
	desc.VS.pShaderBytecode = vertexShader->getCode();
	desc.VS.BytecodeLength = vertexShader->getLength();

	//============================================================
	// Pixel Shader
	//============================================================
	desc.PS.pShaderBytecode = pixelShader->getCode();
	desc.PS.BytecodeLength = pixelShader->getLength();

	//============================================================
	// Rasterizer
	// 既存Screen Pipelineと同じ設定にする
	//============================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
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
	// Blend
	//============================================================
	desc.BlendState.AlphaToCoverageEnable = FALSE;
	desc.BlendState.IndependentBlendEnable = FALSE;

	D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = FALSE;
	blend.LogicOpEnable = FALSE;
	blend.SrcBlend = D3D12_BLEND_ONE;
	blend.DestBlend = D3D12_BLEND_ZERO;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.DestBlendAlpha = D3D12_BLEND_ZERO;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// Depth / Stencil
	//============================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;

	//============================================================
	// Primitive
	//============================================================
	desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//============================================================
	// HDR Render Target
	//============================================================
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;

	//============================================================
	// Depth Bufferなし
	//============================================================
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	//============================================================
	// Sample
	//============================================================
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	//============================================================
	// Pipeline State作成
	//============================================================
	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mPipelineState)
	);

	//============================================================
	// 失敗した場合
	//============================================================
	if (FAILED(hr))
	{
		wchar_t debugText[256] = {};

		swprintf_s(
			debugText,
			L"[LensFlarePass] CreateGraphicsPipelineState failed. HRESULT = 0x%08X\n",
			static_cast<unsigned int>(hr));

		OutputDebugStringW(debugText);
		assert(false);
		return false;
	}

	mPipelineState->SetName(L"LensFlarePass PSO");

	return true;
}