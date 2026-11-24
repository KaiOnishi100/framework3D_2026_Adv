#pragma once

//============================================================
// ShadowMap本体の前方宣言
//============================================================
class ShadowMap;

//============================================================
// ShadowMapLayerの前方宣言
//============================================================
enum class ShadowMapLayer : unsigned int;
//============================================================
// ShadowMapManager
// 役割:
//   Scene_MainGameなどが持っているShadowMapを
//   vnModel::render() や通常描画シェーダー設定側から参照するための窓口
//   このクラス自身はShadowMapをnew / deleteしない
//   実体の生成・破棄は Scene_MainGame 側で行う
//============================================================
class ShadowMapManager
{
public:
	//============================================================
	// 使用するShadowMapを登録
	//============================================================
	static void SetShadowMap(ShadowMap* shadowMap);

	//============================================================
	// 登録解除
	//============================================================
	static void Clear();

	//============================================================
	// 指定ShadowMapが現在登録されている場合だけ解除
	//============================================================
	static void ClearShadowMap(const ShadowMap* shadowMap);

	//============================================================
	// ShadowMapが有効か
	//============================================================
	static bool IsValid();

	//============================================================
	// LightViewProjection取得
	// 通常描画時に World * LightViewProj を作るために使う
	//============================================================
	static const XMMATRIX& GetLightViewProj(UINT cascadeIndex = 0);

	//============================================================
	// ShadowMapのSRV Heap取得
	// 通常描画のPixelShaderでShadowMapを読むために使う
	//============================================================
	static ID3D12DescriptorHeap* GetSRVHeap();

	//============================================================
	// ShadowMapのSRV GPUハンドル取得
	// SetGraphicsRootDescriptorTable() に渡す
	//============================================================
	static D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle(UINT cascadeIndex, ShadowMapLayer layer);

	//============================================================
	// ShadowMapサイズ取得
	// シェーダー側でtexelSizeを使う場合などに使える
	//============================================================
	static UINT GetShadowMapSize();

	static D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCopyCPUHandle(UINT cascadeIndex, ShadowMapLayer layer);

	//============================================================
	// ShadowMap用PSO取得
	// isSkin:
	//   false = 通常モデル
	//   true  = スキニングモデル
	//============================================================
	static ID3D12PipelineState* GetPipelineState(bool isSkin);

	//============================================================
	// Shadow描画用Root Signature取得
	// Directional ShadowとSpot Shadowで共通使用する
	//============================================================
	static ID3D12RootSignature* GetRootSignature();

	//============================================================
	// Cascade数取得
	//============================================================
	static UINT GetCascadeCount();

	//============================================================
	// 指定Cascadeの終了距離を取得
	//============================================================
	static float GetCascadeSplitDepth(UINT cascadeIndex);

	//============================================================
	// Cascadeのブレンド開始距離取得
	//============================================================
	static float GetCascadeBlendStartDepth(UINT cascadeIndex);

	//============================================================
	// 指定Cascadeの描画オブジェクト数を取得
	//============================================================
	static UINT GetCascadeRenderedObjectCount(UINT cascadeIndex);

	//============================================================
	// 全Cascadeの描画オブジェクト数合計を取得
	//============================================================
	static UINT GetTotalRenderedObjectCount();

	//============================================================
	// 現在描画中のCascadeへDraw Call数を加算
	//============================================================
	static void AddShadowDrawCall(UINT count = 1);

	//============================================================
	// 指定CascadeのDraw Call数を取得
	//============================================================
	static UINT GetCascadeDrawCallCount(UINT cascadeIndex);

	//============================================================
	// 全CascadeのDraw Call数合計を取得
	//============================================================
	static UINT GetTotalDrawCallCount();

	//============================================================
	// Timestamp結果を更新
	//============================================================
	static void UpdateTimestampResults();

	//============================================================
	// 指定CascadeのGPU時間を取得
	//============================================================
	static double GetCascadeGpuTimeMs(UINT cascadeIndex);

	//============================================================
	// Shadow Pass全体のGPU時間を取得
	//============================================================
	static double GetTotalGpuTimeMs();

	//============================================================
	// 指定Layer・Cascadeが現在フレームで更新されたか
	//============================================================
	static bool WasCascadeUpdatedThisFrame(
		UINT cascadeIndex,
		ShadowMapLayer layer
	);

	//============================================================
	// 指定Layer・Cascadeへ描画したオブジェクト数を取得
	//============================================================
	static UINT GetLayerCascadeRenderedObjectCount(
		UINT cascadeIndex,
		ShadowMapLayer layer
	);

	//============================================================
	// 指定Layer・CascadeのDraw Call数を取得
	//============================================================
	static UINT GetLayerCascadeDrawCallCount(
		UINT cascadeIndex,
		ShadowMapLayer layer
	);

	//============================================================
	// 指定Layer・CascadeのGPU時間を取得
	// 戻り値: ミリ秒
	//============================================================
	static double GetLayerCascadeGpuTimeMs(
		UINT cascadeIndex,
		ShadowMapLayer layer
	);

	//============================================================
	// Static ShadowMapキャッシュを無効化
	// Static Casterの状態が変化した場合に使用する
	//============================================================
	static void InvalidateStaticShadowCache();
private:
	//============================================================
	// ShadowMapの実体ポインタ
	// 所有権は持たない
	// deleteしない
	//============================================================
	static ShadowMap* sShadowMap;
};