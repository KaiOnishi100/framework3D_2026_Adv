//============================================================
// Cascaded Shadow Map数
//============================================================
#define SHADOW_CASCADE_COUNT 3

struct vertexIn
{
	float4 pos : POSITION0;
	float4 nor : NORMAL0;
	float4 col : COLOR0;
	float2 tx0 : TEXCOORD0;
    float4 tan : TANGENT0;
};

struct vertexOut
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 tx0 : TEXCOORD0;
	float3 nor : TEXCOORD1;
    float4 eye : TEXCOORD2; //xyz:視線ベクトル(非正規化), w:線形深度
	
	//cascadeごとのクリップライト視点クリップ作業
	float4 sdw0 : TEXCOORD3; //shadowMap用
    float4 sdw1 : TEXCOORD4;
    float4 sdw2 : TEXCOORD5;
	
	// xyz : ワールド空間Tangent
    // w   : Bitangent復元用符号
    float4 tan  : TEXCOORD6;
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
	
    float4		FogColor;	//フォグの色
    float4		FogInfo;	//フォグ情報
	
    float4		CascadeSplits; //Cascadeの分割終了距離
    float4		CascadeBlendStarts; // Cascadeブレンド開始距離
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
	float4x4	WVP;		// World*View*Proj
	float4x4	World;		// ワールド行列
    float4x4	WV;			// World*View
    float4x4	WorldInvTranspose; // Normal変換用逆転置行列
    float4x4    PreviousWVP; // Previous Frame
	//========================================================
    // Cascade 0～2のライト空間変換行列
    //========================================================
    float4x4	LightWVP[SHADOW_CASCADE_COUNT]; //lightからみたWVP
    float4x4    Bones[256];
}

vertexOut main(vertexIn IN)
{
	
    vertexOut OUT = (vertexOut) 0;
	//頂点座標をビューポート座標に変換
    OUT.pos = mul(IN.pos, WVP);

	//ワールド座標
    float4 wpos = mul(IN.pos, World);

	//法線ベクトルをワールド行列で変換
    OUT.nor = mul(float4(IN.nor.xyz, 0), World).xyz;

	//頂点カラー
    OUT.col = IN.col;

	//テクスチャ座標
    OUT.tx0 = IN.tx0 * TilingOffset.xy + TilingOffset.zw;;

	//視線ベクトル
    OUT.eye.xyz = CameraPos.xyz - wpos.xyz;

	//フォグ
    float4 v = mul(IN.pos, WV);
    OUT.eye.w = v.z;
	
    //============================================================
    // ローカル法線をワールド空間へ変換
    // w=0なので平行移動の影響を受けない
    //============================================================
    float3 worldNormal = mul(float4(IN.nor.xyz, 0.0f), WorldInvTranspose).xyz;

    //============================================================
    // 補間前に正規化
    //============================================================
    worldNormal = normalize(worldNormal);

    OUT.nor = worldNormal;
    
    //============================================================
    // Tangentをワールド空間へ変換
    // Tangentは面に沿った方向なのでWorldで変換する
    //============================================================
    float3 worldTangent = mul(float4(IN.tan.xyz, 0.0f), World).xyz;

    //============================================================
    // 非均一Scaleや補間誤差によって
    // TangentがNormalと直交しなくなる可能性がある
    // Gram-Schmidt法でNormal方向成分を除去する
    //============================================================
    worldTangent = worldTangent - worldNormal * dot(worldNormal, worldTangent);

    //============================================================
    // 正規化
    //============================================================
    worldTangent = normalize(worldTangent);

    //============================================================
    // Tangent.xyzとHandedness.wをPixel Shaderへ渡す
    //============================================================
    OUT.tan = float4(worldTangent, IN.tan.w);
    
    // ShadowMapのUVと比較用Depthに変換する
    OUT.sdw0 = mul(wpos, LightWVP[0]);
    OUT.sdw1 = mul(wpos, LightWVP[1]);
    OUT.sdw2 = mul(wpos, LightWVP[2]);

    return OUT;
}