#pragma once

#include "../render_format.h"
#include "../shadow/spotLightShadowManager.h"
#include "../shadow/pointLightShadowManager.h"

#define ENABLE_D3D12_VALIDATION 0

class Direct3D
{
public:
	static constexpr UINT frameCount = 2;

	enum ePostProcess
	{
		Copy,
		GrayScale,
		PostPorcessMax,
	};

	struct stSceneBuffer
	{
		XMFLOAT4X4	VP;			//View * Proj
		XMFLOAT4X4	LightVP;	//シャドウマップ用
		XMFLOAT4	LightDir;	//平行光源の(逆)方向
		XMFLOAT4	LightCol;	//平行光源の色
		XMFLOAT4	LightAmb;	//環境光の色
		XMFLOAT4	CameraPos;	//カメラのワールド座標
		XMFLOAT4    FogColor;   //フォグの色
		XMFLOAT4    FogInfo;    //フォグの情報(Start, End, Density, End-Start)
		XMFLOAT4	CascadeSplits;// Cascaded Shadow Mapの分割終了距離
		XMFLOAT4	CascadeBlendStarts;//Cascadeブレンド開始距離
		XMFLOAT4	PBRDebugParam;// PBRデバッグ表示
		XMFLOAT4	LocalLightParam;// Local Light Parameter x : 登録ライト数 y : 最大ライト数 zw : 未使用
		XMFLOAT4X4  SpotLightViewProjection[SpotLightShadowManager::MaxShadowLights];// Shadow Spot LightのViewProjection
		XMFLOAT4	SpotShadowParam[SpotLightShadowManager::MaxShadowLights];// x : Shadow有効 y : PCF半径 z : Minimum Bias w : Slope Bias
		XMFLOAT4X4  PointLightViewProjection[POINT_SHADOW_TOTAL_SLICES];// Point Light Shadow 4灯 × 6面 = 24行列
		XMFLOAT4	PointShadowParam[MAX_POINT_SHADOW_LIGHTS];// Point LightごとのShadow設定
		XMFLOAT4	EnvironmentSceneParam;// x : Rotation Y [radian] y : Exposure Scale = 2^EV zw : unused
	};

private:
	static UINT rtvIndex;

	static IDXGIFactory4* pFactory;	//DXGIファクトリ
	static IDXGIAdapter3* pAdapter;	//アダプター
	static ID3D12Device* pDevice;	//D3D12デバイス
	static IDXGISwapChain3* pSwapChain;	//スワップチェイン
	static ID3D12CommandQueue* pCmdQueue;	//コマンドキュー
	static HANDLE						hFenceEvent;	//フェンスイベント
	static ID3D12Fence* pQueueFence;	//コマンドキュー用のフェンス
	static ID3D12DescriptorHeap* pDH_RTV;	//ディスクリプタヒープ(RenderTargetView)
	static ID3D12DescriptorHeap* pDH_DSV;	//ディスクリプタヒープ(DepthStencilView)
	static D3D12_CPU_DESCRIPTOR_HANDLE		hRTV[frameCount];	//ディスクリプタハンドル(RenderTargetView)
	static D3D12_CPU_DESCRIPTOR_HANDLE		hDSV;				//ディスクリプタハンドル(DepthStencilView)
	static ID3D12Resource* pRenderTarget[frameCount];	//レンダーターゲット
	static ID3D12Resource* pDepthBuffer;
	static ID3D12CommandAllocator* pCmdAllocator;	//コマンドアロケータ
	static ID3D12GraphicsCommandList* pCmdList;	//コマンドリスト
	static ID3D12RootSignature* pRootSignature;	//ルートシグネチャ

	static ID3D12RootSignature* pRootSignature2D;	//ルートシグネチャ

	static UINT fenceValue;

	static ID3D12Resource* pWhiteTex;
	static D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;


	static ID3D12Resource* pSceneBuffer;		//シーン用の定数バッファ
	static ID3D12DescriptorHeap* pSceneHeap;
	static stSceneBuffer* pScene;

	static D3D12_RESOURCE_STATES sMainDepthState;

	//ポストプロセス用
	static D3D12_CLEAR_VALUE screenClearValue;

	static ID3D12Resource* pScreenResource;
	static D3D12_CPU_DESCRIPTOR_HANDLE		hPPRTV;				//ディスクリプタハンドル(RenderTargetView)
	static D3D12_CPU_DESCRIPTOR_HANDLE		hPPSRV;				//ディスクリプタハンドル(ShaderResourceView)

	static ID3D12DescriptorHeap* pPPRTVHeap;
	static ID3D12DescriptorHeap* pPPSRVHeap;

	static ID3D12Resource* pScreenVB;
	static D3D12_VERTEX_BUFFER_VIEW screenVBView;

	static ID3D12RootSignature* pRootSignatureScr;
	static ID3D12PipelineState* pPipelineStateScr[ePostProcess::PostPorcessMax];

	static PBRDebugMode pbrDebugMode;// 現在のPBRデバッグ表示モード

	static int currentPostProcess;


	//初期化関数
	static void createFactory();
	static bool createDevice();
	static void createCommandQueue();
	static void createSwapChain();
	static void createRenderTargetView();
	static void createDepthStencilBuffer();
	static void createCommandList();
	static void createRootSignature();
	static void createRootSignature2D();

	static void createWhiteTexture();

	//ポストプロセス用
	static void createSceneBuffer();
	static void createScreenVertex();
	static void createScreenPipeline();

	static void createPostProcessResource();

public:
	//フレームワーク管理
	static int initialize(void);
	static void terminate(void);

	//描画システムコール
	static void beginRender(void);
	static void beginRender2D(void);
	static void finishRender(void);
	static void present(void);
	static void setMainRenderTarget(void);
	static void SetMain3DRootState(void);

	//インターフェイス関連の取得
	static ID3D12Device* getDevice(void);
	static ID3D12CommandQueue* getCommandQueue(void);
	static ID3D12RootSignature* getRootSignature(void);
	static ID3D12RootSignature* getRootSignatureScr(void);
	static ID3D12RootSignature* getRootSignature2D(void);
	static ID3D12Resource* getScreenResource();
	static ID3D12Resource* getDepthStencilResource();
	static ID3D12DescriptorHeap* getScreenSRVHeap();
	static D3D12_CPU_DESCRIPTOR_HANDLE getScreenSRVCPUHandle();
	static D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRTV();
	static D3D12_CPU_DESCRIPTOR_HANDLE getScreenRTVCPUHandle();
	static D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView();
	static D3D12_VERTEX_BUFFER_VIEW* getScreenVBView();
	static ID3D12PipelineState* getScreenPipelineState(ePostProcess type);

	static ID3D12GraphicsCommandList* getCommandList(void);

	static ID3D12Resource* getRenderTarget(int id);

	static UINT getIndex(void);

	static ID3D12Resource* getWhiteTexture();
	static D3D12_SHADER_RESOURCE_VIEW_DESC* getWhiteTextueViewDesc();

	static void TransitionMainDepthToSRV();
	static void TransitionMainDepthToDepthWrite();

	//============================================================
	// PBRデバッグ表示を設定
	//============================================================
	static void setPBRDebugMode(PBRDebugMode mode);

	//============================================================
	// 現在のPBRデバッグ表示を取得
	//============================================================
	static PBRDebugMode getPBRDebugMode();

	//============================================================
	// 次のデバッグ表示へ切り替える
	//============================================================
	static void cyclePBRDebugMode();

	static void DumpDRED();

#if _DEBUG
	//============================================================
	// D3D12の未解放オブジェクトを報告
	//
	// 呼び出し条件:
	//   ・子ResourceやPSOなどを解放した後
	//   ・pDeviceをReleaseする前
	//============================================================
	static void reportLiveD3D12Objects();

	//============================================================
	// DXGIの未解放オブジェクトを報告
	// SwapChain、Factory、Adapterなどを解放した後に呼ぶ
	//============================================================
	static void reportLiveDXGIObjects();
#endif

#if defined(_DEBUG)

	//============================================================
	// Direct3D Debug Environment
	// ・Debug Layer
	// ・GPU-Based Validation
	// ・Synchronized Queue Validation
	// ・DRED
	//============================================================
	static void enableDebugValidation();

#endif
};

