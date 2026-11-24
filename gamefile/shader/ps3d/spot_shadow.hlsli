#ifndef SPOT_SHADOW_HLSLI
#define SPOT_SHADOW_HLSLI

#include "ps_3d_bindings.hlsli"

//============================================================
// Spot Shadow計算結果
// visibility:
//   0 = 完全な影
//   1 = 完全に明るい
// shadowUV: Shadow Map上のUV
// depth: Spot Lightから見た現在Pixelの深度
// valid:
//   0 = Shadow Map範囲外
//   1 = Shadow Map範囲内
// 主にデバッグ表示にも利用できる
//============================================================
struct SpotShadowResult
{
    float visibility;

    float2 shadowUV;

    float depth;

    float valid;
	
    uint shadowIndex;
};

//============================================================
// UVがShadow Map内部にあるか確認
//============================================================
bool IsInsideSpotShadowMap(float2 shadowUV)
{
    return
		shadowUV.x >= 0.0f &&
		shadowUV.x <= 1.0f &&
		shadowUV.y >= 0.0f &&
		shadowUV.y <= 1.0f;
}

//============================================================
// Spot Light Shadowを評価
// worldPosition: 現在Pixelのワールド座標
// geometricNormal: Normal Map適用前の形状法線
// pixelToLightDirection: PixelからSpot Lightへ向かう方向
// 戻り値: SpotShadowResult
//============================================================
SpotShadowResult EvaluateSpotLightShadow(
	float3 worldPosition,
	float3 geometricNormal,
	float3 pixelToLightDirection,
	uint shadowIndex
)
{
    SpotShadowResult result = (SpotShadowResult) 0;

	//============================================================
	// 初期値は完全に明るい
	// Shadow無効時やShadow Map範囲外ではこの値をそのまま返す
	//============================================================
    result.visibility = 1.0f;
    result.shadowUV = float2(0.0f, 0.0f);
    result.depth = 1.0f;
    result.valid = 0.0f;
    result.shadowIndex = shadowIndex;
	
	//============================================================
	// Index範囲確認
	//============================================================
    if (shadowIndex >= MAX_SPOT_SHADOW_LIGHTS)
    {
        return result;
    }
	
	//============================================================
	// Spot Shadow全体が無効
	// SpotShadowParam.x:
	//   0 = 無効
	//   1 = 有効
	//============================================================
    if (SpotShadowParam[shadowIndex].x < 0.5f)
    {
        return result;
    }

	//============================================================
	// World座標をSpot LightのClip空間へ変換
	// C++側で行列をTransposeしている構成を想定
	// HLSL: mul(vector, matrix)
	//============================================================
    const float4 shadowClipPosition =
		mul(float4(worldPosition, 1.0f), SpotLightViewProjection[shadowIndex]);

	//============================================================
	// Spot Light Cameraの後方にあるPixel
	// wが0以下の場合、Perspective Divideできない
	//============================================================
    if (shadowClipPosition.w <= 0.00001f)
    {
        return result;
    }

	//============================================================
	// Perspective Divide
	// DirectXのNDC:
	// x : -1 ～ 1
	// y : -1 ～ 1
	// z :  0 ～ 1
	//============================================================
    const float3 shadowNDC = shadowClipPosition.xyz / shadowClipPosition.w;

	//============================================================
	// NDCからTexture UVへ変換
	// X:
	//   -1 ～ 1
	//    ↓
	//    0 ～ 1
	// Y: Texture座標は上が0なので反転する
	//============================================================
    const float2 shadowUV =
		float2(shadowNDC.x * 0.5f + 0.5f, -shadowNDC.y * 0.5f + 0.5f);

    const float currentDepth = shadowNDC.z;
    result.shadowUV = shadowUV;
    result.depth = currentDepth;

	//============================================================
	// Shadow Map範囲外
	//============================================================
    if (!IsInsideSpotShadowMap(shadowUV))
    {
        return result;
    }

	//============================================================
	// DirectXの深度範囲外
	//============================================================
    if (currentDepth < 0.0f || currentDepth > 1.0f)
    {
        return result;
    }

    result.valid = 1.0f;

	//============================================================
	// Shadow Bias
	// Normal Map適用後の法線ではなくモデル本来の形状法線を使用する
	// Normal Map法線を使用すると
	// 細かい凹凸によってShadow Biasが不安定になる
	//============================================================
    const float3 safeGeometricNormal = normalize(geometricNormal);

    const float3 safePixelToLightDirection = normalize(pixelToLightDirection);

	//============================================================
	// 面がライト方向を向いているほど1に近づく
	//============================================================
    const float NdotL = saturate(dot(safeGeometricNormal, safePixelToLightDirection));

	//============================================================
	// Bias設定
	//============================================================
    const float minimumBias = max(SpotShadowParam[shadowIndex].z, 0.000001f);
    const float slopeBias = max(SpotShadowParam[shadowIndex].w, 0.0f);

	//============================================================
	// 面がライトに対して斜めになるほどBiasを増やす
	// NdotL = 1: slopeBias成分は0
	// NdotL = 0: slopeBiasを最大適用
	//============================================================
    const float shadowBias =
		max(minimumBias, slopeBias * (1.0f - NdotL));

	//============================================================
	// 比較するReceiver側深度
	// 深度からBiasを引くことで
	// 自分自身を影と誤判定するShadow Acneを抑える
	//============================================================
    const float compareDepth = currentDepth - shadowBias;

	//============================================================
	// Shadow Map解像度取得
	// 固定値を使用しないため
	// ShadowMap解像度を変更しても追従できる
	//============================================================
	//============================================================
	// Shadow Texture Array情報
	//============================================================
    uint shadowMapWidth = 1u;
    uint shadowMapHeight = 1u;
    uint shadowArraySize = 1u;
    uint shadowMipLevels = 1u;

   //============================================================
	// Mip 0のサイズを取得
	//============================================================
    txSpotLightShadow.GetDimensions(
		0,
		shadowMapWidth,
		shadowMapHeight,
		shadowArraySize,
		shadowMipLevels
	);
    if (shadowIndex >= shadowArraySize)
    {
        return result;
    }


    const float2 shadowTexelSize =
		1.0f / float2((float) shadowMapWidth, (float) shadowMapHeight);

	//============================================================
	// PCFサンプル半径
	// 1.0: 隣接1Texel
	// 2.0: 隣接2Texel
	//============================================================
    const float pcfRadius = max(SpotShadowParam[shadowIndex].y, 0.0f);

    float visibilitySum = 0.0f;

	//============================================================
	// 有効サンプル数
	// 現在は3×3固定なので最大9
	//============================================================
    float sampleCount = 0.0f;

	//============================================================
	// 3×3 PCF
	// Shadow Mapを9回比較サンプリングし
	// 平均を取ることで影の輪郭を少し滑らかにする
	//============================================================
	[unroll]
    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
		[unroll]
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            const float2 sampleUV =
				shadowUV +
				float2((float) offsetX, (float) offsetY) *
				shadowTexelSize *
				pcfRadius;

			//====================================================
			// Shadow Mapの端を超えたサンプル
			// 範囲外は影なしとして扱う
			//====================================================
            if (!IsInsideSpotShadowMap(sampleUV))
            {
                visibilitySum += 1.0f;
                sampleCount += 1.0f;
                continue;
            }

			//====================================================
			// Hardware Comparison Sampling
			// 比較結果:
			//   0 = 影
			//   1 = 明るい
			//====================================================
            visibilitySum += txSpotLightShadow.SampleCmpLevelZero(
				samShadow,
				float3(sampleUV, (float) shadowIndex), compareDepth
			);

            sampleCount += 1.0f;
        }
    }

	//============================================================
	// 0除算防止
	//============================================================
    if (sampleCount <= 0.0f)
    {
        return result;
    }

    result.visibility = saturate(visibilitySum / sampleCount);

    return result;
}

//============================================================
// Visibilityだけ欲しい場合の簡易関数
//============================================================
float CalculateSpotLightShadow(
	float3 worldPosition,
	float3 geometricNormal,
	float3 pixelToLightDirection,
	uint shadowIndex
)
{
    const SpotShadowResult result =
		EvaluateSpotLightShadow(
			worldPosition,
			geometricNormal,
			pixelToLightDirection,
			shadowIndex
		);

    return result.visibility;
}

#endif