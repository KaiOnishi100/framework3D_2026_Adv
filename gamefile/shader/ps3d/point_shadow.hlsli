#ifndef POINT_SHADOW_HLSLI
#define POINT_SHADOW_HLSLI

//============================================================
// SceneBuffer・Point Shadow Texture・Shadow Sampler
//============================================================
#include "ps_3d_bindings.hlsli"

//============================================================
// Point Shadow評価結果
//============================================================
struct PointShadowResult
{
	//========================================================
	// Shadow可視率
	// 0:完全に影
	// 1:完全に明るい
	//========================================================
    float visibility;

	//========================================================
	// 使用したShadow Map上のUV
	//========================================================
    float2 shadowUV;

	//========================================================
	// Light視点での現在Pixelの深度
	//========================================================
    float depth;

	//========================================================
	// Shadow Mapの有効範囲内か
	// 0:無効
	// 1:有効
	//========================================================
    float valid;

	//========================================================
	// 使用したCube Face番号
	//
	// 0 : +X
	// 1 : -X
	// 2 : +Y
	// 3 : -Y
	// 4 : +Z
	// 5 : -Z
	//========================================================
    uint faceIndex;

	//========================================================
	// 使用したTexture2DArrayのSlice番号
	//========================================================
    uint sliceIndex;
};

//============================================================
// Point ShadowのCube Face境界確認
// 赤: Face端から2Texel以内
// 緑: Face端から2～8Texel
// 青: Face中央
//============================================================
float3 DebugPointShadowFaceEdge(float2 shadowUV,float2 shadowTexelSize)
{
    const float2 distanceToEdgeUV =
		min(shadowUV, 1.0f - shadowUV);

    const float2 distanceToEdgeTexels =
		distanceToEdgeUV / max(shadowTexelSize, float2(0.000001f, 0.000001f));

    const float minimumEdgeDistance =
		min(distanceToEdgeTexels.x, distanceToEdgeTexels.y);

    if (minimumEdgeDistance < 2.0f)
    {
        return float3(1.0f, 0.0f, 0.0f);
    }

    if (minimumEdgeDistance < 8.0f)
    {
        return float3(0.0f, 1.0f, 0.0f);
    }

    return float3(0.0f, 0.0f, 1.0f);
}

//============================================================
// UVがShadow Map内にあるか確認
//============================================================
bool IsInsidePointShadowMap(float2 shadowUV)
{
    return
		shadowUV.x >= 0.0f &&
		shadowUV.x <= 1.0f &&
		shadowUV.y >= 0.0f &&
		shadowUV.y <= 1.0f;
}

//============================================================
// Point LightからPixelへ向かう方向をもとに使用するCube Face番号を選択する
// lightToPixelDirection:
//   Point Light位置
//       ↓
//   Pixel位置
// の方向ベクトル
//============================================================
uint SelectPointShadowFace(float3 lightToPixelDirection)
{
    const float3 absoluteDirection = abs(lightToPixelDirection);

	//============================================================
	// X成分が最も大きい
	//============================================================
    if (absoluteDirection.x >= absoluteDirection.y && absoluteDirection.x >= absoluteDirection.z)
    {
		// +X
        if (lightToPixelDirection.x >= 0.0f)
        {
            return 0u;
        }

		// -X
        return 1u;
    }

	//============================================================
	// Y成分が最も大きい
	//============================================================
    if (absoluteDirection.y >= absoluteDirection.x && absoluteDirection.y >= absoluteDirection.z)
    {
		// +Y
        if (lightToPixelDirection.y >= 0.0f)
        {
            return 2u;
        }

		// -Y
        return 3u;
    }

	//============================================================
	// Z成分が最も大きい
	//============================================================

	// +Z
    if (lightToPixelDirection.z >= 0.0f)
    {
        return 4u;
    }

	// -Z
    return 5u;
}

//============================================================
// Point Shadow Texture2DArrayのSlice番号を計算
// Point Shadow 0: Slice 0～5
// Point Shadow 1: Slice 6～11
//============================================================
uint CalculatePointShadowSliceIndex(uint shadowIndex,uint faceIndex)
{
    return shadowIndex * POINT_SHADOW_FACE_COUNT + faceIndex;
}

//============================================================
// Cube Face端を超えないPCF半径を計算
// requestedRadius: PointShadowParam.yで指定された本来のPCF半径
// 戻り値: 現在PixelからFace端までの距離に応じて縮小した半径
// Face中央: requestedRadiusをそのまま使用
// Face境界付近: PCF半径を0へ近づける
//============================================================
float CalculatePointShadowSafePCFRadius(
	float2 shadowUV,
	float2 shadowTexelSize,
	float requestedRadius
)
{
	//============================================================
	// UV座標から上下左右のFace端までの距離を取得
	// shadowUV.x: 左端までの距離
	// 1.0 - shadowUV.x: 右端までの距離
	//============================================================
    const float2 distanceToFaceEdgeUV = min(shadowUV, 1.0f - shadowUV);

	//============================================================
	// UV単位の距離をTexel単位へ変換
	// 端まで4Texelなら4.0付近になる
	//============================================================
    const float2 safeTexelSize = max(shadowTexelSize, float2(0.000001f, 0.000001f));

    const float2 distanceToFaceEdgeTexels = distanceToFaceEdgeUV / safeTexelSize;

	//============================================================
	// 上下左右のうち、最も近い端までの距離
	//============================================================
    const float minimumEdgeDistanceTexels =
		min(distanceToFaceEdgeTexels.x, distanceToFaceEdgeTexels.y);

	//============================================================
	// 半Texel分の余裕を残す
	// requestedRadiusより端までの距離が小さい場合だけ
	// PCF半径を縮小する
	//============================================================
    const float maximumSafeRadius = max(minimumEdgeDistanceTexels - 0.5f, 0.0f);

    return min(max(requestedRadius, 0.0f), maximumSafeRadius);
}

//============================================================
// Point Light Shadowを評価
// worldPosition: 現在PixelのWorld座標
// pointLightPosition: Point LightのWorld座標
// geometricNormal: Normal Map適用前の形状法線
// pixelToLightDirection: PixelからPoint Lightへ向かう方向
// shadowIndex: Point Shadow番号
//============================================================
PointShadowResult EvaluatePointLightShadow(
	float3 worldPosition,
	float3 pointLightPosition,
	float3 geometricNormal,
	float3 pixelToLightDirection,
	uint shadowIndex
)
{
    PointShadowResult result = (PointShadowResult) 0;

	//============================================================
	// 初期状態は影なし
	// 無効なIndexやShadow範囲外の場合は1.0をそのまま返す
	//============================================================
    result.visibility = 1.0f;
    result.shadowUV = float2(0.0f, 0.0f);
    result.depth = 1.0f;
    result.valid = 0.0f;
    result.faceIndex = 0u;
    result.sliceIndex = 0u;

	//============================================================
	// Point Shadow番号確認
	//============================================================
    if (shadowIndex >= MAX_POINT_SHADOW_LIGHTS)
    {
        return result;
    }

	//============================================================
	// 指定Point Shadowが有効か確認
	//============================================================
    const float4 shadowParameters = PointShadowParam[shadowIndex];

    if (shadowParameters.x <= 0.0f)
    {
        return result;
    }
	
    const float activeUVScale = saturate(shadowParameters.x);


	//============================================================
	// Point LightからPixelへ向かう方向
	// Face選択には、PixelからLightではなく
	// LightからPixelへの方向を使用する
	//============================================================
    const float3 lightToPixelDirection = worldPosition - pointLightPosition;

    const float lightToPixelDistanceSquared = dot(lightToPixelDirection, lightToPixelDirection);

	//============================================================
	// Light中心とPixel位置がほぼ同じ場合
	// Face方向を決められないため、影なしとして扱う
	//============================================================
    if (lightToPixelDistanceSquared <= 0.000001f)
    {
        return result;
    }

	//============================================================
	// 使用するCube Faceを決定
	//============================================================
    const uint faceIndex = SelectPointShadowFace(lightToPixelDirection);

	//============================================================
	// Point Shadow行列配列のIndex
	// SceneBufferの行列配列とTexture2DArrayのSliceは
	// 同じ並びになっている
	//============================================================
    const uint sliceIndex = CalculatePointShadowSliceIndex(shadowIndex, faceIndex);

    result.faceIndex = faceIndex;
    result.sliceIndex = sliceIndex;

	//============================================================
	// 配列範囲確認
	//============================================================
    if (sliceIndex >= POINT_SHADOW_TOTAL_SLICES)
    {
        return result;
    }

	//============================================================
	// World座標を選択したPoint Shadow Faceの
	// Clip空間へ変換
	//============================================================
    const float4 shadowClipPosition =
		mul(float4(worldPosition, 1.0f), PointLightViewProjection[sliceIndex]);

	//============================================================
	// Point Light Cameraの後方にある場合
	//============================================================
    if (shadowClipPosition.w <= 0.00001f)
    {
        return result;
    }

	//============================================================
	// Perspective Divide
	// DirectX NDC:
	// x : -1 ～ 1
	// y : -1 ～ 1
	// z :  0 ～ 1
	//============================================================
    const float3 shadowNDC = shadowClipPosition.xyz / shadowClipPosition.w;

	//============================================================
	// NDCからTexture UVへ変換
	// Texture座標では上方向がV=0なのでY座標を反転する
	//============================================================
    const float2 shadowUV =
		float2(shadowNDC.x * 0.5f + 0.5f, -shadowNDC.y * 0.5f + 0.5f);

    const float currentDepth = shadowNDC.z;
	
    result.shadowUV = shadowUV;
    result.depth = currentDepth;

	//============================================================
	// Shadow Map UV範囲外
	//============================================================
    if (!IsInsidePointShadowMap(shadowUV))
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
	// Shadow Bias計算
	// Normal Map適用後の法線ではなくモデル形状本来の法線を使用する
	//============================================================
    const float3 safeGeometricNormal = normalize(geometricNormal);

    const float3 safePixelToLightDirection = normalize(pixelToLightDirection);

    const float NdotL = saturate(dot(safeGeometricNormal, safePixelToLightDirection));

	//============================================================
	// Point LightごとのBias設定
	//============================================================
    const float minimumBias = max(shadowParameters.z, 0.000001f);

    const float slopeBias = max(shadowParameters.w, 0.0f);

	//============================================================
	// 面がLightに対して斜めになるほどBiasを増やす
	//============================================================
    const float shadowBias = max(minimumBias, slopeBias * (1.0f - NdotL));

	//============================================================
	// Receiver側深度
	//============================================================
    const float compareDepth = currentDepth - shadowBias;

	//============================================================
	// Texture2DArray情報を取得
	//============================================================
    uint shadowMapWidth = 1u;
    uint shadowMapHeight = 1u;
    uint shadowArraySize = 1u;
    uint shadowMipLevels = 1u;

    txPointLightShadow.GetDimensions(
		0,
		shadowMapWidth,
		shadowMapHeight,
		shadowArraySize,
		shadowMipLevels
	);

	//============================================================
	// Resource全体の1Texelサイズ
	//============================================================
    const float2 resourceTexelSize = 1.0f / float2((float) shadowMapWidth, (float) shadowMapHeight);

	//============================================================
	// 実際に使用している解像度
	// High: 1024
	// Medium: 512
	// Low: 256
	//============================================================
    const float2 activeResolution =
		float2((float) shadowMapWidth, (float) shadowMapHeight) * activeUVScale;

	//============================================================
	// 論理Face上の1Texelサイズ
	// Face端までの距離計算に使用する
	//============================================================
    const float2 activeFaceTexelSize = 1.0f / max(activeResolution, float2(1.0f, 1.0f));

	//============================================================
	// 0～1の論理Face UVを、Texture2DArray上の
	// 実際の使用領域へ変換する
	// High: 0～1.0
	// Medium: 0～0.5
	// Low: 0～0.25
	//============================================================
    const float2 textureShadowUV = shadowUV * activeUVScale;
	
	//============================================================
	// 実際のTexture Array範囲も確認
	//============================================================
    if (sliceIndex >= shadowArraySize)
    {
        result.valid = 0.0f;
        return result;
    }

    const float2 shadowTexelSize = 1.0f / float2((float) shadowMapWidth, (float) shadowMapHeight);

	//============================================================
	// ライトごとに指定されたPCF半径
	//============================================================
    const float requestedPCFRadius = max(shadowParameters.y, 0.0f);

	//============================================================
	// Cube Face端を超えない安全なPCF半径
	// Face中央ではrequestedPCFRadiusと同じ値になる
	// Face境界付近では0へ近づく
	//============================================================
    const float safePCFRadius =
		CalculatePointShadowSafePCFRadius(
			shadowUV,
			activeFaceTexelSize,
			requestedPCFRadius
		);

	//============================================================
	// Texel中央からサンプリングするための範囲
	// UVの0または1を直接サンプリングすると
	// 隣接FaceではなくTexture境界の値を参照する可能性がある
	//============================================================
    const float2 minimumSampleUV = resourceTexelSize * 0.5f;
    const float2 maximumSampleUV = activeUVScale.xx - minimumSampleUV;
    float visibilitySum = 0.0f;
    float sampleCount = 0.0f;

	//============================================================
	// Cube Face境界に非常に近い場合
	// safePCFRadiusが0なら9サンプルすべて同じ位置になるため
	// 中央を1回だけサンプリングする
	//============================================================
    if (safePCFRadius <= 0.0001f)
    {
        const float2 centerSampleUV =
			clamp(
				textureShadowUV,
				minimumSampleUV,
				maximumSampleUV
			);

        result.visibility = txPointLightShadow.SampleCmpLevelZero(
			samShadow,
			float3(centerSampleUV, (float) sliceIndex),
			compareDepth
		);

        return result;
    }
	
	//============================================================
	// 3×3 PCF
	//============================================================
	[unroll]
    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
		[unroll]
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            //====================================================
			// PCFサンプル位置
			// safePCFRadiusはFace端付近で小さくなるため
			// 基本的にFace範囲外へ出ない
			//====================================================
			//float2 sampleUV =
			//shadowUV + float2((float) offsetX, (float) offsetY) *
			//resourceTexelSize * safePCFRadius;
            const float2 sampleOffset =
				float2((float) offsetX, (float) offsetY) *
				resourceTexelSize * safePCFRadius;

			//====================================================
			// 浮動小数点誤差による範囲外アクセスを防止
			// UVをTexel中央の有効範囲へ制限する
			//====================================================
            const float2 sampleUV = clamp(
				    textureShadowUV + sampleOffset,
				    minimumSampleUV,
				    maximumSampleUV
				);
			//====================================================
			// Texture2DArray比較サンプリング
			// sampleUV: 現在Face内のUV
			// sliceIndex: Point Shadow番号×6 + Face番号
			//====================================================
            visibilitySum += txPointLightShadow.SampleCmpLevelZero(
				samShadow,
				float3(sampleUV, (float) sliceIndex),
				compareDepth
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
// Point Shadow可視率だけを返す簡易関数
//============================================================
float CalculatePointLightShadow(
	float3 worldPosition,
	float3 pointLightPosition,
	float3 geometricNormal,
	float3 pixelToLightDirection,
	uint shadowIndex
)
{
    const PointShadowResult result =
		EvaluatePointLightShadow(
			worldPosition,
			pointLightPosition,
			geometricNormal,
			pixelToLightDirection,
			shadowIndex
		);
	
	//return
	//DebugPointShadowFaceEdge(
	//	result.shadowUV,
	//	1.0f /float2(1024.0f,1024.0f)
	//);
	
    return result.visibility;
}

#endif