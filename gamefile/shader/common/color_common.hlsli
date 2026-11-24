#ifndef COLOR_COMMON_HLSLI
#define COLOR_COMMON_HLSLI

//============================================================
// sRGBからLinearへの簡易変換
//============================================================
float3 SRGBToLinear(float3 color)
{
    return pow(max(color, 0.0f), 2.2f);
}


//============================================================
// LinearからsRGBへ変換
// 現在はTone Mapping Pass側でGamma処理を行うため
// ps_3d.pshでは基本的に使用しない
//============================================================
float3 LinearToSRGB(float3 color)
{
    return pow(max(color, 0.0f), 1.0f / 2.2f);
}

#endif