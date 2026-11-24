#ifndef SURFACE_DATA_HLSLI
#define SURFACE_DATA_HLSLI

#include "ps_3d_bindings.hlsli"
#include "../common/color_common.hlsli"

//============================================================
// Pixel単位のマテリアル情報
// Texture、MaterialBuffer、Vertex Shader出力から
// ライティングに必要な値をまとめた構造体
// Lighting関数側はTextureを直接参照せず
// 基本的にこのSurfaceDataを受け取る
//============================================================
struct SurfaceData
{
	//========================================================
	// Material Textureへ使用するUV
	// TilingOffset適用後の値
	//========================================================
    float2 materialUV;

	//========================================================
	// Linear空間のBaseColor
	// rgb: Linear BaseColor
	// a: Alpha
	//========================================================
    float4 baseColor;

	//========================================================
	// モデル本来の形状法線
	// Shadow Biasなど、
	// Normal Mapの細かい凹凸を使いたくない処理で使用する
	//========================================================
    float3 geometricNormal;

	//========================================================
	// Normal Map適用後のLighting用法線
	//========================================================
    float3 shadingNormal;

	//========================================================
	// World空間Tangent / Bitangent
	// デバッグ表示や将来の異方性反射でも使用できる
	//========================================================
    float3 tangent;

    float3 bitangent;

	//========================================================
	// ORM Textureの生値
	// x = AO
	// y = Roughness
	// z = Metallic
	//========================================================
    float3 sampledORM;

	//========================================================
	// 最終的にLightingへ使用するPBR値
	//========================================================
    float metallic;

    float roughness;

    float ambientOcclusion;

	//========================================================
	// PBR基本反射率
	// 非金属: 約0.04
	// 金属: BaseColorに近い値
	//========================================================
    float3 F0;

	//========================================================
	// Emissive最終値
	// Emissive Color × Intensity
	//========================================================
    float3 emissive;
	
    float useEmissiveMap;

	//========================================================
	// 使用フラグ
	// usePBR:
	//   0 = Legacy
	//   1 = PBR
	// useORM:
	//   0 = PBRParam固定値
	//   1 = ORM Texture
	//========================================================
    float usePBR;

    float useORM;
	
	// 0 / 1
    float useSubsurface;

    // SSS全体強度
    float subsurfaceStrength;

    // 0 = 厚い / 透過しない
    // 1 = 薄い / 強く透過
    float subsurfaceThickness;
	
	// Skin Mask
	// 0 = Skinではない
	// 1 = Skin
    float skinMask;

    // 光の透過方向を法線側へ曲げる量
    float subsurfaceDistortion;

    // 散乱色
    float3 subsurfaceColor;

    // 透過光の鋭さ
    float subsurfacePower;
};

//============================================================
// SurfaceData構築
// ここで行う処理:
// ・Material UV計算
// ・BaseColor Texture取得
// ・sRGBからLinearへの変換
// ・TBN構築
// ・Normal Map適用
// ・ORM Texture取得
// ・Metallic / Roughness / AO決定
// ・F0計算
// ・Emissive計算
//============================================================
SurfaceData BuildSurfaceData(pixelIn IN)
{
    SurfaceData surface = (SurfaceData) 0;

	//============================================================
	// Material UV
	// TilingOffset:
	//   xy = Tiling
	//   zw = Offset
	//============================================================
    surface.materialUV = IN.tx0 * TilingOffset.xy + TilingOffset.zw;

	//============================================================
	// BaseColor Texture取得
	//============================================================
    const float4 sampledBaseColor =
		txDiffuse.Sample(
		    samLinear,
		    surface.materialUV
		);
	
	//============================================================
	// Texture、Vertex Color、Material Colorを合成
	//============================================================
    surface.baseColor =
		float4(
		    sampledBaseColor.rgb * IN.col.rgb * Diffuse.rgb,
		    sampledBaseColor.a * IN.col.a * Diffuse.a
		);
	
	//============================================================
	// モデル本来のWorld法線
	//============================================================
    surface.geometricNormal = normalize(IN.nor);

    if (NormalParam.x < 0.5f)
    {
        surface.shadingNormal = surface.geometricNormal;


		//========================================================
		// 安全なTangent Basisも用意しておく
		//========================================================
        const float3 helperAxis =
			abs(surface.geometricNormal.y) < 0.999f ?
			float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);

        surface.tangent = normalize(cross(helperAxis, surface.geometricNormal));
        surface.bitangent = normalize(cross(surface.geometricNormal, surface.tangent));
    }
    else
    {
		
		//============================================================
		// 補間後のWorld Tangent
		//============================================================
        float3 tangent = normalize(IN.tan.xyz);

		//============================================================
		// Gram-Schmidt法による再直交化
		// Pixel補間によってTangentとNormalが
		// 完全な直角ではなくなるため、Normal成分を除去する
		//============================================================
        tangent = normalize(tangent - surface.geometricNormal * dot(surface.geometricNormal, tangent));

		//============================================================
		// Bitangent復元
		// IN.tan.w: Tangent空間の向きを示す符号
		//============================================================
        const float3 bitangent =
			normalize(cross(surface.geometricNormal, tangent)) * IN.tan.w;

        surface.tangent = tangent;

        surface.bitangent = bitangent;

		//============================================================
		// Tangent空間からWorld空間へ変換する行列
		//============================================================
        const float3x3 TBN =
			float3x3(
				tangent,
				bitangent,
				surface.geometricNormal
			);

		//============================================================
		// Normal Map取得
		// Texture: 0～1
		// Tangent法線: -1～1
		//============================================================
        float3 tangentNormal =
			txNormal.Sample(
				samLinear,
				surface.materialUV
			).xyz *
			2.0f - 1.0f;

		//============================================================
		// Normal Map強度
		// XYだけへ強度を適用しZは維持する
		//============================================================
        tangentNormal.xy *=
			NormalParam.y;

		//============================================================
		// Green Channel方向補正
		// 通常: NormalParam.z = 1
		// Y反転: NormalParam.z = -1
		//============================================================
        tangentNormal.y *= NormalParam.z;
        tangentNormal = normalize(tangentNormal);

		//============================================================
		// Tangent空間法線をWorld空間へ変換
		//============================================================
        const float3 mappedNormal = normalize(mul(tangentNormal, TBN));

		//============================================================
		// Normal Map使用フラグ
		// NormalParam.x:
		//   0 = Geometric Normal
		//   1 = Normal Map
		//============================================================
        surface.shadingNormal =
			normalize(lerp(surface.geometricNormal, mappedNormal, saturate(NormalParam.x)));
    }
	

	//============================================================
	// ORM Texture
	//
	// R = Ambient Occlusion
	// G = Roughness
	// B = Metallic
	//============================================================
    surface.sampledORM =
		txORM.Sample(
			samLinear,
			surface.materialUV
		).rgb;

	//============================================================
	// ORM使用フラグ
	//============================================================
    surface.useORM = step(0.5f, ORMParam.x);

	//============================================================
	// Texture側AO
	//============================================================
    const float textureAO = saturate(surface.sampledORM.r * ORMParam.y);

	//============================================================
	// Texture側Roughness
	// GGXの数値不安定を防ぐため、最低値を0.04にする
	//============================================================
    const float textureRoughness =
		clamp(
			surface.sampledORM.g * ORMParam.z,
			0.04f,
			1.0f
		);

	//============================================================
	// Texture側Metallic
	//============================================================
    const float textureMetallic = saturate(surface.sampledORM.b * ORMParam.w);

	//============================================================
	// 固定値とTexture値を切り替える
	//============================================================
    surface.metallic =
		lerp(
			saturate(PBRParam.x),
			textureMetallic,
			surface.useORM
		);

    surface.roughness =
		lerp(
			clamp(PBRParam.y, 0.04f, 1.0f),
			textureRoughness,
			surface.useORM
		);

    surface.ambientOcclusion =
		lerp(
			saturate(PBRParam.z),
			textureAO,
			surface.useORM
		);

	//============================================================
	// PBR使用フラグ
	//============================================================
    surface.usePBR = step(0.5f, PBRParam.w);

	//============================================================
	// Fresnel F0
	//============================================================
    surface.F0 =
		lerp(
			float3(0.04f, 0.04f, 0.04f),
			surface.baseColor.rgb,
			surface.metallic
		);

	//============================================================
	// Emissive
	// EmissiveParam:
	// rgb : Emissive Color / Texture Multiplier
	// w   : Emissive Texture使用フラグ
	//      0 = Textureなし
	//      1 = Textureあり
	//============================================================


	//============================================================
	// Texture使用フラグ
	//============================================================
    surface.useEmissiveMap = step(0.5f, EmissiveParam.w);

	//============================================================
	// Emissive Texture取得
	//============================================================
    const float3 sampledEmissive =
    txEmissive.Sample(
        samLinear,
        surface.materialUV
    ).rgb;


	//============================================================
	// Texture倍率
	//============================================================
    const float3 emissiveMap =
		lerp(
		    float3(1.0f, 1.0f, 1.0f),
		    sampledEmissive,
		    surface.useEmissiveMap
		);

	//============================================================
	// 最終Emissive
	//============================================================
    surface.emissive = max(EmissiveParam.rgb, 0.0f) * emissiveMap;
	
	//============================================================
	// Subsurface Scattering
	//============================================================

	//------------------------------------------------------------
	// SSSはPBR Materialだけで使用する
	// Legacy Lightingでは無効
	//------------------------------------------------------------
    surface.useSubsurface = (SSSParam.x >= 0.5f && surface.usePBR >= 0.5f) ? 1.0f : 0.0f;

	//============================================================
	// Strength
	//============================================================
    surface.subsurfaceStrength = max(SSSParam.y, 0.0f);

	//============================================================
	// Global Thickness / Transmission
	//============================================================
    float subsurfaceThickness = saturate(SSSParam.z);

    float skinMask = 1.0f;

	//============================================================
	// Thickness Map
	//============================================================
    if (SSSMapParam.x >= 0.5f)
    {
		//========================================================
		// R Channelのみ使用
		// Engine Convention:
		// 0 = Thick
		// 1 = Thin
		//========================================================
        float thicknessMask =
        txThickness.Sample(
            samLinear,
            surface.materialUV
        ).r;


		//========================================================
		// Texture規約が逆の場合
		// 0 → 1
		// 1 → 0
		//========================================================
        if (SSSMapParam.z >= 0.5f)
        {
            thicknessMask = 1.0f - thicknessMask;
        }


		//========================================================
		// Map強度
		//========================================================
        thicknessMask = saturate(thicknessMask * max(SSSMapParam.y, 0.0f));

        skinMask = saturate(txSkinMask.Sample(samLinear, surface.materialUV).r);

		//========================================================
		// Global値 × Local Texture
		//========================================================
        subsurfaceThickness *= thicknessMask;
    }


	//============================================================
	// SurfaceDataへ保存
	//============================================================
    surface.subsurfaceThickness = saturate(subsurfaceThickness);
    surface.skinMask = skinMask;
	
	//============================================================
	// Distortion
	//============================================================
    surface.subsurfaceDistortion = saturate(SSSParam.w);


	//============================================================
	// Scatter Color
	//============================================================
    surface.subsurfaceColor = max(SSSColor.rgb, 0.0f);

	//============================================================
	// Power
	// 0付近だとpow()が極端になるので最低値を設定
	//============================================================
    surface.subsurfacePower = max(SSSColor.w, 0.1f);
	
    return surface;
}

#endif