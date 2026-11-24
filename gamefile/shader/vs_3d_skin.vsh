//============================================================
// Cascaded Shadow Map数
//============================================================
#define SHADOW_CASCADE_COUNT 3

#define BONE_MAX	(512)

struct vertexIn
{
	float4 pos : POSITION0;
	float4 nor : NORMAL0;
	float4 col : COLOR0;
	float2 tx0 : TEXCOORD0;
    float4 tan : TANGENT0;
	uint4  idx : SKIN_INDEX;
	float4 wet : SKIN_WEIGHT;
};

struct vertexOut
{
	float4 pos  : SV_POSITION;
	float4 col  : COLOR0;
	float2 tx0  : TEXCOORD0;
	float3 nor  : TEXCOORD1;
	float4 eye  : TEXCOORD2; //xyz:視線ベクトル(非正規化), w:線形深度
    float4 sdw0 : TEXCOORD3; //shadowMap用
    float4 sdw1 : TEXCOORD4;
    float4 sdw2 : TEXCOORD5;
    float4 tan  : TEXCOORD6; // 法線から作ったFallback Tangentを渡す
};

//シーン(カメラ & ライト)
cbuffer SceneBuffer : register(b0)
{
	float4x4	VP;			//View * Proj
	float4x4	LightVP;	//シャドウマップ用
	float4		LightDir;	//平行光源の(逆)方向
	float4		LightCol;	//平行光源の色
	float4		LightAmb;	//環境光の色
	float4		CameraPos;	//カメラのワールド座標
    float4		FogColor; //フォグの色
    float4		FogInfo; //フォグ情報
    float4		CascadeSplits; //Cascadeの分割終了距離
}

//マテリアル
cbuffer MaterialBuffer : register(b1)
{
    float4 Diffuse;		//マテリアルの拡散色
    float4 Ambient;		//マテリアルの環境色
    float4 Specular;	//マテリアルの鏡面反射光
    float4 TilingOffset;
}

//マトリクス
cbuffer ConstantBuffer : register(b2)
{
	float4x4	WVP;		//World*View*Proj
	float4x4	World;		//ワールド行列
    float4x4	WV;			//World*View
    float4x4	LightWVP[SHADOW_CASCADE_COUNT];
}

cbuffer BoneBuffer : register(b3)
{
	float4x4 Bones[BONE_MAX];
}

vertexOut main(vertexIn IN)
{
	vertexOut OUT = (vertexOut)0;

	//スキニング変換(Worldマトリクスは単位行列としておく)
	float4x4 mtx;
	mtx  = Bones[IN.idx.x] * IN.wet.x;
	mtx += Bones[IN.idx.y] * IN.wet.y;
	mtx += Bones[IN.idx.z] * IN.wet.z;
	mtx += Bones[IN.idx.w] * IN.wet.w;
	float4 wpos = mul(mtx, IN.pos);

	//頂点座標をビューポート座標に変換
	OUT.pos = mul(wpos, WVP);

    //============================================================
    // Skinning Normal
    //============================================================
    float3 skinnedNormal = mul(float4(IN.nor.xyz, 0.0f), transpose(mtx)).xyz;

    skinnedNormal = normalize(skinnedNormal);

    //============================================================
    // Skinning Tangent
    //============================================================
    float3 skinnedTangent = mul(float4(IN.tan.xyz, 0.0f), transpose(mtx)).xyz;

    //============================================================
    // TangentをNormalへ再直交化
    //============================================================
    skinnedTangent = skinnedTangent - skinnedNormal * dot(skinnedNormal, skinnedTangent);


    skinnedTangent = normalize(skinnedTangent);


    OUT.nor = skinnedNormal;

    OUT.tan = float4(skinnedTangent, IN.tan.w);
    
	//頂点カラー
	OUT.col = IN.col;

	//テクスチャ座標
	OUT.tx0 = IN.tx0 * TilingOffset.xy + TilingOffset.zw;

	//視線ベクトル
	OUT.eye.xyz = CameraPos.xyz - wpos.xyz;
	
	//フォグ
    float4 v = mul(wpos, WV);
    OUT.eye.w = v.z;
	
    OUT.sdw0 = mul(wpos, LightWVP[0]);
    OUT.sdw1 = mul(wpos, LightWVP[1]);
    OUT.sdw2 = mul(wpos, LightWVP[2]);
	
	return OUT;
}