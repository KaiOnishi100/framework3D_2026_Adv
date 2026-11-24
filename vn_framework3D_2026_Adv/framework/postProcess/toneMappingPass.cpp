#include "../../framework.h"
#include "../vn_environment.h"

#include "toneMappingPass.h"

//============================================================
// 初期化
//============================================================
bool ToneMappingPass::Initialize()
{
	if (mInitialized)
	{
		return true;
	}

	if (!CreateDescriptorHeap())
	{
		return false;
	}

	if (!CreatePipelineState())
	{
		return false;
	}

	mInitialized =
		true;

	return true;
}

//============================================================
// 終了処理
//============================================================
void ToneMappingPass::Terminate()
{
	SAFE_RELEASE(mPipelineState);
	SAFE_RELEASE(mSRVHeap);

	mDescriptorSize = 0;

	mInitialized = false;
}

//============================================================
// 2枚の入力SRVを格納するHeap
//============================================================
bool ToneMappingPass::CreateDescriptorHeap()
{
	ID3D12Device* device =Direct3D::getDevice();

	assert(device != nullptr);

	mDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type =D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors =2;
	heapDesc.Flags =D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask =0;

	const HRESULT hr =device->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(&mSRVHeap)
		);

	assert(SUCCEEDED(hr));

	if (FAILED(hr))
	{
		return false;
	}

	mSRVHeap->SetName(L"ToneMappingPass SRVHeap");

	return true;
}

//============================================================
// Tone Mapping PSO
//============================================================
bool ToneMappingPass::CreatePipelineState()
{
	ID3D12Device* device = Direct3D::getDevice();

	assert(device != nullptr);

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
		layout,
		_countof(layout)
	};

	desc.pRootSignature = Direct3D::getRootSignatureScr();

	desc.VS.pShaderBytecode =
		Shader::getVShader(Shader::eVertexShader::VS_Copy)->getCode();

	desc.VS.BytecodeLength =
		Shader::getVShader(Shader::eVertexShader::VS_Copy)->getLength();

	desc.PS.pShaderBytecode =
		Shader::getPShader(Shader::ePixelShader::PS_ToneMapping)->getCode();

	desc.PS.BytecodeLength =
		Shader::getPShader(Shader::ePixelShader::PS_ToneMapping)->getLength();

	//============================================================
	// Rasterizer
	//============================================================
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	desc.RasterizerState.DepthClipEnable = true;

	//============================================================
	// Blendなし
	//============================================================
	desc.BlendState.RenderTarget[0].BlendEnable = false;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// FullScreen PassなのでDepthなし
	//============================================================
	desc.DepthStencilState.DepthEnable = false;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;

	//============================================================
	// Tone Mappingの出力先はSDR BackBuffer
	//============================================================
	desc.RTVFormats[0] = RenderFormat::BackBuffer;
	desc.SampleDesc.Count = 1;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mPipelineState)
	);

	assert(SUCCEEDED(hr));

	if (FAILED(hr))
	{
		return false;
	}

	mPipelineState->SetName(L"ToneMappingPass PSO");

	return true;
}

void ToneMappingPass::Apply(
	ID3D12Resource* sceneResource,
	ID3D12Resource* bloomResource,
	D3D12_GPU_VIRTUAL_ADDRESS postEffectCB
)
{
	if (!mInitialized)
	{
		return;
	}

	ID3D12Device* device = Direct3D::getDevice();

	ID3D12GraphicsCommandList* cmdList = Direct3D::getCommandList();

	assert(device != nullptr);
	assert(cmdList != nullptr);
	assert(sceneResource != nullptr);
	assert(bloomResource != nullptr);

	//============================================================
	// ToneMapping専用HeapのDescriptor位置
	// [0] t0 : HDR Scene
	// [1] t1 : Bloom
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE destination0 = mSRVHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE destination1 = destination0;

	destination1.ptr += static_cast<SIZE_T>(mDescriptorSize);

	//============================================================
	// ResourceからTexture2D SRVを作る補助処理
	// Shader Visible HeapはCopy元にはできないが
	// CreateShaderResourceViewの書き込み先には使用できる
	//============================================================
	auto CreateTexture2DSRV = [device](ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle)
		{
			assert(resource != nullptr);
			const D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
			assert(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = resourceDesc.Format;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			device->CreateShaderResourceView(
				resource,
				&srvDesc,
				destinationHandle
			);
		};

	//============================================================
	// [0] HDR Scene SRV
	//============================================================
	CreateTexture2DSRV(
		sceneResource,
		destination0
	);

	//============================================================
	// [1] Bloom SRV
	//============================================================
	CreateTexture2DSRV(
		bloomResource,
		destination1
	);

	//============================================================
	// 以下は既存のBackBuffer描画処理
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = Direct3D::getCurrentRTV();

	cmdList->OMSetRenderTargets(
		1,
		&backBufferRTV,
		false,
		nullptr
	);

	cmdList->ClearRenderTargetView(
		backBufferRTV,
		vnMainFrame::clearColor,
		0,
		nullptr
	);

	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(vnMainFrame::screenWidth),
		static_cast<float>(vnMainFrame::screenHeight),
		0.0f,
		1.0f
	};

	D3D12_RECT scissor =
	{
		0,
		0,
		static_cast<LONG>(vnMainFrame::screenWidth),
		static_cast<LONG>(vnMainFrame::screenHeight)
	};

	cmdList->RSSetViewports(
		1,
		&viewport
	);

	cmdList->RSSetScissorRects(
		1,
		&scissor
	);

	cmdList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());
	cmdList->SetPipelineState(mPipelineState);

	//============================================================
	// b0 : ToneMapping Parameter
	//============================================================
	cmdList->SetGraphicsRootConstantBufferView(
		1,
		postEffectCB
	);

	ID3D12DescriptorHeap* heaps[] =
	{
		mSRVHeap
	};

	cmdList->SetDescriptorHeaps(1, heaps);

	//============================================================
	// t0から連続してt0、t1を渡す
	//============================================================
	cmdList->SetGraphicsRootDescriptorTable(
		0,
		mSRVHeap->GetGPUDescriptorHandleForHeapStart()
	);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	cmdList->IASetVertexBuffers(
		0,
		1,
		Direct3D::getScreenVBView()
	);

	cmdList->DrawInstanced(4, 1, 0, 0);
}