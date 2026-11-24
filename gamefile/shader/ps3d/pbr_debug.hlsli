#ifndef PBR_DEBUG_HLSLI
#define PBR_DEBUG_HLSLI

#include "surface_data.hlsli"
#include "cascade_shadow.hlsli"
#include "pbr_lighting.hlsli"

//============================================================
// PBRデバッグ表示
// 各PBR要素を最終合成前に個別表示する
//============================================================
float3 SelectPBRDebugColor(
	int debugMode,
	float3 linearBaseColor,
	float3 worldNormal,
	float metallic,
	float roughness,
	float ambientOcclusion,
	float3 emissive,
	float3 directDiffuse,
	float3 directSpecular,
	float3 diffuseIBL,
	float3 specularIBL,
	float shadowFactor,
	float NdotL,
	float3 F0,
	float2 integratedBRDF,
	float3 pointLights,
	float3 spotLights,
	float3 localLights,
	float3 finalColor
)
{
	//========================================================
	// 0 : 最終結果
	//========================================================
    if (debugMode == 0)
    {
        return finalColor;
    }

	//========================================================
	// 1 : BaseColor
	// Linear BaseColorを表示する
	// 最後のTone MappingとGamma補正を通る
	//========================================================
    if (debugMode == 1)
    {
        return linearBaseColor;
    }

	//========================================================
	// 2 : World Normal
	// -1～1を0～1へ変換して色として表示する
	//========================================================
    if (debugMode == 2)
    {
        return normalize(worldNormal) * 0.5f + 0.5f;
    }

	//========================================================
	// 3 : Metallic
	//========================================================
    if (debugMode == 3)
    {
        return metallic.xxx;
    }

	//========================================================
	// 4 : Roughness
	//========================================================
    if (debugMode == 4)
    {
        return roughness.xxx;
    }

	//========================================================
	// 5 : Ambient Occlusion
	//========================================================
    if (debugMode == 5)
    {
        return ambientOcclusion.xxx;
    }

	//========================================================
	// 6 : Emissive
	//========================================================
    if (debugMode == 6)
    {
        return emissive;
    }

	//========================================================
	// 7 : Direct Diffuse
	//========================================================
    if (debugMode == 7)
    {
        return directDiffuse;
    }

	//========================================================
	// 8 : Direct Specular
	//========================================================
    if (debugMode == 8)
    {
        return directSpecular;
    }

	//========================================================
	// 9 : Diffuse IBL
	//========================================================
    if (debugMode == 9)
    {
        return diffuseIBL;
    }

	//========================================================
	// 10 : Specular IBL
	//========================================================
    if (debugMode == 10)
    {
        return specularIBL;
    }

	//========================================================
	// 11 : Shadow
	// 白 = 明るい
	// 黒 = 影
	//========================================================
    if (debugMode == 11)
    {
        return shadowFactor.xxx;
    }

	//========================================================
	// 12 : NdotL
	//========================================================
    if (debugMode == 12)
    {
        return NdotL.xxx;
    }

	//========================================================
	// 13 : F0
	// 非金属なら約0.04の暗い灰色
	// 金属ならBaseColorに近い色になる
	//========================================================
    if (debugMode == 13)
    {
        return F0;
    }

	//========================================================
	// 14 : BRDF LUT
	//
	// R = Scale
	// G = Bias
	//========================================================
    if (debugMode == 14)
    {
        return float3(integratedBRDF.r, integratedBRDF.g, 0.0f);
    }
	// Point Lights
    if (debugMode == 15)
    {
        return pointLights;
    }

	// Spot Lights
    if (debugMode == 16)
    {
        return spotLights;
    }

	// Local Lights全体
    if (debugMode == 17)
    {
        return localLights;
    }
	//========================================================
	// 不正なモードなら最終結果
	//========================================================
    return finalColor;
}

//============================================================
// Cascade番号デバッグ色
// Cascade 0 : 赤
// Cascade 1 : 緑
// Cascade 2 : 青
// 境界では現在Cascadeと次Cascadeの色を補間する
//============================================================
float3 GetCascadeDebugColor(CascadeShadowResult cascadeShadow)
{
    float3 currentColor = float3(1.0f, 0.0f, 0.0f);

    float3 nextColor = currentColor;

	//========================================================
	// 現在Cascade色
	//========================================================
    if (cascadeShadow.cascadeIndex == 1)
    {
        currentColor = float3(0.0f, 1.0f, 0.0f);
    }
    else if (cascadeShadow.cascadeIndex == 2)
    {
        currentColor = float3(0.0f, 0.0f, 1.0f);
    }

	//========================================================
	// 次Cascade色
	//========================================================
    if (cascadeShadow.nextCascadeIndex == 1)
    {
        nextColor = float3(0.0f, 1.0f, 0.0f);
    }
    else if (cascadeShadow.nextCascadeIndex == 2)
    {
        nextColor = float3(0.0f, 0.0f, 1.0f);
    }

    return lerp(
		currentColor,
		nextColor,
		cascadeShadow.blendRate
	);
}

//============================================================
// Point Shadow Face番号を色で表示
// Face 0 +X : 赤
// Face 1 -X : 暗い赤
// Face 2 +Y : 緑
// Face 3 -Y : 暗い緑
// Face 4 +Z : 青
// Face 5 -Z : 暗い青
//============================================================
float3 DebugPointShadowFace(uint faceIndex)
{
    if (faceIndex == 0u)
    {
        return float3(1.0f, 0.0f, 0.0f);
    }

    if (faceIndex == 1u)
    {
        return float3(0.4f, 0.0f, 0.0f);
    }

    if (faceIndex == 2u)
    {
        return float3(0.0f, 1.0f, 0.0f);
    }

    if (faceIndex == 3u)
    {
        return float3(0.0f, 0.4f, 0.0f);
    }

    if (faceIndex == 4u)
    {
        return float3(0.0f, 0.0f, 1.0f);
    }

    return float3(0.0f, 0.0f, 0.4f);
}

#endif