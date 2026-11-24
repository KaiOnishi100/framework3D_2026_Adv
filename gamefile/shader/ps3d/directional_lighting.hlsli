#ifndef DIRECTIONAL_LIGHTING_HLSLI
#define DIRECTIONAL_LIGHTING_HLSLI

#include "ps_3d_bindings.hlsli"
#include "surface_data.hlsli"
#include "pixel_context.hlsli"
#include "../common/pbr_brdf.hlsli"

//============================================================
// Directional LightのPBR計算結果
//============================================================
struct DirectionalLightingResult
{
	// 平行光源の拡散反射
    float3 diffuse;

	// 平行光源の鏡面反射
    float3 specular;

	// 法線とライト方向の内積
	// デバッグ表示にも使用する
    float NdotL;

	// Fresnel項
    float3 fresnel;

	// Diffuse + Specular
    float3 finalColor;
};

//============================================================
// Directional Light PBR評価
// surface: BaseColor、Normal、Metallic、Roughness、F0
// context: View方向、Directional Light方向
// shadowVisibility:
//   Cascade Shadowの結果
//   0 = 完全な影
//   1 = 完全に明るい
//============================================================
DirectionalLightingResult EvaluateDirectionalLighting(
	SurfaceData surface,
	PixelContext context,
	float shadowVisibility
)
{
    DirectionalLightingResult result = (DirectionalLightingResult) 0;

	//============================================================
	// 使用する方向を正規化
	//============================================================
    const float3 N = normalize(surface.shadingNormal);
    const float3 V = normalize(context.viewDirection);
    const float3 L = normalize(context.directionalLightDirection);

	//============================================================
	// NdotL
	//============================================================
    const float NdotL = saturate(dot(N, L));
    const float NdotV = saturate(dot(N, V));

    result.NdotL = NdotL;

	//============================================================
	// ライトが表面の裏側にある場合
	//============================================================
    if (NdotL <= 0.0f || NdotV <= 0.0f)
    {
        return result;
    }

	//============================================================
	// Half Vector
	//============================================================
    const float3 H = normalize(V + L);

    const float HdotV = saturate(dot(H, V));

	//============================================================
	// Cook-Torrance GGX
	//============================================================
    const float distribution =
		DistributionGGX(
			N,
			H,
			surface.roughness
		);

    const float geometry =
		GeometrySmith(
			N,
			V,
			L,
			surface.roughness
		);

    const float3 fresnel =
		FresnelSchlick(
			HdotV,
			surface.F0
		);

    result.fresnel = fresnel;

	//============================================================
	// Specular BRDF
	//============================================================
    const float3 specularNumerator = distribution * geometry * fresnel;

    const float specularDenominator = max(4.0f * NdotV * NdotL, 0.0001f);

    const float3 specularBRDF = specularNumerator / specularDenominator;

	//============================================================
	// エネルギー保存
	// Fresnel反射されなかった光だけをDiffuseへ回す
	// Metallicが1の場合はDiffuseを0にする
	//============================================================
    const float3 specularRatio = fresnel;

    const float3 diffuseRatio = (1.0f - specularRatio) * (1.0f - surface.metallic);

    const float3 diffuseBRDF = diffuseRatio * surface.baseColor.rgb / PBR_PI;

	//============================================================
	// Directional LightのRadiance
	// LightCol.rgbに強度込みの値が設定されている構成
	//============================================================
    const float3 radiance = max(LightCol.rgb, 0.0f);

    const float safeShadowVisibility = saturate(shadowVisibility);

	//============================================================
	// Diffuse
	//============================================================
    result.diffuse =
		diffuseBRDF *
		radiance *
		NdotL *
		safeShadowVisibility;

	//============================================================
	// Specular
	//============================================================
    result.specular =
		specularBRDF *
		radiance *
		NdotL *
		safeShadowVisibility;

    result.finalColor = result.diffuse + result.specular;

    return result;
}

#endif