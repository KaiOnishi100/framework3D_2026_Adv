#pragma once

//============================================================
// MotionVectorPass
// 前フレームと現在フレームのCamera行列を比較し
// Screen Space Motion Vectorを生成する
// 出力:
//   R = Screen UV X方向の移動量
//   G = Screen UV Y方向の移動量
// Format: DXGI_FORMAT_R16G16_FLOAT
//============================================================
class MotionVectorPass
{
public:

	//============================================================
	// 初期化
	//============================================================
	bool Initialize(UINT width, UINT height);


	//============================================================
	// 終了
	//============================================================
	void Terminate();


	//============================================================
	// Motion Vector生成
	// Main Depthを使って現在PixelのWorld座標を復元し
	//   Current VP
	//   Previous VP
	// から前フレームScreen位置を求める
	//============================================================
	void Generate();


	//============================================================
	// Camera Cut / Scene切替時に履歴Reset
	// 前フレーム行列を現在行列へ合わせることで
	// 画面全体に巨大なMotion Vectorが発生するのを防ぐ
	//============================================================
	void ResetHistory();

	//============================================================
	// Object Motion描画開始
	//============================================================
	void BeginObjectMotion();

	//============================================================
	// Object Motion描画終了
	//============================================================
	void EndObjectMotion();

	//============================================================
	// Motion Vector SRV
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPU() const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPU() const;

	ID3D12DescriptorHeap* GetSRVHeap() const;

	ID3D12Resource* GetResource() const;

	// 前フレームCamera ViewProjection取得
	const XMMATRIX& GetPreviousViewProjection() const { return mPreviousVP; }

	void SetRigidObjectMotionPipeline();

	void SetSkinnedObjectMotionPipeline();

private:

	//============================================================
	// GPU用Constant Buffer
	//============================================================
	struct alignas(16) MotionVectorBuffer
	{
		//========================================================
		// 現在フレームのViewProjection逆行列
		// DepthからWorld Positionを復元する
		//========================================================
		XMFLOAT4X4 InverseCurrentVP;

		//========================================================
		// 前フレームViewProjection
		// World → Previous Clip Position
		//========================================================
		XMFLOAT4X4 PreviousVP;
	};


private:

	bool CreateResource();

	bool CreatePipelineState();

	bool CreateObjectMotionPipelineState();

	bool CreateSkinnedObjectMotionPipelineState();

	bool CreateConstantBuffer();

	void Transition(D3D12_RESOURCE_STATES newState);


private:

	UINT mWidth = 0;
	UINT mHeight = 0;


	//============================================================
	// Velocity Texture
	//============================================================
	ID3D12Resource* mResource = nullptr;


	//============================================================
	// RTV
	//============================================================
	ID3D12DescriptorHeap* mRTVHeap = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE mRTV = {};


	//============================================================
	// SRV Heap
	// Screen Root Signatureがt0～t1の2Descriptorを要求するため
	// 2つ確保する
	// [0] Main Depth
	// [1] Main Depth（dummy）
	//============================================================
	ID3D12DescriptorHeap* mInputHeap = nullptr;


	//============================================================
	// MotionVector自身のSRV
	//============================================================
	ID3D12DescriptorHeap* mOutputSRVHeap = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE mOutputSRVCPU = {};
	D3D12_GPU_DESCRIPTOR_HANDLE mOutputSRVGPU = {};


	ID3D12PipelineState* mPipelineState = nullptr;

	// Rigid Object Motion Vector用PSO
	ID3D12PipelineState* mObjectMotionPSO = nullptr;
	// Skinned Object Motion
	ID3D12PipelineState* mSkinnedObjectMotionPSO = nullptr;

	//============================================================
	// Constant Buffer
	//============================================================
	ID3D12Resource* mConstantBuffer = nullptr;

	MotionVectorBuffer* mMappedBuffer = nullptr;


	//============================================================
	// 前フレームCamera VP
	//============================================================
	XMMATRIX mPreviousVP = XMMatrixIdentity();

	bool mHasHistory = false;

	D3D12_RESOURCE_STATES mState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
};