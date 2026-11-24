#ifndef CASCADE_SHADOW_HLSLI
#define CASCADE_SHADOW_HLSLI

#define POISSON_SAMPLE_COUNT 8

#include "ps_3d_bindings.hlsli"

//============================================================
// Cascade Shadowの数
// C++側で生成しているCascade数と一致させる
//============================================================
static const int CASCADE_SHADOW_COUNT = 3;

//============================================================
// Poisson Disk PCFサンプル
//============================================================
static const float2 POISSON_DISK[POISSON_SAMPLE_COUNT] =
{
    float2(-0.94201624f, -0.39906216f),
	float2(0.94558609f, -0.76890725f),
	float2(-0.09418410f, -0.92938870f),
	float2(0.34495938f, 0.29387760f),
	float2(-0.91588581f, 0.45771432f),
	float2(-0.38277543f, 0.27676845f),
	float2(0.97484398f, 0.75648379f),
	float2(0.14383161f, -0.14100790f)
};

struct CascadeShadowResult
{
    float visibility;

    int cascadeIndex;
    int nextCascadeIndex;

    float blendRate;

    float currentVisibility;
    float nextVisibility;
};

//============================================================
// Static ShadowMapで深度比較を行う
// cascadeIndex:
//   0 = Static Cascade 0
//   1 = Static Cascade 1
//   2 = Static Cascade 2
// shadowUV: ShadowMap上のUV座標
// compareDepth:
//   現在のピクセルをライトから見た深度
//   Receiver Bias適用後の値
// 戻り値:
//   1.0 = ライトから見えている
//   0.0 = Static Casterによって遮られている
//============================================================
float SampleStaticCascadeShadowCompare(int cascadeIndex, float2 shadowUV, float compareDepth)
{
    float visibility = 1.0f;
	
	//========================================================
	// Static Cascade 0
	//========================================================
    if (cascadeIndex == 0)
    {
        visibility = txStaticShadow0.SampleCmpLevelZero(
			samShadow,
			shadowUV,
			compareDepth
		);
    }

	//========================================================
	// Static Cascade 1
	//========================================================
    else if (cascadeIndex == 1)
    {
        visibility = txStaticShadow1.SampleCmpLevelZero(
			samShadow,
			shadowUV,
			compareDepth
		);
    }

	//========================================================
	// Static Cascade 2
	//========================================================
    else if (cascadeIndex == 2)
    {
        visibility = txStaticShadow2.SampleCmpLevelZero(
			samShadow,
			shadowUV,
			compareDepth
		);
    }
    return visibility;
}

//============================================================
// Dynamic ShadowMapで深度比較を行う
// 戻り値:
//   1.0 = ライトから見えている
//   0.0 = Dynamic Casterによって遮られている
//============================================================
float SampleDynamicCascadeShadowCompare(int cascadeIndex, float2 shadowUV, float compareDepth)
{
    float visibility = 1.0f;
	//========================================================
	// Dynamic Cascade 0
	//========================================================
    if (cascadeIndex == 0)
    {
        visibility = txDynamicShadow0.SampleCmpLevelZero(
			samShadow,
			shadowUV,
			compareDepth
		);
    }

	//========================================================
	// Dynamic Cascade 1
	//========================================================
    else if (cascadeIndex == 1)
    {
        visibility = txDynamicShadow1.SampleCmpLevelZero(
			samShadow,
			shadowUV,
			compareDepth
		);
    }

	//========================================================
	// Dynamic Cascade 2
	//========================================================
    else if (cascadeIndex == 2)
    {
        visibility = txDynamicShadow2.SampleCmpLevelZero(
		samShadow,
		shadowUV,
		compareDepth);
    }
    return visibility;
}

//============================================================
// StaticとDynamic両方のShadowMapで深度比較する
// Shadow係数の意味:
//   1.0: StaticにもDynamicにも遮られていない
//   0.0: どちらかのCasterによって遮られている
// 合成: min(staticVisibility, dynamicVisibility)
//============================================================
float SampleCombinedCascadeShadowCompare(int cascadeIndex, float2 shadowUV, float compareDepth)
{
	//========================================================
	// Static Casterによる可視率
	//========================================================
    const float staticVisibility =
		SampleStaticCascadeShadowCompare(
			cascadeIndex,
			shadowUV,
			compareDepth
		);

	//========================================================
	// Dynamic Casterによる可視率
	//========================================================
    const float dynamicVisibility =
		SampleDynamicCascadeShadowCompare(
			cascadeIndex,
			shadowUV,
			compareDepth
		);

	//========================================================
	// どちらか一方でも遮られていれば影にする
	//========================================================
    return min(staticVisibility, dynamicVisibility);
}

//============================================================
// View空間深度からCascade番号を選択
// viewDepth: カメラView空間のZ距離
// 戻り値:
//   0 = 近距離
//   1 = 中距離
//   2 = 遠距離
//   3 = Shadow最大距離より奥
//============================================================
int SelectCascadeIndex(float viewDepth)
{
	//========================================================
	// Cascade 0
	//========================================================
    if (viewDepth <= CascadeSplits.x)
    {
        return 0;
    }

	//========================================================
	// Cascade 1
	//========================================================
    if (viewDepth <= CascadeSplits.y)
    {
        return 1;
    }

	//========================================================
	// Cascade 2
	//========================================================
    if (viewDepth <= CascadeSplits.z)
    {
        return 2;
    }
	

	//========================================================
	// Shadow最大距離より奥
	//========================================================
    return 3;
}

//============================================================
// Cascade境界のブレンド係数を計算
//
// 戻り値:
//   0.0 = 現在Cascadeのみ
//   1.0 = 次Cascadeのみ
//============================================================
float CalculateCascadeBlendRate(float viewDepth,float blendStart,float cascadeEnd)
{
	//========================================================
	// 0除算防止
	//========================================================
    float blendLength = max(cascadeEnd - blendStart, 0.0001f);

	//========================================================
	// ブレンド範囲内の位置を0～1へ変換
	//========================================================
    float blendRate = saturate((viewDepth - blendStart) / blendLength);

	//========================================================
	// SmoothStep相当
	// 単純な線形補間より
	// ブレンド開始・終了部分が滑らかになる
	//========================================================
    blendRate = blendRate * blendRate * (3.0f - 2.0f * blendRate);

    return blendRate;
}

//============================================================
// 選択されたCascadeに対応するShadow座標を取得
//============================================================
float4 GetCascadeShadowPosition(pixelIn IN,int cascadeIndex)
{
    if (cascadeIndex == 0)
    {
        return IN.sdw0;
    }

    if (cascadeIndex == 1)
    {
        return IN.sdw1;
    }

    return IN.sdw2;
}

//============================================================
// 選択中CascadeのShadowMap解像度から1テクセル分のUVサイズを取得する
//============================================================
float2 GetCascadeShadowTexelSize(int cascadeIndex)
{
    uint shadowMapWidth = 1;

    uint shadowMapHeight = 1;

	//========================================================
	// Cascade 0
	//========================================================
    if (cascadeIndex == 0)
    {
        txStaticShadow0.GetDimensions(shadowMapWidth,shadowMapHeight);
    }
	//========================================================
	// Cascade 1
	//========================================================
    else if (cascadeIndex == 1)
    {
        txStaticShadow1.GetDimensions(shadowMapWidth,shadowMapHeight);
    }
	//========================================================
	// Cascade 2
	//========================================================
    else
    {
        txStaticShadow2.GetDimensions(shadowMapWidth,shadowMapHeight);
    }

    return 1.0f / float2((float) shadowMapWidth, (float) shadowMapHeight);
}

//============================================================
// Cascadeごとの最小Bias
//============================================================
float GetCascadeMinimumBias(int cascadeIndex)
{
    if (cascadeIndex == 0)
    {
        return 0.00003f;
    }

    if (cascadeIndex == 1)
    {
        return 0.00006f;
    }

    return 0.0009f;
}

//============================================================
// CascadeごとのSlope Bias
// 遠距離Cascadeは深度精度とテクセル密度が低いため
// 少し大きめのBiasを使う
//============================================================
float GetCascadeSlopeBias(int cascadeIndex)
{
    if (cascadeIndex == 0)
    {
        return 0.0003f;
    }

    if (cascadeIndex == 1)
    {
        return 0.0006f;
    }

    return 0.0009f;
}

//============================================================
// CascadeごとのPoisson Disk PCF半径 この値はテクセル単位の半径
//============================================================
float GetCascadePCFRadiusTexels(int cascadeIndex)
{
	//========================================================
	// Cascade 0
	// ※近距離は解像度が高いため少し広めにサンプリングする
	//========================================================
    if (cascadeIndex == 0)
    {
        return 2.0f;
    }

	//========================================================
	// Cascade 1
	// ※中距離は少し狭くする
	//========================================================
    if (cascadeIndex == 1)
    {
        return 1.5f;
    }

	//========================================================
	// Cascade 2
	// ※遠距離で広げすぎると
	// ※ワールド空間上で非常に大きくぼけてしまう
	//========================================================
    return 0.75f;
}

//============================================================
// Poisson Disk PCFによる影係数計算
// shadowPos: ライト視点のクリップ座標
// worldNormal: ワールド空間の法線
// 戻り値:
//   0.0 = 完全な影
//   1.0 = 完全に明るい
//============================================================
float CalcShadowFactorPoissonDiskPCF(
	float4 shadowPos,
	float3 worldNormal,
    int cascadeIndex
)
{
	//============================================================
	// 正しく透視除算できない場合は影なし
	//============================================================
    if (shadowPos.w <= 0.00001f)
    {
        return 1.0f;
    }

	//============================================================
	// ライトのクリップ空間からNDCへ変換
	//============================================================
    float3 projected = shadowPos.xyz / shadowPos.w;

	//============================================================
	// NDCからShadowMap用UVへ変換
	// X: -1～1 → 0～1
	// Y: Texture座標に合わせて上下反転
	//============================================================
    float2 shadowUV;

    shadowUV.x = projected.x * 0.5f + 0.5f;
    shadowUV.y = -projected.y * 0.5f + 0.5f;

	//============================================================
	// ライト視点での現在ピクセルの深度
	//============================================================
    float currentDepth =projected.z;

	//============================================================
	// ShadowMapの範囲外は影なし
	//============================================================
    if (shadowUV.x < 0.0f ||
		shadowUV.x > 1.0f ||
		shadowUV.y < 0.0f ||
		shadowUV.y > 1.0f ||
		currentDepth < 0.0f ||
		currentDepth > 1.0f)
    {
        return 1.0f;
    }

	//============================================================
	// Receiver側Slope Bias
	//============================================================
    float3 normal =normalize(worldNormal);

    float3 lightDirection =normalize(LightDir.xyz);

    float normalDotLight =saturate(dot(normal,lightDirection));

    const float minimumBias =GetCascadeMinimumBias( cascadeIndex);

    const float slopeBias =GetCascadeSlopeBias( cascadeIndex);

    float bias = max(minimumBias, slopeBias * (1.0f - normalDotLight));

	//============================================================
	// ShadowMapの1テクセル分のUVサイズ
	//============================================================
    float2 texelSize = GetCascadeShadowTexelSize(cascadeIndex);

	//============================================================
	// Poisson Diskの広がり
	//============================================================
    const float pcfRadiusTexels = GetCascadePCFRadiusTexels(cascadeIndex);

   //============================================================
	// Comparison Samplerへ渡す深度
	//============================================================
    const float compareDepth =currentDepth -bias;

    float lightAmount =0.0f;

	//============================================================
	// Poisson Disk Hardware PCF
	// 各SampleCmpは内部で2×2の比較補間を行う
	// ※16サンプルの場合、見た目としては単純な16点比較より
	// ※滑らかな結果を得やすい
	//============================================================
	[unroll]
    for (int sampleIndex = 0; sampleIndex < POISSON_SAMPLE_COUNT; ++sampleIndex)
    {
	    //========================================================
	    // Poisson Diskのサンプリング位置
	    //========================================================
        const float2 sampleOffset = POISSON_DISK[sampleIndex] * texelSize * pcfRadiusTexels;

        const float2 sampleUV = shadowUV + sampleOffset;

	    //========================================================
	    // ShadowMap外は影なしとして扱う
	    //========================================================
        if (sampleUV.x < 0.0f ||
		    sampleUV.x > 1.0f ||
		    sampleUV.y < 0.0f ||
		    sampleUV.y > 1.0f)
        {
            lightAmount += 1.0f;

            continue;
        }

	    //========================================================
	    // StaticとDynamicのShadowMapを両方比較する
	    //========================================================
        lightAmount +=
		    SampleCombinedCascadeShadowCompare(
		    cascadeIndex,
		    sampleUV,
		    compareDepth
	    );
    }

	//============================================================
	// 全サンプルの平均
	//============================================================
    return lightAmount / (float) POISSON_SAMPLE_COUNT;
}

//============================================================
// 指定した1つのCascade Shadowを評価
// IN: Vertex Shaderから受け取ったShadow座標
// geometricNormal: Normal Map適用前の形状法線
// cascadeIndex: 評価するCascade番号
//============================================================
float EvaluateSingleCascadeShadow(
	pixelIn IN,
	float3 geometricNormal,
	int cascadeIndex
)
{
	//========================================================
	// 不正なCascade番号を防止
	//========================================================
    const int safeCascadeIndex = clamp(cascadeIndex, 0, CASCADE_SHADOW_COUNT - 1);

	//========================================================
	// 対象CascadeのShadow座標を取得
	//========================================================
    const float4 shadowPosition = GetCascadeShadowPosition(IN, safeCascadeIndex);

	//========================================================
	// Poisson Disk PCFを実行
	// ※Static ShadowとDynamic Shadowの合成も
	// ※CalcShadowFactorPoissonDiskPCF内部で行う
	//========================================================
    const float visibility =
		CalcShadowFactorPoissonDiskPCF(
			shadowPosition,
			geometricNormal,
			safeCascadeIndex
		);

    return saturate(visibility);
}

//============================================================
// Effect用軽量Cascade Shadow
// 通常版との違い:
// 通常版: Poisson PCF 8 samples × Static + Dynamic
// Effect版: Static 1 sample + Dynamic 1 sample
// 大量の半透明ParticleではShadow品質より
// Sample数削減の効果が非常に大きい
//============================================================
float EvaluateSingleCascadeShadowFast(
    pixelIn IN,
    float3 geometricNormal,
    int cascadeIndex
)
{
    //========================================================
    // Cascade番号を安全な範囲へ
    //========================================================
    const int safeCascadeIndex = clamp(cascadeIndex, 0, CASCADE_SHADOW_COUNT - 1);

    //========================================================
    // Vertex Shaderから受け取った
    // 対象CascadeのShadow座標
    //========================================================
    const float4 shadowPosition = GetCascadeShadowPosition(IN, safeCascadeIndex);

    //========================================================
    // 正しい透視除算ができない場合
    //========================================================
    if (shadowPosition.w <= 0.00001f)
    {
        return 1.0f;
    }

    //========================================================
    // Light Clip Space
    //      ↓
    // Light NDC
    //========================================================
    const float3 projected = shadowPosition.xyz / shadowPosition.w;

    //========================================================
    // NDC → Shadow Map UV
    //========================================================
    const float2 shadowUV =
        float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);


    //========================================================
    // Shadow Mapに対して比較する現在Depth
    //========================================================
    const float currentDepth = projected.z;

    //========================================================
    // Shadow Map範囲外
    // 影なしとして扱う
    //========================================================
    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
		shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        currentDepth < 0.0f || currentDepth > 1.0f)
    {
        return 1.0f;
    }


    //========================================================
    // Effectでも最低限Biasは使用する
    // ※Particle表面にShadow Acneが発生しやすくなる
    //========================================================
    const float3 normal = normalize(geometricNormal);

    const float3 lightDirection = normalize(LightDir.xyz);

    const float normalDotLight = saturate(dot(normal, lightDirection));

    //========================================================
    // CascadeごとのBias
    //========================================================
    const float minimumBias = GetCascadeMinimumBias(safeCascadeIndex);

    const float slopeBias = GetCascadeSlopeBias(safeCascadeIndex);

    const float bias = max(minimumBias, slopeBias * (1.0f - normalDotLight));

    const float compareDepth = currentDepth - bias;


    const float visibility =
        SampleCombinedCascadeShadowCompare(
            safeCascadeIndex,
            shadowUV,
            compareDepth
        );

    return saturate(visibility);
}

//============================================================
// Effect用軽量CSM
// ・現在Cascadeのみ
// ・Cascade境界Blendなし
// ・Poisson PCFなし
// Particle用として負荷を大幅に抑える
//============================================================
float EvaluateCascadeShadowFast(
    pixelIn IN,
    float viewDepth,
    float3 geometricNormal
)
{
    //========================================================
    // Shadow無効なら何も計算しない
    // setReceiveShadow(false)
    // のEmitterではShadow Map Sampleが0回になる
	//========================================================
    if (ShadowParam.x < 0.5f)
    {
        return 1.0f;
    }


    //========================================================
    // View Depth
    //========================================================
    const float safeViewDepth = max(viewDepth,0.0f);


    //========================================================
    // Cameraからの距離に応じてCascade選択
    //========================================================
    const int cascadeIndex =
        clamp(SelectCascadeIndex(safeViewDepth), 0, CASCADE_SHADOW_COUNT - 1);

    //========================================================
    // 現在Cascadeだけ評価 ※境界Blendは行わない
	//========================================================
    return EvaluateSingleCascadeShadowFast(IN, geometricNormal, cascadeIndex);
}

//============================================================
// Cascade Shadow全体を評価
// ・現在Cascadeの選択
// ・Cascade境界ブレンド
// ・最遠Cascadeから影なしへのフェード
// をこの関数内で完結させる
//============================================================
CascadeShadowResult EvaluateCascadeShadow(
	pixelIn IN,
	float viewDepth,
	float3 geometricNormal
)
{
    CascadeShadowResult result = (CascadeShadowResult) 0;

	//============================================================
	// 初期値は完全に明るい
	// Shadow無効時やShadow最大距離より奥では
	// このまま返す
	//============================================================
    result.visibility = 1.0f;
    result.currentVisibility = 1.0f;
    result.nextVisibility = 1.0f;
    result.cascadeIndex = 0;
    result.nextCascadeIndex = 0;
    result.blendRate = 0.0f;

	//============================================================
	// MaterialがShadowを受け取らない場合
	// ShadowParam.x:
	//   0 = Shadowを受けない
	//   1 = Shadowを受ける
	//============================================================
    if (ShadowParam.x < 0.5f)
    {
        return result;
    }

    const float safeViewDepth = max(viewDepth, 0.0f);

	//============================================================
	// 0 = Cascade 0
	// 1 = Cascade 1
	// 2 = Cascade 2
	// 3 = Shadow最大距離より奥
	//============================================================
    const int selectedCascadeIndex = SelectCascadeIndex(safeViewDepth);

	//============================================================
	// Shadow最大距離より奥
	// ここでCascade 2へclampしてはいけない
	//============================================================
    if (selectedCascadeIndex < 0 || selectedCascadeIndex >= CASCADE_SHADOW_COUNT)
    {
        return result;
    }

    result.cascadeIndex = selectedCascadeIndex;
    result.nextCascadeIndex = selectedCascadeIndex;

	//============================================================
	// 現在CascadeのShadowを評価
	//============================================================
    result.currentVisibility =
		EvaluateSingleCascadeShadow(
			IN,
			geometricNormal,
			selectedCascadeIndex
		);

    result.nextVisibility = result.currentVisibility;
    result.visibility = result.currentVisibility;

	//============================================================
	// Cascade 0 → Cascade 1
	//============================================================
    if (selectedCascadeIndex == 0 && safeViewDepth >= CascadeBlendStarts.x)
    {
        const float blendRate =
			CalculateCascadeBlendRate(
				safeViewDepth,
				CascadeBlendStarts.x,
				CascadeSplits.x
			);

        result.nextCascadeIndex = 1;

        result.nextVisibility =
			EvaluateSingleCascadeShadow(
				IN,
				geometricNormal,
				1
			);

        result.blendRate = blendRate;

        result.visibility =
			lerp(
				result.currentVisibility,
				result.nextVisibility,
				blendRate
			);
    }
	//============================================================
	// Cascade 1 → Cascade 2
	//============================================================
    else if (selectedCascadeIndex == 1 && safeViewDepth >= CascadeBlendStarts.y)
    {
        const float blendRate =
			CalculateCascadeBlendRate(
				safeViewDepth,
				CascadeBlendStarts.y,
				CascadeSplits.y
			);

        result.nextCascadeIndex = 2;

        result.nextVisibility =
			EvaluateSingleCascadeShadow(
				IN,
				geometricNormal,
				2
			);

        result.blendRate = blendRate;

        result.visibility =
			lerp(
				result.currentVisibility,
				result.nextVisibility,
				blendRate
			);
    }
	//============================================================
	// Cascade 2 → 影なし
	// Shadow最大距離付近で影を急に消さず
	// visibility = 1.0へフェードさせる
	//============================================================
    else if (selectedCascadeIndex == 2 && safeViewDepth >= CascadeBlendStarts.z)
    {
        const float fadeRate =
			CalculateCascadeBlendRate(
				safeViewDepth,
				CascadeBlendStarts.z,
				CascadeSplits.z
			);

        result.nextVisibility = 1.0f;

        result.blendRate = fadeRate;

        result.visibility =
			lerp(
				result.currentVisibility,
				1.0f,
				fadeRate
			);
    }

    result.visibility = saturate(result.visibility);

    return result;
}

#endif
