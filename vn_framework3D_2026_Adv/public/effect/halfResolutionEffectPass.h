#pragma once

//============================================================
// Half Resolution Effect Pass
// 大量の半透明Particleを1/2解像度で描画し
// Main HDRへAdditive Compositeする
//============================================================
class HalfResolutionEffectPass
{
public:

	static bool Initialize(UINT fullWidth, UINT fullHeight);

	static void Terminate();


	//============================================================
	// Half Resolution描画開始
	// ・Main DepthをHalf DepthへDownsample
	// ・Half HDRをClear
	// ・Half HDR + Half DSVをbind
	//============================================================
	static void Begin();


	//============================================================
	// Half Resolution描画終了
	// Half HDRをMain HDRへAdditive Compositeする
	//============================================================
	static void EndAndComposite();


private:

	static bool CreateResources();

	static bool CreateDepthDownsamplePSO();

	static bool CreateCompositePSO();

	static void DownsampleDepth();

	static void TransitionColor(D3D12_RESOURCE_STATES afterState);


private:

	static UINT sFullWidth;
	static UINT sFullHeight;

	static UINT sHalfWidth;
	static UINT sHalfHeight;


	//============================================================
	// Half Resolution HDR
	//============================================================
	static ID3D12Resource* sColorResource;

	//============================================================
	// Half Resolution Depth
	//============================================================
	static ID3D12Resource* sDepthResource;

	//============================================================
	// RTV
	//============================================================
	static ID3D12DescriptorHeap* sRTVHeap;

	//============================================================
	// DSV
	//============================================================
	static ID3D12DescriptorHeap* sDSVHeap;


	//============================================================
	// SRV Heap
	//
	// [0] Main Depth
	// [1] Half Effect Color
	//============================================================
	static ID3D12DescriptorHeap* sSRVHeap;

	static UINT sSRVDescriptorSize;

	static ID3D12PipelineState* sDepthDownsamplePSO;
	static ID3D12PipelineState* sCompositePSO;

	static D3D12_RESOURCE_STATES sColorState;
};