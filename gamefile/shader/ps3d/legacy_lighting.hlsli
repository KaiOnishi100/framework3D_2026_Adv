#ifndef LEGACY_LIGHTING_HLSLI
#define LEGACY_LIGHTING_HLSLI

#include "ps_3d_bindings.hlsli"
#include "surface_data.hlsli"
#include "pixel_context.hlsli"

//============================================================
// Lambert拡散反射
// normal: ワールド空間の法線
// lightDirection: ピクセルからライトへ向かう方向
// lightColor: ライトの色・強度
// diffuseColor: マテリアルの拡散色
// 戻り値: ライト方向と法線の角度に応じた拡散反射光
//============================================================
float3 EvaluateLambert(
	float3 normal,
	float3 lightDirection,
	float3 lightColor,
	float3 diffuseColor
)
{
	//========================================================
	// 念のため関数内でも正規化する
	// 呼び出し側で正規化済みでも
	// 関数単体で安全に使用できるようにする
	//========================================================
    const float3 N = normalize(normal);
    const float3 L = normalize(lightDirection);

	//========================================================
	// Lambertの基本式
	// N・L:
	//   1.0 = ライトへ正面を向いている
	//   0.0 = ライトと直角
	//  負値 = ライトと反対を向いている
	// saturateにより0～1へ制限する
	//========================================================
    const float NdotL = saturate(dot(N, L));

	//========================================================
	// 拡散反射光
	//========================================================
    return NdotL * lightColor * diffuseColor;
}

//============================================================
// Blinn-Phong鏡面反射
// normal: ワールド空間の法線
// lightDirection: ピクセルからライトへ向かう方向
// viewDirection: ピクセルからカメラへ向かう方向
// lightColor: ライト色
// specularColor: マテリアルの鏡面反射色
// shininess: ハイライトの鋭さ
// 戻り値: 鏡面反射光
//============================================================
float3 EvaluateBlinnPhong(
	float3 normal,
	float3 lightDirection,
	float3 viewDirection,
	float3 lightColor,
	float3 specularColor,
	float shininess
)
{
	//========================================================
	// 各方向を正規化
	//========================================================
    const float3 N =normalize(normal);
    const float3 L =normalize(lightDirection);
    const float3 V =normalize(viewDirection);

	//========================================================
	// ライトと視線の中間方向
	// Blinn-Phongでは反射ベクトルではなく
	// Half Vectorを使って鏡面反射を計算する
	//========================================================
    const float3 H =normalize(L +V);

	//========================================================
	// ライトが裏側にある場合はスペキュラを出さない
	// これを入れないと、裏面側にもハイライトが出る場合がある
	//========================================================
    const float NdotL = saturate(dot(N, L));

    if (NdotL <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

	//========================================================
	// 法線とHalf Vectorの一致度
	//========================================================
    const float NdotH = saturate(dot(N, H));

	//========================================================
	// 0以下になるとpowが不安定になるため最低値を保証
	//========================================================
    const float safeShininess =max(shininess,1.0f);

	//========================================================
	// 鏡面反射強度
	// shininessが大きい: 小さく鋭いハイライト
	// shininessが小さい: 広くぼやけたハイライト
	//========================================================
    const float specularStrength = pow(NdotH, safeShininess);

    return specularColor * lightColor * specularStrength;
}

//============================================================
// Legacy Lighting全体
//============================================================
float3 EvaluateLegacyLighting(
	SurfaceData surface,
	PixelContext context,
	float shadowVisibility
)
{
	//============================================================
	// Lambert Diffuse
	//============================================================
    const float3 diffuse =
		EvaluateLambert(
			surface.shadingNormal,
			context.directionalLightDirection,
			LightCol.rgb,
			surface.baseColor.rgb) *
		shadowVisibility;

	//============================================================
	// Blinn-Phong Specular
	//============================================================
    const float3 specular =
		EvaluateBlinnPhong(
			surface.shadingNormal,
			context.directionalLightDirection,
			context.viewDirection,
			LightCol.rgb,
			Specular.rgb,
			max(Specular.w, 1.0f)) *
		shadowVisibility;

	//============================================================
	// Legacy Ambient
	//============================================================
    const float3 ambient = surface.baseColor.rgb * (LightAmb.rgb + Ambient.rgb);

    return diffuse + specular + ambient + surface.emissive;
}

#endif