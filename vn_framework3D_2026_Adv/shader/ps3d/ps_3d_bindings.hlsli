#ifndef PS_3D_BINDINGS_HLSLI
#define PS_3D_BINDINGS_HLSLI


//============================================================
// Local Light種類
// C++側のLightTypeと一致させる
//============================================================
static const uint LOCAL_LIGHT_TYPE_POINT = 0u;
static const uint LOCAL_LIGHT_TYPE_SPOT = 1u;
static const uint MAX_LOCAL_LIGHTS = 16u;

//============================================================
// Point Light Shadow設定
//============================================================

//============================================================
// 1つのPoint Lightに必要なShadow Map面数
// +X, -X, +Y, -Y, +Z, -Z
//============================================================
static const uint POINT_SHADOW_FACE_COUNT = 6u;

//============================================================
// Shadowを持てるPoint Lightの最大数
// 1灯あたり6Slice必要なので、最大4灯の場合は
// Texture2DArray全体で24Slice使用する
//============================================================
static const uint MAX_POINT_SHADOW_LIGHTS = 3u;

//============================================================
// Shadowを生成できるSpot Lightの最大数
// C++側のSpotLightShadowManager::MaxShadowLightsと同じ値にする
//============================================================
static const uint MAX_SPOT_SHADOW_LIGHTS = 4u;

//============================================================
// Point Shadow Texture2DArrayの総Slice数
//============================================================
static const uint POINT_SHADOW_TOTAL_SLICES = MAX_POINT_SHADOW_LIGHTS * POINT_SHADOW_FACE_COUNT;

//============================================================
// Pixel Shader入力
//============================================================
struct pixelIn
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 tx0 : TEXCOORD0;
    float3 nor : TEXCOORD1;

	// xyz : Pixelからカメラへ向かうベクトル
	// w   : View空間深度
    float4 eye : TEXCOORD2;

	// Cascade Shadow座標
    float4 sdw0 : TEXCOORD3;
    float4 sdw1 : TEXCOORD4;
    float4 sdw2 : TEXCOORD5;

	// xyz : World Tangent
	// w   : Tangent Handedness
    float4 tan : TEXCOORD6;
};

//============================================================
// シーン定数バッファ
//============================================================
cbuffer SceneBuffer : register(b0)
{
    float4x4 VP;
    float4x4 LightVP;

    float4 LightDir;
    float4 LightCol;
    float4 LightAmb;

    float4 CameraPos;

    float4 FogColor;
    float4 FogInfo;

    float4 CascadeSplits;
    float4 CascadeBlendStarts;

    float4 PBRDebugParam;
    float4 LocalLightParam;

    float4x4 SpotLightViewProjection[MAX_SPOT_SHADOW_LIGHTS]; // Shadow付きSpot LightのViewProjection行列
    float4 SpotShadowParam[MAX_SPOT_SHADOW_LIGHTS]; // Spot Shadow設定
    
    //========================================================
	// Point Light Shadow ViewProjection
	// Point Light 0: Index 0～5
	// Point Light 1: Index 6～11
	// 行列Index:pointShadowIndex * 6 + faceIndex
	//========================================================
    float4x4 PointLightViewProjection[POINT_SHADOW_TOTAL_SLICES];
    
    //========================================================
	// Point Shadow設定
	// x:Shadow有効フラグ
	// y:PCF Radius
	// z:Minimum Bias
	// w:Slope Bias
	//========================================================
    float4 PointShadowParam[MAX_POINT_SHADOW_LIGHTS];
    float4 EnvironmentSceneParam;
}

//============================================================
// マテリアル定数バッファ
//============================================================
cbuffer MaterialBuffer : register(b1)
{
    float4 Diffuse;
    float4 Ambient;
    float4 Specular;

    float4 TilingOffset;
    float4 ShadowParam;
    float4 NormalParam;

    float4 PBRParam;
    float4 EmissiveParam;
    float4 ORMParam;

    float4 EnvironmentParam;
    
    float4 SSSParam;
    float4 SSSColor;
    float4 SSSMapParam;
    
    float4 SSSDiffusionParam;
}


//============================================================
// GPU側Local Light構造体
// C++側のLocalLightGPUと完全に一致させる
//============================================================
struct LocalLightData
{
	// xyz : Position
	// w   : Range
    float4 PositionRange;

	// rgb : Color
	// w   : Intensity
    float4 ColorIntensity;

	// xyz : Direction
	// w   : Spot Outer Cos
    float4 DirectionOuterCos;

	// x : Light Type
	// y : Spot Inner Cos
	// z : Enable
	// w : Shadow Enable
    float4 Parameters;
    
    // xyz : 部屋のワールド座標の最小値
    // w   : 1なら部屋境界を使用 0なら距離や角度で減衰
    float4 RoomMin;

    // xyz : 部屋のワールド座標の最大値
    float4 RoomMax;
};

//============================================================
// Material Textures
//============================================================
Texture2D txDiffuse : register(t0);

//============================================================
// Static Cascade Shadow
//============================================================
Texture2D txStaticShadow0 : register(t1);
Texture2D txStaticShadow1 : register(t2);
Texture2D txStaticShadow2 : register(t3);

//============================================================
// Dynamic Cascade Shadow
//============================================================
Texture2D txDynamicShadow0 : register(t4);
Texture2D txDynamicShadow1 : register(t5);
Texture2D txDynamicShadow2 : register(t6);

//============================================================
// Material Maps
//============================================================
Texture2D txNormal : register(t7);
Texture2D txORM : register(t8);

//============================================================
// IBL
//============================================================
TextureCube txEnvironment : register(t9);
TextureCube txPrefilteredEnvironment : register(t10);
TextureCube txIrradianceEnvironment : register(t11);
Texture2D txBRDFLUT : register(t12);

//============================================================
// Local Lights
//============================================================
StructuredBuffer<LocalLightData> txLocalLights : register(t13);

//============================================================
// t14 : Spot Light Shadow Texture2DArray
//============================================================
Texture2DArray txSpotLightShadow : register(t14);

//============================================================
// t15 : Point Light Shadow Texture2DArray
// 1灯につき6Sliceを使用する
//============================================================
Texture2DArray txPointLightShadow : register(t15);

//============================================================
// t16 : SSS Thickness Map
// R Channelを使用する
// Engine Convention:
// 0.0 = Thick 光を通しにくい
// 1.0 = Thin 光を通しやすい
//============================================================
Texture2D txThickness : register(t16);

//============================================================
// t17 : Emissive Texture
//============================================================
Texture2D txEmissive : register(t17);

//============================================================
// t18 : Skin Mask
//============================================================
Texture2D txSkinMask : register(t18);

//============================================================
// Samplers
//============================================================
SamplerState samLinear : register(s0);

SamplerComparisonState samShadow : register(s1);

#endif