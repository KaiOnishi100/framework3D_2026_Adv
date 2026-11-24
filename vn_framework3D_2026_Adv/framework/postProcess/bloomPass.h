#pragma once

class BloomPass
{
public:
	bool Initialize();
	void Terminate();

	//============================================================
	// 指定されたHDR SceneからBloom Textureを生成する
	// BackBufferには描画しない
	//============================================================
	void Generate(
		ID3D12DescriptorHeap* inputSRVHeap,
		D3D12_GPU_DESCRIPTOR_HANDLE inputSRV
	);

	//============================================================
	// ToneMappingPass用Getter
	//============================================================
	ID3D12Resource* GetResultResource() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetResultSRVCPU() const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetResultSRVGPU() const;

	ID3D12DescriptorHeap* GetSRVHeap() const;

private:
	void CreateResources();
	void CreatePipelineStates();

	void ChangeResourceState(
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES beforeState,
		D3D12_RESOURCE_STATES afterState
	);

	void PassBright(ID3D12DescriptorHeap* inputSRVHeap, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void PassBlurH();
	void PassBlurV();

private:
	ID3D12Resource* mBloomRT[2] = { nullptr, nullptr };

	ID3D12DescriptorHeap* mRTVHeap = nullptr;
	ID3D12DescriptorHeap* mSRVHeap = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE mRTV[2] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE mSRVCPU[2] = {};
	D3D12_GPU_DESCRIPTOR_HANDLE mSRVGPU[2] = {};
	D3D12_GPU_DESCRIPTOR_HANDLE mSRVGpuStart = {};

	ID3D12PipelineState* mPSO_Bright = nullptr;
	ID3D12PipelineState* mPSO_BlurH = nullptr;
	ID3D12PipelineState* mPSO_BlurV = nullptr;

	UINT mBloomWidth = 0;
	UINT mBloomHeight = 0;

	bool mInitialized = false;
};