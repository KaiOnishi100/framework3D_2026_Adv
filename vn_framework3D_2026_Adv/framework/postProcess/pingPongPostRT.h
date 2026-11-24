#pragma once

//============================================================
// PingPongPostRT
// ポストプロセスを複数つなぐための中間RenderTarget 2枚管理クラス
//   ColorGrading : ScreenResource -> PingPong Write
//   Swap()
//   Vignette     : PingPong Read   -> PingPong Write
//   Swap()
//   Copy         : PingPong Read   -> BackBuffer
// GPUでは基本的に同じテクスチャを読みながら同時に書くことができないため
// 2枚のRTを交互に使う
//============================================================
class PingPongPostRT
{
public:
	//============================================================
	// 初期化
	//============================================================
	bool Initialize(
		UINT width,
		UINT height,
		DXGI_FORMAT format = RenderFormat::SceneHDR
	);

	//============================================================
	// 終了処理
	//============================================================
	void Terminate();

	//============================================================
	// 読み書きインデックスを初期状態へ戻す
	// フレーム開始時やポストプロセス開始時に呼ぶ
	//============================================================
	void Reset();

	//============================================================
	// Read / Write を入れ替える
	// 1つのPassを書き終わったら呼ぶ
	//============================================================
	void Swap();

	//============================================================
	// 状態遷移
	//============================================================
	void TransitionReadToSRV();
	void TransitionWriteToRTV();
	void TransitionWriteToSRV();

	//============================================================
	// Write先RTVをクリア
	//============================================================
	void ClearWriteRTV(const float clearColor[4]);

	//============================================================
	// ToneMappingPassへDescriptorをコピーするためのCPU Handle
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE GetReadSRVCPU() const;

	//============================================================
	// Getter
	//============================================================
	ID3D12Resource* GetReadResource() const;
	ID3D12Resource* GetWriteResource() const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetReadSRV() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetWriteSRV() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetReadRTV() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetWriteRTV() const;

	ID3D12DescriptorHeap* GetSRVHeap() const;

	UINT GetWidth() const { return mWidth; }
	UINT GetHeight() const { return mHeight; }

	bool IsInitialized() const { return mInitialized; }

private:
	//============================================================
	// 指定したRTの状態を変更する
	//============================================================
	void ChangeResourceState(
		int index,
		D3D12_RESOURCE_STATES afterState
	);

private:
	//============================================================
	// 中間RenderTarget 2枚
	//============================================================
	ID3D12Resource* mRT[2] = { nullptr, nullptr };

	//============================================================
	// RTV / SRV ヒープ
	// RTV : 書き込み用
	// SRV : 読み取り用
	//============================================================
	ID3D12DescriptorHeap* mRTVHeap = nullptr;
	ID3D12DescriptorHeap* mSRVHeap = nullptr;

	//============================================================
	// ハンドル
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE mRTV[2] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE mSRVCPU[2] = {};
	D3D12_GPU_DESCRIPTOR_HANDLE mSRVGPU[2] = {};

	//============================================================
	// 各RTの現在の状態
	// D3D12は状態管理が必要なので保持する
	//============================================================
	D3D12_RESOURCE_STATES mState[2] =
	{
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	};

	//============================================================
	// 現在読むRT / 書くRT
	//============================================================
	int mReadIndex = 0;
	int mWriteIndex = 1;

	UINT mWidth = 0;
	UINT mHeight = 0;

	DXGI_FORMAT mFormat = RenderFormat::SceneHDR;

	UINT mRTVDescriptorSize = 0;
	UINT mSRVDescriptorSize = 0;

	bool mInitialized = false;
};