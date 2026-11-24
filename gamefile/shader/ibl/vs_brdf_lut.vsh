//============================================================
// BRDF LUT生成用Vertex Shader
// 頂点バッファを使用せず、
// SV_VertexIDからフルスクリーントライアングルを生成する
//============================================================

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

//============================================================
// Main
//============================================================
VertexOutput main(uint vertexId : SV_VertexID)
{
    VertexOutput OUT = (VertexOutput) 0;

	//========================================================
	// Fullscreen Triangle
	// vertexId = 0 : (-1, -1)
	// vertexId = 1 : (-1,  3)
	// vertexId = 2 : ( 3, -1)
	// 画面全体を1枚の三角形で覆う
	//========================================================
    const float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
		float2(-1.0f, 3.0f),
		float2(3.0f, -1.0f)
    };

    const float2 textureCoordinates[3] =
    {
        float2(0.0f, 1.0f),
		float2(0.0f, -1.0f),
		float2(2.0f, 1.0f)
    };

    OUT.position = float4(positions[vertexId], 0.0f, 1.0f);

    OUT.uv = textureCoordinates[vertexId];

    return OUT;
}