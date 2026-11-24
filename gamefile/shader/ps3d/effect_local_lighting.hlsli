#ifndef EFFECT_LOCAL_LIGHTING_HLSLI
#define EFFECT_LOCAL_LIGHTING_HLSLI

//============================================================
// Resource / SceneBuffer / LocalLightData
//============================================================
#include "ps_3d_bindings.hlsli"

//============================================================
// Spot / Point Shadow
//============================================================
#include "spot_shadow.hlsli"
#include "point_shadow.hlsli"


//============================================================
// Effect用距離減衰
// 通常モデルのPBR Lightingより簡略化している
// ・LightのRange外なら0
// ・Range境界へ近づくほど滑らかに0
// ・距離が遠いほどInverse Squareで減衰
//============================================================
float CalculateEffectLightAttenuation(float distanceSquared,float lightRange)
{
    //========================================================
    // 不正なRangeを防止
    //========================================================
    const float safeRange = max(lightRange, 0.001f);

    const float rangeSquared = safeRange * safeRange;


    //========================================================
    // Lightの有効範囲外
    //========================================================
    if (distanceSquared >= rangeSquared)
    {
        return 0.0f;
    }


    //========================================================
    // 実距離
    //========================================================
    const float distanceToLight = sqrt(max(distanceSquared, 0.000001f));

    //========================================================
    // Range Fade
    // Light中心: 1
    // Range端: 0
    //========================================================
    float rangeFade = saturate(1.0f - distanceToLight / safeRange);

    //========================================================
    // 境界を滑らかにする
    //========================================================
    rangeFade *= rangeFade;

    //========================================================
    // 簡易Inverse Square
    // 近距離で極端な値にならないようdenominatorを最低1.0にする
    //========================================================
    const float inverseSquare = 1.0f / max(distanceSquared, 1.0f);

    return rangeFade * inverseSquare;
}


//============================================================
// Effect用 Point Light
//============================================================
float3 EvaluateEffectPointLight(
    LocalLightData light,
    float3 worldPosition,
    float3 geometricNormal,
    float3 particleColor,
    float receiveLocalShadow
)
{
    //========================================================
    // Pixel → Light
    //========================================================
    const float3 toLight = light.PositionRange.xyz - worldPosition;

    const float distanceSquared = dot(toLight, toLight);

    //========================================================
    // Light中心と完全一致している場合の0除算防止
    //========================================================
    if (distanceSquared <= 0.000001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }


    //========================================================
    // 距離減衰
    //========================================================
    const float attenuation =
        CalculateEffectLightAttenuation(distanceSquared, light.PositionRange.w);

    if (attenuation <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    //========================================================
    // Pixel → Light方向
    //========================================================
    const float3 pixelToLightDirection = normalize(toLight);

    //========================================================
    // Point Shadow
    //========================================================
    float shadowVisibility = 1.0f;

    //========================================================
    // Parameters.w
    // 0: Shadowなし
    // 1: Point Shadow 0
    // 2: Point Shadow 1
    //========================================================
    const int encodedShadowIndex = (int) (light.Parameters.w + 0.5f);

    //========================================================
    // Local Shadowを有効にしているEffectだけShadow MapをSampleする
    //========================================================
    if (receiveLocalShadow > 0.5f && encodedShadowIndex > 0)
    {
        const uint shadowIndex = (uint) (encodedShadowIndex - 1);


        shadowVisibility =
            CalculatePointLightShadow(
                worldPosition,
                light.PositionRange.xyz,
                geometricNormal,
                pixelToLightDirection,
                shadowIndex
            );
    }


    //========================================================
    // Light Color
    // HDR Intensityもここで掛ける
    //========================================================
    const float3 lightColor = light.ColorIntensity.rgb * light.ColorIntensity.w;

    //========================================================
    // Particle用Lighting
    // ※ParticleはBillboardなので、
    // ※NdotLでSurface LightingするとCamera角度によって
    // ※明るさが不自然に変わりやすい
    // ※そのためLocal Lightの明るさにはNormalを使わず
    // ※NormalはShadow Bias計算だけに使用する
    //========================================================
    return
        particleColor *
        lightColor *
        attenuation *
        shadowVisibility;
}


//============================================================
// Effect用 Spot Light
//============================================================
float3 EvaluateEffectSpotLight(
    LocalLightData light,
    float3 worldPosition,
    float3 geometricNormal,
    float3 particleColor,
    float receiveLocalShadow
)
{
    //========================================================
    // Light → Pixel
    // Spot Cone判定ではLightからPixelへ向かう方向を使う
    //========================================================
    const float3 lightToPixel = worldPosition - light.PositionRange.xyz;

    const float distanceSquared = dot(lightToPixel, lightToPixel);

    if (distanceSquared <= 0.000001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }


    //========================================================
    // 距離減衰
    //========================================================
    const float attenuation =
        CalculateEffectLightAttenuation(distanceSquared, light.PositionRange.w);


    if (attenuation <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }


    //========================================================
    // Spot Lightの照射方向
    //========================================================
    const float3 spotDirection = normalize(light.DirectionOuterCos.xyz);

    //========================================================
    // Light → Pixel方向
    //========================================================
    const float3 lightToPixelDirection = normalize(lightToPixel);

    //========================================================
    // Spot中心軸との角度
    //========================================================
    const float coneCos = dot(spotDirection, lightToPixelDirection);

    //========================================================
    // CPU側で保存されているCone値
    // Parameters.y Inner Cos
    // DirectionOuterCos.w Outer Cos
    //========================================================
    const float innerCos = light.Parameters.y;

    const float outerCos = light.DirectionOuterCos.w;


    //========================================================
    // Spot Coneの完全な外側
    //========================================================
    if (coneCos <= outerCos)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }


    //========================================================
    // Outer → Innerを0～1へ
    //========================================================
    const float coneDenominator = max(innerCos - outerCos, 0.0001f);

    float coneAttenuation = saturate((coneCos - outerCos) / coneDenominator);

    //========================================================
    // SmoothStep相当
    //========================================================
    coneAttenuation = coneAttenuation * coneAttenuation * (3.0f - 2.0f * coneAttenuation);

    //========================================================
    // Pixel → Light
    // Shadow Bias用
    //========================================================
    const float3 pixelToLightDirection = -lightToPixelDirection;

    //========================================================
    // Spot Shadow
    //========================================================
    float shadowVisibility = 1.0f;


    const int encodedShadowIndex = (int) (light.Parameters.w + 0.5f);


    if (receiveLocalShadow > 0.5f && encodedShadowIndex > 0)
    {
        const uint shadowIndex = (uint) (encodedShadowIndex - 1);


        shadowVisibility =
            CalculateSpotLightShadow(
                worldPosition,
                geometricNormal,
                pixelToLightDirection,
                shadowIndex
            );
    }


    //========================================================
    // HDR Light Color
    //========================================================
    const float3 lightColor = light.ColorIntensity.rgb * light.ColorIntensity.w;


    return
        particleColor *
        lightColor *
        attenuation *
        coneAttenuation *
        shadowVisibility;
}


//============================================================
// Effect Local Lighting全体
//============================================================
float3 EvaluateEffectLocalLighting(
    float3 worldPosition,
    float3 geometricNormal,
    float3 particleColor,
    float localLightStrength,
    float receiveLocalShadow
)
{
    //========================================================
    // Strength 0ならLight Buffer処理を終了
    //========================================================
    if (localLightStrength <= 0.0001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float3 result = float3(0.0f, 0.0f, 0.0f);

    //========================================================
    // SceneBufferから現在のLocal Light数を取得
    //========================================================
    const uint localLightCount = min((uint) (LocalLightParam.x + 0.5f), MAX_LOCAL_LIGHTS);


    //========================================================
    // 登録されているPoint / Spot Lightを評価
    //========================================================
    [loop]
    for (uint lightIndex = 0u; lightIndex < localLightCount; ++lightIndex)
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
        // Point Light
        //====================================================
        if (lightType == LOCAL_LIGHT_TYPE_POINT)
        {
            result += EvaluateEffectPointLight(
                    light,
                    worldPosition,
                    geometricNormal,
                    particleColor,
                    receiveLocalShadow
                );
        }

        //====================================================
        // Spot Light
        //====================================================
        else if (lightType == LOCAL_LIGHT_TYPE_SPOT)
        {
            result += EvaluateEffectSpotLight(
                    light,
                    worldPosition,
                    geometricNormal,
                    particleColor,
                    receiveLocalShadow
                );
        }
    }


    //========================================================
    // EffectごとのLocal Light影響度
    //========================================================
    return result * saturate(localLightStrength);
}

#endif