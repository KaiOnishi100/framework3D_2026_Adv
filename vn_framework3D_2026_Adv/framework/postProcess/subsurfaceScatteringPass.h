#pragma once

//============================================================
// Screen Space Subsurface Scattering Pass
//============================================================
class SubsurfaceScatteringPass
{
public:

    SubsurfaceScatteringPass() = default;
    ~SubsurfaceScatteringPass() = default;


    //========================================================
    // 初期化
    //========================================================
    bool Initialize(UINT width, UINT height);


    //========================================================
    // 終了
    //========================================================
    void Terminate();


    //========================================================
    // SSS Mask生成
    // Main SceneのDepthを利用して
    // 現在画面に見えているSSS Materialだけを描画する
    //========================================================
    void GenerateMask();

    //============================================================
    // Screen Space Diffusion適用
    //============================================================
    void ApplyToRT(
        ID3D12Resource* inputSceneResource,
        ID3D12Resource* motionVectorResource,
        ID3D12Resource* depthResource,
        D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
        UINT width,
        UINT height
    );

    ID3D12Resource* ResolveTemporal(
        ID3D12Resource* currentBlurredResource,
        ID3D12Resource* motionVectorResource,
        ID3D12Resource* depthResource
    );

    void CommitTemporalHistory();

    //========================================================
    // Debug / 後のBlur Pass用
    //========================================================
    ID3D12Resource* GetMaskResource() const
    {
        return mMaskResource.Get();
    }


    ID3D12DescriptorHeap* GetMaskSRVHeap() const
    {
        return mMaskSRVHeap.Get();
    }


    D3D12_GPU_DESCRIPTOR_HANDLE GetMaskSRV() const
    {
        if (!mMaskSRVHeap)
        {
            return {};
        }

        return mMaskSRVHeap->GetGPUDescriptorHandleForHeapStart();
    }



    //============================================================
    // 外部調整
    //============================================================
    void SetDiffusionStrength(float strength);

    void SetBlurRadius(int radius);

    void SetDepthFalloff(float falloff);

    void SetScatteringProfile(float red, float green, float blue);

    void SetSampleQuality(float quality);


    //============================================================
    // SSS散乱半径をWorld Space単位で設定
    // 0.0:
    //   World-Space Radiusを無効化し、
    //   従来の固定Pixel RadiusへFallbackする
    //============================================================
    void SetWorldSpaceRadius(float radius);

    //============================================================
    // Temporal SSS
    //============================================================
    void SetTemporalEnabled(bool enabled);

    void SetTemporalHistoryWeight(float weight);

    void SetTemporalDepthThreshold(float threshold);

    void SetTemporalMotionResponse(float response);

    void ResetTemporalHistory();

public:

    //============================================================
   // SSS Debug View
   //============================================================
    enum class DebugView
    {
        //========================================================
        // 通常描画
        //========================================================
        None = 0,

        //========================================================
        // Full Resolution SSS Mask
        //========================================================
        Mask,

        //========================================================
        // Full Resolution
        // PBR Diffuse + Transmission SSS
        //========================================================
        DiffusionSource,

        //========================================================
        // Full Resolution Material Profile
        // RGB: Scattering Profile
        // A: World Radius
        //========================================================
        MaterialProfile,

        //========================================================
        // Half Resolution Horizontal Blur
        //========================================================
        HorizontalBlur,

        //========================================================
        // Half Resolution Vertical Blur
        //========================================================
        VerticalBlur,

        //========================================================
        // Half Resolution Temporal Result
        //========================================================
        TemporalResolved
    };

    DebugView GetDebugView() const { return mDebugView; }

    bool IsDebugViewEnabled() const { return mDebugView != DebugView::None; }

    ID3D12DescriptorHeap* GetDebugSRVHeap() const { return mDebugSRVHeap.Get(); }

    void SetDebugView(DebugView view);

    D3D12_GPU_DESCRIPTOR_HANDLE GetDebugSRV() const
    {
        if (!mDebugSRVHeap)
        {
            return {};
        }

        return mDebugSRVHeap->GetGPUDescriptorHandleForHeapStart();
    }

private:
    //============================================================
    // Screen-Space SSS Diffusion用Constant Buffer
    //============================================================
    struct alignas(16) SSSDiffusionBuffer
    {
        //========================================================
        // Depth → World Position復元用
        //========================================================
        XMFLOAT4X4 InverseViewProjection;

        //========================================================
        // xyz : Camera Position
        // w   : unused
        //========================================================
        XMFLOAT4 CameraPosition;

        //========================================================
        // x : Work Width
        // y : Work Height
        // z : 1 / Work Width
        // w : 1 / Work Height
        //========================================================
        XMFLOAT4 ScreenParameters;

        //========================================================
        // Full Resolution
        // Composite / Upsampling用
        // x : Full Width
        // y : Full Height
        // z : 1 / Full Width
        // w : 1 / Full Height
        //========================================================
        XMFLOAT4 FullScreenParameters;

        //========================================================
        // x : Blur Direction X
        // y : Blur Direction Y
        // z : Blur Radius(pixel)
        // w : Depth Falloff
        //========================================================
        XMFLOAT4 BlurParameters;

        //========================================================
        // x : Diffusion Strength
        // y : Mask Threshold
        // z : Sky Depth Threshold
        // w : World-Space Diffusion Radius
        //========================================================
        XMFLOAT4 DiffusionParameters;

        //========================================================
        // RGBごとの散乱半径倍率
        // x : Red
        // y : Green
        // z : Blue
        // w : Blur Sample Quality 0.0 ～ 1.0
        //========================================================
        XMFLOAT4 ScatteringProfile;
    };

    //============================================================
    // SSS Temporal Constant Buffer
    //============================================================
    struct alignas(16) SSSTemporalBuffer
    {
        //========================================================
        // DepthからWorld Positionを復元するために使用
        //========================================================
        XMFLOAT4X4 InverseViewProjection;

        //========================================================
        // xyz : Camera Position
        // w   : unused
        //========================================================
        XMFLOAT4 CameraPosition;

        //========================================================
        // x : Width
        // y : Height
        // z : 1 / Width
        // w : 1 / Height
        //========================================================
        XMFLOAT4 ScreenParameters;

        //========================================================
        // x : History Weight
        // y : Relative Depth Threshold
        // z : Motion Reject Scale
        // w : History Valid
        //========================================================
        XMFLOAT4 TemporalParameters;
    };


private:

    //========================================================
    // Mask Texture
    //========================================================
    bool CreateMaskResource();

    //========================================================
    // Mask描画用PSO
    // Skinなし / Skinありの2種類を作る
    //========================================================
    bool CreateMaskPipelineStates();

    bool CreateMaskPipelineState(bool skinned, ComPtr<ID3D12PipelineState>& outputPSO);

    bool CreateDiffusionResources();

    bool CreateDiffusionSourceResource();

    bool CreateDiffusionProfileResource();

    bool CreateTemporalResources();

    bool CreateDebugResources();

    bool CreateDiffusionRootSignature();

    bool CreateTemporalRootSignature();

    bool CreateDiffusionConstantBuffers();

    bool CreateTemporalConstantBuffer();

    bool CreateDiffusionPipelineStates();

    bool CreateDiffusionPipelineState(
        Shader::ePixelShader pixelShaderType,
        ComPtr<ID3D12PipelineState>& outputPSO
    );

    bool CreateTemporalPipelineState();

    void UpdateDebugSRV();

    void UpdateDiffusionConstantBuffers();

    void UpdateTemporalConstantBuffer();

    //========================================================
    // Resource Barrier
    //========================================================
    void TransitionMask(D3D12_RESOURCE_STATES newState);

    void TransitionBlurResource(UINT index, D3D12_RESOURCE_STATES newState);

    void TransitionDiffusionSource(D3D12_RESOURCE_STATES newState);

    void TransitionDiffusionProfile(D3D12_RESOURCE_STATES newState);

    void TransitionTemporalHistory(UINT index, D3D12_RESOURCE_STATES newState);

private:

    UINT mWidth = 0u;
    UINT mHeight = 0u;
    UINT mWorkWidth = 0u;
    UINT mWorkHeight = 0u;

    UINT mTemporalReadIndex = 0u;
    UINT mTemporalWriteIndex = 1u;

    UINT mSRVDescriptorSize = 0u;


    float mResolutionScale = 0.5f; // 内部解像度Scale

    bool mTemporalHistoryValid = false; // 一度でも有効なHistoryを書いたか

    //========================================================
    // SSS Mask
    // R:
    //   0 = SSSなし
    //   1 = SSS最大
    //========================================================
    ComPtr<ID3D12Resource> mMaskResource;

    //============================================================
    // Diffusion中間Render Target
    // [0] Horizontal Blur
    // [1] Vertical Blur
    //============================================================
    ComPtr<ID3D12Resource> mBlurResources[2];


    //============================================================
    // SSS Diffusion Source
    // RGB: PBR Diffuse + Transmission SSS
    //============================================================
    ComPtr<ID3D12Resource> mDiffusionSourceResource;

    //============================================================
    // Per-Material SSS Diffusion Profile
    // Format: R16G16B16A16_FLOAT
    // RGB: Material Scattering Profile
    // A: Material World-Space Radius
    //============================================================
    ComPtr<ID3D12Resource> mDiffusionProfileResource;

    //============================================================
    // Temporal SSS History
    // RGB: Temporal Resolve済みの Diffuse + Transmission SSS
    // A: そのPixelのCamera Distance
    //============================================================
    ComPtr<ID3D12Resource> mTemporalHistoryResources[2];


    D3D12_RESOURCE_STATES mMaskState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES mDiffusionSourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES mDiffusionProfileState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES mBlurStates[2] =
    {
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    };

    D3D12_RESOURCE_STATES mTemporalHistoryStates[2] =
    {
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    };
    //========================================================
    // RTV Heap
    //========================================================
    ComPtr<ID3D12DescriptorHeap>mMaskRTVHeap;

    //========================================================
    // SRV Heap
    // Screen Root Signatureが現在t0～t1の2Descriptorを
    // 要求するのでDebug用として同じMaskを2個登録する
    //========================================================
    ComPtr<ID3D12DescriptorHeap> mMaskSRVHeap;

    //============================================================
    // Blur RTV Heap
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mBlurRTVHeap;

    //============================================================
    // History RTV Heap
    // [0] History 0
    // [1] History 1
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mTemporalRTVHeap;

    //============================================================
    // Temporal SRV Heap
    // t0 Current Blurred SSS
    // t1 Previous History
    // t2 Motion Vector
    // t3 Main Depth
    // t4 SSS Mask
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mTemporalSRVHeap;

    //============================================================
    // SRV Heap
    // [0～4] Horizontal
    // [5～9] Vertical
    // [10～14] Composite
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mDiffusionSRVHeap;

    //============================================================
    // Debug SRV Heap
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mDebugSRVHeap;



    //============================================================
    // Diffusion PSO
    //============================================================
    ComPtr<ID3D12PipelineState> mDiffusionBlurPSO;

    ComPtr<ID3D12PipelineState> mDiffusionCompositePSO;

    //========================================================
    // Geometry Mask PSO
    //========================================================
    ComPtr<ID3D12PipelineState>mRigidMaskPSO;

    ComPtr<ID3D12PipelineState> mSkinMaskPSO;

    //============================================================
    // Temporal Resolve PSO
    //============================================================
    ComPtr<ID3D12PipelineState> mTemporalPSO;


    //============================================================
    // Diffusion Source RTV
    //
    // Mask RTV Heap:
    //
    // [0] Mask
    // [1] Diffusion Source
    // [2]Material SSS Profile
    //============================================================
    D3D12_CPU_DESCRIPTOR_HANDLE mMaskRTV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE mDiffusionSourceRTV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE mDiffusionProfileRTV = {};

    D3D12_CPU_DESCRIPTOR_HANDLE mBlurRTV[2] = {};

    D3D12_CPU_DESCRIPTOR_HANDLE mTemporalRTV[2] = {};


    //============================================================
    // Diffusion専用Root Signature
    //
    // Root[0] : t0～t24
    // Root[1] : b0
    //============================================================
    ComPtr<ID3D12RootSignature> mDiffusionRootSignature;
    //============================================================
    // Temporal専用Root Signature
    //
    // Root[0] : t0 ～ t4
    // Root[1] : b0
    //============================================================
    ComPtr<ID3D12RootSignature> mTemporalRootSignature;

    //============================================================
    // Horizontal / Vertical用Constant Buffer
    // 同じConstant BufferをDraw間で書き換えるのではなく
    // 2つ別Resourceを持つ
    // ※GPUがHorizontalを実行する前に
    // CPUがVertical値へ上書きしてしまう問題を避ける
    //============================================================
    ComPtr<ID3D12Resource> mDiffusionConstantBuffers[2];

    ComPtr<ID3D12Resource> mTemporalConstantBuffer;

    SSSDiffusionBuffer* mDiffusionMapped[2] =
    {
        nullptr,
        nullptr
    };


    SSSTemporalBuffer* mTemporalMapped = nullptr;

    DebugView mDebugView = DebugView::None;

    //============================================================
    // Diffusion設定
    //============================================================

    // 最大Screen-Space Blur Radius
    int mBlurRadius = 12;

    // SSS Blur全体の合成率
    float mDiffusionStrength = 0.75f;
    // Depth差に対するReject強度
    float mDepthFalloff = 5.0f;

    //============================================================
    // RGB別散乱幅 赤が最も広く 青が最も狭い
    //============================================================
    XMFLOAT3 mScatteringProfile =
        XMFLOAT3(
            1.0f,
            0.55f,
            0.25f
        );


    float mMaskThreshold = 0.001f;

    float mWorldSpaceRadius = 0.01f;// World-Space SSS Diffusion Radius

    //============================================================
    // Temporal ON / OFF
    //============================================================
    bool mTemporalEnabled = true;

    //============================================================
    // 前フレームをどれだけ残すか
    // 0: Currentのみ
    // 1: Historyのみ
    //============================================================
    float mTemporalHistoryWeight = 0.85f;

    //============================================================
    // Depth差Reject
    // Camera Distanceに対する相対差で比較する
    // 0.02: 約2%のDepth差
    //============================================================
    float mTemporalDepthThreshold = 0.02f;

    //============================================================
    // Motionが大きいPixelではHistory Weightを減らす
    //============================================================
    float mTemporalMotionResponse = 40.0f;

    //============================================================
    // SSS Blur Sample Quality
    //============================================================
    float mSampleQuality = 0.65f;

};