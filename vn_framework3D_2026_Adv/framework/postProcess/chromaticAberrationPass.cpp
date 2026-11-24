#include "../../framework.h"
#include "../vn_environment.h"

#include "chromaticAberrationPass.h"

//============================================================
// 初期化
//============================================================
bool ChromaticAberrationPass::Initialize()
{
	if (mInitialized) return true;

	CreatePipelineState();

	mInitialized = true;
	return true;
}

//============================================================
// 終了処理
//============================================================
void ChromaticAberrationPass::Terminate()
{
	SAFE_RELEASE(mPSO);

	mInitialized = false;
}

//============================================================
// PSO作成
//============================================================
void ChromaticAberrationPass::CreatePipelineState()
{
	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();
	assert(device != nullptr);

	//============================================================
	// フルスクリーン四角形用頂点レイアウト
	// POSITION : float3
	// TEXCOORD : float2
	//============================================================
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
	// スクリーン描画用RootSignature
	// t0 に1枚のSRVを渡す構成
	//============================================================
	desc.pRootSignature = Direct3D::getRootSignatureScr();

	//============================================================
	// 頂点シェーダー
	// 画面全体に板を出すだけなのでVS_Copyを使い回す
	//============================================================
	desc.VS.pShaderBytecode =
		Shader::getVShader(Shader::eVertexShader::VS_Copy)->getCode();

	desc.VS.BytecodeLength =
		Shader::getVShader(Shader::eVertexShader::VS_Copy)->getLength();

	//============================================================
	// ピクセルシェーダー
	//============================================================
	desc.PS.pShaderBytecode =
		Shader::getPShader(Shader::ePixelShader::PS_ChromaticAberration)->getCode();

	desc.PS.BytecodeLength =
		Shader::getPShader(Shader::ePixelShader::PS_ChromaticAberration)->getLength();

	//============================================================
	// Rasterizer
	//============================================================
	desc.RasterizerState.MultisampleEnable = false;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.DepthClipEnable = true;
	desc.RasterizerState.FrontCounterClockwise = false;
	desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	desc.RasterizerState.AntialiasedLineEnable = false;
	desc.RasterizerState.ForcedSampleCount = 0;
	desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//============================================================
	// Blendなし
	// 色収差結果で出力先を上書きする
	//============================================================
	desc.BlendState.AlphaToCoverageEnable = false;
	desc.BlendState.IndependentBlendEnable = false;
	desc.BlendState.RenderTarget[0].BlendEnable = false;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// Depthなし
	//============================================================
	desc.DepthStencilState.DepthEnable = false;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.DepthStencilState.StencilEnable = false;

	//============================================================
	// 出力先は PingPongRT / BackBuffer
	//============================================================
	desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = RenderFormat::SceneHDR;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&mPSO)
	);
	assert(hr == S_OK);

	mPSO->SetName(L"ChromaticAberrationPass PSO");
}

//============================================================
// pScreenResource -> BackBuffer
//============================================================
void ChromaticAberrationPass::Apply()
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
}

//============================================================
// 任意のSRV -> 任意のRTV
//============================================================
void ChromaticAberrationPass::ApplyToRT(
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
	// PingPongRTならPingPongRTサイズ
	// BackBufferなら画面サイズ
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
	// outputRTVはPingPongRTでもBackBufferでもよい
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
	// RootSignature -> PSO -> DescriptorHeap -> DescriptorTable
	// の順番で設定
	//============================================================
	cmdList->SetGraphicsRootSignature(
		Direct3D::getRootSignatureScr()
	);

	//============================================================
    // rp[1] = b0 PostEffectParam
    //============================================================
	cmdList->SetGraphicsRootConstantBufferView(
		1,
		PostProcessManager::GetPostEffectParamAddress()
	);

	cmdList->SetPipelineState(mPSO);

	//============================================================
	// 入力SRVのDescriptorHeapをセット
	// pScreenResourceを読む場合: Direct3D::getScreenSRVHeap()
	// PingPongRTを読む場合: sPingPongRT.GetSRVHeap()
	//============================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		inputSRVHeap
	};
	cmdList->SetDescriptorHeaps(1, heaps);

	//============================================================
	// t0 に入力テクスチャを渡す
	//============================================================
	cmdList->SetGraphicsRootDescriptorTable(
		0,
		inputSRV
	);

	//============================================================
	// フルスクリーン四角形を描画
	//============================================================
	cmdList->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
	);

	cmdList->IASetVertexBuffers(
		0,
		1,
		Direct3D::getScreenVBView()
	);

	cmdList->DrawInstanced(4, 1, 0, 0);
}