//============================================================
// Rigid Object Motion Vector Vertex Shader
// 現在Frame:
//   Local Position
//      ↓
//   Current WVP
//      ↓
//   Current Clip Position
// 前Frame:
//   同じLocal Position
//      ↓
//   Previous WVP
//      ↓
//   Previous Clip Position
// この2つをPixel Shaderへ渡す
//============================================================


struct VertexInput
{
    float3 position : POSITION;
};


//============================================================
// Matrix Buffer
// C++側 stConstantBuffer と並びを一致させること
//============================================================
cbuffer MatrixBuffer : register(b2)
{
    float4x4 WVP;

    float4x4 World;

    float4x4 WorldView;

    float4x4 WorldInvTranspose;

    //========================================================
    // 前フレームの
    // PreviousWorld * PreviousViewProjection
    //========================================================
    float4x4 PreviousWVP;
};


struct VertexOutput
{
    //========================================================
    // RasterizeにはCurrent位置を使用
    //========================================================
    float4 position : SV_POSITION;


    //========================================================
    // Current Clip Position
    // SV_POSITIONはPixel Shaderへ入った時点では
    // Screen座標になっているため
    // Clip Positionを別途保持する
    //========================================================
    float4 currentClip : TEXCOORD0;


    //========================================================
    // Previous Clip Position
    //========================================================
    float4 previousClip : TEXCOORD1;
};


VertexOutput main( VertexInput input)
{
    VertexOutput output;


    const float4 localPosition = float4(input.position, 1.0f);


    //========================================================
    // Current Frame
    //========================================================
    output.currentClip = mul(localPosition, WVP);


    //========================================================
    // Previous Frame
    //========================================================
    output.previousClip = mul(localPosition, PreviousWVP);


    //========================================================
    // 現在位置でRasterize
    //========================================================
    output.position = output.currentClip;


    return output;
}