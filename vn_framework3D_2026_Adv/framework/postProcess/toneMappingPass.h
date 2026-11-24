#pragma once

//============================================================
// ToneMappingPass
// HDR SceneとBloom Textureを合成し
// SDR BackBufferへ出力する最終Pass
//============================================================
class ToneMappingPass
{
public:
	bool Initialize();
	void Terminate();

	//============================================================
	// HDR SceneとBloom Resourceを受け取り、
	// ToneMapping用Descriptor HeapへSRVを直接作成する
	//============================================================
	void Apply(
		ID3D12Resource* sceneResource,
		ID3D12Resource* bloomResource,
		D3D12_GPU_VIRTUAL_ADDRESS postEffectCB
	);
private:
	bool CreateDescriptorHeap();
	bool CreatePipelineState();

private:
	//============================================================
	// [0] t0 : HDR Scene
	// [1] t1 : Bloom
	//============================================================
	ID3D12DescriptorHeap* mSRVHeap = nullptr;
	ID3D12PipelineState* mPipelineState = nullptr;
	UINT mDescriptorSize = 0;
	bool mInitialized = false;
};