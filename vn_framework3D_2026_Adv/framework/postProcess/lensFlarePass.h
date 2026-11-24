#pragma once

//============================================================
// LensFlarePass
// Directional LightのScreen位置を元に
// HDR SceneへLens Flareを加算するPostProcess Pass
//============================================================
class LensFlarePass
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
	// Directional LightのScreen位置を更新
	// 毎フレームPostProcess前に呼ぶ
	//============================================================
	void UpdateDirectionalLight();

	//============================================================
	// Lens Flare適用
	//============================================================
	void ApplyToRT(
		ID3D12Resource* inputResource,
		D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
		UINT width,
		UINT height
	);

	//============================================================
	// 全体Intensity
	//============================================================
	void SetIntensity(float intensity);

	//============================================================
	// Flare Color
	//============================================================
	void SetColor(const XMFLOAT3& color);

	//============================================================
	// Temporal処理用Delta Time
	//============================================================
	void SetDeltaTime(float deltaTime);

	//============================================================
	// Screen Edge Fade幅
	//============================================================
	void SetEdgeFadeWidth(float width);

	//============================================================
	// Angular Fade
	//============================================================
	void SetAngularFade(float start,float end);

private:

	//============================================================
	// GPU用Constant Buffer
	//============================================================
	struct alignas(16) LensFlareBuffer
	{
		XMFLOAT4 LightScreenParam;//========================================================
		// xy = Sun Screen UV
		// z  = Enable
		// w  = Intensity
		//========================================================
		XMFLOAT4 FlareColor;//========================================================
		// rgb = Flare Color
		//========================================================
		XMFLOAT4 FlareShapeParam;//========================================================
		// x = Core Radius
		// y = Halo Radius
		// z = Ghost Strength
		// w = Streak Strength
		//========================================================
		XMFLOAT4 ScreenParam;//========================================================
		// xy = Screen Size
		// zw = Inverse Screen Size
		//========================================================
		XMFLOAT4 TemporalParam;//========================================================
		// x = Delta Time
		// y = Fade In Speed
		// z = Fade Out Speed
		// w = History Valid
		//========================================================
		XMFLOAT4 FadeParam;//========================================================
	// x = Screen Edge Fade Width
	// y = Angular Fade Start
	// z = Angular Fade End
	// w = 未使用
	//========================================================
	};

private:

	bool CreatePipelineState();

	bool CreateConstantBuffer();

	bool CreateRootSignature();

	bool CreateDescriptorHeap();

	//============================================================
	// 1x1 Temporal Visibility Texture作成
	//============================================================
	bool CreateVisibilityHistoryResources();

	//============================================================
	// Occlusion PSO作成
	//============================================================
	bool CreateOcclusionPipelineState();

	//============================================================
	// Visibility Resource State変更
	//============================================================
	void TransitionVisibilityResource(UINT index,D3D12_RESOURCE_STATES afterState);

	//============================================================
	// Temporal Occlusionを更新
	//============================================================
	void RenderOcclusionVisibility();

private:

	ID3D12PipelineState* mPipelineState = nullptr;

	ID3D12PipelineState* mOcclusionPipelineState = nullptr;// Lens Flare Occlusion PSO

	ID3D12Resource* mConstantBuffer = nullptr;

	//============================================================
	// Temporal Visibility履歴
	// Ping-Pong方式:
	// Frame N:
	//   Read  = History 0
	//   Write = History 1
	// Frame N+1:
	//   Read  = History 1
	//   Write = History 0
	//============================================================
	ID3D12Resource* mVisibilityHistory[2] = { nullptr,nullptr };


	//============================================================
	// Lens Flare専用Root Signature
	// RootParameter 0:
	//   SRV Table
	//   t0 = HDR Scene
	//   t1 = Main Depth
	// RootParameter 1: CBV b0
	//============================================================
	ID3D12RootSignature* mRootSignature = nullptr;

	//============================================================
	// Lens Flare専用SRV Heap
	// Descriptor 0: HDR Scene
	// Descriptor 1: Main Depth
	//============================================================
	ID3D12DescriptorHeap* mSRVHeap = nullptr;

	//============================================================
	// 1x1 Visibility用RTV Heap
	//============================================================
	ID3D12DescriptorHeap* mVisibilityRTVHeap = nullptr;

	//============================================================
	// 各履歴TextureのResource State
	//============================================================
	D3D12_RESOURCE_STATES mVisibilityStates[2] = { D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE };

	//============================================================
	// SRV Descriptor Size
	//============================================================
	UINT mSRVDescriptorSize = 0u;

	LensFlareBuffer* mMappedBuffer = nullptr;

	bool mInitialized = false;

	float mIntensity = 3.0f;

	XMFLOAT3 mColor =
	{
		1.0f,
		0.75f,
		0.35f
	};

	//============================================================
	// 現在読み込む履歴
	//============================================================
	UINT mVisibilityReadIndex = 0u;

	//============================================================
	// 現在書き込む履歴
	//============================================================
	UINT mVisibilityWriteIndex = 1u;

	//============================================================
	// 一度でもOcclusion履歴を書いたか
	//============================================================
	bool mVisibilityHistoryValid = false;

	//============================================================
	// Temporal Parameters
	//============================================================
	float mDeltaTime = 1.0f / 60.0f;
	float mFadeInSpeed = 8.0f;
	float mFadeOutSpeed = 14.0f;

	static_assert(sizeof(LensFlareBuffer) == 96u, "LensFlareBuffer size mismatch.");
};

