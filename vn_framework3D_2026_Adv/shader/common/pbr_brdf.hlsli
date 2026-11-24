#ifndef PBR_BRDF_HLSLI
#define PBR_BRDF_HLSLI
//============================================================
// PBR共通定数
//============================================================
static const float PBR_PI = 3.14159265359f;

static const float PBR_EPSILON = 0.0001f;

//============================================================
// GGX Normal Distribution Function
// マイクロファセット法線がHalf Vector方向を向く割合を求める
// roughnessが小さい: 鋭い反射
// roughnessが大きい: 広くぼやけた反射
//============================================================
float DistributionGGX(float3 normal, float3 halfVector, float roughness)
{
    const float3 N = normalize(normal);

    const float3 H = normalize(halfVector);

	//========================================================
	// roughnessを二乗して知覚的な変化を自然にする
	//========================================================
    const float alpha = roughness * roughness;
    const float alphaSquared = alpha * alpha;
    const float NdotH = saturate(dot(N, H));
    const float NdotHSquared = NdotH * NdotH;
    const float denominatorBase = NdotHSquared * (alphaSquared - 1.0f) + 1.0f;

    const float denominator =
		PBR_PI * denominatorBase * denominatorBase;

    return alphaSquared / max(denominator, PBR_EPSILON);
}

//============================================================
// Schlick-GGX Geometry Function
// 視線またはライト方向から見たときに
// マイクロファセット同士がどれだけ遮蔽するかを求める
//============================================================
float GeometrySchlickGGX(float NdotDirection, float roughness)
{
	//========================================================
	// 直接光用のk
	//========================================================
    const float roughnessPlusOne = roughness + 1.0f;

    const float k = (roughnessPlusOne * roughnessPlusOne) / 8.0f;

    const float denominator = NdotDirection * (1.0f - k) + k;

    return NdotDirection / max(denominator, PBR_EPSILON);
}

//============================================================
// Smith Geometry Function
// 視線方向とライト方向の両方に対して
// マイクロファセットの遮蔽率を計算する
//============================================================
float GeometrySmith(float3 normal, float3 viewDirection, float3 lightDirection, float roughness)
{
    const float NdotV = saturate(dot(normal, viewDirection));
    const float NdotL = saturate(dot(normal, lightDirection));
    const float geometryView = GeometrySchlickGGX(NdotV, roughness);
    const float geometryLight = GeometrySchlickGGX(NdotL, roughness);

    return geometryView * geometryLight;
}

//============================================================
// Schlick Fresnel
// 見る角度によって反射率が変化する現象を近似する
// 正面: 反射が弱い
// 斜め: 反射が強い
//============================================================
float3 FresnelSchlick(float cosTheta, float3 reflectanceAtNormalIncidence)
{
    return
		reflectanceAtNormalIncidence +
		(1.0f - reflectanceAtNormalIncidence) *
		pow(saturate(1.0f - cosTheta), 5.0f);
}

//============================================================
// Roughness対応Schlick Fresnel
// 通常のFresnelSchlickにRoughnessを考慮したもの
// Roughnessが高い場合、
// 斜め方向の反射率が過剰に1.0へ近づくのを抑える
//============================================================
float3 FresnelSchlickRoughness(
	float cosTheta,
	float3 reflectanceAtNormalIncidence,
	float roughness
)
{
	//========================================================
	// Roughnessが高いほど、最大反射率を抑える
	//========================================================
    const float3 maximumReflectance =
		max(float3(1.0f - roughness,
				   1.0f - roughness,
				   1.0f - roughness),
			reflectanceAtNormalIncidence
		);

    return reflectanceAtNormalIncidence + (maximumReflectance - reflectanceAtNormalIncidence) *
		pow(saturate(1.0f - cosTheta), 5.0f);
}

//============================================================
// Directional Light用Cook-Torrance PBR
// normal: ワールド空間法線
// viewDirection: ピクセルからカメラへ向かう方向
// lightDirection: ピクセルからライトへ向かう方向
// radiance: ライトの色と強度
// baseColor: Linear空間のBaseColor
// metallic:
//   0 = 非金属
//   1 = 金属
// roughness:
//   0に近い = 鏡面
//   1に近い = 粗い表面
//============================================================
float3 EvaluateDirectPBR(
	float3 normal,
	float3 viewDirection,
	float3 lightDirection,
	float3 radiance,
	float3 baseColor,
	float metallic,
	float roughness
)
{
    const float3 N = normalize(normal);
    const float3 V = normalize(viewDirection);
    const float3 L = normalize(lightDirection);
    const float NdotL = saturate(dot(N, L));
    const float NdotV = saturate(dot(N, V));

	//========================================================
	// ライトまたはカメラが裏側なら直接光は0
	//========================================================
    if (NdotL <= 0.0f || NdotV <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

	//========================================================
	// Half Vector
	//========================================================
    const float3 H = normalize(V + L);

	//========================================================
	// 非金属の基本反射率は約4%
	// 金属の場合はBaseColorが鏡面反射色になる
	//========================================================
    float3 F0 = float3(0.04f, 0.04f, 0.04f);

    F0 = lerp(F0, baseColor, metallic);

	//========================================================
	// Cook-Torrance BRDFの各項
	//========================================================
    const float distribution = DistributionGGX(N, H, roughness);
    const float geometry = GeometrySmith(N, V, L, roughness);
    const float3 fresnel = FresnelSchlick(saturate(dot(H, V)), F0);

	//========================================================
	// Cook-Torrance鏡面反射
	//       D * G * F
	// ---------------------
	// 4 * NdotV * NdotL
	//========================================================
    const float3 specularNumerator =
		distribution * geometry * fresnel;

    const float specularDenominator =
		max(4.0f * NdotV * NdotL, PBR_EPSILON);

    const float3 specular =
		specularNumerator / specularDenominator;

	//========================================================
	// エネルギー保存
	// 鏡面反射した分は拡散反射へ回さない
	//========================================================
    const float3 specularRatio = fresnel;

    float3 diffuseRatio = 1.0f - specularRatio;

	//========================================================
	// ※金属は内部で光を吸収するため原則として拡散反射を持たない
	//========================================================
    diffuseRatio *= 1.0f - metallic;

	//========================================================
	// Lambert Diffuse
	//========================================================
    const float3 diffuse = diffuseRatio * baseColor / PBR_PI;

	//========================================================
	// 最終的な直接光
	//========================================================
    return (diffuse + specular) * radiance * NdotL;
}

//============================================================
// Specular Occlusion
// ※AOを鏡面反射へそのまま掛けるのではなく
// ※視線角度とRoughnessを考慮して
// ※凹部での不自然な環境反射を抑える
// NdotV: 法線と視線方向の内積
// ambientOcclusion: ORMのRチャンネル
// roughness: 表面の粗さ
//============================================================
float ComputeSpecularOcclusion(
	float NdotV,
	float ambientOcclusion,
	float roughness
)
{
	//========================================================
	// Roughnessが低い面: 視線角度の影響を強く受ける
	// Roughnessが高い面: より広い範囲から環境光を受ける
	//========================================================
    const float exponent =
		exp2(-16.0f * roughness - 1.0f);

    const float specularOcclusion =
		pow(saturate(NdotV + ambientOcclusion), exponent) - 1.0f + ambientOcclusion;

    return saturate(specularOcclusion);
}


#endif