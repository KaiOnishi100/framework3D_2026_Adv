struct vertexIn
{
    float4 pos : POSITION0;
    float4 col : COLOR0;
};
 
struct vertexOut
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
};
 
cbuffer ConstantBuffer : register(b0)
{
	float4x4	VP;		//View*Proj
}
 
vertexOut main(vertexIn IN )
{
    vertexOut OUT = (vertexOut) 0;
    return OUT;
	//頂点座標をビューポート座標に変換
	OUT.pos = mul(IN.pos, VP);

	//頂点カラー
	OUT.col = IN.col;

    return OUT;
}