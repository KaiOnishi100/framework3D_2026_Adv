#pragma once

//============================================================
// レンダーターゲットで使用するFormatの共通定義
// Formatを各クラスへ直接書くと
// ResourceとPSOのFormat変更漏れが発生しやすいため
// 一か所で管理する
//============================================================
namespace RenderFormat
{
	//========================================================
	// HDRシーンカラー
	// 1.0を超える光の強度を保持する
	//========================================================
	inline constexpr DXGI_FORMAT SceneHDR = DXGI_FORMAT_R16G16B16A16_FLOAT;

	//========================================================
	// 最終表示用のSwapChain Format
	// Tone Mapping後の0～1の色を保存する
	//========================================================
	inline constexpr DXGI_FORMAT BackBuffer = DXGI_FORMAT_R8G8B8A8_UNORM;

	inline DXGI_FORMAT ConvertToLinearFormat(DXGI_FORMAT format) 
	{
		switch (format)
		{
			//========================================================
			// 非圧縮RGBA
			//========================================================
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM;

			//========================================================
			// BGRA形式
			//========================================================
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM;

		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8X8_UNORM;

			//========================================================
			// BC圧縮形式
			//========================================================
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return DXGI_FORMAT_BC1_UNORM;

		case DXGI_FORMAT_BC2_UNORM_SRGB:
			return DXGI_FORMAT_BC2_UNORM;

		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return DXGI_FORMAT_BC3_UNORM;

		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return DXGI_FORMAT_BC7_UNORM;

			//========================================================
			// すでにLinear形式の場合そのまま返す
			//========================================================
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:

		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16G16_UNORM:

		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC7_UNORM:

		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return format;

		default:
			//====================================================
			// 未対応形式
			// UNKNOWNを返して、呼び出し側でエラーにする
			//====================================================
			return DXGI_FORMAT_UNKNOWN;
		}
	}
}

