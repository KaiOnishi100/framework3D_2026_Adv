#pragma once


//============================================================
// GI Surface Pass
// SSGIで必要になるSurface情報を生成する
// RGB: World-Space Normal
// A: Roughness
// Main Depthは既存のDepth Bufferをそのまま利用するため
// このPassでは新しいDepth Textureは作成しない
//============================================================
class GISurfacePass
{
public:

    GISurfacePass() = default;

    ~GISurfacePass() = default;


    //============================================================
    // 初期化
    //============================================================
    bool Initialize(UINT width, UINT height);


    //============================================================
    // 終了
    //============================================================
    void Terminate();


    //============================================================
    // GI Surface Buffer生成
    // Main SceneのDepthを利用し最前面Geometryだけを書き込む
    //============================================================
    void Generate();


    //============================================================
    // Resource取得
    //============================================================

    // Normal + Roughness
    ID3D12Resource* GetSurfaceResource() const { return mSurfaceResource.Get(); }
    // BaseColor + Metallic
    ID3D12Resource* GetMaterialResource() const { return mMaterialResource.Get(); }

    //============================================================
    // SRV Heap取得
    //============================================================
    ID3D12DescriptorHeap* GetSRVHeap() const { return mSRVHeap.Get(); }


    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPU() const
    {
        if (!mSRVHeap)
        {
            return {};
        }


        return
            mSRVHeap->GetGPUDescriptorHandleForHeapStart();
    }


private:

    //============================================================
    // Resource生成
    //============================================================
    bool CreateResource();


    //============================================================
    // Pipeline State
    //
    // Rigid / Skinの2種類
    //============================================================
    bool CreatePipelineStates();


    bool CreatePipelineState(bool skinned, ComPtr<ID3D12PipelineState>& outputPSO);


    //============================================================
    // Resource Barrier
    //============================================================
    void Transition(D3D12_RESOURCE_STATES newState);

    void TransitionMaterial(D3D12_RESOURCE_STATES newState);

private:

    UINT mWidth = 0u;
    UINT mHeight = 0u;


    //============================================================
    // GI Surface Texture
    // RGB: World-Space Shading Normal
    // A: Roughness
    //============================================================
    ComPtr<ID3D12Resource> mSurfaceResource;

    //============================================================
    // GI Surface Material
    // RGB: Linear BaseColor
    // A: Metallic
    // SSGIのReceiver側Diffuse Reflectanceを求めるために使用する
    //============================================================
    ComPtr<ID3D12Resource> mMaterialResource;

    D3D12_RESOURCE_STATES mSurfaceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES mMaterialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    //============================================================
    // RTV
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mRTVHeap;


    D3D12_CPU_DESCRIPTOR_HANDLE mSurfaceRTV = {}; //Normal + Roughness
    D3D12_CPU_DESCRIPTOR_HANDLE mMaterialRTV = {}; //BaseColor + Metallic

    //============================================================
    // SRV
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mSRVHeap;


    //============================================================
    // Geometry PSO
    //============================================================
    ComPtr<ID3D12PipelineState> mRigidPSO;

    ComPtr<ID3D12PipelineState> mSkinPSO;
};