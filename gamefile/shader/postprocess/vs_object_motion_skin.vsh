//============================================================
// Skinned Object Motion Vector Vertex Shader
//
// Current Pose:
//   CurrentBones b3
//
// Previous Pose:
//   PreviousBones b4
//
// 同じBind Pose頂点を2回Skinningし、
// Current / Previous Clip Positionを作る
//============================================================


//============================================================
// Skin Vertex
//============================================================
struct VertexInput
{
    float3 position   : POSITION;
    float3 normal     : NORMAL;
    float4 color      : COLOR;
    float2 uv         : TEXCOORD0;
    float4 tangent    : TANGENT;
    uint4  skinIndex  : SKIN_INDEX;
    float4 skinWeight : SKIN_WEIGHT;
};


//============================================================
// Matrix Buffer
//
// C++ stConstantBufferと同じ並び。
//============================================================
cbuffer MatrixBuffer : register(b2)
{
    float4x4 WVP;

    float4x4 World;

    float4x4 WorldView;

    float4x4 WorldInvTranspose;

    //========================================================
    // Skinned ModelではPrevious Camera VPとして使用
    //========================================================
    float4x4 PreviousWVP;
};


//============================================================
// Current Bone
//============================================================
cbuffer BoneBuffer : register(b3)
{
    float4x4 Bones[256];
};


//============================================================
// Previous Bone
//============================================================
cbuffer PreviousBoneBuffer : register(b4)
{
    float4x4 PreviousBones[256];
};


struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 currentClip : TEXCOORD0;
    float4 previousClip : TEXCOORD1;
};


//============================================================
// Skinning Matrix生成
//============================================================
float4x4 CalculateSkinMatrix( uint4 indices, float4 weights)
{
    //========================================================
    // Current Frame
    //========================================================
    return
        Bones[indices.x] * weights.x +
        Bones[indices.y] * weights.y +
        Bones[indices.z] * weights.z +
        Bones[indices.w] * weights.w;
}


//============================================================
// Previous Skinning Matrix
//============================================================
float4x4 CalculatePreviousSkinMatrix(uint4 indices,float4 weights)
{
    return
        PreviousBones[indices.x] * weights.x +
        PreviousBones[indices.y] * weights.y +
        PreviousBones[indices.z] * weights.z +
        PreviousBones[indices.w] * weights.w;
}


//============================================================
// Main
//============================================================
VertexOutput main(VertexInput input)
{
    VertexOutput output;

    const float4 localPosition = float4(input.position, 1.0f);

    //========================================================
    // Current Pose
    //========================================================
    const float4x4 currentSkin =
        CalculateSkinMatrix(input.skinIndex, input.skinWeight);

    const float4 currentPosition = mul(localPosition, currentSkin);

    //========================================================
    // Previous Pose
    //========================================================
    const float4x4 previousSkin =
        CalculatePreviousSkinMatrix(input.skinIndex, input.skinWeight);

    const float4 previousPosition = mul(localPosition, previousSkin);

    //========================================================
    // Current Skin Position
    //        ↓
    // Current Camera VP
    // Skinned Modelでは現在のvnModel実装に合わせて
    // WVP = Current Camera VP
    //========================================================
    output.currentClip = mul(currentPosition, WVP);

    //========================================================
    // Previous Skin Position
    //        ↓
    // Previous Camera VP
    //========================================================
    output.previousClip = mul(previousPosition, PreviousWVP);

    //========================================================
    // Current位置でRasterize
    //========================================================
    output.position = output.currentClip;

    return output;
}