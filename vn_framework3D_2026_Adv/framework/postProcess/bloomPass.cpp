#include "../../framework.h"
#include "../vn_environment.h"

#include "bloomPass.h"

//============================================================
// 初期化
//============================================================
bool BloomPass::Initialize()
{
	if (mInitialized) return true;

	CreateResources();
	CreatePipelineStates();

	mInitialized = true;
	return true;
}

//============================================================
// 終了処理
//============================================================
void BloomPass::Terminate()
{
	SAFE_RELEASE(mPSO_BlurV);
	SAFE_RELEASE(mPSO_BlurH);
	SAFE_RELEASE(mPSO_Bright);

	SAFE_RELEASE(mSRVHeap);
	SAFE_RELEASE(mRTVHeap);

	SAFE_RELEASE(mBloomRT[0]);
	SAFE_RELEASE(mBloomRT[1]);

	mInitialized = false;
}

void BloomPass::Generate(ID3D12DescriptorHeap* inputSRVHeap, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	if (!mInitialized)
	{
		return;
	}

	//============================================================
	// HDR Sceneから高輝度部分を抽出
	//============================================================
	PassBright(
		inputSRVHeap,
		inputSRV
	);

	//============================================================
	// 横方向Blur
	//============================================================
	PassBlurH();

	//============================================================
	// 縦方向Blur
	// 最終結果はmBloomRT[0]に残る
	//============================================================
	PassBlurV();
}

ID3D12Resource* BloomPass::GetResultResource() const
{
	return mBloomRT[0];
}

D3D12_CPU_DESCRIPTOR_HANDLE BloomPass::GetResultSRVCPU() const
{
	return mSRVCPU[0];
}

D3D12_GPU_DESCRIPTOR_HANDLE BloomPass::GetResultSRVGPU() const
{
	return mSRVGPU[0];
}

ID3D12DescriptorHeap*BloomPass::GetSRVHeap() const
{
	return mSRVHeap;
}

//============================================================
// リソース状態変更
//============================================================
void BloomPass::ChangeResourceState(
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES beforeState,
	D3D12_RESOURCE_STATES afterState
)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = beforeState;
	barrier.Transition.StateAfter = afterState;

	Direct3D::getCommandList()->ResourceBarrier(1, &barrier);
}

//============================================================
// Pass 1 : Bright
// 元シーン pScreenResource から明るい部分だけ抽出してmBloomRT[0]に描く
//============================================================
void BloomPass::PassBright(ID3D12DescriptorHeap* inputSRVHeap, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	ID3D12GraphicsCommandList* cmdList = Direct3D::getCommandList();

	assert(Direct3D::getScreenSRVHeap() != nullptr);
	assert(Direct3D::getRootSignatureScr() != nullptr);
	assert(inputSRVHeap != nullptr);
	assert(mPSO_Bright != nullptr);

	// Bloom用の低解像度Viewport
	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		(float)mBloomWidth,
		(float)mBloomHeight,
		0.0f,
		1.0f
	};

	D3D12_RECT scissor =
	{
		0,
		0,
		(LONG)mBloomWidth,
		(LONG)mBloomHeight
	};

	float clearColor[4] =
	{
		0.0f, 0.0f, 0.0f, 0.0f
	};

	// mBloomRT[0] を描き込み用にする
	ChangeResourceState(
		mBloomRT[0],
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	// 描画先を mBloomRT[0] にする
	cmdList->OMSetRenderTargets(1, &mRTV[0], false, nullptr);
	cmdList->ClearRenderTargetView(mRTV[0], clearColor, 0, nullptr);
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);
	cmdList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());
	cmdList->SetPipelineState(mPSO_Bright);

	// pScreenResource のSRVをt0にセットする
	ID3D12DescriptorHeap* heaps[] ={ inputSRVHeap };
	cmdList->SetDescriptorHeaps(1, heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputSRV);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmdList->IASetVertexBuffers(0, 1, Direct3D::getScreenVBView());
	cmdList->DrawInstanced(4, 1, 0, 0);

	// 次のBlurHで読むためSRVに戻す
	ChangeResourceState(
		mBloomRT[0],
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
}

void BloomPass::PassBlurH()
{
	ID3D12GraphicsCommandList* cmdList = Direct3D::getCommandList();

	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		(float)mBloomWidth,
		(float)mBloomHeight,
		0.0f,
		1.0f
	};

	D3D12_RECT scissor =
	{
		0,
		0,
		(LONG)mBloomWidth,
		(LONG)mBloomHeight
	};

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// mBloomRT[1] に横ブラー結果を書く
	ChangeResourceState(
		mBloomRT[1],
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	cmdList->OMSetRenderTargets(1, &mRTV[1], false, nullptr);
	cmdList->ClearRenderTargetView(mRTV[1], clearColor, 0, nullptr);

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);

	cmdList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());

	cmdList->SetPipelineState(mPSO_BlurH);

	// mBloomRT[0] を読む
	ID3D12DescriptorHeap* heaps[] = { mSRVHeap };
	cmdList->SetDescriptorHeaps(1, heaps);

	D3D12_GPU_DESCRIPTOR_HANDLE handle =
		mSRVHeap->GetGPUDescriptorHandleForHeapStart();

	// 0番 = mBloomRT[0]
	cmdList->SetGraphicsRootDescriptorTable(0, handle);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmdList->IASetVertexBuffers(0, 1, Direct3D::getScreenVBView());

	cmdList->DrawInstanced(4, 1, 0, 0);

	// 次のBlurVで読むためSRVに戻す
	ChangeResourceState(
		mBloomRT[1],
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
}

void BloomPass::PassBlurV()
{
	ID3D12GraphicsCommandList* cmdList = Direct3D::getCommandList();

	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		(float)mBloomWidth,
		(float)mBloomHeight,
		0.0f,
		1.0f
	};

	D3D12_RECT scissor =
	{
		0,
		0,
		(LONG)mBloomWidth,
		(LONG)mBloomHeight
	};

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// mBloomRT[0] に縦ブラー結果を書く
	ChangeResourceState(
		mBloomRT[0],
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	cmdList->OMSetRenderTargets(1, &mRTV[0], false, nullptr);
	cmdList->ClearRenderTargetView(mRTV[0], clearColor, 0, nullptr);

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);

	cmdList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());
	cmdList->SetPipelineState(mPSO_BlurV);

	// mBloomRT[1] を読む
	ID3D12DescriptorHeap* heaps[] = { mSRVHeap };
	cmdList->SetDescriptorHeaps(1, heaps);

	D3D12_GPU_DESCRIPTOR_HANDLE handle =
		mSRVHeap->GetGPUDescriptorHandleForHeapStart();

	UINT inc =
		Direct3D::getDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

	// 1番 = mBloomRT[1]
	handle.ptr += inc;

	cmdList->SetGraphicsRootDescriptorTable(0, handle);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmdList->IASetVertexBuffers(0, 1, Direct3D::getScreenVBView());

	cmdList->DrawInstanced(4, 1, 0, 0);

	// Compositeで読むためSRVに戻す
	ChangeResourceState(
		mBloomRT[0],
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
}


//============================================================
// Bloom用RT / RTV / SRV 作成
//============================================================
void BloomPass::CreateResources()
{
	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();

	mBloomWidth = (UINT)vnMainFrame::screenWidth / 4;
	mBloomHeight = (UINT)vnMainFrame::screenHeight / 4;

	if (mBloomWidth < 1)  mBloomWidth = 1;
	if (mBloomHeight < 1) mBloomHeight = 1;

	//============================================================
	// RTVヒープ作成
	// BloomRT[0], BloomRT[1] 用
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.NumDescriptors = 2;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 0;

		hr = device->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(&mRTVHeap)
		);
		assert(hr == S_OK);

		UINT inc =
			device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV
			);

		D3D12_CPU_DESCRIPTOR_HANDLE handle =
			mRTVHeap->GetCPUDescriptorHandleForHeapStart();

		for (int i = 0; i < 2; ++i)
		{
			mRTV[i] = handle;
			handle.ptr += inc;
		}
	}

	//============================================================
	// Bloom用RenderTexture作成
	//============================================================
	for (int i = 0; i < 2; ++i)
	{
		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Width = mBloomWidth;
		resDesc.Height = mBloomHeight;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Format = RenderFormat::SceneHDR;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = RenderFormat::SceneHDR;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 0.0f;

		hr = device->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(&mBloomRT[i])
		);
		assert(hr == S_OK);

		device->CreateRenderTargetView(
			mBloomRT[i],
			nullptr,
			mRTV[i]
		);
	}

	//============================================================
	// SRVヒープ作成
	// BloomRT[0], BloomRT[1] をシェーダーで読むため
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = 2;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;

		hr = device->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(&mSRVHeap)
		);
		assert(hr == S_OK);

		mSRVGpuStart = mSRVHeap->GetGPUDescriptorHandleForHeapStart();

		UINT inc =
			device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mSRVHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = mSRVHeap->GetGPUDescriptorHandleForHeapStart();

		for (int i = 0; i < 2; ++i)
		{
			mSRVCPU[i] = cpuHandle;
			mSRVGPU[i] = gpuHandle;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = RenderFormat::SceneHDR;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.PlaneSlice =0;
			srvDesc.Texture2D.ResourceMinLODClamp =0.0f;
			device->CreateShaderResourceView(
				mBloomRT[i],
				&srvDesc,
				cpuHandle
			);

			cpuHandle.ptr += inc;
			gpuHandle.ptr += inc;
		}
	}
}

//============================================================
// Bloom用PSO作成
//============================================================
void BloomPass::CreatePipelineStates()
{
	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();

	D3D12_INPUT_ELEMENT_DESC layout[2] =
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
		},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.InputLayout = { layout, _countof(layout) };

	//============================================================
	// 既存のスクリーン描画用RootSignatureを流用
	// t0 に1枚のSRVを渡す構成
	//============================================================
	desc.pRootSignature = Direct3D::getRootSignatureScr();

	desc.VS.pShaderBytecode =
		Shader::getVShader(Shader::eVertexShader::VS_Copy)->getCode();

	desc.VS.BytecodeLength =
		Shader::getVShader(Shader::eVertexShader::VS_Copy)->getLength();

	desc.RasterizerState.MultisampleEnable = false;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.DepthClipEnable = true;

	desc.BlendState.AlphaToCoverageEnable = false;
	desc.BlendState.IndependentBlendEnable = false;
	desc.BlendState.RenderTarget[0].BlendEnable = false;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	desc.DepthStencilState.DepthEnable = false;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.DepthStencilState.StencilEnable = false;

	desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	//============================================================
	// Bright
	//============================================================
	desc.PS.pShaderBytecode =
		Shader::getPShader(Shader::ePixelShader::PS_BloomBright)->getCode();

	desc.PS.BytecodeLength =
		Shader::getPShader(Shader::ePixelShader::PS_BloomBright)->getLength();

	hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mPSO_Bright)
	);
	assert(hr == S_OK);

	//============================================================
	// Blur Horizontal
	//============================================================
	desc.PS.pShaderBytecode =
		Shader::getPShader(Shader::ePixelShader::PS_BloomBlurH)->getCode();

	desc.PS.BytecodeLength =
		Shader::getPShader(Shader::ePixelShader::PS_BloomBlurH)->getLength();

	hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mPSO_BlurH)
	);
	assert(hr == S_OK);

	//============================================================
	// Blur Vertical
	//============================================================
	desc.PS.pShaderBytecode =
		Shader::getPShader(Shader::ePixelShader::PS_BloomBlurV)->getCode();

	desc.PS.BytecodeLength =
		Shader::getPShader(Shader::ePixelShader::PS_BloomBlurV)->getLength();

	hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mPSO_BlurV)
	);
	assert(hr == S_OK);
}
