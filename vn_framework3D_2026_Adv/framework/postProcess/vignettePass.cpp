#include "../../framework.h"
#include "../vn_environment.h"

#include "vignettePass.h"

bool VignettePass::Initialize()
{
	if (mInitialized) return true;

	CreatePipelineState();

	mInitialized = true;
	return true;
}

void VignettePass::Terminate()
{
	SAFE_RELEASE(mPSO);
	mInitialized = false;
}

void VignettePass::CreatePipelineState()
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
	desc.pRootSignature = Direct3D::getRootSignatureScr();

	desc.VS.pShaderBytecode =
		Shader::getVShader(Shader::eVertexShader::VS_Copy)->getCode();

	desc.VS.BytecodeLength =
		Shader::getVShader(Shader::eVertexShader::VS_Copy)->getLength();

	desc.PS.pShaderBytecode =
		Shader::getPShader(Shader::ePixelShader::PS_Vignette)->getCode();

	desc.PS.BytecodeLength =
		Shader::getPShader(Shader::ePixelShader::PS_Vignette)->getLength();

	desc.RasterizerState.MultisampleEnable = false;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.DepthClipEnable = true;

	desc.BlendState.AlphaToCoverageEnable = false;
	desc.BlendState.IndependentBlendEnable = false;
	desc.BlendState.RenderTarget[0].BlendEnable = false;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

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

	hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mPSO)
	);
	assert(hr == S_OK);
}

void VignettePass::Apply()
{
	if (!mInitialized) return;

	ApplyToRT(
		Direct3D::getScreenSRVHeap(),
		Direct3D::getScreenSRVHeap()->GetGPUDescriptorHandleForHeapStart(),
		Direct3D::getCurrentRTV(),
		(UINT)vnMainFrame::screenWidth,
		(UINT)vnMainFrame::screenHeight,
		true
	);

#if 0
	ID3D12GraphicsCommandList* cmdList =
		vnDirect3D::getCommandList();

	const UINT screenW = (UINT)vnMainFrame::screenWidth;
	const UINT screenH = (UINT)vnMainFrame::screenHeight;

	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		(float)screenW,
		(float)screenH,
		0.0f,
		1.0f
	};

	D3D12_RECT scissor =
	{
		0,
		0,
		(LONG)screenW,
		(LONG)screenH
	};

	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV =
		vnDirect3D::getCurrentRTV();

	cmdList->OMSetRenderTargets(1, &backBufferRTV, true, nullptr);

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);

	cmdList->SetGraphicsRootSignature(vnDirect3D::getRootSignatureScr());
	cmdList->SetPipelineState(mPSO);

	ID3D12DescriptorHeap* heaps[] =
	{
		vnDirect3D::getScreenSRVHeap()
	};
	cmdList->SetDescriptorHeaps(1, heaps);

	cmdList->SetGraphicsRootDescriptorTable(
		0,
		vnDirect3D::getScreenSRVHeap()->GetGPUDescriptorHandleForHeapStart()
	);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmdList->IASetVertexBuffers(0, 1, vnDirect3D::getScreenVBView());

	cmdList->DrawInstanced(4, 1, 0, 0);
#endif
}

void VignettePass::ApplyToRT(
	ID3D12DescriptorHeap* inputSRVHeap,
	D3D12_GPU_DESCRIPTOR_HANDLE inputSRV,
	D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
	UINT width,
	UINT height,
	bool clear
)
{
	if (!mInitialized) return;

	ID3D12GraphicsCommandList* cmdList =
		Direct3D::getCommandList();

	assert(cmdList != nullptr);
	assert(inputSRVHeap != nullptr);
	assert(mPSO != nullptr);
	assert(Direct3D::getRootSignatureScr() != nullptr);
	assert(Direct3D::getScreenVBView() != nullptr);

	//============================================================
	// 描画先サイズ
	// PingPongRT に描く場合は PingPongRT のサイズ
	// BackBuffer に描く場合は画面サイズ
	//============================================================
	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		(float)width,
		(float)height,
		0.0f,
		1.0f
	};

	D3D12_RECT scissor =
	{
		0,
		0,
		(LONG)width,
		(LONG)height
	};

	//============================================================
	// 出力先RTVを設定
	//============================================================
	cmdList->OMSetRenderTargets(
		1,
		&outputRTV,
		true,
		nullptr
	);

	//============================================================
	// 必要なら出力先をクリア
	//============================================================
	if (clear)
	{
		float clearColor[4] =
		{
			0.0f,
			0.0f,
			0.0f,
			1.0f
		};

		cmdList->ClearRenderTargetView(
			outputRTV,
			clearColor,
			0,
			nullptr
		);
	}

	//============================================================
	// Viewport / Scissor
	//============================================================
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);

	//============================================================
	// RootSignature
	// t0 に1枚のSRVを渡す構成
	//============================================================
	cmdList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());

	//============================================================
	// CBV rp[1] = b0 PostEffectParam
	//============================================================
	cmdList->SetGraphicsRootConstantBufferView(
		1,
		PostProcessManager::GetPostEffectParamAddress()
	);

	//============================================================
	// Vignette用PSO
	//============================================================
	cmdList->SetPipelineState(mPSO);

	//============================================================
	// 入力テクスチャのSRVヒープをセット
	// pScreenResourceを読む場合: Direct3D::getScreenSRVHeap()
	// PingPongRTを読む場合: sPingPongRT.GetSRVHeap()
	//============================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		inputSRVHeap
	};

	cmdList->SetDescriptorHeaps(1, heaps);

	//============================================================
	// t0 に入力SRVを渡す
	//============================================================
	cmdList->SetGraphicsRootDescriptorTable(
		0,
		inputSRV
	);

	//============================================================
	// フルスクリーン四角形を描画
	//============================================================
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	cmdList->IASetVertexBuffers(
		0,
		1,
		Direct3D::getScreenVBView()
	);

	cmdList->DrawInstanced(4, 1, 0, 0);
}