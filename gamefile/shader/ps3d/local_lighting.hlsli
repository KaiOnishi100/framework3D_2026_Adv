#ifndef LOCAL_LIGHTING_HLSLI
#define LOCAL_LIGHTING_HLSLI

#include "ps_3d_bindings.hlsli"
#include "surface_data.hlsli"
#include "pixel_context.hlsli"
#include "../common/pbr_brdf.hlsli"
#include "spot_shadow.hlsli"
#include "point_shadow.hlsli"

//============================================================
// 1つのLocal LightによるPBR結果
//============================================================
struct LocalPBRContribution
{
    //========================================================
    // 拡散反射
    // SSS Diffusionの対象
    //========================================================
    float3 diffuse;

    //========================================================
    // 鏡面反射
    // SSS Diffusionしない
    //========================================================
    float3 specular;

    //========================================================
    // diffuse + specular
    //========================================================
    float3 finalColor;
};


//============================================================
// Point LightのGGX PBR評価
// worldPosition: 現在のPixelのワールド座標
// N: ワールド空間法線
// V: Pixelからカメラへ向かう方向
// F0: 垂直入射時の鏡面反射率
//============================================================
LocalPBRContribution EvaluatePointLightPBRBase(
	LocalLightData light,
	float3 worldPosition,
	float3 N,
	float3 V,
	float3 linearBaseColor,
	float metallic,
	float roughness,
	float3 F0
)
{
	//============================================================
	// Point Light 1灯分の結果
	//============================================================
    LocalPBRContribution result = (LocalPBRContribution) 0;
	
	//============================================================
	// Pixelから光源へのベクトル
	//============================================================
    const float3 toLight =
		light.PositionRange.xyz - worldPosition;
	
    const float actualDistanceSquared =
		max(dot(toLight, toLight), 0.000001f);
	
    const float distanceToLight =
		sqrt(actualDistanceSquared);

    const float lightRange =
		max(light.PositionRange.w, 0.001f);

	//============================================================
	// 範囲外なら光の影響なし
	//============================================================
    if (distanceToLight >= lightRange)
    {
        return result;
    }

    const float3 L = toLight / distanceToLight;

    const float NdotL = saturate(dot(N, L));

    if (NdotL <= 0.0f)
    {
        return result;
    }
	
	//============================================================
	// Half Vector
	//============================================================
    const float3 H = normalize(V + L);

    const float NdotV = saturate(dot(N, V));

    const float HdotV = saturate(dot(H, V));

	//============================================================
	// GGX Cook-Torrance BRDF
	//============================================================
    const float distribution =
		DistributionGGX(
			N,
			H,
			roughness
		);

    const float geometry =
		GeometrySmith(
			N,
			V,
			L,
			roughness
		);

    const float3 fresnel =
		FresnelSchlick(
			HdotV,
			F0
		);

    const float3 specularNumerator = distribution * geometry * fresnel;

    const float specularDenominator = max(4.0f * NdotV * NdotL, 0.0001f);

    const float3 specularBRDF = specularNumerator / specularDenominator;

	//============================================================
	// エネルギー保存
	// Fresnel反射されなかった光だけが拡散反射へ回る
	// ※金属は拡散反射を持たない
	//============================================================
    const float3 specularRatio = fresnel;

    const float3 diffuseRatio = (1.0f - specularRatio) * (1.0f - metallic);
	
    const float3 diffuseBRDF =
		diffuseRatio * linearBaseColor / PBR_PI;

	//============================================================
	// 距離減衰
	// 逆二乗減衰に加え、Range付近で滑らかに0へする
	//============================================================
    const float normalizedDistance = distanceToLight / lightRange;

    float rangeAttenuation = saturate(1.0f - pow(normalizedDistance, 4.0f));

    rangeAttenuation *= rangeAttenuation;

	//============================================================
	// 有効光源半径
	// PointLight : 0.12m
	// SpotLight  : 0.35m
	//============================================================
    const uint lightType = (uint) (light.Parameters.x + 0.5f);

    const float sourceRadius = (lightType == LOCAL_LIGHT_TYPE_SPOT) ? 0.35f : 0.12f;
	
	//============================================================
	// Softened Distance Attenuation
	// 距離が0に近づいても
	// 分母がsourceRadius^2より小さくならない
	//============================================================
    const float softenedDistanceSquared = actualDistanceSquared + sourceRadius * sourceRadius;
	
    const float distanceAttenuation = 1.0f / softenedDistanceSquared;

    const float attenuation = rangeAttenuation * distanceAttenuation;

	//============================================================
	// HDR Radiance
	//============================================================
    const float3 radiance =
		light.ColorIntensity.rgb * max(light.ColorIntensity.w, 0.0f) * attenuation;

    //============================================================
	// Diffuse
	// Screen-Space SSS Diffusion対象
	//============================================================
    result.diffuse = diffuseBRDF * radiance * NdotL;


	//============================================================
	// Specular
	// SSS Diffusionしない
	//============================================================
    result.specular = specularBRDF * radiance * NdotL;


	//============================================================
	// 最終結果
	//============================================================
    result.finalColor = result.diffuse + result.specular;

    return result;
}

//============================================================
// Shadow対応Point Light PBR
// shadingNormal: PBR Lighting用法線
// geometricNormal: Shadow Bias用法線
//============================================================
LocalPBRContribution EvaluatePointLightPBR(
	LocalLightData light,
	float3 worldPosition,
	float3 shadingNormal,
	float3 geometricNormal,
	float3 viewDirection,
	float3 linearBaseColor,
	float metallic,
	float roughness,
	float3 materialF0
)
{
    //========================================================
    // Point LightのDiffuse / Specularを分離した状態で取得する
    //========================================================
    LocalPBRContribution result =
        EvaluatePointLightPBRBase(
            light,
            worldPosition,
            shadingNormal,
            viewDirection,
            linearBaseColor,
            metallic,
            roughness,
            materialF0
        );


    //========================================================
    // 初期状態ではShadowなし
    //========================================================
    float pointShadowVisibility = 1.0f;


    //========================================================
    // Parameters.w
    //
    // 0 = Shadowなし
    // 1 = ShadowIndex 0
    // 2 = ShadowIndex 1
    //========================================================
    const int encodedShadowIndex = (int) (light.Parameters.w + 0.5f);


    if (encodedShadowIndex > 0)
    {
        const uint shadowIndex = (uint) (encodedShadowIndex - 1);


        //====================================================
        // Pixel → Point Light
        //
        // Shadow Bias計算用。
        //====================================================
        const float3 pixelToLightDirection =
            normalize(light.PositionRange.xyz - worldPosition);


        pointShadowVisibility = CalculatePointLightShadow(
                worldPosition,
                light.PositionRange.xyz,
                geometricNormal,
                pixelToLightDirection,
                shadowIndex
            );
    }


    //========================================================
    // Diffuse / SpecularそれぞれへShadow適用
    //========================================================
    result.diffuse *= pointShadowVisibility;

    result.specular *= pointShadowVisibility;


    //========================================================
    // 最終結果を再構築
    //========================================================
    result.finalColor = result.diffuse + result.specular;

    return result;
}

//============================================================
// Spot LightのGGX PBR評価
// Spot LightはPoint Lightと同様に、
// 位置・距離・Rangeによる減衰を持つ
// さらにライトの向きとPixelの位置関係から
// コーン内の角度減衰を追加する
//============================================================
LocalPBRContribution EvaluateSpotLightPBR(
	LocalLightData light,
	float3 worldPosition,
	float3 shadingNormal,
	float3 geometricNormal,
	float3 viewDirection,
	float3 linearBaseColor,
	float metallic,
	float roughness,
	float3 materialF0
)
{
    LocalPBRContribution result = (LocalPBRContribution) 0;
	
	//============================================================
	// ライト位置からPixelへ向かうベクトル
	// Spot Lightの向きもライト位置から外側へ向かう方向として保存されている
	//============================================================
    const float3 lightToPixel =
		worldPosition -light.PositionRange.xyz;

	//============================================================
	// 距離の2乗
	// 0除算やnormalizeの不正値を防ぐ
	//============================================================
    const float lightToPixelLengthSquared = dot(lightToPixel, lightToPixel);

    if (lightToPixelLengthSquared <= 0.000001f)
    {
        return result;
    }

	//============================================================
	// ライトからPixelへ向かう方向
	//============================================================
    const float3 lightToPixelDirection = lightToPixel * rsqrt(lightToPixelLengthSquared);

	//============================================================
	// Spot Lightの中心軸
	// C++側で正規化済みだが
	// Shader側でも念のため正規化する
	//============================================================
    const float3 spotDirection =
		normalize(light.DirectionOuterCos.xyz);

	//============================================================
	// ライトの中心軸と、
	// ライトからPixelへ向かう方向の内積
	// 1.0: コーン中心
	// 小さくなる: コーンの外側へ近づく
	//============================================================
    const float coneCos = dot(spotDirection, lightToPixelDirection);

	//============================================================
	// コーン角度
	// innerCos: この値以上なら最大強度
	// outerCos: この値以下なら完全に0
	// 角度が小さい方がcos値は大きいため
	// 通常はinnerCos > outerCosになる
	//============================================================
    const float innerCos = light.Parameters.y;
    const float outerCos = light.DirectionOuterCos.w;

	//============================================================
	// コーンの外側なら光の影響なし
	//============================================================
    if (coneCos <= outerCos)
    {
        return result;
    }

	//============================================================
	// Outer ConeからInner Coneまでを0～1へ変換
	// outerCos: 0
	// innerCos: 1
	//============================================================
    const float coneDenominator = max(innerCos - outerCos, 0.0001f);

    float coneAttenuation = saturate((coneCos - outerCos) / coneDenominator);

	//============================================================
	// SmoothStep相当
	// 単純な線形減衰よりもInner・Outer境界が滑らかになる
	//============================================================
    coneAttenuation = coneAttenuation * coneAttenuation * (3.0f - 2.0f * coneAttenuation);

	//============================================================
	// Point Lightと同じ距離減衰・GGX BRDFを計算
	//============================================================
    result =
		EvaluatePointLightPBRBase(
		    light,
		    worldPosition,
		    shadingNormal,
		    viewDirection,
		    linearBaseColor,
		    metallic,
		    roughness,
		    materialF0
		);
	//============================================================
	// Spot Shadow可視率
	// 初期値: 1.0 = 影なし
	//============================================================
    float spotShadowVisibility = 1.0f;

    const int encodedShadowIndex = (int) (light.Parameters.w + 0.5f);

    if (encodedShadowIndex > 0)
    {
		//========================================================
		// 保存時に+1した値を元へ戻す
		//========================================================
        const uint shadowIndex = (uint) (encodedShadowIndex - 1);

        const float3 pixelToLightDirection = normalize(light.PositionRange.xyz -worldPosition);

        spotShadowVisibility =
			CalculateSpotLightShadow(
				worldPosition,
				geometricNormal,
				pixelToLightDirection,
				shadowIndex
			);
		//const SpotShadowResult spotShadow =
		//EvaluateSpotLightShadow(
		//worldPosition,
		//geometricNormal,
		//pixelToLightDirection,
		//shadowIndex
		//);

    }
	
    //============================================================
	// Spot固有のConeとShadowをDiffuse / Specular双方へ適用
	//============================================================
	
    //spotShadowVisibility = 1.0f;
    const float visibility = coneAttenuation * spotShadowVisibility;

    result.diffuse *= visibility;
    result.specular *= visibility;
    result.finalColor = result.diffuse + result.specular;

    return result;
}

//============================================================
// Local Light全体の計算結果
//============================================================
struct LocalLightingResult
{
	//========================================================
    // Point Light
    //========================================================

    // Point LightのDiffuse成分
    float3 pointDiffuse;

    // Point LightのSpecular成分
    float3 pointSpecular;


    //========================================================
    // Spot Light
    //========================================================

    // Spot LightのDiffuse成分
    float3 spotDiffuse;

    // Spot LightのSpecular成分
    float3 spotSpecular;


    //========================================================
    // Local Light全体
    //========================================================

    // Point + SpotのDiffuse
    // Screen-Space SSS Diffusion対象
    float3 diffuseFinal;


    // Point + SpotのSpecular
    // Blurしない
    float3 specularFinal;


    //========================================================
    // 最終Local Lighting
    //
    // diffuseFinal + specularFinal
    //========================================================
    float3 finalColor;


    //========================================================
    // Debug用
    //========================================================
    uint evaluatedLightCount;
	
};

//============================================================
// Local Light全体を評価
// Point Light・Spot Lightのループをmain()から移動する
//============================================================
LocalLightingResult EvaluateLocalLighting(SurfaceData surface,PixelContext context)
{
    LocalLightingResult result = (LocalLightingResult) 0;

	//============================================================
	// SceneBufferから有効ライト数を取得
	//============================================================
    const uint localLightCount =
		min((uint) (LocalLightParam.x + 0.5f), MAX_LOCAL_LIGHTS);

	//============================================================
	// 登録されたライトを順番に評価
	//============================================================
	[loop]
    for (uint lightIndex = 0; lightIndex < localLightCount; ++lightIndex)
    {
        const LocalLightData light = txLocalLights[lightIndex];

		//========================================================
		// 無効ライトを除外
		//========================================================
        if (light.Parameters.z < 0.5f)
        {
            continue;
        }

		// 光を受ける表面がこのライトの所属部屋の外なら加算しない
		// 懐中電灯や脱出口ライトはRoomMin.wが0なので判定を通過する
        if (light.RoomMin.w >= 0.5f &&
			(any(context.worldPosition < light.RoomMin.xyz) ||
			 any(context.worldPosition > light.RoomMax.xyz)))
        {
            continue;
        }
		
        const uint lightType = (uint) (light.Parameters.x + 0.5f);

		//========================================================
		// Point Light
		//========================================================
        if (lightType == LOCAL_LIGHT_TYPE_POINT)
        {
            const LocalPBRContribution contribution =
				EvaluatePointLightPBR(
				    light,
				    context.worldPosition,
				    surface.shadingNormal,
				    surface.geometricNormal,
				    context.viewDirection,
				    surface.baseColor.rgb,
				    surface.metallic,
				    surface.roughness,
				    surface.F0
				);
			
			//========================================================
			// Diffuse / Specularを別々に蓄積
			//========================================================
            result.pointDiffuse += contribution.diffuse;

            result.pointSpecular += contribution.specular;

            ++result.evaluatedLightCount;
        }
		//========================================================
		// Spot Light
		//========================================================
        else if (lightType == LOCAL_LIGHT_TYPE_SPOT)
        {
            const LocalPBRContribution contribution =
				EvaluateSpotLightPBR(
				    light,
				    context.worldPosition,
				    surface.shadingNormal,
				    surface.geometricNormal,
				    context.viewDirection,
				    surface.baseColor.rgb,
				    surface.metallic,
				    surface.roughness,
				    surface.F0
				);


            result.spotDiffuse += contribution.diffuse;

            result.spotSpecular += contribution.specular;

            ++result.evaluatedLightCount;
        }
    }

    //============================================================
	// Local Diffuse
	//============================================================
    result.diffuseFinal = result.pointDiffuse + result.spotDiffuse;

	//============================================================
	// Local Specular
	//============================================================
    result.specularFinal = result.pointSpecular + result.spotSpecular;

	//============================================================
	// 従来と同じLocal Light総結果
	//============================================================
    result.finalColor = result.diffuseFinal + result.specularFinal;

    return result;
}


#endif