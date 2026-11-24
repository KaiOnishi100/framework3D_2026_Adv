#pragma once

//============================================================
// MotionBlurPass
// Scene HDRとMotion Vectorを使用して画面空間のMotion Blurを生成する
// 入力:
//   t0 : Scene HDR
//   t1 : Motion Vector
// 出力:
//   SceneHDR FormatのRenderTarget
// Motion Vector:
//   CurrentUV - PreviousUV
//============================================================
class MotionBlurPass
{
public:

	//============================================================
	// 初期化
	//============================================================
	bool Initialize();

	//============================================================
	// 終了
	//============================================================
	void Terminate();


	//============================================================
	// Motion Blur適用
	// inputSceneResource: HDR Scene
	// motionVectorResource: Motion Vector Buffer
	// depthResource: Main Depth Buffer
	// outputRTV: PingPong Write側
	//============================================================
	void ApplyToRT(
		ID3D12Resource* inputSceneResource,
		ID3D12Resource* motionVectorResource,
		ID3D12Resource* depthResource,
		D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
		UINT width,
		UINT height
	);


	//============================================================
	// Blurの強さ
	// 0: Blurなし
	// 1: 基準
	// 2: 2倍
	//============================================================
	void SetStrength(float strength);


	//============================================================
	// 最大Blur幅
	// Pixel単位
	// Cameraを高速回転させた場合でも
	// 画面全体が極端に引き伸ばされるのを防ぐ
	//============================================================
	void SetMaxBlurPixels(float pixels);


	//============================================================
	// Sample数
	//============================================================
	void SetSampleCount(int count);


	//============================================================
	// Motion Vector倍率
	// 通常1.0 演出として強調したい場合に変更する
	//============================================================
	void SetVelocityScale(float scale);

	void SetDepthThreshold(float threshold);

	void SetVelocityThreshold(float threshold);

private:

	//============================================================
	// GPU Constant Buffer
	//============================================================
	struct alignas(16) MotionBlurBuffer
	{
		//========================================================
		// x : Strength
		// y : Max Blur Pixels
		// z : Sample Count
		// w : Velocity Scale
		//========================================================
		XMFLOAT4 MotionParam;

		//========================================================
		// x : Width
		// y : Height
		// z : 1 / Width
		// w : 1 / Height
		//========================================================
		XMFLOAT4 ScreenParam;

		//========================================================
		// Blur境界判定
		//
		// x : Depth Threshold
		// y : Velocity Difference Threshold
		// z : Depth Reject Strength
		// w : Velocity Reject Strength
		//========================================================
		XMFLOAT4 RejectionParam;
	};


private:

	bool CreateDescriptorHeap();

	bool CreateConstantBuffer();

	bool CreatePipelineState();

	bool CreateRootSignature();


private:

	//============================================================
	// このPass専用Descriptor Heap
	// Screen Root Signatureは
	//   t0
	//   t1
	// の連続したDescriptorを要求する
	// Scene SRVとMotionVector SRVは別Heapに存在するため
	// Apply時にこのHeapへCopyする
	//============================================================
	ID3D12DescriptorHeap* mDescriptorHeap = nullptr;

	UINT mDescriptorSize = 0u;

	//============================================================
	// MotionBlur専用Root Signature
	// Root[0]
	//   t0 : Scene HDR
	//   t1 : Motion Vector
	//   t2 : Main Depth
	// Root[1]
	//   b0 : MotionBlurBuffer
	// Sampler
	//   s0 : Linear Clamp
	//   s1 : Point Clamp
	//============================================================
	ID3D12RootSignature* mRootSignature =nullptr;

	ID3D12PipelineState* mPipelineState = nullptr;

	ID3D12Resource* mConstantBuffer = nullptr;

	MotionBlurBuffer* mMappedBuffer = nullptr;

	int mSampleCount = 8;

	float mStrength = 1.0f;
	float mMaxBlurPixels = 32.0f;
	float mDepthThreshold = 0.0025f;// Depth境界の許容差
	float mVelocityThreshold = 0.015f;// Velocity差の許容値
	float mVelocityScale = 1.0f;

	bool mInitialized = false;
};