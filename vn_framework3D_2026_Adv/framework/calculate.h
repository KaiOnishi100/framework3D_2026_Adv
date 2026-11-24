#pragma once

///-------------------------------- 
///計算用便利関数(拡張予定)
///--------------------------------

namespace Calculate
{
	//--------------------------------------------
    // 0～1の範囲にクランプ
    //--------------------------------------------
	float Clamp(float v, float a, float b);
	float Clamp01(float t);

	//--------------------------------------------
	// 線形補間
	//--------------------------------------------
	float Lerp(float a, float b, float t);

	//--------------------------------------------
	// SmoothStep
	//--------------------------------------------
	float SmoothStep01(float t);

	//============================================================
	// Direct3D 12サブリソース番号計算
	// D3D12CalcSubresourceと同じ目的で使用する
	// d3dx12.hへ依存しない
	//============================================================
	constexpr UINT CalculateD3D12Subresource(
		UINT mipSlice,
		UINT arraySlice,
		UINT planeSlice,
		UINT mipLevels,
		UINT arraySize
	)
	{
		//============================================================
		// Subresourceの並び:
		//
		//   Mip Slice
		//   ↓
		//   Array Slice
		//   ↓
		//   Plane Slice
		//
		// という順序で格納される
		//============================================================
		return
			mipSlice +
			arraySlice *
			mipLevels +
			planeSlice *
			mipLevels *
			arraySize;
	}

}