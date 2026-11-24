#include "../../framework.h"
#include "../vn_environment.h"

#include "motionVectorPass.h"

//============================================================
// Initialize
//============================================================
bool MotionVectorPass::Initialize(UINT width, UINT height)
{
	mWidth = (std::max)(width, 1u);
	mHeight = (std::max)(height, 1u);


	if (!CreateResource())
	{
		Terminate();
		return false;
	}

	if (!CreateConstantBuffer())
	{
		Terminate();
		return false;
	}

	if (!CreatePipelineState())
	{
		Terminate();
		return false;
	}

	if (!CreateObjectMotionPipelineState())
	{
		Terminate();
		return false;
	}

	if (!CreateSkinnedObjectMotionPipelineState())
	{
		Terminate();
		return false;
	}

	//============================================================
	// 初回は履歴なし
	//============================================================
	mHasHistory = false;

	return true;
}

void MotionVectorPass::Terminate()
{
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
	SAFE_RELEASE(mObjectMotionPSO);
	SAFE_RELEASE(mSkinnedObjectMotionPSO);
	SAFE_RELEASE(mOutputSRVHeap);
	SAFE_RELEASE(mInputHeap);
	SAFE_RELEASE(mRTVHeap);
	SAFE_RELEASE(mResource);

	mHasHistory = false;

	mWidth = 0u;
	mHeight = 0u;
}

void MotionVectorPass::Generate()
{
	if (mResource == nullptr ||
		mPipelineState == nullptr ||
		mMappedBuffer == nullptr)
	{
		return;
	}


	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	//============================================================
	// 現在のViewProjection
	//============================================================
	const XMMATRIX currentVP = *vnCamera::getScreen();


	//============================================================
	// 初回Frame
	// Previous = Current とすることでVelocity=0にする
	//============================================================
	if (!mHasHistory)
	{
		mPreviousVP = currentVP;
		mHasHistory = true;
	}


	//============================================================
	// Current VP逆行列
	//============================================================
	const XMMATRIX inverseCurrentVP =
		XMMatrixInverse(
			nullptr,
			currentVP
		);


	XMStoreFloat4x4(
		&mMappedBuffer->InverseCurrentVP,
		XMMatrixTranspose(inverseCurrentVP)
	);


	XMStoreFloat4x4(
		&mMappedBuffer->PreviousVP,
		XMMatrixTranspose(mPreviousVP)
	);


	//============================================================
	// Main DepthをShaderから読む
	//============================================================
	Direct3D::TransitionMainDepthToSRV();


	//============================================================
	// Velocity Buffer → RTV
	//============================================================
	Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);


	//============================================================
	// Motionなし = 0
	//============================================================
	const float clearColor[] =
	{
		0.0f,
		0.0f,
		0.0f,
		0.0f
	};


	commandList->ClearRenderTargetView(
		mRTV,
		clearColor,
		0u,
		nullptr
	);


	commandList->OMSetRenderTargets(
		1u,
		&mRTV,
		FALSE,
		nullptr
	);


	//============================================================
	// Full Resolution
	//============================================================
	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(mWidth),
		static_cast<float>(mHeight),
		0.0f,
		1.0f
	};


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
	// Screen Pipeline
	//============================================================
	commandList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());
	commandList->SetPipelineState(mPipelineState);


	//============================================================
	// b0
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		1u,
		mConstantBuffer->GetGPUVirtualAddress()
	);


	//============================================================
	// t0 Main Depth
	//============================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		mInputHeap
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	commandList->SetGraphicsRootDescriptorTable(
		0u,
		mInputHeap->GetGPUDescriptorHandleForHeapStart()
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


	commandList->DrawInstanced(4u, 1u, 0u, 0u);

	//============================================================
	// 今回のCurrent VPを次FrameのPreviousへ保存
	// ※IMPORTANT: Drawの前に上書きしないこと
	//============================================================
	mPreviousVP = currentVP;
}

void MotionVectorPass::ResetHistory()
{
	mHasHistory = false;
}

void MotionVectorPass::BeginObjectMotion()
{
	if (mResource == nullptr || mObjectMotionPSO == nullptr)
	{
		return;
	}


	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	Direct3D::TransitionMainDepthToDepthWrite();


	//============================================================
	// Velocity BufferがRTV状態であることを保証
	//============================================================
	Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);


	//============================================================
	// Velocity RTV
	//============================================================
	const D3D12_CPU_DESCRIPTOR_HANDLE velocityRTV = mRTV;


	//============================================================
	// Main Depth DSV
	//============================================================
	const D3D12_CPU_DESCRIPTOR_HANDLE mainDSV = Direct3D::getDepthStencilView();


	//============================================================
	// Velocity + Main Depth
	//============================================================
	commandList->OMSetRenderTargets(
		1u,
		&velocityRTV,
		FALSE,
		&mainDSV
	);


	//============================================================
	// Full Resolution
	//============================================================
	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(mWidth),
		static_cast<float>(mHeight),
		0.0f,
		1.0f
	};


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
	// Main 3D Root Signature
	// vnModel側はRoot[1]へconstBuffを設定する
	//============================================================
	commandList->SetGraphicsRootSignature(Direct3D::getRootSignature());


	//============================================================
	// Object Motion PSO
	//============================================================
	commandList->SetPipelineState(mObjectMotionPSO);
}

void MotionVectorPass::EndObjectMotion()
{
	//============================================================
	// Velocity Buffer
	// MotionBlur / Debug Shaderから読むためSRVへ
	//============================================================
	Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//============================================================
	// Main Depth
	// Lens Flareなど後続PostProcessから
	// Shader Resourceとして使用できるよう戻す
	//============================================================
	Direct3D::TransitionMainDepthToSRV();
}

D3D12_CPU_DESCRIPTOR_HANDLE MotionVectorPass::GetSRVCPU() const
{
	return mOutputSRVCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE MotionVectorPass::GetSRVGPU() const
{
	return mOutputSRVGPU;
}

ID3D12DescriptorHeap* MotionVectorPass::GetSRVHeap() const
{
	return mOutputSRVHeap;
}

ID3D12Resource* MotionVectorPass::GetResource() const
{
	return mResource;
}

void MotionVectorPass::SetRigidObjectMotionPipeline()
{
	Direct3D::getCommandList()->SetPipelineState(mObjectMotionPSO);
}

void MotionVectorPass::SetSkinnedObjectMotionPipeline()
{
	Direct3D::getCommandList()->SetPipelineState(mSkinnedObjectMotionPSO);
}

//============================================================
// Motion Vector Resource
//============================================================
bool MotionVectorPass::CreateResource()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// Motion Vector Texture
	//============================================================
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = mWidth;
	resourceDesc.Height = mHeight;
	resourceDesc.DepthOrArraySize = 1u;
	resourceDesc.MipLevels = 1u;
	resourceDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	resourceDesc.SampleDesc.Count = 1u;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


	//============================================================
	// Motionなし = (0, 0)
	//============================================================
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_R16G16_FLOAT;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 0.0f;


	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(&mResource)
	);


	if (FAILED(hr))
	{
		return false;
	}


	mResource->SetName(L"MotionVectorPass::Velocity");


	mState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


	//============================================================
	// RTV Heap
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors = 1u;


		hr = device->CreateDescriptorHeap(
			&desc,
			IID_PPV_ARGS(&mRTVHeap)
		);


		if (FAILED(hr))
		{
			return false;
		}


		mRTV = mRTVHeap->GetCPUDescriptorHandleForHeapStart();


		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


		device->CreateRenderTargetView(
			mResource,
			&rtvDesc,
			mRTV
		);
	}


	//============================================================
	// 入力Heap
	// Screen Root Signatureはt0～t1を要求するため2個
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 2u;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


		hr = device->CreateDescriptorHeap(
			&desc,
			IID_PPV_ARGS(&mInputHeap)
		);


		if (FAILED(hr))
		{
			return false;
		}


		const UINT descriptorSize =
			device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


		D3D12_SHADER_RESOURCE_VIEW_DESC depthSRV = {};
		depthSRV.Format = DXGI_FORMAT_R32_FLOAT;
		depthSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		depthSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		depthSRV.Texture2D.MipLevels = 1u;


		D3D12_CPU_DESCRIPTOR_HANDLE handle = mInputHeap->GetCPUDescriptorHandleForHeapStart();


		//========================================================
		// [0] t0 Main Depth
		//========================================================
		device->CreateShaderResourceView(
			Direct3D::getDepthStencilResource(),
			&depthSRV,
			handle
		);


		//========================================================
		// [1] t1 Dummy
		//========================================================
		handle.ptr += descriptorSize;


		device->CreateShaderResourceView(
			Direct3D::getDepthStencilResource(),
			&depthSRV,
			handle
		);
	}


	//============================================================
	// Motion Vector出力SRV
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 2u;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


		hr = device->CreateDescriptorHeap(
			&desc,
			IID_PPV_ARGS(&mOutputSRVHeap)
		);


		if (FAILED(hr))
		{
			return false;
		}

		const UINT descriptorSize =
			device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		mOutputSRVCPU = mOutputSRVHeap->GetCPUDescriptorHandleForHeapStart();
		mOutputSRVGPU = mOutputSRVHeap->GetGPUDescriptorHandleForHeapStart();


		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1u;

		//============================================================
		// [0] t0
		//============================================================
		device->CreateShaderResourceView(
			mResource,
			&srvDesc,
			mOutputSRVCPU
		);

		//============================================================
		// [1] t1
		// 同じMotion VectorをDummyとして登録
		//============================================================
		D3D12_CPU_DESCRIPTOR_HANDLE secondHandle = mOutputSRVCPU;

		secondHandle.ptr += static_cast<SIZE_T>(descriptorSize);


		device->CreateShaderResourceView(
			mResource,
			&srvDesc,
			secondHandle
		);
	}

	return true;
}

//============================================================
// Constant Buffer
//============================================================
bool MotionVectorPass::CreateConstantBuffer()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	const UINT bufferSize = (sizeof(MotionVectorBuffer) + 255u) & ~255u;


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


	return
		SUCCEEDED(hr) &&
		mMappedBuffer != nullptr;
}

void MotionVectorPass::Transition(D3D12_RESOURCE_STATES newState)
{
	if (mResource == nullptr || mState == newState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = mResource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = mState;
	barrier.Transition.StateAfter = newState;


	Direct3D::getCommandList()->ResourceBarrier(
		1u,
		&barrier
	);

	mState = newState;
}

//============================================================
// Motion Vector PSO
//============================================================
bool MotionVectorPass::CreatePipelineState()
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


	desc.pRootSignature = Direct3D::getRootSignatureScr();


	auto* vertexShader =Shader::getVShader(Shader::eVertexShader::VS_Copy);
	auto* pixelShader =Shader::getPShader(Shader::ePixelShader::PS_MotionVector);


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
	// Blendなし
	//============================================================
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


	//============================================================
	// Full Screen PassなのでDepth Testなし
	//============================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1u;

	//============================================================
	// Velocity Buffer Format
	//============================================================
	desc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mPipelineState)
	);

	return SUCCEEDED(hr);
}

bool MotionVectorPass::CreateObjectMotionPipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	//============================================================
	// Vertex3Dと同じInput Layout
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


	auto* vertexShader =Shader::getVShader(Shader::eVertexShader::VS_ObjectMotion);
	auto* pixelShader =Shader::getPShader(Shader::ePixelShader::PS_ObjectMotion);


	if (vertexShader == nullptr || pixelShader == nullptr)
	{
		return false;
	}


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};


	//============================================================
	// Main 3D Root Signatureを使用
	// Root[1] = MatrixBuffer b2
	// vnModelのconstBuffをそのまま再利用できる
	//============================================================
	desc.pRootSignature = Direct3D::getRootSignature();


	desc.InputLayout =
	{
		layout,
		_countof(layout)
	};


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
	// ※Motion Vector PassではMaterialのDoubleSided状態を
	// 個別に切り替えないため、まずNONEにしておく
	//============================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;

	//============================================================
	// Blendなし
	// Camera Motion VectorをObject Motionで完全に上書きする
	//============================================================
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// Main DepthとDepth Test
	// 通常Sceneですでに完成したDepthを利用する
	// Depth書き込み: なし
	// Depth比較: LESS_EQUAL
	// 他Objectの後ろに隠れている面がVelocity Bufferへ書き込むのを防ぐ
	//============================================================
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.DepthStencilState.StencilEnable = FALSE;

	//============================================================
	// Velocity Buffer
	//============================================================
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;

	//============================================================
	// Main Depth
	//============================================================
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mObjectMotionPSO)
	);


	if (FAILED(hr))
	{
		OutputDebugStringW(L"[MotionVectorPass] CreateObjectMotionPipelineState failed.\n");

		assert(false);
		return false;
	}


	mObjectMotionPSO->SetName(L"MotionVectorPass Rigid Object Motion PSO");

	return true;
}

bool MotionVectorPass::CreateSkinnedObjectMotionPipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// 現在のvnObject::inputElementDescs_Skinと同一構成
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
			48,     // offset 48
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


	auto* vs = Shader::getVShader(Shader::eVertexShader::VS_ObjectMotionSkin);
	auto* ps = Shader::getPShader(Shader::ePixelShader::PS_ObjectMotion);


	if (vs == nullptr || ps == nullptr)
	{
		return false;
	}


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};


	desc.InputLayout =
	{
		layout,
		_countof(layout)
	};


	//============================================================
	// Main Root Signature
	// b2 Current/Previous WVP
	// b3 Current Bones
	// b4 Previous Bones
	//============================================================
	desc.pRootSignature = Direct3D::getRootSignature();

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
	// Rasterizer
	//============================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;

	//============================================================
	// Velocityを上書き
	//============================================================
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// Main Depthを使って見えているPixelだけ書く
	//============================================================
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	//============================================================
	// Velocity
	//============================================================
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mSkinnedObjectMotionPSO)
	);


	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	mSkinnedObjectMotionPSO->SetName(L"Skinned Object Motion PSO");

	return true;
}
