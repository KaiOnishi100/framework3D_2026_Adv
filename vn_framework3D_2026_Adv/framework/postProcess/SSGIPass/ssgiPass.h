#pragma once
//============================================================
// Screen-Space Global Illumination Pass
// ・GI Surface Buffer生成
// ・Screen-Space Ray March
// ・Raw Indirect Lighting生成
//============================================================
class SSGIPass
{
public:

    SSGIPass() = default;
    ~SSGIPass() = default;


    //============================================================
    // 初期化
    //============================================================
    bool Initialize(
        UINT width,
        UINT height
    );


    //============================================================
    // 終了
    //============================================================
    void Terminate();


    //============================================================
    // Raw SSGI生成
    // inputSceneResource: GIを取得する元となるHDR Scene
    // depthResource: Main Depth
    //============================================================
    void Generate(
        ID3D12Resource* inputSceneResource,
        ID3D12Resource* depthResource,
        ID3D12Resource* motionVectorResource,
        const XMMATRIX& previousViewProjection
    );

    //============================================================
    // SSGI Composite
    // sceneResource: GIを加算する元のHDR Scene
    // outputRTV: Composite結果を書き込むRenderTarget
    // 最終式: Output = Scene + IndirectDiffuse
    //============================================================
    void ApplyCompositeToRT(
        ID3D12Resource* sceneResource,
        D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
        UINT width,
        UINT height
    );


    void ApplySpatialFilter(ID3D12Resource* depthResource);

    ID3D12Resource* ResolveTemporal(
        ID3D12Resource* motionVectorResource,
        ID3D12Resource* depthResource,
        const XMMATRIX& previousViewProjection
    );

    void ApplyUpsampling(
        ID3D12Resource* sourceGI,
        ID3D12Resource* depthResource
    );

    UINT GetWorkWidth() const
    {
        return mWorkWidth;
    }


    UINT GetWorkHeight() const
    {
        return mWorkHeight;
    }

    ID3D12Resource* GetHalfResolutionResultResource() const;

    //============================================================
    // Raw GI Resource取得
    //============================================================
    ID3D12Resource* GetResultResource() const
    {
        if (mUpsampleResource)
        {
            return mUpsampleResource.Get();
        }

        //========================================================
        // Fallback
        //========================================================
        return GetHalfResolutionResultResource();
    }

    ID3D12Resource* GetRawResultResource() const
    {
        return mResultResource.Get();
    }

    ID3D12DescriptorHeap* GetResultSRVHeap() const
    {
        //============================================================
        // Full Resolution Result
        //============================================================
        if (mUpsampleResultSRVHeap)
        {
            return mUpsampleResultSRVHeap.Get();
        }
        return mResultSRVHeap.Get();
    }


    D3D12_GPU_DESCRIPTOR_HANDLE GetResultSRVGPU() const
    {
        if (mUpsampleResultSRVHeap)
        {
            return mUpsampleResultSRVHeap->GetGPUDescriptorHandleForHeapStart();
        }

        if (mResultSRVHeap)
        {
            return mResultSRVHeap->GetGPUDescriptorHandleForHeapStart();
        }
        return {};
    }

    ////============================================================
    //// Debug表示用
    ////============================================================
    ID3D12DescriptorHeap* GetDebugSRVHeap() const { return mDebugSRVHeap.Get(); }


    D3D12_GPU_DESCRIPTOR_HANDLE GetDebugSRV() const
    {
        if (!mDebugSRVHeap)
        {
            return {};
        }

        return mDebugSRVHeap->GetGPUDescriptorHandleForHeapStart();
    }

    //============================================================
    // 設定
    //============================================================
    void SetRayCount(int count);

    void SetStepCount(int count);

    void SetMaxDistance(float distance);

    void SetThickness(float thickness);

    void SetNormalBias(float bias);

    void SetIntensity(float intensity);

    void SetMaxRadiance(float value);

    //============================================================
    // SSGI Spatial Filter
    //============================================================
    void SetFilterRadius(int radius);

    void SetFilterDepthFalloff(float falloff);

    void SetFilterNormalPower(float power);

    void SetFilterConfidenceMin(float value);

    //============================================================
    // Temporal SSGI
    //============================================================
    void SetTemporalEnabled(bool enabled);

    void SetTemporalHistoryWeight(float weight);

    void SetTemporalWorldPositionThreshold(float threshold);

    void SetTemporalNormalThreshold(float threshold);

    void SetTemporalMotionResponse(float response);

    void SetTemporalClampExpand(float expand);

    void ResetTemporalHistory();

    //============================================================
    // SSGI Full Resolution Upsampling
    //============================================================
    void SetUpsampleDepthFalloff(float falloff);

    void SetUpsampleNormalPower(float power);

    void SetUpsampleConfidenceMin(float value);

    //============================================================
    // SSGI Multi Bounce
    // 前フレームのGIをHit Radianceへ少量加えることで、
    // 2回目以降のDiffuse Bounceを近似する
    //============================================================
    void SetMultiBounceEnabled(bool enabled);

    //============================================================
    // Previous GIをどの程度再利用するか
    //============================================================
    void SetMultiBounceStrength(float strength);

public:
   //============================================================
   // SSGI Debug View
   // None: 通常描画
   // Raw: Ray March直後
   // Filtered: Bilateral Filter後
   // Temporal: Temporal Resolve後
   // Upsampled: Full Resolution Upsample後
   // FinalComposite: HDR SceneへCompositeした結果
   // FinalCompositeだけはSSGIPass内部Textureではなく、
   // PostProcessManager側のcurrentResourceを表示する
   //============================================================
   enum class DebugView
   {
       None = 0,
       Raw,
       Filtered,
       Temporal,
       Upsampled,
       FinalComposite
   };

   //============================================================
   // Debug View設定
   //============================================================
   void SetDebugView(DebugView view);

   DebugView GetDebugView() const{return mDebugView;}

   bool IsDebugViewEnabled() const{ return mDebugView != DebugView::None;}

public:

    //============================================================
    // SSGI Quality Preset
    //============================================================
    enum class Quality
    {
        Low = 0,
        Medium,
        High
    };

    void SetQuality(Quality quality);

    Quality GetQuality() const { return mQuality; }

private:

    //============================================================
    // 現在表示するSSGI Debug View
    //============================================================
    DebugView mDebugView = DebugView::None;

    Quality mQuality = Quality::Medium;

    //============================================================
    // Debug SRV Heap
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mDebugSRVHeap;

    //============================================================
    // Debug用Descriptorを更新する
    //============================================================
    void UpdateDebugSRV();

private:

    //============================================================
    // Ray March Constant Buffer
    //============================================================
    struct alignas(16) SSGIRayMarchBuffer
    {
        //========================================================
        // World → Clip
        //========================================================
        XMFLOAT4X4 ViewProjection;

        //========================================================
        // Depth → World
        //========================================================
        XMFLOAT4X4 InverseViewProjection;

        //========================================================
        // xyz:
        //   Camera Position
        //========================================================
        XMFLOAT4 CameraPosition;

        //========================================================
        // SSGI Working Resolution
        //
        // x : Work Width
        // y : Work Height
        // z : 1 / Work Width
        // w : 1 / Work Height
        //========================================================
        XMFLOAT4 ScreenParameters;

        //========================================================
        // Full Resolution
        // x : Full Width
        // y : Full Height
        // z : 1 / Full Width
        // w : 1 / Full Height
        //========================================================
        XMFLOAT4 FullScreenParameters;

        //========================================================
        // x : Max Ray Distance
        // y : Step Count
        // z : Hit Thickness
        // w : Normal Bias
        //========================================================
        XMFLOAT4 RayParameters;

        //========================================================
        // x : Ray Count
        // y : GI Intensity
        // z : Sky Depth Threshold
        // w : Frame Index
        //========================================================
        XMFLOAT4 SamplingParameters;

        //============================================================
        // x : Maximum Sample Radiance
        // yzw : unused
        //============================================================
        XMFLOAT4 RadianceParameters;

        //============================================================
        // Multi Bounce Parameters
        //
        // x : Multi Bounce Enabled
        // y : Strength
        // z : Previous GI History Valid
        // w : unused
        //============================================================
        XMFLOAT4 MultiBounceParameters;
    };

    //============================================================
    // SSGI Bilateral Spatial Filter
    //============================================================
    struct alignas(16) SSGIFilterBuffer
    {
        //========================================================
        // Depth → World Position
        //========================================================
        XMFLOAT4X4 InverseViewProjection;

        //========================================================
        // xyz:
        //   Camera Position
        //========================================================
        XMFLOAT4 CameraPosition;

        //========================================================
        // Working Resolution
        //
        // x : Work Width
        // y : Work Height
        // z : 1 / Work Width
        // w : 1 / Work Height
        //========================================================
        XMFLOAT4 ScreenParameters;

        //========================================================
        // x : Direction X
        // y : Direction Y
        // z : Filter Radius
        // w : Depth Falloff
        //========================================================
        XMFLOAT4 FilterParameters;

        //========================================================
        // x : Normal Power
        // y : Confidence Minimum
        // z : Sky Depth Threshold
        // w : unused
        //========================================================
        XMFLOAT4 EdgeParameters;
    };

    struct alignas(16) SSGITemporalBuffer
    {
        //========================================================
        // Current Depth → Current World Position
        //========================================================
        XMFLOAT4X4 InverseViewProjection;

        //========================================================
        // Previous Depth → Previous World Position
        //========================================================
        XMFLOAT4X4 PreviousInverseViewProjection;

        //========================================================
        // x : Work Width
        // y : Work Height
        // z : 1 / Work Width
        // w : 1 / Work Height
        //========================================================
        XMFLOAT4 ScreenParameters;

        //========================================================
        // x : Full Width
        // y : Full Height
        // z : 1 / Full Width
        // w : 1 / Full Height
        //========================================================
        XMFLOAT4 FullScreenParameters;

        //========================================================
        // x : History Weight
        // y : World Position Threshold
        // z : Normal Threshold
        // w : Motion Response
        //========================================================
        XMFLOAT4 TemporalParameters;

        //========================================================
        // x : History Valid
        // y : Neighborhood Clamp Expand
        // z,w : unused
        //========================================================
        XMFLOAT4 HistoryParameters;
    };

    struct alignas(16) SSGIUpsampleBuffer
    {
        //========================================================
        // Depth → World Position
        //========================================================
        XMFLOAT4X4 InverseViewProjection;

        //========================================================
        // xyz: Camera Position
        //========================================================
        XMFLOAT4 CameraPosition;

        //========================================================
        // Half Resolution
        //
        // x : Work Width
        // y : Work Height
        // z : 1 / Work Width
        // w : 1 / Work Height
        //========================================================
        XMFLOAT4 WorkScreenParameters;

        //========================================================
        // Full Resolution
        //
        // x : Full Width
        // y : Full Height
        // z : 1 / Full Width
        // w : 1 / Full Height
        //========================================================
        XMFLOAT4 FullScreenParameters;

        //========================================================
        // x : Depth Falloff
        // y : Normal Power
        // z : Confidence Minimum
        // w : Sky Depth Threshold
        //========================================================
        XMFLOAT4 UpsampleParameters;
    };



private:

    //============================================================
    // Resource
    //============================================================
    bool CreateResultResource();
    bool CreateRootSignature();
    bool CreateConstantBuffer();
    bool CreatePipelineState();

    //============================================================
    // Filter Resource
    //============================================================
    bool CreateFilterResources();
    bool CreateFilterRootSignature();
    bool CreateFilterConstantBuffers();
    bool CreateFilterPipelineState();

    //============================================================
    // Temporal
    //============================================================
    bool CreateTemporalResources();
    bool CreateTemporalRootSignature();
    bool CreateTemporalConstantBuffer();
    bool CreateTemporalPipelineState();

    //============================================================
    // Full Resolution Upsampling
    //============================================================
    bool CreateUpsampleResources();
    bool CreateUpsampleRootSignature();
    bool CreateUpsampleConstantBuffer();
    bool CreateUpsamplePipelineState();

    //============================================================
    // Composite
    //============================================================
    bool CreateCompositeResources();
    bool CreateCompositeRootSignature();
    bool CreateCompositePipelineState();

    //============================================================
    // Debug
    //============================================================
    bool CreateDebugResources();

    //============================================================
    // CB更新
    //============================================================
    void UpdateConstantBuffer();
    void UpdateFilterConstantBuffers();
    void UpdateTemporalConstantBuffer(const XMMATRIX& previousViewProjection);
    void UpdateUpsampleConstantBuffer();

    //============================================================
    // Barrier
    //============================================================
    void TransitionResult(D3D12_RESOURCE_STATES newState);
    void TransitionFilterResource(UINT index,D3D12_RESOURCE_STATES newState);
    void TransitionTemporalGI(UINT index,D3D12_RESOURCE_STATES newState);
    void TransitionTemporalGeometry(UINT index,D3D12_RESOURCE_STATES newState);
    void TransitionUpsample(D3D12_RESOURCE_STATES newState);


    void CommitTemporalHistory();

private:

    UINT mWidth = 0u;
    UINT mHeight = 0u;

    //============================================================
    // SSGI内部処理解像度
    //============================================================
    UINT mWorkWidth = 0u;
    UINT mWorkHeight = 0u;

    //============================================================
    // History Ping-Pong
    //============================================================
    UINT mTemporalReadIndex = 0u;
    UINT mTemporalWriteIndex = 1u;

    UINT mFrameIndex = 0u;// Frame番号

    UINT mSRVDescriptorSize = 0u;

    bool mTemporalHistoryValid = false;
    bool mTemporalEnabled = true; // Temporal ON/OFF

    //============================================================
    // GI Geometry Surface Buffer
    //============================================================
    GISurfacePass mSurfacePass;


    //============================================================
    // Raw SSGI Result
    // RGB: Gathered Indirect Radiance
    // A: Ray Hit Confidence
    //============================================================
    ComPtr<ID3D12Resource> mResultResource;

   //============================================================
   // SSGI Spatial Filter中間Texture
   // [0] Horizontal Filter結果
   // [1] Vertical Filter結果 = 最終Filtered SSGI
   // どちらもHalf Resolution
   //============================================================
    ComPtr<ID3D12Resource> mFilterResources[2];

    //============================================================
    // SSGI Temporal History
    // GI History:
    //   RGB = Temporalで安定化したIndirect Diffuse
    //   A   = Confidence
    // Geometry History:
    //   RGB = Previous World Normal
    //   A   = Previous Device Depth
    // Ping-Pongで管理する
    //============================================================
    ComPtr<ID3D12Resource> mTemporalGIResources[2];

    ComPtr<ID3D12Resource> mTemporalGeometryResources[2];

    //============================================================
    // Full Resolution SSGI
    // Half ResolutionでTemporalまで処理したSSGIを、
    // Depth / Normal-aware Upsamplingによって
    // Full Resolutionへ戻した結果
    // RGB: Indirect Diffuse
    // A: Confidence
    //============================================================
    ComPtr<ID3D12Resource> mUpsampleResource;

    D3D12_RESOURCE_STATES mResultState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES mFilterStates[2] =
    {
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    };

    D3D12_RESOURCE_STATES mTemporalGIStates[2] =
    {
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    };

    D3D12_RESOURCE_STATES mTemporalGeometryStates[2] =
    {
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    };

    D3D12_RESOURCE_STATES mUpsampleState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    //============================================================
    // Result RTV
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mResultRTVHeap;

    D3D12_CPU_DESCRIPTOR_HANDLE mResultRTV = {};

    //============================================================
    // Filter RTV Heap
    //
    // [0] Horizontal
    // [1] Vertical
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mFilterRTVHeap;

    D3D12_CPU_DESCRIPTOR_HANDLE mFilterRTV[2] = { {},{} };

    //============================================================
    // Temporal RTV Heap
    //
    // [0] GI History 0
    // [1] Geometry History 0
    // [2] GI History 1
    // [3] Geometry History 1
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mTemporalRTVHeap;

    D3D12_CPU_DESCRIPTOR_HANDLE mTemporalGIRTV[2] = { {},{} };;

    D3D12_CPU_DESCRIPTOR_HANDLE mTemporalGeometryRTV[2] = { {},{} };

    //============================================================
    // Upsample RTV
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mUpsampleRTVHeap;

    D3D12_CPU_DESCRIPTOR_HANDLE mUpsampleRTV = {};

    //============================================================
    // Ray March入力
    //
    // t0 : Scene HDR
    // t1 : Main Depth
    // t2 : GI Surface Buffer
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mRayMarchSRVHeap;


    //============================================================
    // Debug表示用
    //
    // [0] Raw GI
    // [1] Dummy Raw GI
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mResultSRVHeap;


    //============================================================
    // Filter SRV Heap
    //
    // Horizontal:
    //   [0] t0 Raw GI
    //   [1] t1 Depth
    //   [2] t2 Normal + Roughness
    //
    // Vertical:
    //   [3] t0 Horizontal Result
    //   [4] t1 Depth
    //   [5] t2 Normal + Roughness
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mFilterSRVHeap;


    //============================================================
    // Temporal Input
    //
    // t0 Current Filtered SSGI
    // t1 Previous GI History
    // t2 Motion Vector
    // t3 Current Depth
    // t4 Current Normal
    // t5 Previous Geometry History
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mTemporalSRVHeap;


    //============================================================
    // Upsample SRV Heap
    // t0: Half Resolution SSGI
    // t1: Full Resolution Depth
    // t2: Full Resolution Normal + Roughness
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mUpsampleSRVHeap;


    //============================================================
    // Full Resolution Result Debug / 後続Pass用SRV
    // Screen Root Signatureがt0～t1を要求するため、
    // 同じTextureを2個登録する
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mUpsampleResultSRVHeap;

    //============================================================
    // SSGI Composite
    // t0: HDR Scene
    // t1: Full Resolution SSGI
    //============================================================
    ComPtr<ID3D12DescriptorHeap> mCompositeSRVHeap;


    //============================================================
    // Pipeline
    //============================================================
    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12RootSignature> mFilterRootSignature;
    ComPtr<ID3D12RootSignature> mTemporalRootSignature;
    ComPtr<ID3D12RootSignature> mUpsampleRootSignature;
    ComPtr<ID3D12RootSignature> mCompositeRootSignature;

    ComPtr<ID3D12PipelineState> mRayMarchPSO;
    ComPtr<ID3D12PipelineState> mFilterPSO;
    ComPtr<ID3D12PipelineState> mTemporalPSO;
    ComPtr<ID3D12PipelineState> mUpsamplePSO;
    ComPtr<ID3D12PipelineState> mCompositePSO;

    //============================================================
    // Constant Buffer
    //============================================================
    ComPtr<ID3D12Resource> mConstantBuffer;
    ComPtr<ID3D12Resource> mFilterConstantBuffers[2];
    ComPtr<ID3D12Resource> mTemporalConstantBuffer;
    ComPtr<ID3D12Resource> mUpsampleConstantBuffer;

    SSGIRayMarchBuffer* mMappedConstantBuffer = nullptr;
    SSGIFilterBuffer* mFilterMapped[2] = { nullptr,nullptr };
    SSGITemporalBuffer* mTemporalMappedBuffer = nullptr;
    SSGIUpsampleBuffer* mUpsampleMappedBuffer = nullptr;

    //============================================================
    // Ray March Settings
    //============================================================

    // レイの本数
    int mRayCount = 4;

    // 1 Rayあたり12 Step
    int mStepCount = 12;

    // World Space Ray長
    float mMaxDistance = 3.0f;

    // Depth交差判定の厚み
    float mThickness = 0.08f;

    // Surface自身へ即Hitするのを防ぐ
    float mNormalBias = 0.02f;

    // Debug段階のGI強度
    float mIntensity = 1.0f;

    // 内部Resolution Scale
    float mResolutionScale = 0.5f;

    // Scene HDRから拾う最大Radiance
    float mMaxRadiance = 8.0f;

    //============================================================
    // Bilateral Filter Settings
    //============================================================

    // Filter半径
    int mFilterRadius = 4;

    // Depth差に対するReject強度
    float mFilterDepthFalloff = 8.0f;

    // Normal差に対するReject強度
    float mFilterNormalPower = 32.0f;

    // Confidenceが極端に低いSampleを弱くする
    float mFilterConfidenceMin = 0.05f;

    //============================================================
    // Temporal Parameters
    //============================================================

    // Previous Frameをどの程度残すか
    float mTemporalHistoryWeight = 0.90f;

    // World Positionがこれ以上ずれたらHistoryをReject
    float mTemporalWorldPositionThreshold = 0.08f;

    // Normal dotがこの値未満ならHistoryをReject
    float mTemporalNormalThreshold = 0.85f;

    // Motionが大きいPixelほどHistoryを弱くする
    float mTemporalMotionResponse = 0.05f;

    // Neighborhood Clampの許容幅
    float mTemporalClampExpand = 0.05f;

    //============================================================
    // Upsampling Settings
    //============================================================

    // Depth差に対するReject強度
    float mUpsampleDepthFalloff = 32.0f;

    // Normal差に対するReject強度
    float mUpsampleNormalPower = 64.0f;

    // Confidenceが低いHalf-res Sampleを弱める
    float mUpsampleConfidenceMin = 0.05f;

    //============================================================
    // Multi Bounce
    //============================================================
    bool mMultiBounceEnabled = true;

    //============================================================
    // 前フレームGIの再利用率
    //============================================================
    float mMultiBounceStrength = 0.25f;

    //============================================================
    // 前フレームのFull Resolution GIが有効か
    // 最初のFrameではmUpsampleResourceの内容を使わない
    //============================================================
    bool mMultiBounceHistoryValid = false;
};