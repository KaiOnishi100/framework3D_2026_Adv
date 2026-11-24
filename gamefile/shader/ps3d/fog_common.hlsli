#ifndef FOG_COMMON_HLSLI
#define FOG_COMMON_HLSLI

#include "ps_3d_bindings.hlsli"
#include "../common/color_common.hlsli"

//============================================================
// Linear Fogの係数を計算
//
// FogInfo:
//
// x : Fog開始距離
// y : Fog終了距離
// z : Fog濃度
// w : 現在の処理では未使用
//
// 戻り値:
//
// 0.0 = Fogなし
// 1.0 = 完全にFogColor
//============================================================
float CalculateFogRate(float viewDepth)
{
	//============================================================
	// Fog開始・終了距離
	//============================================================
    const float fogStart = FogInfo.x;

    const float fogEnd = max(FogInfo.y, fogStart + 0.0001f);

	//============================================================
	// Fogの濃度
	// vnFogが無効な場合はFogInfoが0になるため
	// FogInfo.zも0になり、最終的なFog率は0になる
	//============================================================
    const float fogDensity = saturate(FogInfo.z);

	//============================================================
	// 負のView深度を防止
	//============================================================
    const float safeViewDepth = max(viewDepth, 0.0f);

	//============================================================
	// Fog開始地点から終了地点までを0～1へ変換
	// viewDepth <= fogStart: 0
	// viewDepth >= fogEnd: 1
	//============================================================
    float fogRate =
		(safeViewDepth - fogStart) / max(fogEnd - fogStart, 0.0001f);

    fogRate = saturate(fogRate);

	//============================================================
	// 最後にFog濃度を適用
	// Densityが0.5なら最遠距離でも50%だけFogColorになる
	//============================================================
    fogRate *= fogDensity;

    return saturate(fogRate);
}

//============================================================
// Fogを適用
// sourceColor: Lighting計算後の色
// viewDepth: Camera View空間の前方距離
// usePBR:
//   0 = Legacy Lighting
//   1 = PBR Lighting
// 現在の見た目を維持するため
// PBR時だけFogColorをLinearへ変換する
//============================================================
float3 ApplyFog(float3 sourceColor,float viewDepth,float usePBR)
{
    const float fogRate = CalculateFogRate(viewDepth);

	//============================================================
	// Fog率が0なら、そのまま返す
	// Fog無効時の不要な色変換を避ける
	//============================================================
    if (fogRate <= 0.0f)
    {
        return sourceColor;
    }

	//============================================================
	// Legacy用FogColor
	// 既存の色空間を維持する
	//============================================================
    const float3 legacyFogColor = max(FogColor.rgb, 0.0f);

	//============================================================
	// PBR用FogColor
	// PBR計算結果はLinear HDRなので
	// FogColorもLinearへ変換してから合成する
	//============================================================
    const float3 linearFogColor = SRGBToLinear(saturate(FogColor.rgb));

	//============================================================
	// Legacy / PBRでFogColorの色空間を切り替える
	//============================================================
    const float3 outputFogColor =
		lerp(
			legacyFogColor,
			linearFogColor,
			saturate(usePBR)
		);

	//============================================================
	// Lighting結果とFogColorを補間
	//============================================================
    return lerp(sourceColor, outputFogColor, fogRate);
}

#endif