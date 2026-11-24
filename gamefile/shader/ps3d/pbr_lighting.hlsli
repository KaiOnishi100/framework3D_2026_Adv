#ifndef PBR_LIGHTING_HLSLI
#define PBR_LIGHTING_HLSLI

#include "surface_data.hlsli"
#include "pixel_context.hlsli"
#include "directional_lighting.hlsli"
#include "local_lighting.hlsli"
#include "ibl_lighting.hlsli"

//============================================================
// PBR全体の計算結果
// 最終色だけでなく、デバッグ表示に必要な中間結果も保持する
//============================================================
struct PBRLightingResult
{
    //========================================================
    // Directional Light
    //========================================================

    // Directional Diffuse
    float3 directionalDiffuse;

    // Directional Specular
    float3 directionalSpecular;

    // Diffuse + Specular
    float3 directionalFinal;


    //========================================================
    // Local Lights
    // デバッグ用としてPoint / Spotの最終結果も保持する
    //========================================================

    // Point Diffuse + Point Specular
    float3 pointLights;

    // Spot Diffuse + Spot Specular
    float3 spotLights;

    // Point + Spot
    float3 localFinal;


    //========================================================
    // Image Based Lighting
    //========================================================

    // Diffuse Irradiance
    float3 diffuseIBL;

    // Prefiltered Specular IBL
    float3 specularIBL;

    // Diffuse + Specular IBL
    float3 iblFinal;


    //========================================================
    // Emissive
    // SSS Diffusionの対象にはしない
    //========================================================
    float3 emissive;


    //========================================================
    // デバッグ用
    //========================================================
    float NdotL;

    float2 integratedBRDF;

    float specularOcclusion;

    uint evaluatedLocalLightCount;

    //========================================================
    // Screen-Space SSS用
    // Diffuse成分だけを全部まとめたもの
    // Directional Diffuse + Point Diffuse + Spot Diffuse + Diffuse IBL
    // Screen-Space SSSではここをBlurする
    //========================================================
    float3 diffuseFinal;

    //========================================================
    // Specular成分だけをまとめたもの
    // Directional Specular + Point Specular + Spot Specular + Specular IBL
    // こちらはBlurしない
    //========================================================
    float3 specularFinal;

    //========================================================
    // 最終PBR色
    // diffuseFinal + specularFinal + emissive
    //========================================================
    float3 finalColor;
};

//============================================================
// PBR Lighting全体を評価
// main()では基本的にこの関数だけを呼び出す
//============================================================
PBRLightingResult EvaluatePBRLighting(
    SurfaceData surface,
    PixelContext context,
    float directionalShadowVisibility
)
{
    //========================================================
    // 全メンバ0初期化
    //========================================================
    PBRLightingResult result = (PBRLightingResult) 0;


    //============================================================
    // Directional Light
    //============================================================
    const DirectionalLightingResult directional =
        EvaluateDirectionalLighting(
            surface,
            context,
            directionalShadowVisibility
        );


    //========================================================
    // Diffuse / Specularを個別に保存
    //========================================================
    result.directionalDiffuse = directional.diffuse;
    result.directionalSpecular = directional.specular;

    //========================================================
    // Directional最終結果
    //========================================================
    result.directionalFinal = directional.finalColor;
    result.NdotL = directional.NdotL;


    //============================================================
    // Point / Spot Local Lights
    //============================================================
    const LocalLightingResult local = EvaluateLocalLighting(surface, context);


    //========================================================
    // Point Lightの最終結果
    //========================================================
    result.pointLights = local.pointDiffuse + local.pointSpecular;

    //========================================================
    // Spot Lightの最終結果
    //========================================================
    result.spotLights = local.spotDiffuse + local.spotSpecular;

    //========================================================
    // 全Local Light
    //========================================================
    result.localFinal = local.finalColor;
    result.evaluatedLocalLightCount = local.evaluatedLightCount;


    //============================================================
    // Image Based Lighting
    //============================================================
    const IBLLightingResult ibl = EvaluateIBLLighting(surface, context);

    result.diffuseIBL = ibl.diffuse;
    result.specularIBL = ibl.specular;
    result.iblFinal = ibl.finalColor;
    result.integratedBRDF = ibl.integratedBRDF;
    result.specularOcclusion = ibl.specularOcclusion;


    //============================================================
    // Emissive
    // EmissiveはMaterial自身が発光する成分なので
    // Subsurface Diffusion対象には含めない
    //============================================================
    result.emissive = surface.emissive;

    //============================================================
    // Diffuse成分を統合
    //============================================================
    result.diffuseFinal = result.directionalDiffuse + local.diffuseFinal + result.diffuseIBL;

    //============================================================
    // Specular成分を統合
    // Screen-Space SSSではBlurしない
    //============================================================
    result.specularFinal = result.directionalSpecular + local.specularFinal + result.specularIBL;

    //============================================================
    // PBR最終結果
    //============================================================
    result.finalColor = result.diffuseFinal + result.specularFinal;

    return result;
}

#endif