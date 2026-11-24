#include "../../framework.h"
#include "../vn_environment.h"

#include "motionBlurPass.h"

//============================================================
// 初期化
//============================================================
bool MotionBlurPass::Initialize()
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


	mInitialized = true;

	return true;
}

//============================================================
// 終了
//============================================================
void MotionBlurPass::Terminate()
{
	mInitialized = false;

	if (mConstantBuffer != nullptr && mMappedBuffer != nullptr)
	{
		mConstantBuffer->Unmap(
			0u,
			nullptr
		);

		mMappedBuffer = nullptr;
	}

	SAFE_RELEASE(mRootSignature);
	SAFE_RELEASE(mConstantBuffer);
	SAFE_RELEASE(mPipelineState);
	SAFE_RELEASE(mDescriptorHeap);

	mDescriptorSize = 0u;
}

//============================================================
// Motion Blur適用
//============================================================
void MotionBlurPass::ApplyToRT(
	ID3D12Resource* inputSceneResource,
	ID3D12Resource* motionVectorResource,
	ID3D12Resource* depthResource,
	D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
	UINT width,
	UINT height
)
{
	if (!mInitialized ||
		mPipelineState == nullptr ||
		mRootSignature == nullptr ||
		mDescriptorHeap == nullptr ||
		mMappedBuffer == nullptr ||
		inputSceneResource == nullptr ||
		motionVectorResource == nullptr ||
		depthResource == nullptr)
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
	// MotionBlur専用Descriptor HeapへSRVを直接生成する
	// [0] t0 = Scene HDR
	// [1] t1 = Motion Vector
	// [2] t2 Main Depth
	// 外部DescriptorをCopyしないことで
	// CPU Descriptor Handleの有効性やHeap依存を避ける
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE destination =
		mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();


	//============================================================
	// [0] t0 : Scene HDR
	//============================================================
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		//========================================================
		// Scene HDR Resourceと同じFormat
		//========================================================
		srvDesc.Format = RenderFormat::SceneHDR;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0u;
		srvDesc.Texture2D.MipLevels = 1u;
		srvDesc.Texture2D.PlaneSlice = 0u;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;


		//========================================================
		// MotionBlur専用Heap[0]へ直接SRV作成
		//========================================================
		device->CreateShaderResourceView(
			inputSceneResource,
			&srvDesc,
			destination
		);
	}

	//============================================================
	// [1] t1 : Motion Vector
	//============================================================
	destination.ptr += static_cast<SIZE_T>(mDescriptorSize);


	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		//========================================================
		// MotionVectorPassのResource Format
		// R : X Velocity
		// G : Y Velocity
		//========================================================
		srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0u;
		srvDesc.Texture2D.MipLevels = 1u;
		srvDesc.Texture2D.PlaneSlice = 0u;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		//========================================================
		// MotionBlur専用Heap[1]へ直接SRV作成
		//========================================================
		device->CreateShaderResourceView(
			motionVectorResource,
			&srvDesc,
			destination
		);
	}

	//============================================================
	// [2] Main Depth
	// Resource: R32_TYPELESS
	// SRV: R32_FLOAT
	//============================================================
	destination.ptr += static_cast<SIZE_T>(mDescriptorSize);

	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0u;
		srvDesc.Texture2D.MipLevels = 1u;

		device->CreateShaderResourceView(
			depthResource,
			&srvDesc,
			destination
		);
	}

	//============================================================
	// Constant Buffer更新
	//============================================================
	mMappedBuffer->MotionParam =
		XMFLOAT4(
			mStrength,
			mMaxBlurPixels,
			static_cast<float>(mSampleCount),
			mVelocityScale
		);


	const float safeWidth = static_cast<float>((std::max)(width, 1u));
	const float safeHeight = static_cast<float>((std::max)(height, 1u));


	mMappedBuffer->ScreenParam =
		XMFLOAT4(
			safeWidth,
			safeHeight,
			1.0f / safeWidth,
			1.0f / safeHeight
		);

	mMappedBuffer->RejectionParam =
		XMFLOAT4(
			mDepthThreshold,
			mVelocityThreshold,
			1.0f,
			1.0f
		);

	//============================================================
	// Viewport
	//============================================================
	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		safeWidth,
		safeHeight,
		0.0f,
		1.0f
	};


	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(width),
		static_cast<LONG>(height)
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
	// 出力先
	// PingPongPostRT Write
	//============================================================
	commandList->OMSetRenderTargets(
		1u,
		&outputRTV,
		FALSE,
		nullptr
	);


	//============================================================
	// Pipeline
	//============================================================
	commandList->SetGraphicsRootSignature(mRootSignature);
	commandList->SetPipelineState(mPipelineState);


	//============================================================
	// SRV Heap
	//============================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		mDescriptorHeap
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	//============================================================
	// Root 0
	// t0 = HDR
	// t1 = MotionVector
	//============================================================
	commandList->SetGraphicsRootDescriptorTable(
		0u,
		mDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);


	//============================================================
	// Root 1
	// b0 MotionBlurBuffer
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		1u,
		mConstantBuffer->GetGPUVirtualAddress()
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
}

//============================================================
// Strength
//============================================================
void MotionBlurPass::SetStrength(float strength)
{
	mStrength =(std::max)(strength,0.0f);
}


//============================================================
// Max Blur Pixels
//============================================================
void MotionBlurPass::SetMaxBlurPixels(float pixels)
{
	mMaxBlurPixels = (std::max)(pixels, 0.0f);
};


//============================================================
// Sample Count
//============================================================
void MotionBlurPass::SetSampleCount(int count)
{
	mSampleCount = (std::clamp)(count, 2, 12);
}


//============================================================
// Velocity Scale
//============================================================
void MotionBlurPass::SetVelocityScale(float scale)
{
	mVelocityScale =(std::max)(scale,0.0f);
}

void MotionBlurPass::SetDepthThreshold(float threshold)
{
	mDepthThreshold = (std::max)(threshold, 0.000001f);
}

void MotionBlurPass::SetVelocityThreshold(float threshold)
{
	mVelocityThreshold = (std::max)(threshold, 0.000001f);
}

//============================================================
// Descriptor Heap作成
//============================================================
bool MotionBlurPass::CreateDescriptorHeap()
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
		IID_PPV_ARGS(&mDescriptorHeap)
	);


	if (FAILED(hr))
	{
		return false;
	}


	mDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mDescriptorHeap->SetName(L"MotionBlurPass DescriptorHeap");

	return true;
}

//============================================================
// Constant Buffer作成
//============================================================
bool MotionBlurPass::CreateConstantBuffer()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// CBVは256byte Alignmentが必要
	//============================================================
	const UINT bufferSize = (sizeof(MotionBlurBuffer) + 255u) & ~255u;


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


	if (FAILED(hr) || mMappedBuffer == nullptr)
	{
		return false;
	}


	return true;
}

//============================================================
// Motion Blur PSO
//============================================================
bool MotionBlurPass::CreatePipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();


	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// VS_Copy用
	// Screen Vertex:
	//   float3 POSITION
	//   float2 TEXCOORD
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


	auto* vertexShader =Shader::getVShader(Shader::eVertexShader::VS_Copy);
	auto* pixelShader =Shader::getPShader(Shader::ePixelShader::PS_MotionBlur);


	if (vertexShader == nullptr || pixelShader == nullptr)
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
	// 既存Screen Root Signature
	// Root 0: t0～t1
	// Root 1: b0
	//============================================================
	desc.pRootSignature = mRootSignature;

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
	// Hardware Blend不要
	// Shader内で完成Colorを出力する
	//============================================================
	desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// PostProcessなのでDepth不要
	//============================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	//============================================================
	// HDR出力
	//============================================================
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;


	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mPipelineState)
	);


	if (FAILED(hr))
	{
		return false;
	}


	mPipelineState->SetName(L"MotionBlurPass PSO");

	return true;
}

bool MotionBlurPass::CreateRootSignature()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	//============================================================
	// t0～t2
	//
	// t0 : Scene HDR
	// t1 : Motion Vector
	// t2 : Main Depth
	//============================================================
	D3D12_DESCRIPTOR_RANGE range = {};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors = 3u;
	range.BaseShaderRegister = 0u;
	range.RegisterSpace = 0u;
	range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	//============================================================
	// Root Parameter
	//============================================================
	D3D12_ROOT_PARAMETER rootParameters[2] = {};

	//============================================================
	// Root[0]
	// t0～t2
	//============================================================
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &range;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;

	//============================================================
	// Root[1]
	// b0 MotionBlurBuffer
	//============================================================
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0u;
	rootParameters[1].Descriptor.RegisterSpace = 0u;

	//============================================================
	// s0 : Linear Clamp
	// Scene Color / Motion Vector用
	//============================================================
	D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
	samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].ShaderRegister = 0u;
	samplers[0].RegisterSpace = 0u;
	samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplers[0].MinLOD = 0.0f;
	samplers[0].MaxLOD = D3D12_FLOAT32_MAX;


	//============================================================
	// s1 : Point Clamp
	// Depth用
	// DepthをLinear SampleするとObject境界で
	// 深度そのものが補間されてしまうため
	// Point Sampleを使用する
	//============================================================
	samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[1].ShaderRegister = 1u;
	samplers[1].RegisterSpace = 0u;
	samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplers[1].MinLOD = 0.0f;
	samplers[1].MaxLOD = D3D12_FLOAT32_MAX;

	//============================================================
	// Root Signature
	//============================================================
	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = _countof(rootParameters);
	desc.pParameters = rootParameters;
	desc.NumStaticSamplers = _countof(samplers);
	desc.pStaticSamplers = samplers;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;


	HRESULT hr = D3D12SerializeRootSignature(
		&desc,
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

		SAFE_RELEASE(errorBlob);
		SAFE_RELEASE(signatureBlob);

		return false;
	}


	hr = device->CreateRootSignature(
		0u,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)
	);


	SAFE_RELEASE(errorBlob);
	SAFE_RELEASE(signatureBlob);


	if (FAILED(hr))
	{
		return false;
	}

	mRootSignature->SetName(L"MotionBlurPass RootSignature");

	return true;
}