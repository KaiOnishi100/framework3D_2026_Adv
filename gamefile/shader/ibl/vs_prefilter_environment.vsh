//============================================================
// Prefiltered Environment Map生成用Vertex Shader
// 立方体を6方向のカメラから描画し
// 各CubeMap Faceへ書き込む
//============================================================

struct VertexInput
{
    float3 position : POSITION;
};

struct VertexOutput
{
    float4 position : SV_POSITION;

	//========================================================
	// CubeMap上の方向
	// この方向を基準としてGGX Importance Samplingを行う
	//========================================================
    float3 direction : TEXCOORD0;
};

//============================================================
// CubeMap Face用ViewProjection
// Root ConstantsとしてC++側から設定する
//============================================================
cbuffer CaptureBuffer : register(b0)
{
    float4x4 CaptureViewProjection;
}

VertexOutput main(VertexInput IN)
{
    VertexOutput OUT =(VertexOutput) 0;

	//========================================================
	// 対象Faceのクリップ座標へ変換
	//========================================================
    OUT.position = mul(float4(IN.position, 1.0f), CaptureViewProjection);

	//========================================================
	// 立方体のローカル方向をそのまま出力
	//========================================================
    OUT.direction = IN.position;

    return OUT;
}