//============================================================
// Skybox Vertex Shader
// 入力:
//   立方体のローカル座標
// 出力:
//   クリップ座標
//   CubeMapを参照する方向ベクトル
//============================================================

struct VertexInput
{
    float3 position : POSITION;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
};

//============================================================
// Skybox用定数バッファ
//
// ViewProjection:
//   カメラView行列から平行移動を除去した行列
//   × Projection行列
//============================================================
cbuffer SkyboxBuffer : register(b0)
{
    float4x4 ViewProjection;
    float4 EnvironmentSceneParam;
}

VertexOutput main(VertexInput IN)
{
    VertexOutput OUT = (VertexOutput) 0;

    //========================================================
    // 立方体をクリップ空間へ変換
    //========================================================
    float4 clipPosition = mul(float4(IN.position, 1.0f), ViewProjection);

    //========================================================
    // Skyboxを常にFar Planeへ配置
    // 透視除算後: z / w = w / w = 1.0
    // となるため、通常の3Dオブジェクトより後方へ描画される
    //========================================================
    clipPosition.z = clipPosition.w;

    OUT.position = clipPosition;

    //========================================================
    // 立方体の頂点方向をそのままCubeMap参照方向として使用
    //========================================================
    OUT.direction = IN.position;

    return OUT;
}