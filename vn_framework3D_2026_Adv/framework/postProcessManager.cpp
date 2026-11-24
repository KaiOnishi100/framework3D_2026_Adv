#include "../framework.h"
#include "vn_environment.h"
#include "postProcessManager.h"

#define DEBUG_PPM 0
#define DEBUG_SSS_MASK 0
#if DEBUG_PPM
int PostProcessManager::sMask =
PostProcessManager::Bloom |
PostProcessManager::ColorGrading |
PostProcessManager::Vignette |
PostProcessManager::Fxaa |
PostProcessManager::ChromaticAberration;

#endif // 0

int PostProcessManager::sMask = PostProcessManager::None;

BloomPass PostProcessManager::sBloomPass;
VignettePass PostProcessManager::sVignettePass;
ColorGradingPass PostProcessManager::sColorGradingPass;
PingPongPostRT PostProcessManager::sPingPongRT;
FxaaPass PostProcessManager::sFxaaPass;
ChromaticAberrationPass PostProcessManager::sChromaticAberrationPass;
ToneMappingPass PostProcessManager::sToneMappingPass;
LensFlarePass PostProcessManager::sLensFlarePass;
MotionVectorPass PostProcessManager::sMotionVectorPass;
MotionBlurPass PostProcessManager::sMotionBlurPass;
VolumetricFogPass PostProcessManager::sVolumetricFogPass;
SubsurfaceScatteringPass PostProcessManager::sSubsurfaceScatteringPass;
SSGIPass PostProcessManager::sSSGIPass;

ID3D12Resource* PostProcessManager::sPostEffectCB = nullptr;
stPostEffectParam* PostProcessManager::sMappedPostEffectParam = nullptr;

ID3D12PipelineState* PostProcessManager::sMotionVectorDebugPSO = nullptr;
ID3D12Resource* PostProcessManager::sMotionVectorDebugCB = nullptr;
XMFLOAT4* PostProcessManager::sMotionVectorDebugMapped = nullptr;

bool PostProcessManager::sMotionVectorDebugEnabled = false;

//各ポストエフェクトの初期値を設定
stPostEffectParam PostProcessManager::sPostEffectParam =
{
	//============================================================
	// ColorGrading
	//============================================================
	1.04f,  // brightness
	1.18f,  // contrast
	1.15f,  // saturation
	0.0f,

	//============================================================
	// Vignette
	//============================================================
	0.38f,  // vignetteStart
	0.80f,  // vignetteEnd
	0.35f,  // vignetteStrength
	0.0f,

	//============================================================
	// Chromatic Aberration
	//============================================================
	0.002f, // chromaticStrength
	{ 0.0f, 0.0f, 0.0f },

	//============================================================
	// Tone Mapping
	//============================================================
	0.0f,   // exposure
	0.5f,   // bloomIntensity
	2.2f,   // gamma
	0.0f    // bloomEnabled
};

bool PostProcessManager::Initialize()
{

	// PostEffectParam 定数バッファ作成　※各ポストプロセスPassより前に作っておく
	CreatePostEffectConstantBuffer();

	sPingPongRT.Initialize((UINT)vnMainFrame::screenWidth, (UINT)vnMainFrame::screenHeight, RenderFormat::SceneHDR);

	sBloomPass.Initialize();
	sVignettePass.Initialize();
	sColorGradingPass.Initialize();
	sFxaaPass.Initialize();
	sChromaticAberrationPass.Initialize();
	sLensFlarePass.Initialize();
	sToneMappingPass.Initialize();
	sMotionVectorPass.Initialize((UINT)vnMainFrame::screenWidth, (UINT)vnMainFrame::screenHeight);
	sMotionBlurPass.Initialize();
	sVolumetricFogPass.Initialize((UINT)vnMainFrame::screenWidth, (UINT)vnMainFrame::screenHeight);
	sSubsurfaceScatteringPass.Initialize((UINT)vnMainFrame::screenWidth, (UINT)vnMainFrame::screenHeight);
	sSSGIPass.Initialize((UINT)vnMainFrame::screenWidth, (UINT)vnMainFrame::screenHeight);
	if (!CreateMotionVectorDebugBuffer())
	{
		return false;
	}
	if (!CreateMotionVectorDebugPSO())
	{
		return false;
	}
	return true;
}

void PostProcessManager::Terminate()
{
	sFxaaPass.Terminate();
	sVignettePass.Terminate();
	sBloomPass.Terminate();
	sColorGradingPass.Terminate();
	sChromaticAberrationPass.Terminate();
	sLensFlarePass.Terminate();
	sToneMappingPass.Terminate();
	if (sMotionVectorDebugCB != nullptr && sMotionVectorDebugMapped != nullptr)
	{
		sMotionVectorDebugCB->Unmap(0, nullptr);
		sMotionVectorDebugMapped = nullptr;
	}
	SAFE_RELEASE(sMotionVectorDebugCB);
	SAFE_RELEASE(sMotionVectorDebugPSO);
	sMotionVectorPass.Terminate();
	sMotionBlurPass.Terminate();
	sVolumetricFogPass.Terminate();
	sSubsurfaceScatteringPass.Terminate();
	sSSGIPass.Terminate();
	//一番最後に消去
	sPingPongRT.Terminate();

	// 定数バッファ解放
	if (sPostEffectCB)
	{
		sPostEffectCB->Unmap(0, nullptr);
		sMappedPostEffectParam = nullptr;
	}

	SAFE_RELEASE(sPostEffectCB);
}

void PostProcessManager::Apply()
{

	if (IsEnabled(SSSDiffusion))
	{
		sSubsurfaceScatteringPass.GenerateMask();

#if DEBUG_SSS_MASK

		//========================================================
		// MaskだけをBackBufferへ表示
		//
		// Mask SRVではR ChannelをRGBへComponent Mappingしているため
		// PS_CopyでもGray Scaleとして確認できる
		//========================================================
		CopyToBackBuffer(
			sSubsurfaceScatteringPass.
			GetMaskSRVHeap(),

			sSubsurfaceScatteringPass.
			GetMaskSRV()
		);


		//========================================================
		// Debug中は他のPostProcessを行わない
		//========================================================
		return;

#endif
	}


	sMotionVectorPass.Generate();// Depthから画面全体のCamera Motionを生成

	sMotionVectorPass.BeginObjectMotion();// Velocity BufferへDynamic ObjectのVelocityを上書きする
	vnScene::renderMotionVector();// Scene内の移動Rigid Objectだけ再描画
	sMotionVectorPass.EndObjectMotion();// Velocity Buffer完成

	if (sMotionVectorDebugEnabled)
	{
		RenderMotionVectorDebug();

		return;
	}

	if (IsEnabled(SSGI))
	{
		Direct3D::TransitionMainDepthToSRV();

		sSSGIPass.Generate(
			Direct3D::getScreenResource(),
			Direct3D::getDepthStencilResource(),
			sMotionVectorPass.GetResource(),
			sMotionVectorPass.GetPreviousViewProjection()
		);

		//============================================================
		// SSGI内部Debug
		//============================================================
		const SSGIPass::DebugView debugView = sSSGIPass.GetDebugView();


		if (debugView != SSGIPass::DebugView::None && debugView != SSGIPass::DebugView::FinalComposite)
		{
			CopyToBackBuffer(
				sSSGIPass.GetDebugSRVHeap(),
				sSSGIPass.GetDebugSRV()
			);
			return;
		}
	}


	ID3D12Resource* currentResource = Direct3D::getScreenResource();

	assert(currentResource != nullptr);

	//============================================================
	// PingPongRTの読み書き状態を初期化
	// read  = 0
	// write = 1
	//============================================================
	sPingPongRT.Reset();

	//============================================================
	// 現在の入力テクスチャ
	// 最初の入力は pScreenResource
	// 1つPassを通すたびに入力をPingPongRTのRead側へ差し替える
	//============================================================
	ID3D12DescriptorHeap* currentSRVHeap =
		Direct3D::getScreenSRVHeap();

	D3D12_CPU_DESCRIPTOR_HANDLE currentSRVCPU =
		Direct3D::getScreenSRVCPUHandle();

	D3D12_GPU_DESCRIPTOR_HANDLE currentSRVGPU =
		Direct3D::getScreenSRVHeap()->GetGPUDescriptorHandleForHeapStart();

	bool usedPingPong = false;

	//============================================================
	// Screen-Space Global Illumination Composite
	//
	// Original HDR Scene
	//        +
	// Full Resolution SSGI
	//        ↓
	// PingPong Write
	//============================================================
	if (IsEnabled(SSGI))
	{
		//========================================================
		// Output
		//========================================================
		sPingPongRT.TransitionWriteToRTV();

		//========================================================
		// Scene + Indirect Diffuse
		//========================================================
		sSSGIPass.ApplyCompositeToRT(
			currentResource,
			sPingPongRT.GetWriteRTV(),
			sPingPongRT.GetWidth(),
			sPingPongRT.GetHeight()
		);


		//========================================================
		// 次Passから読めるようSRVへ
		//========================================================
		sPingPongRT.TransitionWriteToSRV();

		//========================================================
		// Write → Read
		//========================================================
		sPingPongRT.Swap();

		//========================================================
		// Current Input更新
		//========================================================
		currentSRVHeap = sPingPongRT.GetSRVHeap();
		currentSRVCPU = sPingPongRT.GetReadSRVCPU();
		currentSRVGPU = sPingPongRT.GetReadSRV();
		currentResource = sPingPongRT.GetReadResource();

		usedPingPong = true;

		//============================================================
		// Final Composite Debug
		// SSGIをHDR Sceneへ加算した直後だけを見る
		// SSS / MotionBlur / Fog / ColorGradingなどは
		// まだ通していない状態
		//============================================================
		if (sSSGIPass.GetDebugView() == SSGIPass::DebugView::FinalComposite)
		{
			CopyToBackBuffer(
				currentSRVHeap,
				currentSRVGPU
			);

			return;
		}
	}


	//============================================================
	// Screen-Space Subsurface Scattering
	//
	// Main HDR
	//      +
	// SSS Mask
	//      +
	// Main Depth
	//      ↓
	// Depth-aware RGB Diffusion
	//      ↓
	// PingPong Write
	//============================================================
	if (IsEnabled(SSSDiffusion))
	{
		//========================================================
		// 現在入力がPingPongの場合にも対応
		//========================================================
		if (usedPingPong)
		{
			sPingPongRT.TransitionReadToSRV();
		}


		//========================================================
		// Output
		//========================================================
		sPingPongRT.TransitionWriteToRTV();


		//========================================================
		// SSS Diffusion
		//========================================================
		sSubsurfaceScatteringPass.ApplyToRT(
			currentResource,
			sMotionVectorPass.GetResource(),
			Direct3D::getDepthStencilResource(),
			sPingPongRT.GetWriteRTV(),
			sPingPongRT.GetWidth(),
			sPingPongRT.GetHeight()
		);


		//========================================================
		// 次Passから読める状態へ
		//========================================================
		sPingPongRT.TransitionWriteToSRV();


		//========================================================
		// Write → Read
		//========================================================
		sPingPongRT.Swap();

		// SSS Debug View
		if (sSubsurfaceScatteringPass.IsDebugViewEnabled())
		{
			CopyToBackBuffer(
				sSubsurfaceScatteringPass.GetDebugSRVHeap(),
				sSubsurfaceScatteringPass.GetDebugSRV()
			);
			return;
		}

		currentSRVHeap = sPingPongRT.GetSRVHeap();
		currentSRVCPU = sPingPongRT.GetReadSRVCPU();
		currentSRVGPU = sPingPongRT.GetReadSRV();
		currentResource = sPingPongRT.GetReadResource();

		usedPingPong = true;
	}

	//============================================================
	// Motion Blur
	//
	// HDR Scene
	//      +
	// Motion Vector
	//      ↓
	// PingPong Write
	//============================================================
	if (IsEnabled(MotionBlur))
	{
		//========================================================
		// すでにPingPongを使っているケースにも対応
		//========================================================
		if (usedPingPong)
		{
			sPingPongRT.TransitionReadToSRV();
		}


		//========================================================
		// 出力先をRenderTargetへ
		//========================================================
		sPingPongRT.TransitionWriteToRTV();


		//========================================================
		// Motion Blur
		//
		// t0: 現在のHDR Scene
		// t1: Motion Vector
		//========================================================
		sMotionBlurPass.ApplyToRT(
			currentResource,
			sMotionVectorPass.GetResource(),
			Direct3D::getDepthStencilResource(),
			sPingPongRT.GetWriteRTV(),
			sPingPongRT.GetWidth(),
			sPingPongRT.GetHeight()
		);


		//========================================================
		// 次Passから読めるようSRVへ戻す
		//========================================================
		sPingPongRT.TransitionWriteToSRV();

		//========================================================
		// Write → Read
		//========================================================
		sPingPongRT.Swap();

		//========================================================
		// Motion Blur結果を次Passの入力にする
		//========================================================
		currentSRVHeap = sPingPongRT.GetSRVHeap();
		currentSRVCPU = sPingPongRT.GetReadSRVCPU();
		currentSRVGPU = sPingPongRT.GetReadSRV();
		currentResource = sPingPongRT.GetReadResource();

		usedPingPong = true;
	}




	//============================================================
	// Volumetric Fog
	//
	// HDR Scene
	//      +
	// Main Depth
	//      ↓
	// Half Resolution Ray March
	//      ↓
	// Full Resolution Composite
	//============================================================
	if (IsEnabled(VolumetricFog))
	{
		//========================================================
		// 現在のPingPong ReadをSRVへ
		//========================================================
		if (usedPingPong)
		{
			sPingPongRT.TransitionReadToSRV();
		}

		//========================================================
		// Output
		//========================================================
		sPingPongRT.TransitionWriteToRTV();

		//========================================================
		// Main DepthをFog Shaderから読む
		//========================================================
		Direct3D::TransitionMainDepthToSRV();


		//========================================================
		// Volumetric Fog
		// input: currentResource
		// depth: Main Depth
		// output: PingPong Write
		//========================================================
		sVolumetricFogPass.ApplyToRT(
			currentResource,
			Direct3D::getDepthStencilResource(),
			sPingPongRT.GetWriteRTV(),
			sPingPongRT.GetWidth(),
			sPingPongRT.GetHeight()
		);


		//========================================================
		// 次Passから読めるようSRVへ戻す
		//========================================================
		sPingPongRT.TransitionWriteToSRV();

		//========================================================
		// Write → Read
		//========================================================
		sPingPongRT.Swap();

		currentSRVHeap = sPingPongRT.GetSRVHeap();
		currentSRVCPU = sPingPongRT.GetReadSRVCPU();
		currentSRVGPU = sPingPongRT.GetReadSRV();
		currentResource = sPingPongRT.GetReadResource();

		usedPingPong = true;
	}

	//============================================================
	// ColorGrading
	// 入力 : currentSRV
	// 出力 : PingPongRT Write
	//============================================================
	if (IsEnabled(ColorGrading))
	{
		// 書き込み先をRTV状態にする
		sPingPongRT.TransitionWriteToRTV();

		// 現在の入力を読んで、PingPongRTのWrite側へ書く
		sColorGradingPass.ApplyToRT(
			currentSRVHeap,
			currentSRVGPU,
			sPingPongRT.GetWriteRTV(),
			sPingPongRT.GetWidth(),
			sPingPongRT.GetHeight(),
			true
		);

		// 書いたRTを次のPassで読めるようにSRVへ戻す
		sPingPongRT.TransitionWriteToSRV();

		// 今書いたRTをRead側にする
		sPingPongRT.Swap();

		// 次の入力をPingPongRTのRead側にする
		currentSRVHeap = sPingPongRT.GetSRVHeap();
		currentSRVCPU = sPingPongRT.GetReadSRVCPU();
		currentSRVGPU = sPingPongRT.GetReadSRV();
		currentResource = sPingPongRT.GetReadResource();
		usedPingPong = true;
	}

	//============================================================
	// Vignette
	//
	// 入力 : currentSRV
	// 出力 : PingPongRT Write
	//
	// ColorGradingがONなら:
	//   ColorGrading結果 -> Vignette
	//
	// ColorGradingがOFFなら:
	//   pScreenResource -> Vignette
	//============================================================
	if (IsEnabled(Vignette))
	{
		// 念のため、現在のRead側をSRV状態にする
		// pScreenResource入力の場合は関係ないが、
		// PingPongRT入力の場合に安全
		if (usedPingPong)
		{
			sPingPongRT.TransitionReadToSRV();
		}

		// 書き込み先をRTV状態にする
		sPingPongRT.TransitionWriteToRTV();

		// 現在の入力を読んで、PingPongRTのWrite側へ書く
		sVignettePass.ApplyToRT(
			currentSRVHeap,
			currentSRVGPU,
			sPingPongRT.GetWriteRTV(),
			sPingPongRT.GetWidth(),
			sPingPongRT.GetHeight(),
			true
		);

		// 書いたRTを次で読めるようにSRVへ戻す
		sPingPongRT.TransitionWriteToSRV();

		// 今書いたRTをRead側にする
		sPingPongRT.Swap();

		// 最終入力をPingPongRTのRead側にする
		currentSRVHeap = sPingPongRT.GetSRVHeap();
		currentSRVCPU = sPingPongRT.GetReadSRVCPU();
		currentSRVGPU = sPingPongRT.GetReadSRV();
		currentResource = sPingPongRT.GetReadResource();
		usedPingPong = true;
	}

	//============================================================
	// Chromatic Aberration
	//
	// 入力 : currentSRV
	// 出力 : PingPongRT Write
	//
	// Vignette後に入れることで、
	// 画面端の暗さ + 色ズレが乗り、演出感が出る
	// FXAAの前に入れることで、ズレた輪郭を少しなじませられる
	//============================================================
	if (IsEnabled(ChromaticAberration))
	{
		//============================================================
		// すでにPingPongRTを使っている場合、
		// 現在のRead側をSRV状態にしておく
		//============================================================
		if (usedPingPong)
		{
			sPingPongRT.TransitionReadToSRV();
		}

		//============================================================
		// 書き込み先をRTV状態にする
		//============================================================
		sPingPongRT.TransitionWriteToRTV();

		//============================================================
		// 現在の入力を読んで、色収差結果をPingPongRTへ書く
		//============================================================
		sChromaticAberrationPass.ApplyToRT(
			currentSRVHeap,
			currentSRVGPU,
			sPingPongRT.GetWriteRTV(),
			sPingPongRT.GetWidth(),
			sPingPongRT.GetHeight(),
			true
		);

		//============================================================
		// 書いたRTを次で読めるようにSRVへ戻す
		//============================================================
		sPingPongRT.TransitionWriteToSRV();

		//============================================================
		// 今書いたRTをRead側にする
		//============================================================
		sPingPongRT.Swap();

		//============================================================
		// 次の入力をPingPongRTのRead側にする
		//============================================================
		currentSRVHeap = sPingPongRT.GetSRVHeap();
		currentSRVCPU = sPingPongRT.GetReadSRVCPU();
		currentSRVGPU = sPingPongRT.GetReadSRV();
		currentResource = sPingPongRT.GetReadResource();
		usedPingPong = true;
	}

	//============================================================
	// FXAA
	//
	// 入力 : currentSRV
	// 出力 : PingPongRT Write
	//
	// ColorGrading / Vignette の結果に対して、
	// 最後に輪郭のギザギザを軽減する
	//============================================================
	if (IsEnabled(Fxaa))
	{
		// すでにPingPongRTを使っている場合は、
		// 現在のRead側をSRV状態にしておく
		if (usedPingPong)
		{
			sPingPongRT.TransitionReadToSRV();
		}

		// 書き込み先をRTV状態にする
		sPingPongRT.TransitionWriteToRTV();

		// 現在の入力を読んで、FXAA結果をPingPongRTへ書く
		sFxaaPass.ApplyToRT(
			currentSRVHeap,
			currentSRVGPU,
			sPingPongRT.GetWriteRTV(),
			sPingPongRT.GetWidth(),
			sPingPongRT.GetHeight(),
			true
		);

		// 書いたRTを次で読めるようにSRVへ戻す
		sPingPongRT.TransitionWriteToSRV();

		// 今書いたRTをRead側にする
		sPingPongRT.Swap();

		// 最終入力をPingPongRTのRead側にする
		currentSRVHeap = sPingPongRT.GetSRVHeap();
		currentSRVCPU = sPingPongRT.GetReadSRVCPU();
		currentSRVGPU = sPingPongRT.GetReadSRV();
		currentResource = sPingPongRT.GetReadResource();
		usedPingPong = true;
	}

	//============================================================
	// Lens Flare
	// HDR SceneへFlareを追加する
	//============================================================
	if (IsEnabled(LensFlare))
	{
		//============================================================
		// すでにPingPongを使用している場合、
		// Read側をSRV状態にしておく
		//============================================================
		if (usedPingPong)
		{
			sPingPongRT.TransitionReadToSRV();
		}

		//============================================================
		// Lens Flareの書き込み先
		//============================================================
		sPingPongRT.TransitionWriteToRTV();

		Direct3D::TransitionMainDepthToSRV();// Lens FlareからMain Depthを読む

		//============================================================
		// 現在のHDR Scene
		//     +
		// Lens Flare
		// をWrite側へ出力
		//============================================================
		sLensFlarePass.ApplyToRT(
			currentResource,
			sPingPongRT.GetWriteRTV(),
			sPingPongRT.GetWidth(),
			sPingPongRT.GetHeight()
		);

		//============================================================
		// 次のBloom Passから読める状態へ変更
		//============================================================
		sPingPongRT.TransitionWriteToSRV();

		sPingPongRT.Swap();

		//============================================================
		// 今書いたLens Flare結果を次の入力にする
		//============================================================
		currentSRVHeap = sPingPongRT.GetSRVHeap();
		currentSRVCPU = sPingPongRT.GetReadSRVCPU();
		currentSRVGPU = sPingPongRT.GetReadSRV();
		currentResource = sPingPongRT.GetReadResource();

		usedPingPong = true;
	}
	//============================================================
	// HDR Bloom生成
	//============================================================
	ID3D12Resource* bloomResource = currentResource;

	if (IsEnabled(Bloom))
	{
		sBloomPass.Generate(currentSRVHeap, currentSRVGPU);
		bloomResource = sBloomPass.GetResultResource();
		sPostEffectParam.bloomEnabled = 1.0f;
	}
	else
	{
		//========================================================
		// t1には有効なDescriptorが必要なので、
		// Bloom無効時はScene SRVをダミーとして渡す
		// Shader側ではbloomEnabledが0なので加算されない
		//========================================================
		sPostEffectParam.bloomEnabled = 0.0f;
	}

	UpdatePostEffectConstantBuffer();

	//============================================================
	// HDR Scene + Bloom
	//          ↓
	// ACES Tone Mapping
	//          ↓
	// SDR BackBuffer
	//============================================================
	sToneMappingPass.Apply(
		currentResource,
		bloomResource,
		GetPostEffectParamAddress());

}

void PostProcessManager::CopyToBackBuffer(
	ID3D12DescriptorHeap* inputSRVHeap,
	D3D12_GPU_DESCRIPTOR_HANDLE inputSRV
)
{
	ID3D12GraphicsCommandList* cmdList =
		Direct3D::getCommandList();

	assert(cmdList != nullptr);
	assert(inputSRVHeap != nullptr);
	assert(Direct3D::getRootSignatureScr() != nullptr);
	assert(Direct3D::getScreenVBView() != nullptr);

	//============================================================
	// BackBufferのサイズ
	//============================================================
	const UINT screenW = (UINT)vnMainFrame::screenWidth;
	const UINT screenH = (UINT)vnMainFrame::screenHeight;

	//============================================================
	// 画面全体に描画するViewport
	//============================================================
	D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		(float)screenW,
		(float)screenH,
		0.0f,
		1.0f
	};

	//============================================================
	// 描画範囲
	//============================================================
	D3D12_RECT scissor =
	{
		0,
		0,
		(LONG)screenW,
		(LONG)screenH
	};

	//============================================================
	// 現在のBackBuffer RTVを取得
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV =
		Direct3D::getCurrentRTV();

	//============================================================
	// 描画先をBackBufferにする
	//============================================================
	cmdList->OMSetRenderTargets(
		1,
		&backBufferRTV,
		true,
		nullptr
	);

	//============================================================
	// BackBufferをクリア
	//============================================================
	cmdList->ClearRenderTargetView(
		backBufferRTV,
		vnMainFrame::clearColor,
		0,
		nullptr
	);

	//============================================================
	// Viewport / Scissor設定
	//============================================================
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);

	//============================================================
	// スクリーン描画用RootSignature
	// t0に1枚のSRVを渡す構成
	//============================================================
	cmdList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());

	//============================================================
	// Copy用PSO
	// 入力テクスチャをそのままBackBufferへ描く
	//============================================================
	cmdList->SetPipelineState(Direct3D::getScreenPipelineState(Direct3D::Copy));

	//============================================================
    // rp[1] = b0 PostEffectParam unused
    //============================================================
	cmdList->SetGraphicsRootConstantBufferView(
		1,
		PostProcessManager::GetPostEffectParamAddress()
	);

	//============================================================
	// 入力SRVのDescriptorHeapをセット
	//
	// pScreenResourceを読む場合:
	//   vnDirect3D::getScreenSRVHeap()
	//
	// PingPongRTを読む場合:
	//   sPingPongRT.GetSRVHeap()
	//============================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		inputSRVHeap
	};

	cmdList->SetDescriptorHeaps(1, heaps);

	//============================================================
	// t0に入力SRVを渡す
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

bool PostProcessManager::CreatePostEffectConstantBuffer()
{
	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();
	assert(device != nullptr);

	//============================================================
	// ConstantBufferは256byteアラインが必要
	//============================================================
	UINT bufferSize =
		(sizeof(stPostEffectParam) + 255) & ~255;

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProp.CreationNodeMask = 1;
	heapProp.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Alignment = 0;
	resDesc.Width = bufferSize;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sPostEffectCB)
	);
	assert(hr == S_OK);

	sPostEffectCB->SetName(L"PostEffectParam ConstantBuffer");

	//============================================================
	// 毎フレーム値を書き換えられるようにする
	//============================================================
	hr = sPostEffectCB->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&sMappedPostEffectParam)
	);
	assert(hr == S_OK);

	UpdatePostEffectConstantBuffer();

	return true;
}

void PostProcessManager::UpdatePostEffectConstantBuffer()
{
	if (sMappedPostEffectParam == nullptr)
	{
		return;
	}

	//============================================================
	// CPU側の値をGPUが読む定数バッファへコピー
	//============================================================
	*sMappedPostEffectParam = sPostEffectParam;
}

bool PostProcessManager::CreateMotionVectorDebugBuffer()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// CBVは256byte Alignmentが必要
	//============================================================
	const UINT bufferSize = 256u;


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


	const HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sMotionVectorDebugCB)
	);


	if (FAILED(hr))
	{
		return false;
	}


	//============================================================
	// CPUから毎Frame値を書き込めるようMap
	//============================================================
	if (FAILED(sMotionVectorDebugCB->Map(
		0u,
		nullptr,
		reinterpret_cast<void**>(&sMotionVectorDebugMapped))))
	{
		return false;
	}


	//============================================================
	// 初期倍率
	//============================================================
	*sMotionVectorDebugMapped =
		XMFLOAT4(
			100.0f,
			0.0f,
			0.0f,
			0.0f
		);


	return true;
}

bool PostProcessManager::CreateMotionVectorDebugPSO()
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}


	//============================================================
	// VS_Copy用Input Layout
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
	auto* pixelShader =Shader::getPShader(Shader::ePixelShader::PS_MotionVectorDebug);


	if (vertexShader == nullptr || pixelShader == nullptr)
	{
		return false;
	}


	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	desc.InputLayout =
	{
		layout,_countof(layout)
	};


	//============================================================
	// Screen Root Signature
	//============================================================
	desc.pRootSignature = Direct3D::getRootSignatureScr();


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
	// Full Screen DebugなのでDepthなし
	//============================================================
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	//============================================================
	// Full Screen Quad
	//============================================================
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//============================================================
	// 最終表示なのでBackBuffer Format
	//============================================================
	desc.NumRenderTargets = 1u;
	desc.RTVFormats[0] = RenderFormat::BackBuffer;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1u;

	const HRESULT hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&sMotionVectorDebugPSO)
	);


	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

void PostProcessManager::RenderMotionVectorDebug()
{
	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	if (commandList == nullptr || sMotionVectorDebugPSO == nullptr)
	{
		return;
	}


	//============================================================
	// BackBufferへ直接描画する
	//============================================================
	const D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = Direct3D::getCurrentRTV();

	commandList->OMSetRenderTargets(
		1u,
		&backBufferRTV,
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


	commandList->RSSetViewports(
		1u,
		&viewport
	);

	commandList->RSSetScissorRects(
		1u,
		&scissor
	);


	//============================================================
	// Screen Root Signature
	//============================================================
	commandList->SetGraphicsRootSignature(Direct3D::getRootSignatureScr());
	commandList->SetPipelineState(sMotionVectorDebugPSO);


	//============================================================
	// Motion Vector SRV Heap
	//============================================================
	ID3D12DescriptorHeap* heaps[] =
	{
		sMotionVectorPass.GetSRVHeap()
	};


	commandList->SetDescriptorHeaps(
		1u,
		heaps
	);


	//============================================================
	// Root 0
	//
	// t0 = Motion Vector
	//============================================================
	commandList->SetGraphicsRootDescriptorTable(
		0u,
		sMotionVectorPass.GetSRVGPU()
	);


	//============================================================
	// Root 1
	//
	// b0 = Debug Parameter
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		1u,
		sMotionVectorDebugCB->GetGPUVirtualAddress()
	);


	//============================================================
	// Screen Quad
	//============================================================
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	commandList->IASetVertexBuffers(
		0u,
		1u,
		Direct3D::getScreenVBView()
	);


	commandList->DrawInstanced(4u, 1u, 0u, 0u);
}

void PostProcessManager::Enable(eEffectMask mask)
{
	sMask |= mask;
}

void PostProcessManager::Disable(eEffectMask mask)
{
#if defined(_DEBUG)

	// VolumetricFogがDisableされた瞬間を検出
	//if ((mask & VolumetricFog) != 0)
	//{
	//	OutputDebugStringW(
	//		L"[PostProcessManager] "
	//		L"!!! VolumetricFog DISABLE CALLED !!!\n"
	//	);

	//	
	//	// Visual StudioのDebugger使用中なら
	//	// ここで停止させる
	//	//
	//	// Call Stackを見ることで
	//	//
	//	// PostProcessManager::Disable
	//	//        ↑
	//	// PostProcessDebugInspector::TogglePostEffect
	//	//        ↑
	//	// PostProcessDebugInspector::UpdatePostEffect
	//	//
	//	// 呼び出し元を完全に特定できる
	//	
	//	__debugbreak();
	//}

#endif

	sMask &= ~mask;
	if ((mask & SSSDiffusion) != 0)
	{
		sSubsurfaceScatteringPass.ResetTemporalHistory();
	}
	if ((mask & SSGI) != 0)
	{
		sSSGIPass.ResetTemporalHistory();
	}
	if ((mask & VolumetricFog) != 0)
	{
		sVolumetricFogPass.ResetTemporalHistory();
	}

}

void PostProcessManager::AllDisable()
{
	sMask = PostProcessManager::None;
	sSubsurfaceScatteringPass.ResetTemporalHistory();
	sVolumetricFogPass.ResetTemporalHistory();
	sSSGIPass.ResetTemporalHistory();
}

bool PostProcessManager::IsEnabled(eEffectMask mask)
{
	return (sMask & mask) != 0;
}

void PostProcessManager::SetBrightness(float value)
{
	sPostEffectParam.brightness = value;
	UpdatePostEffectConstantBuffer();
}

void PostProcessManager::SetContrast(float value)
{
	sPostEffectParam.contrast = value;
	UpdatePostEffectConstantBuffer();
}

void PostProcessManager::SetSaturation(float value)
{
	sPostEffectParam.saturation = value;
	UpdatePostEffectConstantBuffer();
}

void PostProcessManager::SetVignette(float start, float end, float strength)
{
	sPostEffectParam.vignetteStart = start;
	sPostEffectParam.vignetteEnd = end;
	sPostEffectParam.vignetteStrength = strength;

	UpdatePostEffectConstantBuffer();
}

void PostProcessManager::SetVignetteStrength(float strength)
{
	sPostEffectParam.vignetteStrength = strength;
	UpdatePostEffectConstantBuffer();
}

void PostProcessManager::SetChromaticStrength(float strength)
{
	sPostEffectParam.chromaticStrength = strength;
	UpdatePostEffectConstantBuffer();
}

void PostProcessManager::SetExposure(float exposure)
{
	sPostEffectParam.exposure = exposure;
	UpdatePostEffectConstantBuffer();
}

void PostProcessManager::SetBloomIntensity(float intensity)
{
	sPostEffectParam.bloomIntensity = (std::max)(intensity, 0.0f);
	UpdatePostEffectConstantBuffer();
}

void PostProcessManager::SetGamma(float gamma)
{
	sPostEffectParam.gamma =(std::max)(gamma,0.001f);
	UpdatePostEffectConstantBuffer();
}

void PostProcessManager::SetDeltaTime(float deltaTime)
{
	sLensFlarePass.SetDeltaTime(deltaTime);
}

void PostProcessManager::SetLensFlareEdgeFade(float width)
{
	sLensFlarePass.SetEdgeFadeWidth(width);
}

void PostProcessManager::SetLensFlareAngularFade(float start, float end)
{
	sLensFlarePass.SetAngularFade(start, end);
}

void PostProcessManager::SetMotionBlurStrength(float strength)
{
	sMotionBlurPass.SetStrength(strength);
}

void PostProcessManager::SetMotionBlurMaxPixels(float pixels)
{
	sMotionBlurPass.SetMaxBlurPixels(pixels);
}

void PostProcessManager::SetMotionBlurSampleCount(int count)
{
	sMotionBlurPass.SetSampleCount(count);
}

void PostProcessManager::SetMotionBlurDepthThreshold(float threshold)
{
	sMotionBlurPass.SetDepthThreshold(threshold);
}

void PostProcessManager::SetMotionBlurVelocityThreshold(float threshold)
{
	sMotionBlurPass.SetVelocityThreshold(threshold);
}

void PostProcessManager::SetVolumetricFogDensity(float density)
{
	sVolumetricFogPass.SetDensity(density);
}

void PostProcessManager::SetVolumetricFogMaxDistance(float distance)
{
	sVolumetricFogPass.SetMaxDistance(distance);
}

void PostProcessManager::SetVolumetricFogColor(float r, float g, float b)
{
	sVolumetricFogPass.SetColor(r, g, b);
}

void PostProcessManager::SetVolumetricFogAnisotropy(float anisotropy)
{
	sVolumetricFogPass.SetAnisotropy(anisotropy);
}

void PostProcessManager::SetVolumetricFogHeight(float baseHeight, float heightFalloff)
{
	sVolumetricFogPass.SetHeightFog(baseHeight,heightFalloff);
}

void PostProcessManager::SetVolumetricFogScatteringAlbedo(float albedo)
{
	sVolumetricFogPass.SetScatteringAlbedo(albedo);
}

void PostProcessManager::SetVolumetricFogDirectionalStrength(float strength)
{
	sVolumetricFogPass.SetDirectionalStrength(strength);
}

void PostProcessManager::SetVolumetricFogAmbientStrength(float strength)
{
	sVolumetricFogPass.SetAmbientStrength(strength);
}

void PostProcessManager::SetVolumetricFogSpotLightStrength(float strength)
{
	sVolumetricFogPass.SetSpotLightStrength(strength);
}

void PostProcessManager::SetVolumetricFogPointLightStrength(float strength)
{
	sVolumetricFogPass.SetPointLightStrength(strength);
}

void PostProcessManager::SetVolumetricFogSampleCount(int count)
{
	sVolumetricFogPass.SetSampleCount(count);
}

void PostProcessManager::SetVolumetricFogShadowStrength(float strength)
{
	sVolumetricFogPass.SetShadowStrength(strength);
}

void PostProcessManager::SetVolumetricFogShadowBias(float bias)
{
	sVolumetricFogPass.SetShadowBias(bias);
}

void PostProcessManager::SetVolumetricFogTemporalEnabled(bool enabled)
{
	sVolumetricFogPass.SetTemporalEnabled(enabled);
}

void PostProcessManager::SetVolumetricFogTemporalHistoryWeight(float weight)
{
	sVolumetricFogPass.SetTemporalHistoryWeight(weight);
}

void PostProcessManager::SetVolumetricFogTemporalResponse(float response)
{
	sVolumetricFogPass.SetTemporalResponse(response);
}

void PostProcessManager::ResetVolumetricFogTemporalHistory()
{
	sVolumetricFogPass.ResetTemporalHistory();
}

void PostProcessManager::SetVolumetricFogDepthAwareUpsamplingEnabled(bool enabled)
{
	sVolumetricFogPass.SetDepthAwareUpsamplingEnabled(enabled);
}

void PostProcessManager::SetVolumetricFogUpsampleDepthFalloff(float falloff)
{
	sVolumetricFogPass.SetUpsampleDepthFalloff(falloff);
}

void PostProcessManager::SetVolumetricFogBlueNoiseEnabled(bool enabled)
{
	sVolumetricFogPass.SetBlueNoiseEnabled(enabled);
}

void PostProcessManager::SetSSSDiffusionStrength(float strength)
{
	sSubsurfaceScatteringPass.SetDiffusionStrength(strength);
}

void PostProcessManager::SetSSSBlurRadius(int radius)
{
	sSubsurfaceScatteringPass.SetBlurRadius(radius);
}

void PostProcessManager::SetSSSDepthFalloff(float falloff)
{
	sSubsurfaceScatteringPass.SetDepthFalloff(falloff);
}

void PostProcessManager::SetSSSScatteringProfile(float red, float green, float blue)
{
	sSubsurfaceScatteringPass.SetScatteringProfile(red, green, blue);
}

void PostProcessManager::SetSSSSampleQuality(float quality)
{
	sSubsurfaceScatteringPass.SetSampleQuality(quality);
}

void PostProcessManager::SetSSSWorldSpaceRadius(float radius)
{
	sSubsurfaceScatteringPass.SetWorldSpaceRadius(radius);
}

void PostProcessManager::SetSSSTemporalEnabled(bool enabled)
{
	sSubsurfaceScatteringPass.SetTemporalEnabled(enabled);
}

void PostProcessManager::SetSSSTemporalHistoryWeight(float weight)
{
	sSubsurfaceScatteringPass.SetTemporalHistoryWeight(weight);
}

void PostProcessManager::SetSSSTemporalDepthThreshold(float threshold)
{
	sSubsurfaceScatteringPass.SetTemporalDepthThreshold(threshold);
}

void PostProcessManager::SetSSSTemporalMotionResponse(float response)
{
	sSubsurfaceScatteringPass.SetTemporalMotionResponse(response);
}

void PostProcessManager::ResetSSSTemporalHistory()
{
	sSubsurfaceScatteringPass.ResetTemporalHistory();
}

void PostProcessManager::SetSSSDebugView(SubsurfaceScatteringPass::DebugView view)
{
	sSubsurfaceScatteringPass.SetDebugView(view);
}

void PostProcessManager::SetSSGIRayCount(int count)
{
	sSSGIPass.SetRayCount(count);
}

void PostProcessManager::SetSSGIStepCount(int count)
{
	sSSGIPass.SetStepCount(count);
}

void PostProcessManager::SetSSGIMaxDistance(float distance)
{
	sSSGIPass.SetMaxDistance(distance);
}

void PostProcessManager::SetSSGIThickness(float thickness)
{
	sSSGIPass.SetThickness(thickness);
}

void PostProcessManager::SetSSGINormalBias(float bias)
{
	sSSGIPass.SetNormalBias(bias);
}

void PostProcessManager::SetSSGIIntensity(float intensity)
{
	sSSGIPass.SetIntensity(intensity);
}

void PostProcessManager::SetSSGIFilterRadius(int radius)
{
	sSSGIPass.SetFilterRadius(radius);
}

void PostProcessManager::SetSSGIFilterDepthFalloff(float falloff)
{
	sSSGIPass.SetFilterDepthFalloff(falloff);
}

void PostProcessManager::SetSSGIFilterNormalPower(float power)
{
	sSSGIPass.SetFilterNormalPower(power);
}

void PostProcessManager::SetSSGIFilterConfidenceMin(float value)
{
	sSSGIPass.SetFilterConfidenceMin(value);
}

void PostProcessManager::SetSSGITemporalEnabled(bool enabled)
{
	sSSGIPass.SetTemporalEnabled(enabled);
}

void PostProcessManager::SetSSGITemporalHistoryWeight(float weight)
{
	sSSGIPass.SetTemporalHistoryWeight(weight);
}

void PostProcessManager::SetSSGITemporalWorldPositionThreshold(float threshold)
{
	sSSGIPass.SetTemporalWorldPositionThreshold(threshold);
}

void PostProcessManager::SetSSGITemporalNormalThreshold(float threshold)
{
	sSSGIPass.SetTemporalNormalThreshold(threshold);
}

void PostProcessManager::SetSSGITemporalMotionResponse(float response)
{
	sSSGIPass.SetTemporalMotionResponse(response);
}

void PostProcessManager::SetSSGITemporalClampExpand(float expand)
{
	sSSGIPass.SetTemporalClampExpand(expand);
}

void PostProcessManager::ResetSSGITemporalHistory()
{
	sSSGIPass.ResetTemporalHistory();
}

void PostProcessManager::SetSSGIUpsampleDepthFalloff(float falloff)
{
	sSSGIPass.SetUpsampleDepthFalloff(falloff);
}

void PostProcessManager::SetSSGIUpsampleNormalPower(float power)
{
	sSSGIPass.SetUpsampleNormalPower(power);
}

void PostProcessManager::SetSSGIUpsampleConfidenceMin(float value)
{
	sSSGIPass.SetUpsampleConfidenceMin(value);
}

void PostProcessManager::SetSSGIDebugView(SSGIPass::DebugView view)
{
	sSSGIPass.SetDebugView(view);
}

void PostProcessManager::SetSSGIQuality(SSGIPass::Quality quality)
{
	sSSGIPass.SetQuality(quality);
}

void PostProcessManager::SetSSGIMaxRadiance(float value)
{
	sSSGIPass.SetMaxRadiance(value);
}

void PostProcessManager::SetSSGIMultiBounceEnabled(bool enabled)
{
	sSSGIPass.SetMultiBounceEnabled(enabled);
}

void PostProcessManager::SetSSGIMultiBounceStrength(float strength)
{
	sSSGIPass.SetMultiBounceStrength(strength);
}

const XMMATRIX& PostProcessManager::GetPreviousViewProjection()
{
	return sMotionVectorPass.GetPreviousViewProjection();
}

D3D12_GPU_VIRTUAL_ADDRESS PostProcessManager::GetPostEffectParamAddress()
{
	if (sPostEffectCB == nullptr)
	{
		return 0;
	}

	return sPostEffectCB->GetGPUVirtualAddress();
}

MotionVectorPass& PostProcessManager::GetMotionVectorPass()
{
	return sMotionVectorPass;
}

void PostProcessManager::ApplyCopy()
{
	//============================================================
	// pScreenResourceをそのままBackBufferへコピー
	//============================================================
	CopyToBackBuffer(
		Direct3D::getScreenSRVHeap(),
		Direct3D::getScreenSRVHeap()->GetGPUDescriptorHandleForHeapStart()
	);
}


