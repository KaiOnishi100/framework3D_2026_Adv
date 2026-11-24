#pragma once

class ColorGradingPass
{
public:
	bool Initialize();
	void Terminate();

	//============================================================
	// pScreenResource を読み
	// 色補正して BackBuffer へ描画する
	//============================================================
	void Apply();

	//============================================================
	// 任意の入力SRVを読み、任意のRTVへColorGrading結果を書く
	//
	// inputSRVHeap : 入力テクスチャが入っているSRVヒープ
	// inputSRV     : 実際に読むSRVのGPUハンドル
	// outputRTV    : 描画先RTV
	// width/height : 描画先サイズ
	// clear        : 描画前にRTVをクリアするか
	//============================================================
	void ApplyToRT(
		ID3D12DescriptorHeap* inputSRVHeap,
		D3D12_GPU_DESCRIPTOR_HANDLE inputSRV,
		D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
		UINT width,
		UINT height,
		bool clear = true
	);

private:
	void CreatePipelineState();

private:
	ID3D12PipelineState* mPSO = nullptr;
	bool mInitialized = false;
};