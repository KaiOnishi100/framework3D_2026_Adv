#ifndef PIXEL_CONTEXT_HLSLI
#define PIXEL_CONTEXT_HLSLI

#include "ps_3d_bindings.hlsli"
//============================================================
// Pixel位置と方向に関する情報
// Materialとは無関係なCamera・Light・座標情報をまとめる
//============================================================
struct PixelContext
{
	//========================================================
	// 現在Pixelのワールド座標
	//========================================================
    float3 worldPosition;

	//========================================================
	// PixelからCameraへ向かう方向
	//========================================================
    float3 viewDirection;

	//========================================================
	// PixelからDirectional Lightへ向かう方向
	//========================================================
    float3 directionalLightDirection;

	//========================================================
	// Camera View空間での前方距離
	// Cascade ShadowとFogで使用する
	//========================================================
    float viewDepth;
};

//============================================================
// PixelContext構築
//============================================================
PixelContext BuildPixelContext(pixelIn IN)
{
    PixelContext context = (PixelContext) 0;

	//============================================================
	// Vertex Shaderから渡されたeye.xyz
	// eye.xyz: CameraPos - WorldPosition
	//============================================================
    context.viewDirection = normalize(IN.eye.xyz);

	//============================================================
	// World座標を逆算
	// IN.eye.xyz = CameraPos.xyz - WorldPosition
	// よって:
	//   WorldPosition = CameraPos.xyz - IN.eye.xyz
	//============================================================
    context.worldPosition = CameraPos.xyz - IN.eye.xyz;

	//============================================================
	// Directional Light方向
	//============================================================
    context.directionalLightDirection = normalize(LightDir.xyz);

	//============================================================
	// View空間深度
	// 負値を使用しないように0以上へ制限する
	//============================================================
    context.viewDepth = max(IN.eye.w, 0.0f);

    return context;
}

#endif