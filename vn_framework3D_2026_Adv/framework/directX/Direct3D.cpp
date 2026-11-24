#include "../../framework.h"
#include "../vn_environment.h"

extern HWND hWnd;	// ウィンドウのハンドル

#define USE_GPU (1)	//ディスクリートGPUを検索して使用する

UINT Direct3D::rtvIndex = 0;

IDXGIFactory4* Direct3D::pFactory = NULL;	//DXGIファクトリ
IDXGIAdapter3* Direct3D::pAdapter = NULL;	//アダプター
ID3D12Device* Direct3D::pDevice = NULL;	//D3D12デバイス
IDXGISwapChain3* Direct3D::pSwapChain = NULL;	//スワップチェイン
ID3D12CommandQueue* Direct3D::pCmdQueue = NULL;	//コマンドキュー
HANDLE						Direct3D::hFenceEvent = NULL;	//フェンスイベント
ID3D12Fence* Direct3D::pQueueFence = NULL;	//コマンドキュー用のフェンス
ID3D12DescriptorHeap* Direct3D::pDH_RTV = NULL;	//ディスクリプタヒープ(RenderTargetView)
ID3D12DescriptorHeap* Direct3D::pDH_DSV = NULL;	//ディスクリプタヒープ(DepthStencilView)
D3D12_CPU_DESCRIPTOR_HANDLE	Direct3D::hRTV[frameCount];	//ディスクリプタハンドル(RenderTargetView)
D3D12_CPU_DESCRIPTOR_HANDLE	Direct3D::hDSV;				//ディスクリプタハンドル(DepthStencilView)
ID3D12Resource* Direct3D::pRenderTarget[frameCount];	//レンダーターゲット
ID3D12Resource* Direct3D::pDepthBuffer = NULL;
ID3D12CommandAllocator* Direct3D::pCmdAllocator = NULL;	//コマンドアロケータ
ID3D12GraphicsCommandList* Direct3D::pCmdList = NULL;	//コマンドリスト
ID3D12RootSignature* Direct3D::pRootSignature = NULL;	//ルートシグネチャ
ID3D12RootSignature* Direct3D::pRootSignature2D = NULL;	//ルートシグネチャ

UINT Direct3D::fenceValue = 0;

ID3D12Resource* Direct3D::pWhiteTex = NULL;
D3D12_SHADER_RESOURCE_VIEW_DESC Direct3D::srvDesc = {};

ID3D12Resource* Direct3D::pSceneBuffer = NULL;		//シーン用の定数バッファ
ID3D12DescriptorHeap* Direct3D::pSceneHeap = NULL;
Direct3D::stSceneBuffer* Direct3D::pScene = NULL;

D3D12_RESOURCE_STATES Direct3D::sMainDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
//ポストプロセス用
D3D12_CLEAR_VALUE Direct3D::screenClearValue;

ID3D12Resource* Direct3D::pScreenResource = NULL;
D3D12_CPU_DESCRIPTOR_HANDLE		Direct3D::hPPRTV;				//ディスクリプタハンドル(RenderTargetView)
D3D12_CPU_DESCRIPTOR_HANDLE		Direct3D::hPPSRV;				//ディスクリプタハンドル(ShaderResourceView)

ID3D12DescriptorHeap* Direct3D::pPPRTVHeap = NULL;
ID3D12DescriptorHeap* Direct3D::pPPSRVHeap = NULL;

ID3D12Resource* Direct3D::pScreenVB = NULL;
D3D12_VERTEX_BUFFER_VIEW Direct3D::screenVBView;

ID3D12RootSignature* Direct3D::pRootSignatureScr = NULL;
ID3D12PipelineState* Direct3D::pPipelineStateScr[Direct3D::PostPorcessMax];

PBRDebugMode Direct3D::pbrDebugMode = PBRDebugMode::Final;

int Direct3D::currentPostProcess = 0;

int Direct3D::initialize()
{
	HRESULT hr = S_OK;

#if defined(_DEBUG) && ENABLE_D3D12_VALIDATION

	enableDebugValidation();

#endif


	createFactory();

	if (!createDevice())
	{
		SAFE_RELEASE(pFactory);
		return 0;
	}
	

#if defined(_DEBUG) && ENABLE_D3D12_VALIDATION
	{
		ID3D12InfoQueue* infoQueue =
			nullptr;

		const HRESULT hr =
			pDevice->QueryInterface(
				IID_PPV_ARGS(
					&infoQueue
				)
			);

		if (SUCCEEDED(hr) &&infoQueue != nullptr)
		{
			infoQueue->SetBreakOnSeverity(
				D3D12_MESSAGE_SEVERITY_CORRUPTION,
				TRUE
			);

			infoQueue->SetBreakOnSeverity(
				D3D12_MESSAGE_SEVERITY_ERROR,
				TRUE
			);

			infoQueue->Release();
			infoQueue =
				nullptr;
		}
	}
#endif

	createCommandQueue();

	createSwapChain();

	createRenderTargetView();

	createDepthStencilBuffer();

	createCommandList();

	createRootSignature();
	createRootSignature2D();

	createWhiteTexture();
	
	// Local Light Manager初期化
	// vnModelのコンストラクタでは
	// Material Descriptor HeapへLocalLightのSRVを作成する
	// そのため、どのvnModelも生成されていないこの段階で、
	// LocalLight StructuredBufferを作成しておく必要がある
	if (!LocalLightManager::Initialize())
	{
		OutputDebugStringW(L"[vnDirect3D] LocalLightManager initialization failed.\n");
		assert(false);
		return 0;
	}

	if (!SpotLightShadowManager::Initialize())
	{
		OutputDebugStringW(L"[vnDirect3D] SpotLightShadowManager initialization failed.\n");
		assert(false);
		return 0;
	}

	if (!PointLightShadowManager::Initialize())
	{
		OutputDebugStringW(L"[vnDirect3D] PointLightShadowManager initialization failed.\n");
		assert(false);
		return false;
	}
	createSceneBuffer();

	createPostProcessResource();
	createScreenVertex();
	createScreenPipeline();

	HalfResolutionEffectPass::Initialize((UINT)vnMainFrame::screenWidth,(UINT)vnMainFrame::screenHeight);
	PostProcessManager::Initialize();

	hr = pCmdList->Close();

	return 1;
}

void Direct3D::createFactory()
{
	HRESULT hr = S_OK;
	UINT dxgiFactoryFlags = 0;

	//デバッグモードの場合はデバッグレイヤーを有効にする
#if defined(_DEBUG) && ENABLE_D3D12_VALIDATION
	{

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

	}
#endif

	//ファクトリの作成
	hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&pFactory));
	assert(hr == S_OK);

	//フルスクリーンへの遷移をサポートしない
//	hr = pFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
}

bool Direct3D::createDevice()
{
	if (pFactory == nullptr)
	{
		return false;
	}

	// Factory4からFactory6を取得できれば、高性能優先の順序を使う。
	ComPtr<IDXGIFactory6> factory6;
	const bool hasGpuPreference = SUCCEEDED(
		pFactory->QueryInterface(IID_PPV_ARGS(factory6.GetAddressOf())));

	constexpr D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0;

	for (UINT index = 0; ; ++index)
	{
		ComPtr<IDXGIAdapter1> candidate;

		const HRESULT enumResult = hasGpuPreference
			? factory6->EnumAdapterByGpuPreference(
				index,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(candidate.GetAddressOf()))
			: pFactory->EnumAdapters1(index, candidate.GetAddressOf()
			);

		// 列挙終了と列挙エラーのどちらでも安全にループを抜ける
		if (FAILED(enumResult))
		{
			break;
		}

		DXGI_ADAPTER_DESC1 desc{};
		if (FAILED(candidate->GetDesc1(&desc)) ||
			(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
		{
			continue;
		}

		// 既存のメンバーpAdapterはIDXGIAdapter3*
		ComPtr<IDXGIAdapter3> adapter;
		if (FAILED(candidate.As(&adapter)))
		{
			continue;
		}

		ComPtr<ID3D12Device> device;
		if (FAILED(D3D12CreateDevice(
			adapter.Get(), minFeatureLevel, IID_PPV_ARGS(device.GetAddressOf()))))
		{
			// このGPUで作れなければ次の候補を試す
			continue;
		}

		// 作成に成功した組だけを既存のメンバーへ渡す
		pAdapter = adapter.Detach();
		pDevice = device.Detach();
		pDevice->SetName(L"vnDirect3D::pDevice");

		OutputDebugStringW(L"Selected Adapter : ");
		OutputDebugStringW(desc.Description);
		OutputDebugStringW(L"\n");
		return true;
	}

	OutputDebugStringW(L"[Direct3D] No compatible hardware adapter found.\n");
	return false;
//	HRESULT hr = S_OK;
//
//	//アダプターを列挙し、最初に見つかったアダプターを使用する
//	hr = pFactory->EnumAdapters(0, (IDXGIAdapter**)&pAdapter);
//
//#ifdef USE_GPU
//	//ディスクリートGPUを検索して使用する
//	IDXGIAdapter3* tmpAdapter = NULL;
//	for (int i = 1; pFactory->EnumAdapters(i, (IDXGIAdapter**)&tmpAdapter) != DXGI_ERROR_NOT_FOUND; i++)
//	{
//		DXGI_ADAPTER_DESC1 adesc = {};
//		tmpAdapter->GetDesc1(&adesc);
//		if (adesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
//		{
//
//		}
//		else
//		{
//			pAdapter = tmpAdapter;
//			break;
//		}
//	}
//#endif
//	assert(hr == S_OK);
//
//#if 1
//	{
//		DXGI_ADAPTER_DESC1 adesc = {};
//		pAdapter->GetDesc1(&adesc);
//		OutputDebugStringW(L"Selected Adapter : ");
//		OutputDebugStringW(adesc.Description);
//		OutputDebugStringW(L"\n");
//	}
//#endif
//
//	//フィーチャレベル列挙
//	D3D_FEATURE_LEVEL levels[] = {
//		D3D_FEATURE_LEVEL_12_1,
//		D3D_FEATURE_LEVEL_12_0,
//		D3D_FEATURE_LEVEL_11_1,
//		D3D_FEATURE_LEVEL_11_0,
//	};
//	//デバイスの作成
//	for (int i = 0; i < sizeof(levels) / sizeof(D3D_FEATURE_LEVEL); i++)
//	{
//		hr = D3D12CreateDevice(
//			pAdapter,
//			levels[i],
//			IID_PPV_ARGS(&pDevice)
//		);
//		if (hr == S_OK)
//		{
//			pDevice->SetName(L"vnDirect3D::pDevice");
//			break;
//		}
//	}
//	assert(pDevice != NULL);
}

void Direct3D::createCommandQueue()
{
	HRESULT hr = S_OK;

	//コマンドキューの設定
	D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
	command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;			//コマンドリストと合わせる
	command_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;	//指定なし
	command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;			//タイムアウトなし
	command_queue_desc.NodeMask = 0;	//アダプターを一つしか使わないときは0

	//コマンドキューの作成
	hr = pDevice->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&pCmdQueue));
	assert(hr == S_OK);
	pCmdQueue->SetName(L"vnDirect3D::pCmdQueue");

	//フェンスイベントの作成
	hFenceEvent = CreateEventEx(NULL, FALSE, FALSE, EVENT_ALL_ACCESS);
	assert(hFenceEvent != NULL);
	//コマンドキュー用のフェンスの生成
	hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pQueueFence));
	assert(hr == S_OK);
	pQueueFence->SetName(L"vnDirect3D::pQueueFence");

	fenceValue = 1;
}

void Direct3D::createSwapChain()
{
	HRESULT hr = S_OK;

	//スワップチェインの設定
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = (UINT)vnMainFrame::screenWidth;
	swapChainDesc.Height = (UINT)vnMainFrame::screenHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.BufferCount = frameCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	//スワップチェインの作成
	hr = pFactory->CreateSwapChainForHwnd(
		pCmdQueue,
		hWnd,
		&swapChainDesc,
		NULL,
		NULL,
		(IDXGISwapChain1**)&pSwapChain
	);
	assert(hr == S_OK);

	//カレントのバックバッファのインデックスを取得する
	rtvIndex = pSwapChain->GetCurrentBackBufferIndex();

	//フルスクリーンへの遷移をサポートしない
	hr = pFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);	//スワップチェインの作成後
}

void Direct3D::createRenderTargetView()
{
	HRESULT hr = S_OK;

	//RTV用デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.NumDescriptors = frameCount;
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heap_desc.NodeMask = 0;

	//RTV用デスクリプタヒープの作成
	hr = pDevice->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&pDH_RTV));
	assert(hr == S_OK);
	pDH_RTV->SetName(L"vnDirect3D::pDH_RTV");

	//ディスクリプタのサイズを取得
	UINT size = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//レンダーターゲットを作成
	for (UINT i = 0; i < frameCount; i++)
	{
		//スワップチェインからバッファを受け取る
		hr = pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pRenderTarget[i]));
		assert(hr == S_OK);
		pRenderTarget[i]->SetName(L"vnDirect3D::pRenderTarget");
		//RenderTargetViewを作成してヒープデスクリプタに登録
		hRTV[i] = pDH_RTV->GetCPUDescriptorHandleForHeapStart();
		hRTV[i].ptr += size * i;
		pDevice->CreateRenderTargetView(pRenderTarget[i], NULL, hRTV[i]);
	}
}

void Direct3D::createDepthStencilBuffer()
{
	HRESULT hr = S_OK;

	//深度バッファ作成
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Alignment = 0;
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	//2次元のテクスチャデータ
	depthResDesc.Width = vnMainFrame::screenWidth;					//レンダーターゲットと同じ
	depthResDesc.Height = vnMainFrame::screenHeight;				//レンダーターゲットと同じ
	depthResDesc.DepthOrArraySize = 1;
	depthResDesc.Format = DXGI_FORMAT_R32_TYPELESS;					//深度値書き込み用フォーマット
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.SampleDesc.Quality = 0;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	//深度ステンシルとして使用
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthResDesc.MipLevels = 1;

	//デプス用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	depthHeapProp.CreationNodeMask = 0;
	depthHeapProp.VisibleNodeMask = 0;

	//クリアバリュー
	D3D12_CLEAR_VALUE _depthClearValue = {};
	_depthClearValue.DepthStencil.Depth = 1.0f;
	_depthClearValue.DepthStencil.Stencil = 0;
	_depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;	//32bit深度値としてクリア

	hr = pDevice->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, //デプス書き込みに使用
		&_depthClearValue,
		IID_PPV_ARGS(&pDepthBuffer));
	assert(hr == S_OK);
	pDepthBuffer->SetName(L"vnDirect3D::pDepthBuffer");

	//深度用デスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;	//深度ビュー1つのみ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;//デプスステンシルビューとして使う
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	hr = pDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&pDH_DSV));
	assert(hr == S_OK);
	pDH_DSV->SetName(L"vnDirect3D::pDH_DSV");

	//深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	pDevice->CreateDepthStencilView(pDepthBuffer, &dsvDesc, pDH_DSV->GetCPUDescriptorHandleForHeapStart());

	//ハンドルの取得
	hDSV = pDH_DSV->GetCPUDescriptorHandleForHeapStart();
}

void Direct3D::createCommandList()
{
	HRESULT hr = S_OK;

	//コマンドアロケータの作成
	hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCmdAllocator));
	assert(hr == S_OK);
	pCmdAllocator->SetName(L"vnDirect3D::pCmdAllocator");

	//コマンドリストの作成(コマンドアロケータとバインド)
	hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCmdAllocator, NULL, IID_PPV_ARGS(&pCmdList));
	assert(hr == S_OK);
	pCmdList->SetName(L"vnDirect3D::pCmdList");
}

void Direct3D::createRootSignature()
{
	HRESULT hr = S_OK;

	//ルートシグネチャの設定

	//============================================================
	// Descriptor Range
	//
	// 1つのDescriptor Table内に、次の3Rangeを配置する
	//
	// Range 0:
	//   SRV t0～t3
	//
	// Range 1:
	//   CBV b1
	//
	// Range 2:
	// t4  Dynamic Shadow 0
	// t5  Dynamic Shadow 1
	// t6  Dynamic Shadow 2
	// t7  Normal
	// t8  ORM
	// t9  Environment
	// t10 Prefiltered Environment
	// t11 Irradiance Environment
	// t12 : BRDF LUT
	// t13 : Local Light StructuredBuffer
	// t14 : Spot Light Shadow
	// t15 : Point Light Shadow
	// t16 : Thickness
	// t17 : Emissive
	// t18 : Skin Mask
	//============================================================
	D3D12_DESCRIPTOR_RANGE descTblRange[3] = {};//staticShadowMapテクスチャと定数とdynamicShadowMapからskinMaskまでの3つ
	descTblRange[0].NumDescriptors = 4;							//テクスチャ4
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//種別はテクスチャ
	descTblRange[0].BaseShaderRegister = 0;						//0番スロットから
	descTblRange[0].RegisterSpace = 0;
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[1].NumDescriptors = 1;							//定数ひとつ
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//種別は定数
	descTblRange[1].BaseShaderRegister = 1;						//1番スロットから b1(Material)
	descTblRange[1].RegisterSpace = 0;	
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[2].NumDescriptors = 15;
	descTblRange[2].BaseShaderRegister = 4; // t4から開始
	descTblRange[2].RegisterSpace = 0;
	descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	D3D12_ROOT_PARAMETER rootparam[5] = {};
	//============================================================
	// RootParameter[0] : SceneBuffer b0
	//============================================================
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootparam[0].Descriptor.ShaderRegister = 0;	//b0(Scene)
	rootparam[0].Descriptor.RegisterSpace = 0;

	//============================================================
	// RootParameter[1] : MatrixBuffer b2
	//============================================================
	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootparam[1].Descriptor.ShaderRegister = 2;	//b2(Matrix)
	rootparam[1].Descriptor.RegisterSpace = 0;

	//============================================================
	// RootParameter[2] : SRV + Material CBV Table
	//============================================================
	rootparam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[2].DescriptorTable.pDescriptorRanges = &descTblRange[0];			//デスクリプタレンジのアドレス
	rootparam[2].DescriptorTable.NumDescriptorRanges = _countof(descTblRange);	//デスクリプタレンジ数
	rootparam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;				//全てのシェーダから見える

	//============================================================
	// RootParameter[3] : BoneBuffer b3
	//============================================================
	rootparam[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootparam[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootparam[3].Descriptor.ShaderRegister = 3;	//b3(Bones)
	rootparam[3].Descriptor.RegisterSpace = 0;

	//============================================================
	// RootParameter[4] : Previous BoneBuffer b4
	//============================================================
	rootparam[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootparam[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootparam[4].Descriptor.ShaderRegister = 4; // b4
	rootparam[4].Descriptor.RegisterSpace = 0;

	//============================================================
    // s0 : 通常テクスチャ用Sampler
    // Diffuse Texture 用なので WRAP
    //============================================================
	D3D12_STATIC_SAMPLER_DESC samplerDesc[2] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc[0].MinLOD = 0.0f;
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc[0].ShaderRegister = 0;
	samplerDesc[0].RegisterSpace = 0;
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//============================================================
    // s1 : ShadowMap用Sampler
    // ShadowMapは端でWRAPすると影が回り込むことがあるのでCLAMP
    //============================================================
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;;
	samplerDesc[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;// 1回のSampleCmpで周囲4テクセルの比較結果が補完される
	samplerDesc[1].MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc[1].MinLOD = 0.0f;
	samplerDesc[1].MipLODBias = 0.0f;
	samplerDesc[1].MaxAnisotropy = 1;
	samplerDesc[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;// SampleCmpでは、指定した比較値とTexture値の比較を行う。
	samplerDesc[1].ShaderRegister = 1;
	samplerDesc[1].RegisterSpace = 0;
	samplerDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC	root_signature_desc = {};
	root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	root_signature_desc.NumParameters = _countof(rootparam);
	root_signature_desc.pParameters = rootparam;
	root_signature_desc.NumStaticSamplers = _countof(samplerDesc);
	root_signature_desc.pStaticSamplers = samplerDesc;

	//ルートシグネチャのバイナリコードを作成
	ID3DBlob* rSigBlob = NULL;
	ID3DBlob* rSigBlobErr = NULL;
	hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &rSigBlob, &rSigBlobErr);
	assert(hr == S_OK);

	//ルートシグネチャの作成
	hr = pDevice->CreateRootSignature(0, rSigBlob->GetBufferPointer(), rSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSignature));
	assert(hr == S_OK);
	pRootSignature->SetName(L"vnDirect3D::pRootSignature");

	SAFE_RELEASE(rSigBlob);
	SAFE_RELEASE(rSigBlobErr);
}

void Direct3D::createRootSignature2D()
{
	HRESULT hr = S_OK;

	//ルートシグネチャの設定
	D3D12_DESCRIPTOR_RANGE range[1] = {};
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[0].BaseShaderRegister = 0;
	range[0].NumDescriptors = 1;
	range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootparam[2] = {};
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootparam[0].Descriptor.ShaderRegister = 0;
	rootparam[0].Descriptor.RegisterSpace = 0;

	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootparam[1].DescriptorTable.pDescriptorRanges = range;
	rootparam[1].DescriptorTable.NumDescriptorRanges = _countof(range);

	D3D12_STATIC_SAMPLER_DESC samplerDesc[1] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc[0].MinLOD = 0.0f;
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC	root_signature_desc = {};
	root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	root_signature_desc.NumParameters = _countof(rootparam);
	root_signature_desc.pParameters = rootparam;
	root_signature_desc.NumStaticSamplers = _countof(samplerDesc);
	root_signature_desc.pStaticSamplers = samplerDesc;

	//ルートシグネチャのバイナリコードを作成
	ID3DBlob* rSigBlob = NULL;
	ID3DBlob* rSigBlobErr = NULL;
	hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &rSigBlob, &rSigBlobErr);
	assert(hr == S_OK);

	//ルートシグネチャの作成
	hr = pDevice->CreateRootSignature(0, rSigBlob->GetBufferPointer(), rSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSignature2D));
	assert(hr == S_OK);
	pRootSignature2D->SetName(L"vnDirect3D::pRootSignature2D");

	SAFE_RELEASE(rSigBlob);
	SAFE_RELEASE(rSigBlobErr);
}

void Direct3D::createWhiteTexture()
{
	HRESULT hr = S_OK;

	//テクスチャの作成

	//テクスチャのサイズ
	UINT32 texWidth = 32;
	UINT32 texHeight = 32;

	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;							//特殊な設定
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;	//ライトバック
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;			//転送はL0(CPU側)から直接行う
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC texResDesc = {};
	texResDesc.Alignment = 0;
	texResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texResDesc.Width = texWidth;
	texResDesc.Height = texHeight;
	texResDesc.DepthOrArraySize = 1;
	texResDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//RGBAフォーマット
	texResDesc.SampleDesc.Count = 1;
	texResDesc.SampleDesc.Quality = 0;
	texResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	texResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texResDesc.MipLevels = 1;

	hr = pDevice->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&texResDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		NULL,
		IID_PPV_ARGS(&pWhiteTex)
	);
	assert(hr == S_OK);
	pWhiteTex->SetName(L"vnDirect3D::pWhiteTex");

	//ピクセルデータ
	UINT32* pixelData = new UINT32[texWidth * texHeight];
	memset(pixelData, 0xff, sizeof(UINT32) * texWidth * texHeight);

	hr = pWhiteTex->WriteToSubresource(
		0,
		NULL,
		pixelData,
		sizeof(UINT32) * texWidth,				//1ラインサイズ
		sizeof(UINT32) * texWidth * texHeight	//全サイズ
	);
	delete[] pixelData;

	//シェーダーリソースビューデスクリプション
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
}

void Direct3D::createSceneBuffer()
{
	HRESULT hr = S_OK;

	//シーン用定数バッファの作成

	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapprop.CreationNodeMask = 0;	//0 を渡すことは、単一 GPU アダプターの使用を簡略化するために、1 つを渡すことと同じ
	heapprop.VisibleNodeMask = 0;	//0 を渡すことは、単一 GPU アダプターの使用を簡略化するために、1 つを渡すことと同じ

	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = ((sizeof(stSceneBuffer) + 0xff) & ~0xff);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	hr = pDevice->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&pSceneBuffer));
	assert(hr == S_OK);
	pSceneBuffer->SetName(L"vnDirect3D::pSceneBuffer");

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NumDescriptors = 1;	///range[0](b1)
	heapDesc.NodeMask = 0;
	hr = Direct3D::getDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pSceneHeap));
	assert(hr == S_OK);
	pSceneHeap->SetName(L"vnDirect3D::pSceneHeap");

	D3D12_CPU_DESCRIPTOR_HANDLE handle = pSceneHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
	viewDesc.BufferLocation = pSceneBuffer->GetGPUVirtualAddress();
	viewDesc.SizeInBytes = (UINT)pSceneBuffer->GetDesc().Width;
	Direct3D::getDevice()->CreateConstantBufferView(&viewDesc, handle);

	pSceneBuffer->Map(0, NULL, (void**)&pScene);
}

void Direct3D::createPostProcessResource()
{
	//バックバッファと同じサイズでリソースを作成(バックバッファのDescを利用)

	D3D12_RESOURCE_DESC resDesc = pRenderTarget[0]->GetDesc();
	resDesc.Format = RenderFormat::SceneHDR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProp.CreationNodeMask = 0;
	heapProp.VisibleNodeMask = 0;

	//D3D12_CLEAR_VALUE clearValue;
	screenClearValue.Format = RenderFormat::SceneHDR;
	screenClearValue.Color[0] = vnMainFrame::clearColor[0];
	screenClearValue.Color[1] = vnMainFrame::clearColor[1];
	screenClearValue.Color[2] = vnMainFrame::clearColor[2];
	screenClearValue.Color[3] = vnMainFrame::clearColor[3];

	HRESULT hr = pDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&screenClearValue,
		IID_PPV_ARGS(&pScreenResource)
	);
	assert(hr == S_OK);


	//ポストプロセスに関連するリソースをレンダーターゲットとして使用するためのディスクリプタヒープを生成し
	//ハンドルを取得して最初の位置にレンダーターゲットビューを作成する

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = pDH_RTV->GetDesc();
	heapDesc.NumDescriptors = 1;

	hr = pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pPPRTVHeap));
	assert(hr == S_OK);
	pPPRTVHeap->SetName(L"vnDirect3D::pPPRTVHeap");

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = RenderFormat::SceneHDR;

	//D3D12_CPU_DESCRIPTOR_HANDLE handle
	hPPRTV = pPPRTVHeap->GetCPUDescriptorHandleForHeapStart();
	pDevice->CreateRenderTargetView(pScreenResource, &rtvDesc, hPPRTV);

	//heapDescをシェーダーリソース用の設定に変更し、ディスクリプタヒープを生成する

	heapDesc.NumDescriptors = 2;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;

	hr = pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pPPSRVHeap));
	assert(hr == S_OK);
	pPPSRVHeap->SetName(L"vnDirect3D::pPPSRVHeap");

	//シェーダーリソースビューを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srcDesc = {};
	srcDesc.Format = RenderFormat::SceneHDR;
	srcDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srcDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srcDesc.Texture2D.MostDetailedMip = 0;
	srcDesc.Texture2D.MipLevels = 1;
	srcDesc.Texture2D.PlaneSlice = 0;
	srcDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	//============================================================
	// Descriptorサイズ
	//============================================================
	const UINT descriptorSize =
		pDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

	//============================================================
	// [0] t0 : ScreenResource
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE screenHandle0 =
		pPPSRVHeap->GetCPUDescriptorHandleForHeapStart();

	pDevice->CreateShaderResourceView(
		pScreenResource,
		&srcDesc,
		screenHandle0
	);

	D3D12_CPU_DESCRIPTOR_HANDLE screenHandle1 = screenHandle0;

	screenHandle1.ptr += static_cast<SIZE_T>(descriptorSize);

	pDevice->CreateShaderResourceView(
		pScreenResource,
		&srcDesc,
		screenHandle1
	);

	//============================================================
	// 外部へ返すCPU Handleは[0]
	//============================================================
	hPPSRV = screenHandle0;

	pScreenResource->SetName(L"vnDirect3D::pScreenResource");
}

void Direct3D::createScreenVertex()
{
	struct stScreenVertex
	{
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};

	stScreenVertex sv[4] = {
		{{-1.0f, -1.0f, 0.1f},{0.0f, 1.0f}},
		{{-1.0f, +1.0f, 0.1f},{0.0f, 0.0f}},
		{{+1.0f, -1.0f, 0.1f},{1.0f, 1.0f}},
		{{+1.0f, +1.0f, 0.1f},{1.0f, 0.0f}},
	};

	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapprop.CreationNodeMask = 1;
	heapprop.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = sizeof(sv);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = pDevice->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(&pScreenVB)
	);
	assert(hr == S_OK);
	pScreenVB->SetName(L"vnDirect3D::pScreenVB");

	stScreenVertex* mapped = NULL;
	pScreenVB->Map(0, NULL, reinterpret_cast<void**>(&mapped));
	memcpy(mapped, sv, sizeof(sv));
	pScreenVB->Unmap(0, NULL);

	screenVBView.BufferLocation = pScreenVB->GetGPUVirtualAddress();
	screenVBView.SizeInBytes = sizeof(sv);
	screenVBView.StrideInBytes = sizeof(stScreenVertex);
}

void Direct3D::createScreenPipeline()
{
	//============================================================
    // RootParameter
    //
    // rp[0] : t0 SRV
    //         ポストプロセスで読む入力テクスチャ
    //
    // rp[1] : b0 CBV
    //         ColorGrading / Vignette / ChromaticAberration 用の
    //         PostEffectParam 定数バッファ
    //============================================================
	D3D12_DESCRIPTOR_RANGE range[2] = {};

	//============================================================
    // t0 : 入力テクスチャ SRV
    //============================================================
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[0].BaseShaderRegister = 0;
	range[0].RegisterSpace = 0;
	range[0].NumDescriptors = 2;
	range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//============================================================
	// t0 : 入力テクスチャ SRV
	//============================================================

	D3D12_ROOT_PARAMETER rp[2] = {};

	//============================================================
    // rp[0] : t0 SRV
    //============================================================
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[0].DescriptorTable.pDescriptorRanges = &range[0];
	rp[0].DescriptorTable.NumDescriptorRanges = 1;

	//============================================================
    // rp[1] : b0 CBV
    // DescriptorHeapにCBVを作らず
    // SetGraphicsRootConstantBufferView() でGPUアドレスを直接渡す
    //============================================================
	rp[1].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_CBV;

	rp[1].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;

	rp[1].Descriptor.ShaderRegister = 0; // b0
	rp[1].Descriptor.RegisterSpace = 0;

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.MinLOD = 0.0f;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.RegisterSpace = 0;
	sampler.ShaderRegister = 0;


	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.pParameters = rp;
	rsDesc.NumParameters = _countof(rp);
	rsDesc.pStaticSamplers = &sampler;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* rsBlob = NULL;
	ID3DBlob* errBlob = NULL;

	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &errBlob);
	assert(hr == S_OK);

	hr = pDevice->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSignatureScr));
	assert(hr == S_OK);
	pRootSignatureScr->SetName(L"vnDirect3D::pRootSignatureScr");



	D3D12_INPUT_ELEMENT_DESC layout[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.InputLayout = { layout, _countof(layout) };
	gpsDesc.pRootSignature = pRootSignatureScr;

	gpsDesc.RasterizerState.MultisampleEnable = false;
	gpsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gpsDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpsDesc.RasterizerState.DepthClipEnable = true;
	gpsDesc.RasterizerState.FrontCounterClockwise = false;
	gpsDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	gpsDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	gpsDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	gpsDesc.RasterizerState.AntialiasedLineEnable = false;
	gpsDesc.RasterizerState.ForcedSampleCount = 0;
	gpsDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	gpsDesc.BlendState.AlphaToCoverageEnable = false;
	gpsDesc.BlendState.IndependentBlendEnable = false;
	gpsDesc.BlendState.RenderTarget[0].BlendEnable = false;
	gpsDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	gpsDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	gpsDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	gpsDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	gpsDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_DEST_ALPHA;
	gpsDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	gpsDesc.BlendState.RenderTarget[0].LogicOpEnable = false;
	gpsDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	gpsDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	gpsDesc.DepthStencilState.DepthEnable = false;
	gpsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	gpsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpsDesc.DepthStencilState.StencilEnable = false;
	gpsDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	gpsDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	gpsDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	gpsDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	gpsDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	gpsDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	gpsDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	gpsDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	gpsDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	gpsDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	gpsDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	gpsDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = RenderFormat::BackBuffer;// BackBuffer描画のため変更なし
	gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpsDesc.SampleDesc.Count = 1;
	gpsDesc.SampleDesc.Quality = 0;
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	for (int i = 0; i < ePostProcess::PostPorcessMax; i++)
	{
		switch (i)
		{
		case ePostProcess::Copy:
			gpsDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_Copy)->getCode();
			gpsDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_Copy)->getLength();
			gpsDesc.PS.pShaderBytecode = Shader::getPShader(Shader::ePixelShader::PS_Copy)->getCode();
			gpsDesc.PS.BytecodeLength = Shader::getPShader(Shader::ePixelShader::PS_Copy)->getLength();
			break;
		case ePostProcess::GrayScale:
			gpsDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_Gray)->getCode();
			gpsDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_Gray)->getLength();
			gpsDesc.PS.pShaderBytecode = Shader::getPShader(Shader::ePixelShader::PS_Gray)->getCode();
			gpsDesc.PS.BytecodeLength = Shader::getPShader(Shader::ePixelShader::PS_Gray)->getLength();
			break;
		}

		hr = pDevice->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(&pPipelineStateScr[i]));
		assert(hr == S_OK);
	}
}


void Direct3D::terminate()
{
	PointLightShadowManager::Terminate();
	SpotLightShadowManager::Terminate();
	LocalLightManager::Terminate();

	SAFE_RELEASE(pScreenResource);

	SAFE_RELEASE(pPPRTVHeap);
	SAFE_RELEASE(pPPSRVHeap);

	SAFE_RELEASE(pScreenVB);
	HalfResolutionEffectPass::Terminate();
	PostProcessManager::Terminate();

	SAFE_RELEASE(pRootSignatureScr);
	for (int i = 0; i < ePostProcess::PostPorcessMax; i++)
	{
		SAFE_RELEASE(pPipelineStateScr[i]);
	}


	SAFE_RELEASE(pSceneHeap);
	// pSceneBuffer が Map されている場合は Unmap してから解放
	if (pSceneBuffer)
	{
		if (pScene != nullptr)
		{
			pSceneBuffer->Unmap(0, nullptr);
			pScene = nullptr;
		}
		SAFE_RELEASE(pSceneBuffer);
	}

	SAFE_RELEASE(pWhiteTex);

	SAFE_RELEASE(pRootSignature2D);
	SAFE_RELEASE(pRootSignature);

	SAFE_RELEASE(pCmdList);
	SAFE_RELEASE(pCmdAllocator);
	for (int i = 0; i < frameCount; i++)
	{
		SAFE_RELEASE(pRenderTarget[i]);
	}
	SAFE_RELEASE(pDepthBuffer);
	SAFE_RELEASE(pDH_RTV);
	SAFE_RELEASE(pDH_DSV);
	SAFE_RELEASE(pQueueFence);
	CloseHandle(hFenceEvent);
	SAFE_RELEASE(pCmdQueue);
	SAFE_RELEASE(pSwapChain);

#if defined(_DEBUG)
	reportLiveD3D12Objects();
#endif


	//ID3D12DebugDevice* debugInterface;
	//pDevice->QueryInterface(&debugInterface);
	//debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
	//debugInterface->Release();
	SAFE_RELEASE(pDevice);
	SAFE_RELEASE(pAdapter);
	SAFE_RELEASE(pFactory);

#if defined(_DEBUG)
	reportLiveDXGIObjects();
#endif
}

void Direct3D::beginRender(void)
{
	HRESULT hr = S_OK;

#if 0
	if (vnKeyboard::trg(DIK_1))
	{
		currentPostProcess = ePostProcess::Copy;
	}
	else if (vnKeyboard::trg(DIK_2))
	{
		currentPostProcess = ePostProcess::GrayScale;
	}
#endif

	static D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)vnMainFrame::screenWidth, (float)vnMainFrame::screenHeight, 0.0f, 1.0f };
	static D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(vnMainFrame::screenWidth), static_cast<LONG>(vnMainFrame::screenHeight) };

#if 0
	if (vnKeyboard::on(DIK_LSHIFT))
	{
		if (vnKeyboard::on(DIK_I))
		{
			viewport.Height--;
		}
		if (vnKeyboard::on(DIK_K))
		{
			viewport.Height++;
		}
		if (vnKeyboard::on(DIK_J))
		{
			viewport.Width--;
		}
		if (vnKeyboard::on(DIK_L))
		{
			viewport.Width++;
		}
		vnCamera::setAspect(viewport.Width / viewport.Height);
	}
	else
	{
		if (vnKeyboard::on(DIK_I))
		{
			viewport.TopLeftY--;
		}
		if (vnKeyboard::on(DIK_K))
		{
			viewport.TopLeftY++;
		}
		if (vnKeyboard::on(DIK_J))
		{
			viewport.TopLeftX--;
		}
		if (vnKeyboard::on(DIK_L))
		{
			viewport.TopLeftX++;
		}
	}
#endif

	hr = pCmdAllocator->Reset();
	//assert(hr == S_OK);

	hr = pCmdList->Reset(pCmdAllocator, NULL);
	//assert(hr == S_OK);

	pCmdList->SetGraphicsRootSignature(pRootSignature);
	//pCmdList->RSSetViewports(1, &viewport);
	pCmdList->RSSetViewports(1, vnCamera::getViewport());
	pCmdList->RSSetScissorRects(1, &scissorRect);

	//ポストプロセス用
	//レンダーターゲットへのリソースバリア
	{
		D3D12_RESOURCE_BARRIER BarrierDesc[2] = {};
		BarrierDesc[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc[0].Transition.pResource = pScreenResource;
		BarrierDesc[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		BarrierDesc[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		BarrierDesc[1] = BarrierDesc[0];
		BarrierDesc[1].Transition.pResource = pRenderTarget[rtvIndex];
		BarrierDesc[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		BarrierDesc[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		pCmdList->ResourceBarrier(_countof(BarrierDesc), BarrierDesc);
		// Scene描画ではDepthを書き込むためDEPTH_WRITEへ戻す
		TransitionMainDepthToDepthWrite();
		//レンダーターゲットと深度バッファの設定
		pCmdList->OMSetRenderTargets(1, &hPPRTV, false, &hDSV);
	}

	//レンダーターゲットのクリア
	pCmdList->ClearRenderTargetView(hPPRTV, screenClearValue.Color, 0, NULL);
	//深度バッファのクリア
	pCmdList->ClearDepthStencilView(hDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

	//シーン環境の設定
	XMStoreFloat4x4(&pScene->VP, XMMatrixTranspose(*vnCamera::getScreen()));

	//XMVECTOR lightPos = *vnCamera::getTarget() + *vnLight::getILightDir() * 20.0f;
	//XMMATRIX view = XMMatrixLookAtLH(lightPos, *vnCamera::getTarget(), XMVectorSet(0, 1, 0, 0));
	//XMMATRIX proj = XMMatrixOrthographicLH(30.0f, 30.0f, 10.0f, 30.0f);
	//XMStoreFloat4x4(&pScene->LightVP, XMMatrixTranspose(view * proj));

	//============================================================
	// Cascadeの分割終了距離をSceneBufferへ設定
	// 各値はカメラのView空間における前方距離
	// Vertex ShaderがOUT.eye.wへ保存している値と比較する
	//============================================================
	if (ShadowMapManager::IsValid())
	{
		//========================================================
		// Cascadeの終了距離
		//========================================================
		XMStoreFloat4(
			&pScene->CascadeSplits,
			XMVectorSet(
				ShadowMapManager::GetCascadeSplitDepth(0),
				ShadowMapManager::GetCascadeSplitDepth(1),
				ShadowMapManager::GetCascadeSplitDepth(2),
				0.0f
			)
		);

		//========================================================
		// Cascadeのブレンド開始距離
		//========================================================
		XMStoreFloat4(
			&pScene->CascadeBlendStarts,
			XMVectorSet(
				ShadowMapManager::GetCascadeBlendStartDepth(0),
				ShadowMapManager::GetCascadeBlendStartDepth(1),
				ShadowMapManager::GetCascadeBlendStartDepth(2),
				0.0f
			)
		);
	}
	else
	{
		//========================================================
		// ShadowMapが存在しない場合
		// 非常に大きい距離を設定しておき、
		// Cascade選択が異常値にならないようにする
		//========================================================
		XMStoreFloat4(
			&pScene->CascadeSplits,
			XMVectorSet(
				1000000.0f,
				1000000.0f,
				1000000.0f,
				0.0f
			)
		);

		XMStoreFloat4(
			&pScene->CascadeBlendStarts,
			XMVectorSet(
				1000000.0f,
				1000000.0f,
				1000000.0f,
				0.0f
			)
		);
	}

	XMStoreFloat4(&pScene->LightDir, *Light::getILightDir());
	XMStoreFloat4(&pScene->LightCol, *Light::getLightColor());
	XMStoreFloat4(&pScene->LightAmb, *Light::getAmbient());
	XMStoreFloat4(&pScene->CameraPos, *vnCamera::getPosition());
	XMStoreFloat4(&pScene->FogColor, *vnFog::getColor());
	XMStoreFloat4(&pScene->FogInfo, vnFog::isEnable() ? *vnFog::getInfo() : XMVectorZero());
	XMStoreFloat4(&pScene->PBRDebugParam, XMVectorSet(static_cast<float>(pbrDebugMode), 0.0f, 0.0f, 0.0f));
	XMStoreFloat4(&pScene->LocalLightParam, XMVectorSet(static_cast<float>(LocalLightManager::GetLightCount()), static_cast<float>(LocalLightManager::MaxLocalLights), 0.0f, 0.0f));
	XMStoreFloat4(&pScene->EnvironmentSceneParam, XMVectorSet(EnvironmentMapManager::GetEnvironmentRotationRadians(), EnvironmentMapManager::GetEnvironmentExposureScale(), 0.0f, 0.0f));

	//============================================================
	// 現在登録されているSpot Shadow数
	//============================================================
	const UINT spotShadowLightCount = SpotLightShadowManager::GetLightCount();

	//============================================================
	// 全Spot Shadow SlotをSceneBufferへ設定
	//============================================================
	for (UINT shadowIndex = 0; shadowIndex < SpotLightShadowManager::MaxShadowLights; ++shadowIndex)
	{
		const bool shadowEnabled = shadowIndex < spotShadowLightCount;
		if (shadowEnabled)
		{
			//========================================================
			// 登録済みSpot Shadow
			//========================================================
			XMStoreFloat4x4(&pScene->SpotLightViewProjection[shadowIndex],
				XMMatrixTranspose(SpotLightShadowManager::GetLightViewProjection(shadowIndex)));

			//========================================================
			// AddLight()で登録した個別設定を取得
			//========================================================
			pScene->SpotShadowParam[shadowIndex] =
				SpotLightShadowManager::GetShadowParameters(shadowIndex);
		}
		else
		{
			XMStoreFloat4x4(&pScene->SpotLightViewProjection[shadowIndex], XMMatrixIdentity());
			XMStoreFloat4(&pScene->SpotShadowParam[shadowIndex], XMVectorSet(
				0.0f,		// PCF Radius
				1.0f,		// Minimum Bias
				0.0003f,	// Slope Bias
				0.0020f
			));
		}
	}

	//============================================================
	// 現在登録されているPoint Shadow Light数
	//============================================================
	const UINT pointShadowLightCount = PointLightShadowManager::GetLightCount();

	//============================================================
	// 最大Point Shadow Light数だけ処理する
	//============================================================
	for (UINT shadowIndex = 0u; shadowIndex < PointLightShadowManager::MaxShadowLights; ++shadowIndex)
	{
		//========================================================
		// このShadow Slotが現在のフレームで有効か
		//========================================================
		const bool shadowEnabled = shadowIndex < pointShadowLightCount;

		if (shadowEnabled)
		{
			//====================================================
			// 登録済みPoint Lightの6面行列を転送
			//
			// Face:
			//   0 = +X
			//   1 = -X
			//   2 = +Y
			//   3 = -Y
			//   4 = +Z
			//   5 = -Z
			//====================================================
			for (UINT faceIndex = 0u; faceIndex < PointLightShadowManager::FaceCount; ++faceIndex)
			{
				//================================================
				// SceneBuffer内の一次元配列Index
				// Point Shadow 0: 0～5
				// Point Shadow 1: 6～11
				//================================================
				const UINT matrixIndex = shadowIndex * PointLightShadowManager::FaceCount + faceIndex;

				//================================================
				// HLSL側でmul(vector, matrix)を使うため
				// Transposeして転送する
				//================================================
				XMStoreFloat4x4(
					&pScene->PointLightViewProjection[matrixIndex],
					XMMatrixTranspose(
						PointLightShadowManager::GetLightViewProjection(
							shadowIndex,
							faceIndex
						)
					)
				);
			}

			//====================================================
			// AddLight()で登録した設定を転送
			//
			// x = Enable
			// y = PCF Radius
			// z = Minimum Bias
			// w = Slope Bias
			//====================================================
			pScene->PointShadowParam[shadowIndex] =
				PointLightShadowManager::GetShadowParameters(shadowIndex);
		}
		else
		{
			//====================================================
			// 未使用Shadow Slot
			// 前フレームの情報が残らないように無効化する
			//====================================================
			for (UINT faceIndex = 0u; faceIndex < PointLightShadowManager::FaceCount; ++faceIndex)
			{
				const UINT matrixIndex = shadowIndex * PointLightShadowManager::FaceCount + faceIndex;

				XMStoreFloat4x4(
					&pScene->PointLightViewProjection[matrixIndex],
					XMMatrixIdentity()
				);
			}

			pScene->PointShadowParam[shadowIndex] =
				XMFLOAT4(
					0.0f,      // Shadow Disable
					1.0f,      // PCF Radius
					0.0003f,   // Minimum Bias
					0.0020f    // Slope Bias
				);
		}
	}

	pCmdList->SetGraphicsRootConstantBufferView(0, pSceneBuffer->GetGPUVirtualAddress());
}

void Direct3D::beginRender2D(void)
{
	// 3D描画が終わったHDRターゲットをポストエフェクトから読める状態にする
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = pScreenResource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	pCmdList->ResourceBarrier(1, &barrier);

	// フォグ・ブルーム・トーンマッピングを適用しバックバッファへ出力する
	PostProcessManager::Apply();

	// ポストエフェクト後のバックバッファへそのままUIを重ねる
	// バックバッファはbeginRender()でRENDER_TARGETへ遷移済み
	const D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = getCurrentRTV();
	pCmdList->OMSetRenderTargets(1, &backBufferRTV, FALSE, nullptr);

	// 3Dカメラの表示範囲や各エフェクトの中間解像度に依存させない
	const D3D12_VIEWPORT viewport{
		0.0f, 0.0f,
		static_cast<float>(vnMainFrame::screenWidth),
		static_cast<float>(vnMainFrame::screenHeight),
		0.0f, 1.0f
	};
	const D3D12_RECT scissor{
		0, 0, vnMainFrame::screenWidth, vnMainFrame::screenHeight
	};

	pCmdList->RSSetViewports(1, &viewport);
	pCmdList->RSSetScissorRects(1, &scissor);
	pCmdList->SetGraphicsRootSignature(pRootSignature2D);
}

void Direct3D::finishRender(void)
{
	HRESULT hr = S_OK;

	//============================================================
	// pScreenResource を描画先から読み取り用に変更
	//============================================================
	//D3D12_RESOURCE_BARRIER BarrierDesc = {};
	//BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//BarrierDesc.Transition.pResource = pScreenResource;
	//BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	//BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	//pCmdList->ResourceBarrier(1, &BarrierDesc);

	//============================================================
	// ポストプロセス適用
	// BloomがONなら内部でBloomPass::Apply()が呼ばれる
	//============================================================
	//PostProcessManager::Apply();
	//pCmdList->SetPipelineState(
	//	pPipelineStateScr[ePostProcess::ShadowDebug]);

	//pCmdList->SetGraphicsRootSignature(
	//	pRootSignatureScr);


#if 0	//この後Direct2Dの描画がある場合はここでプレゼンテーションへ移行しない

	//プレゼンテーションへのリソースバリア
	D3D12_RESOURCE_BARRIER BarrierDesc = {};
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = pRenderTarget[rtvIndex];
	BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	pCmdList->ResourceBarrier(1, &BarrierDesc);

#endif

	//============================================================
	// コマンドリスト終了
	//============================================================
	hr = pCmdList->Close();

	if (FAILED(hr))
	{
		OutputDebugStringW(L"[Direct3D] Failed to close the command list.\n");
		assert(SUCCEEDED(hr));
		return;
	}

	ID3D12CommandList* ppCommandLists[] = { pCmdList };
	pCmdQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

void Direct3D::present(void)
{
	//============================================================
	// Present
	//============================================================
	HRESULT hr =
		pSwapChain->Present(
			1,
			0
		);


	if (FAILED(hr))
	{
		OutputDebugStringW(
			L"[Direct3D] Present failed.\n"
		);


		DumpDRED();

		assert(false);

		return;
	}


	//============================================================
	// Fence Signal
	//============================================================
	const UINT64 fence = fenceValue++;


	hr =
		pCmdQueue->Signal(
			pQueueFence,
			fence
		);


	//============================================================
	// Signal失敗は即座に確認
	//============================================================
	if (FAILED(hr))
	{
		OutputDebugStringW(
			L"[Direct3D] Queue Signal failed.\n"
		);


		DumpDRED();

		assert(false);

		return;
	}


	//============================================================
	// Completed Fence
	//============================================================
	const UINT64 completedValue =
		pQueueFence->GetCompletedValue();


	//============================================================
	// Device Removed時などに異常値になった場合
	//============================================================
	if (completedValue ==
		UINT64_MAX)
	{
		OutputDebugStringW(
			L"[Direct3D] "
			L"Fence returned UINT64_MAX.\n"
		);


		DumpDRED();

		assert(false);

		return;
	}


	//============================================================
	// GPU完了待ち
	//============================================================
	if (completedValue < fence)
	{
		hr =
			pQueueFence->SetEventOnCompletion(
				fence,
				hFenceEvent
			);


		if (FAILED(hr))
		{
			OutputDebugStringW(
				L"[Direct3D] "
				L"SetEventOnCompletion failed.\n"
			);


			DumpDRED();

			assert(false);

			return;
		}


		const DWORD waitResult =
			WaitForSingleObject(
				hFenceEvent,
				INFINITE
			);


		if (waitResult != WAIT_OBJECT_0)
		{
			OutputDebugStringW(
				L"[Direct3D] "
				L"Fence wait failed.\n"
			);


			DumpDRED();

			assert(false);

			return;
		}
	}


	//============================================================
	// Next BackBuffer
	//============================================================
	rtvIndex = pSwapChain->GetCurrentBackBufferIndex();
}

void Direct3D::setMainRenderTarget(void)
{
	ID3D12GraphicsCommandList* cmdList =
		Direct3D::getCommandList();

	assert(cmdList != nullptr);

	TransitionMainDepthToDepthWrite();

	//============================================================
	// 通常描画用Viewportへ戻す
	// ShadowMap描画ではShadowMapサイズのViewportに変わっているため
	// メイン画面サイズのViewportへ戻す
	//============================================================
	cmdList->RSSetViewports(
		1,
		vnCamera::getViewport()
	);

	//============================================================
	// 通常描画用ScissorRectへ戻す
	//============================================================
	D3D12_RECT scissorRect =
	{
		0,
		0,
		static_cast<LONG>(vnMainFrame::screenWidth),
		static_cast<LONG>(vnMainFrame::screenHeight)
	};

	cmdList->RSSetScissorRects(
		1,
		&scissorRect
	);

	//============================================================
	// ShadowMap描画でRootSignature / PSOがShadow用に変わっているため
	// 通常3D描画用RootSignatureへ戻す
	//============================================================
	cmdList->SetGraphicsRootSignature(
		pRootSignature
	);

	//============================================================
	// 通常3D描画用のシーン定数バッファを再セット
	// ShadowMap描画中にRootSignatureが変わるため
	// 通常描画へ戻るタイミングでb0を再セットする
	//============================================================
	cmdList->SetGraphicsRootConstantBufferView(
		0,
		pSceneBuffer->GetGPUVirtualAddress()
	);

	//============================================================
	// 通常描画用RTV
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
		hPPRTV;

	//============================================================
	// 通常描画用DSV
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
		hDSV;

	//============================================================
	// メイン描画先へ戻す
	//============================================================
	cmdList->OMSetRenderTargets(
		1,
		&rtvHandle,
		false,
		&dsvHandle
	);
}

void Direct3D::SetMain3DRootState(void)
{
	if (pCmdList == nullptr ||
		pRootSignature == nullptr ||
		pSceneBuffer == nullptr)
	{
		return;
	}

	//============================================================
	// 通常3D用Root Signature
	//============================================================
	pCmdList->SetGraphicsRootSignature(
		pRootSignature
	);

	//============================================================
	// Root 0
	// SceneBuffer
	//============================================================
	pCmdList->SetGraphicsRootConstantBufferView(
		0u,
		pSceneBuffer->
		GetGPUVirtualAddress()
	);
}

ID3D12Device* Direct3D::getDevice(void)
{
	return pDevice;
}

ID3D12CommandQueue* Direct3D::getCommandQueue(void)
{
	return pCmdQueue;
}

ID3D12RootSignature* Direct3D::getRootSignature(void)
{
	return pRootSignature;
}

ID3D12RootSignature* Direct3D::getRootSignatureScr(void)
{
	return pRootSignatureScr;
}

ID3D12RootSignature* Direct3D::getRootSignature2D(void)
{
	return pRootSignature2D;
}

ID3D12Resource* Direct3D::getScreenResource()
{
	return pScreenResource;
}

ID3D12Resource* Direct3D::getDepthStencilResource()
{
	return pDepthBuffer;
}

ID3D12DescriptorHeap* Direct3D::getScreenSRVHeap()
{
	return pPPSRVHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE Direct3D::getScreenSRVCPUHandle()
{
	return hPPSRV;
}

D3D12_CPU_DESCRIPTOR_HANDLE Direct3D::getCurrentRTV()
{
	return hRTV[rtvIndex];
}

D3D12_CPU_DESCRIPTOR_HANDLE Direct3D::getScreenRTVCPUHandle()
{
	return hPPRTV;
}

D3D12_CPU_DESCRIPTOR_HANDLE Direct3D::getDepthStencilView()
{
	return hDSV;
}

D3D12_VERTEX_BUFFER_VIEW* Direct3D::getScreenVBView()
{
	return &screenVBView;
}

ID3D12PipelineState* Direct3D::getScreenPipelineState(ePostProcess type)
{
	return pPipelineStateScr[type];
}

ID3D12GraphicsCommandList* Direct3D::getCommandList(void)
{
	return pCmdList;
}

ID3D12Resource* Direct3D::getRenderTarget(int id)
{
	return pRenderTarget[id];
}


UINT Direct3D::getIndex(void)
{
	return rtvIndex;
}

ID3D12Resource* Direct3D::getWhiteTexture()
{
	return pWhiteTex;
}

D3D12_SHADER_RESOURCE_VIEW_DESC* Direct3D::getWhiteTextueViewDesc()
{
	return &srvDesc;
}
void Direct3D::TransitionMainDepthToSRV()
{
	if (pDepthBuffer == nullptr)
	{
		return;
	}

	if (sMainDepthState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		return;
	}


	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = pDepthBuffer;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = sMainDepthState;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	pCmdList->ResourceBarrier(
		1u,
		&barrier
	);


	sMainDepthState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void Direct3D::TransitionMainDepthToDepthWrite()
{
	if (pDepthBuffer == nullptr)
	{
		return;
	}

	if (sMainDepthState == D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		return;
	}


	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = pDepthBuffer;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = sMainDepthState;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;


	pCmdList->ResourceBarrier(
		1u,
		&barrier
	);


	sMainDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
}

void Direct3D::setPBRDebugMode(PBRDebugMode mode)
{
	const int modeValue = static_cast<int>(mode);

	//============================================================
	// 不正な値を防止
	//============================================================
	if (modeValue < 0 || modeValue >=static_cast<int>(PBRDebugMode::Count))
	{
		return;
	}

	pbrDebugMode = mode;
}

PBRDebugMode Direct3D::getPBRDebugMode()
{
	return pbrDebugMode;
}

void Direct3D::cyclePBRDebugMode()
{
	int nextMode = static_cast<int>(pbrDebugMode) + 1;

	if (nextMode >= static_cast<int>(PBRDebugMode::Count))
	{
		nextMode = 0;
	}

	pbrDebugMode = static_cast<PBRDebugMode>(nextMode);
}

//============================================================
// DRED Breadcrumb Operation → String
//============================================================
static const wchar_t*GetBreadcrumbOperationName(D3D12_AUTO_BREADCRUMB_OP operation)
{
	switch (operation)
	{
	case D3D12_AUTO_BREADCRUMB_OP_SETMARKER:
		return L"SetMarker";

	case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT:
		return L"BeginEvent";

	case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT:
		return L"EndEvent";

	case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED:
		return L"DrawInstanced";

	case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED:
		return L"DrawIndexedInstanced";

	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT:
		return L"ExecuteIndirect";

	case D3D12_AUTO_BREADCRUMB_OP_DISPATCH:
		return L"Dispatch";

	case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION:
		return L"CopyBufferRegion";

	case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION:
		return L"CopyTextureRegion";

	case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE:
		return L"CopyResource";

	case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE:
		return L"ResolveSubresource";

	case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW:
		return L"ClearRTV";

	case D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW:
		return L"ClearUAV";

	case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW:
		return L"ClearDSV";

	case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER:
		return L"ResourceBarrier";

	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE:
		return L"ExecuteBundle";

	case D3D12_AUTO_BREADCRUMB_OP_PRESENT:
		return L"Present";

	case D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA:
		return L"ResolveQueryData";

	default:
		return L"Other";
	}
}

void Direct3D::DumpDRED()
{
#if defined(_DEBUG)

	//============================================================
	// Deviceが存在しない場合は取得不能
	//============================================================
	if (pDevice == nullptr)
	{
		return;
	}


	//============================================================
	// まずDevice Removed理由を取得
	//============================================================
	const HRESULT removedReason =
		pDevice->GetDeviceRemovedReason();


	wchar_t message[256] = {};

	swprintf_s(
		message,
		L"\n========================================\n"
		L"[DRED] Device Removed\n"
		L"Reason = 0x%08X\n"
		L"========================================\n",
		static_cast<unsigned int>(removedReason)
	);

	OutputDebugStringW(message);


	//============================================================
	// DRED Interface取得
	//============================================================
	ID3D12DeviceRemovedExtendedData1* dred = nullptr;


	HRESULT hr =
		pDevice->QueryInterface(IID_PPV_ARGS(&dred));


	if (FAILED(hr) ||
		dred == nullptr)
	{
		OutputDebugStringW(
			L"[DRED] "
			L"ID3D12DeviceRemovedExtendedData1 "
			L"取得失敗\n"
		);

		return;
	}


	//============================================================
	// Auto Breadcrumb
	//============================================================
	D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};


	hr = dred->GetAutoBreadcrumbsOutput1(&breadcrumbs);


	if (SUCCEEDED(hr))
	{
		OutputDebugStringW(
			L"\n"
			L"[DRED] ===== Auto Breadcrumbs =====\n"
		);


		const D3D12_AUTO_BREADCRUMB_NODE1* node =
			breadcrumbs.pHeadAutoBreadcrumbNode;


		while (node != nullptr)
		{
			//====================================================
			// Command List
			//====================================================
			if (node->pCommandListDebugNameW != nullptr)
			{
				OutputDebugStringW(L"\n[DRED] CommandList : ");
				OutputDebugStringW(node->pCommandListDebugNameW);
				OutputDebugStringW(L"\n");
			}


			//====================================================
			// Command Queue
			//====================================================
			if (node->pCommandQueueDebugNameW != nullptr)
			{
				OutputDebugStringW(L"[DRED] CommandQueue : ");
				OutputDebugStringW(node->pCommandQueueDebugNameW);
				OutputDebugStringW(L"\n");
			}


			if (node->pLastBreadcrumbValue != nullptr &&
				node->pCommandHistory != nullptr)
			{
				const UINT completedCount =
					*node->pLastBreadcrumbValue;

				const UINT breadcrumbCount =
					node->BreadcrumbCount;


				wchar_t buffer[256] = {};


				swprintf_s(
					buffer,
					L"[DRED] Completed = %u / %u\n",
					completedCount,
					breadcrumbCount
				);


				OutputDebugStringW(
					buffer
				);


				//================================================
				// 最後に実行されたCommand周辺を見る
				// completedCountは
				// ここまで完了したという値
				//================================================
				const UINT beginIndex =
					completedCount > 8u ? completedCount - 8u : 0u;

				const UINT endIndex =
					(std::min)(
						breadcrumbCount,
						completedCount + 8u
						);


				OutputDebugStringW(L"[DRED] ---- Command History ----\n");


				for (UINT index = beginIndex; index < endIndex; ++index)
				{
					const wchar_t* operationName =
						GetBreadcrumbOperationName(node->pCommandHistory[index]);


					const wchar_t* state = L" ";


					if (index < completedCount)
					{
						state = L"[DONE]";
					}
					else if (index == completedCount)
					{
						//========================================
						// 最初の未完了Command
						//========================================
						state = L"[NEXT / SUSPECT]";
					}
					else
					{
						state = L"[PENDING]";
					}


					swprintf_s(
						buffer,
						L"[DRED] %s %u : %s\n",
						state,
						index,
						operationName
					);


					OutputDebugStringW(buffer);
				}
			}


			//====================================================
			// Breadcrumb Context
			// Contextを利用している場合、
			// Pass名などを表示できる
			//====================================================
			if (node->pBreadcrumbContexts != nullptr)
			{
				OutputDebugStringW(L"[DRED] ---- Contexts ----\n");


				for (UINT contextIndex = 0; contextIndex < node->BreadcrumbContextsCount; ++contextIndex)
				{
					const D3D12_DRED_BREADCRUMB_CONTEXT& context = node->pBreadcrumbContexts[contextIndex];

					if (context.pContextString == nullptr)
					{
						continue;
					}

					wchar_t contextHeader[128] = {};

					swprintf_s(
						contextHeader,
						L"[DRED] Breadcrumb %u : ",
						context.BreadcrumbIndex
					);


					OutputDebugStringW(contextHeader);
					OutputDebugStringW(context.pContextString);
					OutputDebugStringW(L"\n");
				}
			}

			node = node->pNext;
		}
	}
#endif
}

#if _DEBUG

void Direct3D::reportLiveD3D12Objects()
{
	if (pDevice == nullptr)
	{
		return;
	}

	ID3D12DebugDevice* debugDevice = nullptr;

	HRESULT hr = pDevice->QueryInterface(IID_PPV_ARGS(&debugDevice));

	if (SUCCEEDED(hr) && debugDevice != nullptr)
	{
		//========================================================
		// DETAIL: オブジェクトごとの詳細を表示
		// IGNORE_INTERNAL: アプリ側の外部参照数が0の内部オブジェクトを除外
		//========================================================
		D3D12_RLDO_FLAGS flags =
			static_cast<D3D12_RLDO_FLAGS>(
				D3D12_RLDO_DETAIL |
				D3D12_RLDO_IGNORE_INTERNAL
				);

		debugDevice->ReportLiveDeviceObjects(flags);

		debugDevice->Release();
		debugDevice = nullptr;
	}
}

void Direct3D::reportLiveDXGIObjects()
{
	IDXGIDebug1* dxgiDebug = nullptr;

	HRESULT hr =
		DXGIGetDebugInterface1(
			0,
			IID_PPV_ARGS(&dxgiDebug)
		);

	if (SUCCEEDED(hr) && dxgiDebug != nullptr)
	{
		DXGI_DEBUG_RLO_FLAGS flags =
			static_cast<DXGI_DEBUG_RLO_FLAGS>(
				DXGI_DEBUG_RLO_DETAIL |
				DXGI_DEBUG_RLO_IGNORE_INTERNAL
				);

		dxgiDebug->ReportLiveObjects(
			DXGI_DEBUG_ALL,
			flags
		);

		dxgiDebug->Release();
		dxgiDebug = nullptr;
	}
}

#endif

#if defined(_DEBUG)

//============================================================
// Debug Layer / GBV / DRED
//============================================================
void Direct3D::enableDebugValidation()
{
	//========================================================
	// D3D12 Debug Layer + GPU-Based Validation
	//========================================================
	ID3D12Debug1* debugController = nullptr;


	HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));


	if (SUCCEEDED(hr) && debugController != nullptr)
	{
		//====================================================
		// CPU側D3D12 Validation
		//====================================================
		debugController->EnableDebugLayer();


		//====================================================
		// GPU-Based Validation
		// Shaderからの
		// ・Descriptor範囲外アクセス
		// ・不正なResource State
		// ・削除済みDescriptor
		// ・Root Descriptor関連
		// かなり重くなるのでDebug専用
		//====================================================
		debugController->SetEnableGPUBasedValidation(TRUE);


		//====================================================
		// Command Queue Validationを同期させる
		// エラー位置を特定しやすくする
		//====================================================
		debugController->SetEnableSynchronizedCommandQueueValidation(TRUE);

		OutputDebugStringW(L"[D3D12 Debug] Debug Layer enabled.\n");
		OutputDebugStringW(L"[D3D12 Debug] GPU-Based Validation enabled.\n");


		debugController->Release();
		debugController = nullptr;
	}
	else
	{
		OutputDebugStringW(
			L"[D3D12 Debug] "
			L"ID3D12Debug1 unavailable.\n"
		);
	}


	//========================================================
	// DRED
	//
	// Device Removed Extended Data
	//========================================================
	ID3D12DeviceRemovedExtendedDataSettings1* dredSettings = nullptr;


	hr = D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings));


	if (SUCCEEDED(hr) && dredSettings != nullptr)
	{
		//====================================================
		// GPUがどこまでCommandを実行したか
		//====================================================
		dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);


		//====================================================
		// GPU Page Fault
		// GPU VA不正アクセスを追跡する
		//====================================================
		dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);


		//====================================================
		// Breadcrumb Context
		// Render Pass名などをBreadcrumbへ残しやすくする
		//====================================================
		dredSettings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);


		OutputDebugStringW(L"[DRED] Enabled.\n");


		dredSettings->Release();
		dredSettings = nullptr;
	}
}

#endif