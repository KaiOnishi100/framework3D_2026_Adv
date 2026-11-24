#ifndef SUBSURFACE_LIGHTING_HLSLI
#define SUBSURFACE_LIGHTING_HLSLI

#include "ps_3d_bindings.hlsli"
#include "surface_data.hlsli"
#include "pixel_context.hlsli"
#include "point_shadow.hlsli"
#include "spot_shadow.hlsli"

//============================================================
// Local Light SSS結果
//============================================================
struct LocalSubsurfaceResult
{
    // Point LightによるSSS
    float3 SSSpoint;

    // Spot LightによるSSS
    float3 SSSspot;

    // Point + Spot
    float3 finalColor;

    // デバッグ用
    uint evaluatedLightCount;
};

//============================================================
// 全SSS結果
//============================================================
struct SubsurfaceLightingResult
{
    float3 directional;

    float3 SSSpoint;

    float3 SSSspot;

    float3 local;

    float3 finalColor;
};


//============================================================
// SSS共通Transmission計算
// Directional / Point / Spotで共通する
// ・Back Lighting
// ・View Alignment
// ・Thickness
// ・Scatter Color
// をここへ集約する
//============================================================
float3 EvaluateSubsurfaceTransmission(
    SurfaceData surface,
    float3 geometricNormal,
    // Pixel → Light
    float3 lightDirection,
    // Pixel → Camera
    float3 viewDirection,
    // 距離減衰やLight Intensityまで計算済みのRadiance
    float3 lightRadiance,
    // 0 = 完全Shadow
    // 1 = Lit
    float shadowVisibility
)
{
    //========================================================
    // SSS無効
    //========================================================
    if (surface.useSubsurface < 0.5f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }


    //========================================================
    // Strengthなし
    //========================================================
    if (surface.subsurfaceStrength <= 0.0001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    //========================================================
    // Metallic MaterialにはSSSを適用しない
    //========================================================
    const float dielectricFactor = 1.0f - saturate(surface.metallic);

    if (dielectricFactor <= 0.0001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }
    
    //========================================================
    // SkinMask
    // Skinではない部分にはTransmission SSSを適用しない
    //========================================================
    if (surface.skinMask <= 0.0001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    //========================================================
    // SSSではNormal Mapの細かな凹凸より
    // 形状本来のNormalを使用する
    //========================================================
    const float3 N = normalize(geometricNormal);

    const float3 L = normalize(lightDirection);

    const float3 V = normalize(viewDirection);


    //========================================================
    // Back Lighting
    // 通常Lighting: dot(N, L)
    // Transmission: dot(-N, L)
    // Lightが面の裏側にあるほど強くなる
    //========================================================
    const float rawBackLighting = saturate(dot(-N, L));
    
    const float backLighting = pow(rawBackLighting, 2.0f);


    if (backLighting <= 0.0001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }


    //========================================================
    // Light → Pixel方向
    // LはPixel → Lightなので反転
    //========================================================
    const float3 lightTravelDirection = -L;


    //========================================================
    // Distortion
    // 法線方向へ少し曲げることで、
    // 単純なTransmissionより柔らかくする
    //========================================================
    const float3 distortedDirection =
        normalize(lightTravelDirection + N * surface.subsurfaceDistortion);


    //========================================================
    // Transmissionした光がCamera方向へ抜ける度合い
    //========================================================
    const float viewAlignment =
        saturate(dot(distortedDirection, V));


    //========================================================
    // Transmissionの形
    // Power大: 狭い
    // Power小: 広い
    //========================================================
    const float transmissionShape = pow(max(viewAlignment, 0.0001f), surface.subsurfacePower);


    //========================================================
    // Thickness
    //========================================================
    const float thickness = saturate(surface.subsurfaceThickness);

    const float thinness = saturate(surface.subsurfaceThickness);
    
    const float thinnessGate = smoothstep(0.55f, 0.90f, thinness);
    
    const float thicknessTransmission = pow(thinnessGate, 3.0f);
    
    const float shadowTransmissionFloor = 0.20f * thinness;
    
    //========================================================
    // Shadow
    // ※通常Shadowを100%適用するとSelf Shadowによって
    // ※Transmissionそのものが消えやすい
    //========================================================
    const float subsurfaceShadow = lerp(shadowTransmissionFloor, 1.0f, saturate(shadowVisibility));
    
    //========================================================
    // 散乱色
    //========================================================
    const float3 transmissionColor =
        surface.subsurfaceColor * surface.baseColor.rgb;


    //========================================================
    // Transmission量
    //========================================================
    const float transmission =
        backLighting *
        transmissionShape *
        thicknessTransmission *
        surface.skinMask*
        surface.subsurfaceStrength;

   
    //========================================================
    // 最終SSS
    //========================================================
    return
        transmissionColor *
        lightRadiance *
        transmission *
        subsurfaceShadow *
        dielectricFactor;
}



//============================================================
// Directional Lightによる簡易Subsurface Scattering
//============================================================
float3 EvaluateDirectionalSubsurface(
    SurfaceData surface,
    PixelContext context,
    float shadowVisibility
)
{
    //========================================================
    // Pixel → Directional Light
    //========================================================
    const float3 L = normalize(LightDir.xyz);


    //========================================================
    // Directional Light Radiance
    //========================================================
    const float3 radiance = max(LightCol.rgb, 0.0f);


    return
        EvaluateSubsurfaceTransmission(
            surface,
            surface.geometricNormal,
            L,
            context.viewDirection,
            radiance,
            shadowVisibility
        );
}

//============================================================
// Point Light Subsurface Scattering
//============================================================
float3 EvaluatePointLightSubsurface(
    LocalLightData light,
    SurfaceData surface,
    PixelContext context
)
{
    //========================================================
    // SSS無効ならここで終了
    //========================================================
    if (surface.useSubsurface < 0.5f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    //========================================================
    // Pixel → Point Light
    //========================================================
    const float3 toLight = light.PositionRange.xyz - context.worldPosition;

    const float rawDistanceSquared = dot(toLight, toLight);

    //========================================================
    // Light中心
    //========================================================
    if (rawDistanceSquared <= 0.000001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }


    const float distanceToLight = sqrt(rawDistanceSquared);

    const float lightRange = max(light.PositionRange.w, 0.001f);

    //========================================================
    // Range外
    //========================================================
    if (distanceToLight >= lightRange)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    //========================================================
    // Pixel → Light方向
    //========================================================
    const float3 L = toLight / distanceToLight;

    //========================================================
    // Range Attenuation
    //========================================================
    const float normalizedDistance = distanceToLight / lightRange;

    float rangeAttenuation = saturate(1.0f - pow(normalizedDistance, 4.0f));

    rangeAttenuation *= rangeAttenuation;

    //========================================================
    // Inverse Square
    // Light中心で発散しないよう最低値を持たせる
    //========================================================
    const float distanceSquared = max(rawDistanceSquared, 0.01f);

    const float distanceAttenuation = 1.0f / distanceSquared;

    const float attenuation = rangeAttenuation * distanceAttenuation;

    //========================================================
    // HDR Radiance
    //========================================================
    const float3 radiance =
        light.ColorIntensity.rgb * max(light.ColorIntensity.w, 0.0f) * attenuation;

    //========================================================
    // Point Shadow
    //========================================================
    float shadowVisibility = 1.0f;

    //========================================================
    // Parameters.w
    // 0: Shadowなし
    // 1: Shadow 0
    // 2: Shadow 1
    //========================================================
    const int encodedShadowIndex = (int) (light.Parameters.w + 0.5f);


    if (encodedShadowIndex > 0)
    {
        const uint shadowIndex = (uint) (encodedShadowIndex - 1);

        shadowVisibility =
            CalculatePointLightShadow(
                context.worldPosition,
                light.PositionRange.xyz,
                surface.geometricNormal,
                L,
                shadowIndex
            );
    }


    //========================================================
    // SSS評価
    // 通常PBRではNdotLが負なら0になるが、
    // この関数では裏側のdot(-N,L)を評価する
    //========================================================
    return
        EvaluateSubsurfaceTransmission(
            surface,
            surface.geometricNormal,
            L,
            context.viewDirection,
            radiance,
            shadowVisibility
        );
}

//============================================================
// Spot Light Subsurface Scattering
//============================================================
float3 EvaluateSpotLightSubsurface(
    LocalLightData light,
    SurfaceData surface,
    PixelContext context
)
{
    if (surface.useSubsurface < 0.5f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }


    //========================================================
    // Pixel → Light
    //========================================================
    const float3 toLight = light.PositionRange.xyz - context.worldPosition;

    const float rawDistanceSquared = dot(toLight, toLight);

    if (rawDistanceSquared <= 0.000001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float distanceToLight = sqrt(rawDistanceSquared);

    const float lightRange = max(light.PositionRange.w, 0.001f);

    //========================================================
    // Range外
    //========================================================
    if (distanceToLight >= lightRange)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    //========================================================
    // Pixel → Light
    //========================================================
    const float3 L = toLight / distanceToLight;

    //========================================================
    // Spot Cone判定には逆方向、Light → Pixelが必要
    //========================================================
    const float3 lightToPixelDirection = -L;

    const float3 spotDirection = normalize(light.DirectionOuterCos.xyz);

    //========================================================
    // Spot中心との角度
    //========================================================
    const float coneCos = dot(spotDirection, lightToPixelDirection);

    const float innerCos = light.Parameters.y;

    const float outerCos = light.DirectionOuterCos.w;

    //========================================================
    // Cone外
    //========================================================
    if (coneCos <= outerCos)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    //========================================================
    // Outer → Inner
    //========================================================
    float coneAttenuation =
        saturate((coneCos - outerCos) / max(innerCos - outerCos, 0.0001f));

    //========================================================
    // SmoothStep
    //========================================================
    coneAttenuation =
        coneAttenuation * coneAttenuation * (3.0f - 2.0f * coneAttenuation);

    //========================================================
    // 距離減衰
    //========================================================
    const float normalizedDistance = distanceToLight / lightRange;

    float rangeAttenuation = saturate(1.0f - pow(normalizedDistance, 4.0f));

    rangeAttenuation *= rangeAttenuation;

    const float distanceSquared = max(rawDistanceSquared, 0.01f);

    const float distanceAttenuation = 1.0f / distanceSquared;

    const float attenuation =
        rangeAttenuation * distanceAttenuation * coneAttenuation;


    //========================================================
    // Spot HDR Radiance
    //========================================================
    const float3 radiance =
        light.ColorIntensity.rgb * max(light.ColorIntensity.w, 0.0f) * attenuation;

    //========================================================
    // Spot Shadow
    //========================================================
    float shadowVisibility = 1.0f;

    const int encodedShadowIndex = (int) (light.Parameters.w + 0.5f);

    if (encodedShadowIndex > 0)
    {
        const uint shadowIndex = (uint) (encodedShadowIndex - 1);


        shadowVisibility =
            CalculateSpotLightShadow(
                context.worldPosition,
                surface.geometricNormal,
                L,
                shadowIndex
            );
    }


    return
        EvaluateSubsurfaceTransmission(
            surface,
            surface.geometricNormal,
            L,
            context.viewDirection,
            radiance,
            shadowVisibility
        );
}

//============================================================
// Scene内のLocal Light SSSをすべて評価
//============================================================
LocalSubsurfaceResult EvaluateLocalSubsurface(SurfaceData surface,PixelContext context)
{
    LocalSubsurfaceResult result = (LocalSubsurfaceResult) 0;


    //========================================================
    // SSS無効MaterialならLocal Light Loop自体を回さない
    //========================================================
    if (surface.useSubsurface < 0.5f)
    {
        return result;
    }


    //========================================================
    // 現在有効なLocal Light数
    //========================================================
    const uint localLightCount = min((uint) (LocalLightParam.x + 0.5f), MAX_LOCAL_LIGHTS);


    [loop]
    for (uint lightIndex = 0u;lightIndex < localLightCount;++lightIndex)
    {
        const LocalLightData light = txLocalLights[lightIndex];


        //====================================================
        // 無効Light
        //====================================================
        if (light.Parameters.z < 0.5f)
        {
            continue;
        }

        const uint lightType = (uint) (light.Parameters.x + 0.5f);

        //====================================================
        // Point
        //====================================================
        if (lightType == LOCAL_LIGHT_TYPE_POINT)
        {
            result.SSSpoint +=
                EvaluatePointLightSubsurface(
                    light,
                    surface,
                    context
                );


            ++result.evaluatedLightCount;
        }


        //====================================================
        // Spot
        //====================================================
        else if (lightType == LOCAL_LIGHT_TYPE_SPOT)
        {
            result.SSSspot +=
                EvaluateSpotLightSubsurface(
                    light,
                    surface,
                    context
                );


            ++result.evaluatedLightCount;
        }
    }

    result.finalColor = result.SSSpoint + result.SSSspot;

    return result;
}

//============================================================
// Scene内の全SSSを評価
//============================================================
SubsurfaceLightingResult EvaluateSubsurfaceLighting(
    SurfaceData surface,
    PixelContext context,
    float directionalShadowVisibility
)
{
    SubsurfaceLightingResult result = (SubsurfaceLightingResult) 0;

    //========================================================
    // Material側でSSS無効
    //========================================================
    if (surface.useSubsurface < 0.5f)
    {
        return result;
    }

    //========================================================
    // Directional
    //========================================================
    result.directional =
        EvaluateDirectionalSubsurface(
            surface,
            context,
            directionalShadowVisibility
        );


    //========================================================
    // Point / Spot
    //========================================================
    const LocalSubsurfaceResult localResult = EvaluateLocalSubsurface(surface, context);


    result.SSSpoint = localResult.SSSpoint;
    result.SSSspot = localResult.SSSspot;
    result.local = localResult.finalColor;


    //========================================================
    // 全SSS
    //========================================================
    result.finalColor = result.directional + result.local;


    return result;
}

#endif