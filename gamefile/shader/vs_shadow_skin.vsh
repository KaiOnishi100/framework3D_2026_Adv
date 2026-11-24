//============================================================
// スキニングモデル用ShadowMap VertexShader
//
// 役割:
//   1. ボーン行列によって頂点をアニメーションさせる
//   2. ライト視点のクリップ座標へ変換する
//   3. PixelShaderは使用せず、Depthだけを書き込む
//============================================================

#define BONE_MAX 512

//============================================================
// スキニングモデルの頂点入力
// C++側のinputElementDescs_Skinと同じSemanticにする
//============================================================
struct vertexIn
{
    float4 pos : POSITION0;
    float4 nor : NORMAL0;
    float4 col : COLOR0;
    float2 tx0 : TEXCOORD0;
    float4 tan : TANGENT0;
    uint4  idx : SKIN_INDEX;
    float4 weight : SKIN_WEIGHT;
};

//============================================================
// ShadowMap用オブジェクト定数
// b0は通常Shadow Shaderと同じ構成にする
//============================================================
cbuffer ShadowObjectCB : register(b0)
{
	// 通常モデル用VS_Shadowでは使用する
	// Skin用ではボーン行列がワールド座標まで含んでいるため、
	// 現在のフレームワークでは基本的に使用しない
    float4x4 world;

	// ライトのView * Projection
    float4x4 lightViewProj;
};

//============================================================
// ボーン行列
// 通常描画のVS_3D_Skinと同じb3を使用する
//============================================================
cbuffer BoneBuffer : register(b3)
{
    float4x4 Bones[BONE_MAX];
};

//============================================================
// メイン処理
//============================================================
float4 main(vertexIn IN) : SV_POSITION
{
	//============================================================
	// 4本のボーン行列をウェイト付きで合成
	// 通常描画のVS_3D_Skinと同じ処理にすることで
	// 通常モデルとShadowMap上の形状を一致させる
	//============================================================
    float4x4 skinMatrix;

    skinMatrix = Bones[IN.idx.x] * IN.weight.x;
    skinMatrix += Bones[IN.idx.y] * IN.weight.y;
    skinMatrix += Bones[IN.idx.z] * IN.weight.z;
    skinMatrix += Bones[IN.idx.w] * IN.weight.w;

	//============================================================
	// ボーン変換後のワールド座標
	//============================================================
    float4 worldPosition = mul(skinMatrix, IN.pos);

	//============================================================
	// ライト視点のクリップ座標へ変換
	//============================================================
    float4 shadowPosition = mul(worldPosition, lightViewProj);

    return shadowPosition;
}