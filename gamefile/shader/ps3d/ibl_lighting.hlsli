#ifndef IBL_LIGHTING_HLSLI
#define IBL_LIGHTING_HLSLI

#include "ps_3d_bindings.hlsli"
#include "surface_data.hlsli"
#include "pixel_context.hlsli"
#include "../common/pbr_brdf.hlsli"
#include "../common/environment_common.hlsli"

//============================================================
// IBL計算結果
//============================================================
struct IBLLightingResult
{
    //========================================================
    // IrradianceによるDiffuse IBL
    //========================================================
    float3 diffuse;

    //========================================================
    // Prefiltered Environment + BRDF LUTによる
    // Specular IBL
    //========================================================
    float3 specular;

    //========================================================
    // Diffuse + Specular
    //========================================================
    float3 finalColor;

    //========================================================
    // BRDF LUT値
    //========================================================
    float2 integratedBRDF;

    //========================================================
    // Roughness対応Fresnel
    //========================================================
    float3 fresnel;

    //========================================================
    // Specular Occlusion
    //========================================================
    float specularOcclusion;

    //========================================================
    // IBL有効率
    //========================================================
    float enabled;
};


//============================================================
// Image Based Lighting評価
// EnvironmentParam:
// x : Environment / IBL Enable
// y : Diffuse IBL Intensity
// z : Prefiltered Environment Maximum Mip Level
// w : Specular IBL Intensity
//============================================================
IBLLightingResult EvaluateIBLLighting(SurfaceData surface,PixelContext context)
{
    IBLLightingResult result = (IBLLightingResult) 0;

    //============================================================
    // Runtime Environment Parameter
    //============================================================
    // Y軸Rotation
    const float environmentRotation = EnvironmentSceneParam.x;
    // HDR Exposure倍率
    const float environmentExposure = max(EnvironmentSceneParam.y, 0.0f);


    //============================================================
    // Environment使用フラグ
    // EnvironmentParam.x:
    //   0 = OFF
    //   1 = ON
    //============================================================
    const float useEnvironment = step(0.5f, EnvironmentParam.x);

    result.enabled = useEnvironment;

    //============================================================
    // Diffuse IBL強度
    // Irradiance Mapによる拡散環境光だけへ使用する
    //============================================================
    const float diffuseIBLIntensity = max(EnvironmentParam.y, 0.0f);

    //============================================================
    // Specular IBL強度
    //============================================================
    const float specularIBLIntensity = max(EnvironmentParam.w, 0.0f);


    //============================================================
    // Prefiltered Environment最大Mip
    //============================================================
    const float maximumPrefilterMip = max(EnvironmentParam.z, 0.0f);

    //============================================================
    // Lighting Normal
    //============================================================
    const float3 N = normalize(surface.shadingNormal);

    //============================================================
    // Pixel → Camera
    //============================================================
    const float3 V = normalize(context.viewDirection);


    //============================================================
    // NdotV
    // BRDF LUTやFresnelで使用する
    // 0にすると一部の計算が不安定になるため
    // 最低値を0.001にする
    //============================================================
    const float NdotV = max(saturate(dot(N, V)), 0.001f);


    //============================================================
    // Roughnessを考慮したFresnel
    // 通常のSchlickだけでなくRoughnessを考慮し
    // Rough Surfaceで環境反射が不自然に強くならないようにする
    //============================================================
    const float3 environmentFresnel =
        FresnelSchlickRoughness(
            NdotV,
            surface.F0,
            surface.roughness
        );

    result.fresnel = environmentFresnel;

    //============================================================
    // Diffuseへ回る割合
    // FresnelでSpecularへ反射されなかった成分だけをDiffuseへ回す
    // Metallic = 1の場合はDiffuseを持たない
    //============================================================
    const float3 diffuseRatio = (1.0f - environmentFresnel) * (1.0f - surface.metallic);

    //============================================================
    // Diffuse IBL Sampling Direction
    //============================================================
    const float3 irradianceSampleDirection =
        normalize(RotateEnvironmentDirectionY(N, environmentRotation));

    //============================================================
    // Irradiance
    //============================================================
    const float3 irradiance =
        txIrradianceEnvironment.SampleLevel(
            samLinear,
            irradianceSampleDirection,
            0.0f
        ).rgb;

    //============================================================
    // Lambert Diffuse
    //============================================================
    const float3 diffuseIBL = irradiance * surface.baseColor.rgb / PBR_PI;

    //============================================================
    // Diffuse IBL
    // AOはDiffuse環境光へ適用する
    //============================================================
    const float3 diffuseIBLResult =
        diffuseRatio * diffuseIBL * surface.ambientOcclusion;

    //============================================================
    // World空間の反射方向
    // reflect()へ渡す入射方向はCamera → Pixel方向なので
    // Pixel → CameraであるVを反転して使用する
    //============================================================
    const float3 reflectionDirection = normalize(reflect(-V, N));

    //============================================================
    // Roughness → Prefilter Mip
    // Roughness 0:
    //   Mip 0
    //   シャープな反射
    // Roughness 1:
    //   最終Mip
    //   大きくぼけた反射
    //============================================================
    const float prefilteredMipLevel = surface.roughness * maximumPrefilterMip;


    //============================================================
    // Specular IBL Sampling Direction
    // reflectionDirection自体はWorld空間で求め、
    // CubeMap参照時だけEnvironment Rotationを適用する
    //============================================================
    const float3 prefilteredSampleDirection =
        normalize(RotateEnvironmentDirectionY(reflectionDirection, environmentRotation));


    const float3 prefilteredEnvironment =
        txPrefilteredEnvironment.SampleLevel(
            samLinear,
            prefilteredSampleDirection,
            prefilteredMipLevel
        ).rgb;

    //============================================================
    // BRDF LUT
    // X = NdotV
    // Y = Roughness
    // R = Scale
    // G = Bias
    //============================================================
    const float2 integratedBRDF =
        txBRDFLUT.SampleLevel(samLinear, float2(NdotV, surface.roughness), 0.0f).rg;

    result.integratedBRDF = integratedBRDF;

    //============================================================
    // Split-Sum Specular IBL
    // Prefiltered Environment × Fresnel * BRDF.x + BRDF.y
    //============================================================
    const float3 specularIBL =
        prefilteredEnvironment * (environmentFresnel * integratedBRDF.x + integratedBRDF.y);


    //============================================================
    // Specular Occlusion
    // NdotV / Roughnessを考慮したSpecular Occlusionを使用する
    //============================================================
    const float specularOcclusion =
        ComputeSpecularOcclusion(
            NdotV,
            surface.ambientOcclusion,
            surface.roughness
        );

    result.specularOcclusion = specularOcclusion;

    const float3 specularIBLResult = specularIBL * specularOcclusion;

    //============================================================
    // Environment無効時のFallback Ambient
    //============================================================
    const float3 fallbackAmbient =
        surface.baseColor.rgb *
        (LightAmb.rgb + Ambient.rgb) *
        (1.0f - surface.metallic) *
        surface.ambientOcclusion;

    //============================================================
    // Diffuse IBL
    // Material固有強度 × Scene全体のHDR Exposure
    //============================================================
    const float3 enabledDiffuseIBL =
        diffuseIBLResult * diffuseIBLIntensity * environmentExposure;
    
    
    //============================================================
    // Specular IBL
    //============================================================
    const float3 enabledSpecularIBL =
        specularIBLResult * specularIBLIntensity * environmentExposure;
    
    
    //============================================================
    // Environment OFF時はFallback Ambient
    //============================================================
    result.diffuse =
        lerp(
            fallbackAmbient,
            enabledDiffuseIBL,
            useEnvironment
        );
    
    
    //============================================================
    // Environment OFFならSpecular IBLなし
    //============================================================
    result.specular = enabledSpecularIBL * useEnvironment;

    //============================================================
    // Final IBL
    //============================================================
    result.finalColor = result.diffuse + result.specular;

    return result;
}


#endif