#include "../../framework.h"
#include "../vn_environment.h"

#include "subsurfaceScatteringPass.h"

//============================================================
// 初期化
//============================================================
bool SubsurfaceScatteringPass::Initialize(UINT width, UINT height)
{
    mWidth = (std::max)(width, 1u);
    mHeight = (std::max)(height, 1u);

    //============================================================
    // SSS Working Resolution
    // Blur / TemporalだけHalf Resolutionへ落とす
    //============================================================
    mWorkWidth = (std::max)(static_cast<UINT>(static_cast<float>(mWidth) * mResolutionScale), 1u);
    mWorkHeight = (std::max)(static_cast<UINT>(static_cast<float>(mHeight) * mResolutionScale), 1u);

    ID3D12Device* device = Direct3D::getDevice();

    if (device == nullptr)
    {
        return false;
    }

    mSRVDescriptorSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //============================================================
    // Diffusion Source
    //============================================================
    if (!CreateDiffusionSourceResource())
    {
        Terminate();
        return false;
    }

    //============================================================
    // Material Diffusion Profile
    //============================================================
    if (!CreateDiffusionProfileResource())
    {
        Terminate();
        return false;
    }

    //========================================================
    // Mask Texture
    //========================================================
    if (!CreateMaskResource())
    {
        Terminate();
        return false;
    }

    //========================================================
    // Geometry Mask PSO
    //========================================================
    if (!CreateMaskPipelineStates())
    {
        Terminate();
        return false;
    }

    //============================================================
    // Screen-Space Diffusion
    //============================================================
    if (!CreateDiffusionResources())
    {
        Terminate();
        return false;
    }

    if (!CreateDiffusionRootSignature())
    {
        Terminate();
        return false;
    }

    if (!CreateDiffusionConstantBuffers())
    {
        Terminate();
        return false;
    }

    if (!CreateDiffusionPipelineStates())
    {
        Terminate();
        return false;
    }

    //============================================================
    // Temporal SSS
    //============================================================
    if (!CreateTemporalResources())
    {
        Terminate();
        return false;
    }

    if (!CreateTemporalRootSignature())
    {
        Terminate();
        return false;
    }

    if (!CreateTemporalConstantBuffer())
    {
        Terminate();
        return false;
    }

    if (!CreateTemporalPipelineState())
    {
        Terminate();
        return false;
    }

    if (!CreateDebugResources())
    {
        Terminate();
        return false;
    }
    return true;
}

void SubsurfaceScatteringPass::Terminate()
{
    //============================================================
    // Diffusion Constant Buffers
    //============================================================
    for (UINT index = 0u; index < 2u; ++index)
    {
        if (mDiffusionConstantBuffers[index] && mDiffusionMapped[index] != nullptr)
        {
            mDiffusionConstantBuffers[index]->Unmap(
                0u,
                nullptr
            );
            mDiffusionMapped[index] = nullptr;
        }
        mDiffusionConstantBuffers[index].Reset();
        mBlurResources[index].Reset();
    }
    //============================================================
    // Temporal Constant Buffers
    //============================================================
    if (mTemporalConstantBuffer &&mTemporalMapped != nullptr)
    {
        mTemporalConstantBuffer->Unmap(
                0u,
                nullptr
            );
        mTemporalMapped = nullptr;
    }


    //============================================================
    // ComPtr管理Object
    //============================================================
    mDiffusionBlurPSO.Reset();
    mDiffusionCompositePSO.Reset();

    mDiffusionRootSignature.Reset();
    mTemporalRootSignature.Reset();

    mRigidMaskPSO.Reset();
    mSkinMaskPSO.Reset();
    mTemporalPSO.Reset();
    
    mDiffusionSRVHeap.Reset();
    mMaskSRVHeap.Reset();
    mMaskRTVHeap.Reset();
    mBlurRTVHeap.Reset();
    mTemporalSRVHeap.Reset();
    mTemporalRTVHeap.Reset();
    mDebugSRVHeap.Reset();

    mMaskResource.Reset();
    mDiffusionSourceResource.Reset();
    mDiffusionProfileResource.Reset();
    mTemporalHistoryResources[0].Reset();
    mTemporalHistoryResources[1].Reset();
    mTemporalConstantBuffer.Reset();

    mMaskState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    mDiffusionSourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    mDiffusionProfileState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    mTemporalHistoryStates[0] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    mTemporalHistoryStates[1] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    mMaskRTV = {};
    mDiffusionSourceRTV = {};
    mDiffusionProfileRTV = {};
    mTemporalRTV[0] = {};
    mTemporalRTV[1] = {};
    
    mDebugView = DebugView::None;

    mWidth = 0u;
    mHeight = 0u;
    mWorkWidth = 0u;
    mWorkHeight = 0u;

    mSRVDescriptorSize = 0u;
    
    mTemporalReadIndex = 0u;
    mTemporalWriteIndex =1u;
    mTemporalHistoryValid = false;
}

void SubsurfaceScatteringPass::GenerateMask()
{
    if (!mMaskResource ||
        !mDiffusionSourceResource ||
        !mDiffusionProfileResource ||
        !mMaskRTVHeap ||
        !mRigidMaskPSO ||
        !mSkinMaskPSO)
    {
        return;
    }


    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

    if (commandList == nullptr)
    {
        return;
    }


    //========================================================
    // Main DepthをDSVとして使える状態へ戻す
    //========================================================
    Direct3D::TransitionMainDepthToDepthWrite();


    //========================================================
    // MaskをRenderTargetへ
    //========================================================
    TransitionMask(D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionDiffusionSource(D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionDiffusionProfile(D3D12_RESOURCE_STATE_RENDER_TARGET);

    const D3D12_CPU_DESCRIPTOR_HANDLE maskRTV = 
        mMaskRTVHeap->GetCPUDescriptorHandleForHeapStart();

    const D3D12_CPU_DESCRIPTOR_HANDLE mainDSV = Direct3D::getDepthStencilView();


    //========================================================
    // 毎Frame Maskを0で初期化
    //========================================================
    const float clearColor[4] =
    {
        0.0f,
        0.0f,
        0.0f,
        0.0f
    };


    commandList->ClearRenderTargetView(
        maskRTV,
        clearColor,
        0u,
        nullptr
    );

    const float clearSource[4] =
    {
        0.0f,
        0.0f,
        0.0f,
        0.0f
    };


    commandList->ClearRenderTargetView(
        mDiffusionSourceRTV,
        clearSource,
        0u,
        nullptr
    );

    const float clearProfile[4] =
    {
        0.0f,
        0.0f,
        0.0f,
        0.0f
    };


    commandList->ClearRenderTargetView(
        mDiffusionProfileRTV,
        clearProfile,
        0u,
        nullptr
    );

    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[3] =
    {
        mMaskRTV,
        mDiffusionSourceRTV,
        mDiffusionProfileRTV
    };


    commandList->OMSetRenderTargets(
        3u,
        rtvs,
        FALSE,
        &mainDSV
    );

    //========================================================
    // Full Resolution
    //========================================================
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(mWidth);
    viewport.Height = static_cast<float>(mHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;


    D3D12_RECT scissor =
    {
        0,
        0,
        static_cast<LONG>(mWidth),
        static_cast<LONG>(mHeight)
    };


    commandList->RSSetViewports(
        1u,
        &viewport
    );


    commandList->RSSetScissorRects(
        1u,
        &scissor
    );


    //========================================================
    // Main 3D Root Signature
    // Root[0] Scene Bufferもここで戻す
    //========================================================
    Direct3D::SetMain3DRootState();


    //========================================================
    // Scene内のSSS Materialを再描画
    //========================================================
    vnScene::renderSubsurfaceMask(
        mRigidMaskPSO.Get(),
        mSkinMaskPSO.Get()
    );


    //========================================================
    // 後続のBlur / Debugから読める状態へ
    //========================================================
    TransitionMask(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    TransitionDiffusionSource(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    TransitionDiffusionProfile(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    //========================================================
    // 通常HDR RenderTargetへ戻しておく
    //========================================================
    Direct3D::setMainRenderTarget();
}

//============================================================
// SSS Mask Texture作成
//============================================================
bool SubsurfaceScatteringPass::CreateMaskResource()
{
    ID3D12Device* device = Direct3D::getDevice();

    if (device == nullptr)
    {
        return false;
    }


    //========================================================
    // Default Heap
    //========================================================
    D3D12_HEAP_PROPERTIES heapProperties = {};

    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


    //========================================================
    // R16_FLOAT
    // 0.0 ～ 1.0程度のSSS Maskを保存する
    //========================================================
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = mWidth;
    resourceDesc.Height = mHeight;
    resourceDesc.DepthOrArraySize = 1u;
    resourceDesc.MipLevels = 1u;
    resourceDesc.Format = DXGI_FORMAT_R16_FLOAT;
    resourceDesc.SampleDesc.Count = 1u;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


    //========================================================
    // Maskなし = 0
    //========================================================
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R16_FLOAT;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;


    //========================================================
    // Resource作成
    //========================================================
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(mMaskResource.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr) || !mMaskResource)
    {
        return false;
    }


    mMaskResource->SetName(L"SSS Screen Space Mask");


    mMaskState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


    //========================================================
    // RTV Heap
    //========================================================
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 3u;


    hr = device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(mMaskRTVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr) || !mMaskRTVHeap)
    {
        return false;
    }


    mMaskRTVHeap->SetName(L"SSS Mask RTV Heap");


    const UINT rtvDescriptorSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    //========================================================
    // [0] Mask RTV
    //========================================================
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        mMaskRTVHeap->GetCPUDescriptorHandleForHeapStart();


    mMaskRTV = rtvHandle;

    D3D12_RENDER_TARGET_VIEW_DESC maskRTVDesc = {};
    maskRTVDesc.Format = DXGI_FORMAT_R16_FLOAT;
    maskRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    device->CreateRenderTargetView(
        mMaskResource.Get(),
        &maskRTVDesc,
        mMaskRTV
    );

    //============================================================
    // [1] Diffusion Source RTV
    //============================================================
    rtvHandle.ptr += static_cast<SIZE_T>(rtvDescriptorSize);

    mDiffusionSourceRTV = rtvHandle;

    D3D12_RENDER_TARGET_VIEW_DESC sourceRTVDesc = {};
    sourceRTVDesc.Format = RenderFormat::SceneHDR;
    sourceRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    device->CreateRenderTargetView(
        mDiffusionSourceResource.Get(),
        &sourceRTVDesc,
        mDiffusionSourceRTV
    );

    //============================================================
    // [2] Material SSS Profile RTV
    //============================================================
    rtvHandle.ptr += static_cast<SIZE_T>(rtvDescriptorSize);

    mDiffusionProfileRTV = rtvHandle;

    D3D12_RENDER_TARGET_VIEW_DESC profileRTVDesc = {};
    profileRTVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    profileRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    device->CreateRenderTargetView(
        mDiffusionProfileResource.Get(),
        &profileRTVDesc,
        mDiffusionProfileRTV
    );

    //========================================================
    // SRV Heap
    //========================================================
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 2u;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    hr = device->CreateDescriptorHeap(
        &srvHeapDesc,
        IID_PPV_ARGS(mMaskSRVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr) || !mMaskSRVHeap)
    {
        return false;
    }


    mMaskSRVHeap->SetName(L"SSS Mask SRV Heap");


    //========================================================
    // SRV
    //========================================================
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0u;
    srvDesc.Texture2D.MipLevels = 1u;


    //========================================================
    // R成分をRGB全部へ展開する。
    //========================================================
    srvDesc.Shader4ComponentMapping =
        D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
            D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
            D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
            D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
            D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1
        );


    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
        mMaskSRVHeap->GetCPUDescriptorHandleForHeapStart();


    //========================================================
    // [0] t0 Mask
    //========================================================
    device->CreateShaderResourceView(
        mMaskResource.Get(),
        &srvDesc,
        srvHandle
    );


    //========================================================
    // [1] t1 Dummy Mask
    //========================================================
    srvHandle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize);


    device->CreateShaderResourceView(
        mMaskResource.Get(),
        &srvDesc,
        srvHandle
    );


    return true;
}

void SubsurfaceScatteringPass::ApplyToRT(
    ID3D12Resource* inputSceneResource,
    ID3D12Resource* motionVectorResource,
    ID3D12Resource* depthResource,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
    UINT width,
    UINT height
)
{
    ID3D12Device* device = Direct3D::getDevice();

    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


    if (device == nullptr || commandList == nullptr)
    {
        return;
    }


    //============================================================
    // DepthをSRVとして使用
    //============================================================
    Direct3D::TransitionMainDepthToSRV();


    //============================================================
    // Diffusion CB更新
    //============================================================
    UpdateDiffusionConstantBuffers();


    //============================================================
    // Descriptor Helper
    //============================================================
    const D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
        mDiffusionSRVHeap->GetCPUDescriptorHandleForHeapStart();


    auto getCPUHandle = [&](UINT index)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize) * index;
            return handle;
        };


    auto createTextureSRV = [&](ID3D12Resource* resource, DXGI_FORMAT format, UINT index)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
            desc.Format = format;
            desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Texture2D.MostDetailedMip = 0u;
            desc.Texture2D.MipLevels = 1u;


            device->CreateShaderResourceView(
                resource,
                &desc,
                getCPUHandle(index)
            );
        };


    //============================================================
    // Horizontal
    //
    // [0] t0 Diffusion Source
    // [1] t1 Mask
    // [2] t2 Depth
    // [3] t3 Material Profile
    // [4] t4 Dummy
    //============================================================
    createTextureSRV(
        mDiffusionSourceResource.Get(),
        RenderFormat::SceneHDR,
        0u
    );

    createTextureSRV(
        mMaskResource.Get(),
        DXGI_FORMAT_R16_FLOAT,
        1u
    );

    createTextureSRV(
        depthResource,
        DXGI_FORMAT_R32_FLOAT,
        2u
    );

    createTextureSRV(
        mDiffusionProfileResource.Get(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        3u
    );

    createTextureSRV(
        mMaskResource.Get(),
        DXGI_FORMAT_R16_FLOAT,
        4u
    );


    //============================================================
    // Vertical
    //
    // [5] t0 Horizontal Blur
    // [6] t1 Mask
    // [7] t2 Depth
    // [8] t3 Material Profile
    // [9] t4 Dummy
    //============================================================
    createTextureSRV(
        mBlurResources[0].Get(),
        RenderFormat::SceneHDR,
        5u
    );

    createTextureSRV(
        mMaskResource.Get(),
        DXGI_FORMAT_R16_FLOAT,
        6u
    );

    createTextureSRV(
        depthResource,
        DXGI_FORMAT_R32_FLOAT,
        7u
    );

    createTextureSRV(
        mDiffusionProfileResource.Get(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        8u
    );

    createTextureSRV(
        mMaskResource.Get(),
        DXGI_FORMAT_R16_FLOAT,
        9u
    );


    //============================================================
    // [10] t0 Original Scene
    // [11] t1 Original Diffusion
    // [12] t2 Resolved Diffusion 
    // [13] t3 Mask
    // [14] t4 Depth
    //============================================================
    createTextureSRV(
        inputSceneResource,
        RenderFormat::SceneHDR,
        10u
    );

    createTextureSRV(
        mDiffusionSourceResource.Get(),
        RenderFormat::SceneHDR,
        11u
    );

    createTextureSRV(
        mMaskResource.Get(),
        DXGI_FORMAT_R16_FLOAT,
        13u
    );

    createTextureSRV(
        depthResource,
        DXGI_FORMAT_R32_FLOAT,
        14u
    );


    //============================================================
    // GPU Descriptor Handle
    //============================================================
    auto getGPUHandle = [&](UINT index)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE handle =
                mDiffusionSRVHeap->GetGPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<UINT64>(mSRVDescriptorSize) * index;
            return handle;
        };


    //============================================================
    // Diffusion用Heap
    //============================================================
    ID3D12DescriptorHeap* diffusionHeaps[] =
    {
        mDiffusionSRVHeap.Get()
    };


    commandList->SetDescriptorHeaps(
        1u,
        diffusionHeaps
    );


    commandList->SetGraphicsRootSignature(mDiffusionRootSignature.Get());


    //============================================================
    // Half Resolution Viewport
    //============================================================
    D3D12_VIEWPORT workViewport = {};

    workViewport.Width = static_cast<float>(mWorkWidth);
    workViewport.Height = static_cast<float>(mWorkHeight);
    workViewport.MinDepth = 0.0f;
    workViewport.MaxDepth = 1.0f;


    D3D12_RECT workScissor =
    {
        0,
        0,
        static_cast<LONG>(mWorkWidth),
        static_cast<LONG>(mWorkHeight)
    };


    commandList->RSSetViewports(
        1u,
        &workViewport
    );

    commandList->RSSetScissorRects(
        1u,
        &workScissor
    );


    //============================================================
    // Full Screen Quad共通設定
    //============================================================
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    commandList->IASetVertexBuffers(
        0u,
        1u,
        Direct3D::getScreenVBView()
    );


    //============================================================
    // Horizontal Blur
    //============================================================
    TransitionBlurResource(
        0u,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );


    commandList->OMSetRenderTargets(
        1u,
        &mBlurRTV[0],
        FALSE,
        nullptr
    );


    commandList->SetPipelineState(mDiffusionBlurPSO.Get());


    commandList->SetGraphicsRootDescriptorTable(
        0u,
        getGPUHandle(0u)
    );


    commandList->SetGraphicsRootConstantBufferView(
        1u,
        mDiffusionConstantBuffers[0]->GetGPUVirtualAddress()
    );


    commandList->DrawInstanced(4u, 1u, 0u, 0u);


    TransitionBlurResource(
        0u,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );


    //============================================================
    // Vertical Blur
    //============================================================
    TransitionBlurResource(
        1u,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );


    commandList->OMSetRenderTargets(
        1u,
        &mBlurRTV[1],
        FALSE,
        nullptr
    );


    commandList->SetPipelineState(mDiffusionBlurPSO.Get());


    commandList->SetGraphicsRootDescriptorTable(
        0u,
        getGPUHandle(5u)
    );


    commandList->SetGraphicsRootConstantBufferView(
        1u,
        mDiffusionConstantBuffers[1]->GetGPUVirtualAddress()
    );


    commandList->DrawInstanced(4u, 1u, 0u, 0u);


    TransitionBlurResource(
        1u,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );


    //============================================================
    // Temporal Resolve
    //============================================================
    ID3D12Resource* resolvedDiffusionResource = mBlurResources[1].Get();


    bool temporalResolvedThisFrame = false;


    if (mTemporalEnabled)
    {
        ID3D12Resource* temporalResult =
            ResolveTemporal(
                mBlurResources[1].Get(),
                motionVectorResource,
                depthResource
            );


        if (temporalResult != nullptr)
        {
            resolvedDiffusionResource = temporalResult;
            temporalResolvedThisFrame = temporalResult != mBlurResources[1].Get();
        }
    }


    //============================================================
    // Composite t2
    // Temporal ON: Temporal History Write側
    // Temporal OFF: Vertical Blur
    //============================================================
    createTextureSRV(
        resolvedDiffusionResource,
        RenderFormat::SceneHDR,
        12u
    );



    commandList->SetDescriptorHeaps(
        1u,
        diffusionHeaps
    );


    commandList->SetGraphicsRootSignature(mDiffusionRootSignature.Get());


    //============================================================
    // Full Resolution Composite
    //============================================================
    D3D12_VIEWPORT fullViewport = {};

    fullViewport.Width = static_cast<float>(width);
    fullViewport.Height = static_cast<float>(height);
    fullViewport.MinDepth = 0.0f;
    fullViewport.MaxDepth = 1.0f;


    D3D12_RECT fullScissor =
    {
        0,
        0,
        static_cast<LONG>(width),
        static_cast<LONG>(height)
    };


    commandList->RSSetViewports(
        1u,
        &fullViewport
    );

    commandList->RSSetScissorRects(
        1u,
        &fullScissor
    );


    commandList->OMSetRenderTargets(
        1u,
        &outputRTV,
        FALSE,
        nullptr
    );


    commandList->SetPipelineState(mDiffusionCompositePSO.Get());


    commandList->SetGraphicsRootDescriptorTable(
        0u,
        getGPUHandle(10u)
    );


    commandList->SetGraphicsRootConstantBufferView(
        1u,
        mDiffusionConstantBuffers[0]->GetGPUVirtualAddress()
    );

    commandList->DrawInstanced(4u, 1u, 0u, 0u);

    //============================================================
    // Temporal Resolveが実際に成功したFrameだけSwap
    //============================================================
    if (temporalResolvedThisFrame)
    {
        CommitTemporalHistory();
    }

    // Debug表示用SRVを最新FrameのResourceへ更新
    UpdateDebugSRV();
}

ID3D12Resource* SubsurfaceScatteringPass::ResolveTemporal(ID3D12Resource* currentBlurredResource, ID3D12Resource* motionVectorResource, ID3D12Resource* depthResource)
{
    //============================================================
    // Temporal OFF
    //============================================================
    if (!mTemporalEnabled)
    {
        return currentBlurredResource;
    }


    if (currentBlurredResource == nullptr ||
        motionVectorResource == nullptr ||
        depthResource == nullptr ||
        !mTemporalHistoryResources[0] ||
        !mTemporalHistoryResources[1] ||
        !mTemporalSRVHeap ||
        !mTemporalRootSignature ||
        !mTemporalPSO ||
        !mTemporalConstantBuffer)
    {
        return currentBlurredResource;
    }


    ID3D12Device* device = Direct3D::getDevice();


    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


    if (device == nullptr || commandList == nullptr)
    {
        return currentBlurredResource;
    }


    //============================================================
    // Current DepthをSRVへ
    //============================================================
    Direct3D::TransitionMainDepthToSRV();


    //============================================================
    // Previous HistoryはSRV
    //============================================================
    TransitionTemporalHistory(
        mTemporalReadIndex,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );


    //============================================================
    // Current History Write先はRTV
    //============================================================
    TransitionTemporalHistory(
        mTemporalWriteIndex,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );


    UpdateTemporalConstantBuffer();


    //============================================================
    // Descriptor Helper
    //============================================================
    const UINT descriptorSize = mSRVDescriptorSize;


    D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
        mTemporalSRVHeap->GetCPUDescriptorHandleForHeapStart();


    auto getHandle = [&](UINT index)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
            return handle;
        };


    auto createSRV = [&](ID3D12Resource* resource, DXGI_FORMAT format, UINT index)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
            desc.Format = format;
            desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Texture2D.MostDetailedMip = 0u;
            desc.Texture2D.MipLevels = 1u;


            device->CreateShaderResourceView(
                resource,
                &desc,
                getHandle(index)
            );
        };


    //============================================================
    // t0 Current Vertical Blur
    //============================================================
    createSRV(
        currentBlurredResource,
        RenderFormat::SceneHDR,
        0u
    );


    //============================================================
    // t1 Previous Temporal History
    //============================================================
    createSRV(
        mTemporalHistoryResources[mTemporalReadIndex].Get(),
        RenderFormat::SceneHDR,
        1u
    );


    //============================================================
    // t2 Motion Vector
    //============================================================
    createSRV(
        motionVectorResource,
        DXGI_FORMAT_R16G16_FLOAT,
        2u
    );


    //============================================================
    // t3 Main Depth
    //============================================================
    createSRV(
        depthResource,
        DXGI_FORMAT_R32_FLOAT,
        3u
    );


    //============================================================
    // t4 Current SSS Mask
    //============================================================
    createSRV(
        mMaskResource.Get(),
        DXGI_FORMAT_R16_FLOAT,
        4u
    );

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(mWorkWidth);
    viewport.Height = static_cast<float>(mWorkHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;


    D3D12_RECT scissor =
    {
        0,
        0,
        static_cast<LONG>(mWorkWidth),
        static_cast<LONG>(mWorkHeight)
    };


    commandList->RSSetViewports(
        1u,
        &viewport
    );


    commandList->RSSetScissorRects(
        1u,
        &scissor
    );

    //============================================================
    // Output
    //============================================================
    const D3D12_CPU_DESCRIPTOR_HANDLE outputRTV = mTemporalRTV[mTemporalWriteIndex];


    commandList->OMSetRenderTargets(
        1u,
        &outputRTV,
        FALSE,
        nullptr
    );


    commandList->SetGraphicsRootSignature(mTemporalRootSignature.Get());
    commandList->SetPipelineState(mTemporalPSO.Get());


    //============================================================
    // Heap
    //============================================================
    ID3D12DescriptorHeap* heaps[] =
    {
        mTemporalSRVHeap.Get()
    };


    commandList->SetDescriptorHeaps(
        1u,
        heaps
    );


    commandList->SetGraphicsRootDescriptorTable(
        0u,
        mTemporalSRVHeap->GetGPUDescriptorHandleForHeapStart()
    );


    commandList->SetGraphicsRootConstantBufferView(
        1u,
        mTemporalConstantBuffer->GetGPUVirtualAddress()
    );


    //============================================================
    // Full Screen Quad
    //============================================================
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


    commandList->IASetVertexBuffers(
        0u,
        1u,
        Direct3D::getScreenVBView()
    );


    commandList->DrawInstanced(4u, 1u, 0u, 0u);


    //============================================================
    // Compositeから読めるようにする
    //============================================================
    TransitionTemporalHistory(
        mTemporalWriteIndex,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    return mTemporalHistoryResources[mTemporalWriteIndex].Get();
}

void SubsurfaceScatteringPass::CommitTemporalHistory()
{
    if (!mTemporalEnabled)
    {
        return;
    }
    //============================================================
    // 今フレーム書いたHistoryを 次フレームのRead側へする
    //============================================================
    std::swap(
        mTemporalReadIndex,
        mTemporalWriteIndex
    );
    //============================================================
    // ここからHistory有効
    //============================================================
    mTemporalHistoryValid = true;
}

void SubsurfaceScatteringPass::SetDiffusionStrength(float strength)
{
    mDiffusionStrength = (std::clamp)(strength, 0.0f, 1.0f);
}

void SubsurfaceScatteringPass::SetBlurRadius(int radius)
{
    mBlurRadius = (std::clamp)(radius, 1, 12);
}

void SubsurfaceScatteringPass::SetDepthFalloff(float falloff)
{
    mDepthFalloff = (std::max)(falloff, 0.0f);
}

void SubsurfaceScatteringPass::SetScatteringProfile(float red, float green, float blue)
{
    mScatteringProfile =
        XMFLOAT3(
            (std::max)(red, 0.01f),
            (std::max)(green, 0.01f),
            (std::max)(blue, 0.01f)
        );
}

void SubsurfaceScatteringPass::SetSampleQuality(float quality)
{
    mSampleQuality = (std::clamp)(quality, 0.0f, 1.0f);
}

void SubsurfaceScatteringPass::SetWorldSpaceRadius(float radius)
{
    mWorldSpaceRadius = (std::max)(radius, 0.0f);
}

void SubsurfaceScatteringPass::SetTemporalEnabled(bool enabled)
{
    if (mTemporalEnabled != enabled)
    {
        mTemporalEnabled = enabled;
        ResetTemporalHistory();
    }
}

void SubsurfaceScatteringPass::SetTemporalHistoryWeight(float weight)
{
    mTemporalHistoryWeight = (std::clamp)(weight, 0.0f, 0.98f);
}

void SubsurfaceScatteringPass::SetTemporalDepthThreshold(float threshold)
{
    mTemporalDepthThreshold = (std::max)(threshold, 0.0001f);
}

void SubsurfaceScatteringPass::SetTemporalMotionResponse(float response)
{
    mTemporalMotionResponse = (std::max)(response, 0.0f);
}

void SubsurfaceScatteringPass::ResetTemporalHistory()
{
    mTemporalHistoryValid = false;
    mTemporalReadIndex = 0u;
    mTemporalWriteIndex = 1u;
}

void SubsurfaceScatteringPass::SetDebugView(DebugView view)
{
    mDebugView = view;
}



bool SubsurfaceScatteringPass::CreateMaskPipelineStates()
{
    //========================================================
    // Rigid
    //========================================================
    if (!CreateMaskPipelineState(false, mRigidMaskPSO))
    {
        return false;
    }


    //========================================================
    // Skin
    //========================================================
    if (!CreateMaskPipelineState(true, mSkinMaskPSO))
    {
        return false;
    }


    mRigidMaskPSO->SetName(L"SSS Mask Rigid PSO");
    mSkinMaskPSO->SetName(L"SSS Mask Skin PSO");

    return true;
}

bool SubsurfaceScatteringPass::CreateMaskPipelineState(bool skinned, ComPtr<ID3D12PipelineState>& outputPSO)
{
    ID3D12Device* device = Direct3D::getDevice();

    if (device == nullptr)
    {
        return false;
    }


    //========================================================
    // Rigid Input Layout
    //========================================================
    D3D12_INPUT_ELEMENT_DESC rigidLayout[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "NORMAL",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "TANGENT",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        }
    };


    //========================================================
    // Skin Input Layout
    //========================================================
    D3D12_INPUT_ELEMENT_DESC skinLayout[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "NORMAL",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "TANGENT",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "SKIN_INDEX",
            0,
            DXGI_FORMAT_R16G16B16A16_UINT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "SKIN_WEIGHT",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        }
    };


    auto* vertexShader =
        Shader::getVShader(
            skinned ?
            Shader::eVertexShader::VS_3D_Skin : Shader::eVertexShader::VS_3D
        );

    auto* pixelShader = Shader::getPShader(Shader::ePixelShader::PS_SSSMask);


    if (vertexShader == nullptr || pixelShader == nullptr)
    {
        return false;
    }


    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};


    //========================================================
    // 通常3Dと同じRoot Signature
    // Material Heap内の
    // t0  Diffuse
    // b1  Material
    // t16 Thickness
    // をそのまま使用するため
    //========================================================
    desc.pRootSignature = Direct3D::getRootSignature();


    //========================================================
    // Input Layout
    //========================================================
    if (skinned)
    {
        desc.InputLayout =
        {
            skinLayout,
            _countof(skinLayout)
        };
    }
    else
    {
        desc.InputLayout =
        {
            rigidLayout,
            _countof(rigidLayout)
        };
    }


    //========================================================
    // VS
    //========================================================
    desc.VS =
    {
        vertexShader->getCode(),
        static_cast<SIZE_T>(vertexShader->getLength())
    };


    //========================================================
    // PS
    //========================================================
    desc.PS =
    {
        pixelShader->getCode(),
        static_cast<SIZE_T>(pixelShader->getLength())
    };


    //========================================================
    // Rasterizer
    // Phase 4AではCULL_NONEにしておく
    // Depth EQUALがあるため
    // Main Sceneで見えていない奥側のPolygonは基本的に通らない
    //========================================================
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    desc.RasterizerState.DepthClipEnable = TRUE;

    //========================================================
    // Blend不要
    //========================================================
    desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


    //========================================================
    // Main Depthを再利用する
    // Depth Write: OFF
    // Depth Compare: EQUAL
    // 通常描画ですでに最前面だったPixelだけMaskへ書く
    //========================================================
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
    desc.DepthStencilState.StencilEnable = FALSE;

    //============================================================
    // SSS Profile MRT
    // RT0: Mask
    // RT1: Diffuse + Transmission Source
    // RT2: Material Scattering Profile + Radius
    //============================================================
    desc.NumRenderTargets = 3u;

    //============================================================
    // RT0
    //============================================================
    desc.RTVFormats[0] = DXGI_FORMAT_R16_FLOAT;

    //============================================================
    // RT1
    //============================================================
    desc.RTVFormats[1] = RenderFormat::SceneHDR;

    //============================================================
    // RT2 
    //============================================================
    desc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;

    //============================================================
    // Blendなし
    //============================================================
    desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.BlendState.RenderTarget[1].BlendEnable = FALSE;
    desc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.BlendState.RenderTarget[2].BlendEnable = FALSE;
    desc.BlendState.RenderTarget[2].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    //========================================================
    // Main Depth
    //========================================================
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.SampleDesc.Count = 1u;


    const HRESULT hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(outputPSO.ReleaseAndGetAddressOf())
    );


    return SUCCEEDED(hr);
}

void SubsurfaceScatteringPass::TransitionMask(D3D12_RESOURCE_STATES newState)
{
    if (!mMaskResource || mMaskState == newState)
    {
        return;
    }


    D3D12_RESOURCE_BARRIER barrier = {};

    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mMaskResource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mMaskState;
    barrier.Transition.StateAfter = newState;


    Direct3D::getCommandList()->ResourceBarrier(
        1u,
        &barrier
    );


    mMaskState = newState;
}

bool SubsurfaceScatteringPass::CreateDiffusionResources()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // Working Resolution HDR
    // Horizontal / Vertical BlurはHalf Resolution
    //============================================================
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = mWorkWidth;
    resourceDesc.Height = mWorkHeight;
    resourceDesc.DepthOrArraySize = 1u;
    resourceDesc.MipLevels = 1u;
    resourceDesc.Format = RenderFormat::SceneHDR;
    resourceDesc.SampleDesc.Count = 1u;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = RenderFormat::SceneHDR;


    //============================================================
    // Horizontal / Vertical
    //============================================================
    for (UINT index = 0u; index < 2u; ++index)
    {
        const HRESULT hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(mBlurResources[index].ReleaseAndGetAddressOf())
        );


        if (FAILED(hr))
        {
            return false;
        }


        mBlurStates[index] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }


    mBlurResources[0]->SetName(L"SSS Horizontal Diffusion");
    mBlurResources[1]->SetName(L"SSS Vertical Diffusion");


    //============================================================
    // RTV Heap
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 2u;


    HRESULT hr = device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(mBlurRTVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    const UINT rtvDescriptorSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        mBlurRTVHeap->GetCPUDescriptorHandleForHeapStart();


    for (UINT index = 0u; index < 2u; ++index)
    {
        mBlurRTV[index] = handle;


        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = RenderFormat::SceneHDR;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        device->CreateRenderTargetView(
            mBlurResources[index].Get(),
            &rtvDesc,
            handle
        );


        handle.ptr += static_cast<SIZE_T>(rtvDescriptorSize);
    }


    //============================================================
    // 15 Descriptors
    // [0～4] Horizontal
    // [5～9] Vertical
    // [10～14] Composite
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};

    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 15u;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    hr = device->CreateDescriptorHeap(
        &srvHeapDesc,
        IID_PPV_ARGS(mDiffusionSRVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    mDiffusionSRVHeap->SetName(L"SSS Diffusion SRV Heap");


    return true;
}

bool SubsurfaceScatteringPass::CreateDiffusionSourceResource()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // GPU専用Default Heap
    //============================================================
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


    //============================================================
    // Full Resolution HDR Texture
    // PBR DiffuseはHDR値を持つためSceneHDRと同じFormatを使用する
    //============================================================
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = mWidth;
    resourceDesc.Height = mHeight;
    resourceDesc.DepthOrArraySize = 1u;
    resourceDesc.MipLevels = 1u;
    resourceDesc.Format = RenderFormat::SceneHDR;
    resourceDesc.SampleDesc.Count = 1u;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


    //============================================================
    // Clear Color
    // SSS Materialが存在しないPixelは0
    //============================================================
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = RenderFormat::SceneHDR;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;


    //============================================================
    // 新規ResourceなのでComPtr
    //============================================================
    const HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(mDiffusionSourceResource.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr) || !mDiffusionSourceResource)
    {
        return false;
    }

    mDiffusionSourceResource->SetName(L"SSS Diffusion Source");

    mDiffusionSourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    return true;
}

bool SubsurfaceScatteringPass::CreateDiffusionProfileResource()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // GPU Default Heap
    //============================================================
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


    //============================================================
    // RGBA16_FLOAT
    // RGB: Scattering Profile
    // A: World Space Radius
    // ※HDR Colorではないが、小数値を精度よく保持したいので
    // RGBA16_FLOATを使用する
    //============================================================
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = mWidth;
    resourceDesc.Height = mHeight;
    resourceDesc.DepthOrArraySize = 1u;
    resourceDesc.MipLevels = 1u;
    resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    resourceDesc.SampleDesc.Count = 1u;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


    //============================================================
    // 0でClear
    //============================================================
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;


    const HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(mDiffusionProfileResource.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr) || !mDiffusionProfileResource)
    {
        return false;
    }


    mDiffusionProfileResource->SetName(L"SSS Material Diffusion Profile");

    mDiffusionProfileState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    return true;
}

bool SubsurfaceScatteringPass::CreateTemporalResources()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // RGB: HDR Diffusion
    // A: Camera Distance
    //============================================================
    const DXGI_FORMAT format = RenderFormat::SceneHDR;


    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = mWorkWidth;
    resourceDesc.Height = mWorkHeight;
    resourceDesc.DepthOrArraySize = 1u;
    resourceDesc.MipLevels = 1u;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1u;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = format;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;


    //============================================================
    // History 0 / 1
    //============================================================
    for (UINT index = 0u; index < 2u; ++index)
    {
        const HRESULT hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(mTemporalHistoryResources[index].ReleaseAndGetAddressOf())
        );


        if (FAILED(hr))
        {
            return false;
        }


        mTemporalHistoryStates[index] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }


    mTemporalHistoryResources[0]->SetName(L"SSS Temporal History 0");
    mTemporalHistoryResources[1]->SetName(L"SSS Temporal History 1");


    //============================================================
    // RTV Heap
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 2u;


    HRESULT hr = device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(mTemporalRTVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    const UINT descriptorSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        mTemporalRTVHeap->GetCPUDescriptorHandleForHeapStart();


    for (UINT index = 0u; index < 2u; ++index)
    {
        mTemporalRTV[index] = handle;


        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


        device->CreateRenderTargetView(
            mTemporalHistoryResources[index].Get(),
            &rtvDesc,
            handle
        );


        handle.ptr += static_cast<SIZE_T>(descriptorSize);
    }


    //============================================================
    // SRV Heap
    // Temporal Passは5 Textureを読む
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 5u;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    hr = device->CreateDescriptorHeap(
        &srvHeapDesc,
        IID_PPV_ARGS(mTemporalSRVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    return true;
}

bool SubsurfaceScatteringPass::CreateDebugResources()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // Screen Root Signature:
    // t0
    // t1
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 2u;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    const HRESULT hr = device->CreateDescriptorHeap(
        &heapDesc,
        IID_PPV_ARGS(mDebugSRVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }

    mDebugSRVHeap->SetName(L"SSS Debug SRV Heap");
    return true;
}

bool SubsurfaceScatteringPass::CreateDiffusionRootSignature()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // t0 ～ t4
    //
    // Blur:
    //   t0 Diffusion Input
    //   t1 Mask
    //   t2 Depth
    //   t3 Material Profile
    //   t4 Dummy
    //
    // Composite:
    //   t0 Original Scene
    //   t1 Original Diffusion
    //   t2 Half-Res Resolved Diffusion
    //   t3 Mask
    //   t4 Depth
    //============================================================
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 5u;
    srvRange.BaseShaderRegister = 0u;
    srvRange.RegisterSpace = 0u;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    //============================================================
    // Root[0] SRV
    //============================================================
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;


    //============================================================
    // Root[1] b0
    //============================================================
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0u;


    //============================================================
    // s0 : Linear
    //============================================================
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].ShaderRegister = 0u;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;


    //============================================================
    // s1 : Point
    // Depth / Mask用
    //============================================================
    samplers[1] = samplers[0];
    samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[1].ShaderRegister = 1u;


    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(rootParameters);
    desc.pParameters = rootParameters;
    desc.NumStaticSamplers = _countof(samplers);
    desc.pStaticSamplers = samplers;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


    ComPtr<ID3DBlob> signature;

    ComPtr<ID3DBlob> error;


    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signature.GetAddressOf(),
        error.GetAddressOf()
    );


    if (FAILED(hr))
    {
        if (error)
        {
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }

        return false;
    }


    hr = device->CreateRootSignature(
        0u,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(mDiffusionRootSignature.ReleaseAndGetAddressOf())
    );

    return SUCCEEDED(hr);
}

bool SubsurfaceScatteringPass::CreateTemporalRootSignature()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // t0 ～ t4
    //============================================================
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 5u;
    srvRange.BaseShaderRegister = 0u;
    srvRange.RegisterSpace = 0u;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    //============================================================
    // Root[0] : t0 ～ t4
    //============================================================
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;


    //============================================================
    // Root[1] : b0
    //============================================================
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0u;


    //============================================================
    // s0 Linear Clamp
    // History Color用
    //============================================================
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].ShaderRegister = 0u;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;

    //============================================================
    // s1 Point Clamp
    // Motion / Depth / Mask用
    //============================================================
    samplers[1] = samplers[0];
    samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[1].ShaderRegister = 1u;


    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = _countof(rootParameters);
    rootDesc.pParameters = rootParameters;
    rootDesc.NumStaticSamplers = _countof(samplers);
    rootDesc.pStaticSamplers = samplers;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


    ComPtr<ID3DBlob> signature;

    ComPtr<ID3DBlob> error;


    HRESULT hr = D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signature.GetAddressOf(),
        error.GetAddressOf()
    );


    if (FAILED(hr))
    {
        if (error)
        {
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }

        return false;
    }


    hr = device->CreateRootSignature(
        0u,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(mTemporalRootSignature.ReleaseAndGetAddressOf())
    );

    return SUCCEEDED(hr);
}

bool SubsurfaceScatteringPass::CreateDiffusionConstantBuffers()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    const UINT bufferSize = (sizeof(SSSDiffusionBuffer) + 255u) & ~255u;


    for (UINT index = 0u; index < 2u; ++index)
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;


        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bufferSize;
        desc.Height = 1u;
        desc.DepthOrArraySize = 1u;
        desc.MipLevels = 1u;
        desc.SampleDesc.Count = 1u;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(mDiffusionConstantBuffers[index].ReleaseAndGetAddressOf())
        );


        if (FAILED(hr))
        {
            return false;
        }


        hr = mDiffusionConstantBuffers[index]->Map(
            0u,
            nullptr,
            reinterpret_cast<void**>(&mDiffusionMapped[index])
        );


        if (FAILED(hr))
        {
            return false;
        }
    }

    return true;
}

bool SubsurfaceScatteringPass::CreateTemporalConstantBuffer()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    const UINT bufferSize = (sizeof(SSSTemporalBuffer) + 255u) & ~255u;


    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1u;
    resourceDesc.DepthOrArraySize = 1u;
    resourceDesc.MipLevels = 1u;
    resourceDesc.SampleDesc.Count = 1u;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(mTemporalConstantBuffer.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    hr = mTemporalConstantBuffer->Map(
        0u,
        nullptr,
        reinterpret_cast<void**>(&mTemporalMapped)
    );


    return SUCCEEDED(hr);
}

bool SubsurfaceScatteringPass::CreateDiffusionPipelineStates()
{
    if (!CreateDiffusionPipelineState(
        Shader::ePixelShader::PS_SSSDiffusionBlur,
        mDiffusionBlurPSO
    ))
    {
        return false;
    }

    if (!CreateDiffusionPipelineState(
        Shader::ePixelShader::PS_SSSDiffusionComposite,
        mDiffusionCompositePSO
    ))
    {
        return false;
    }


    return true;
}

bool SubsurfaceScatteringPass::CreateDiffusionPipelineState(Shader::ePixelShader pixelShaderType, ComPtr<ID3D12PipelineState>& outputPSO)
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr || !mDiffusionRootSignature)
    {
        return false;
    }


    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        }
    };


    auto* vs = Shader::getVShader(Shader::eVertexShader::VS_Copy);
    auto* ps = Shader::getPShader(pixelShaderType);


    if (vs == nullptr || ps == nullptr)
    {
        return false;
    }


    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = mDiffusionRootSignature.Get();


    desc.InputLayout =
    {
        layout,
        _countof(layout)
    };


    desc.VS =
    {
        vs->getCode(),
        static_cast<SIZE_T>(vs->getLength())
    };

    desc.PS =
    {
        ps->getCode(),
        static_cast<SIZE_T>(ps->getLength())
    };

    //============================================================
    // Fullscreen
    //============================================================
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.NumRenderTargets = 1u;
    desc.RTVFormats[0] = RenderFormat::SceneHDR;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.SampleDesc.Count = 1u;


    const HRESULT hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(outputPSO.ReleaseAndGetAddressOf())
    );


    return SUCCEEDED(hr);
}

bool SubsurfaceScatteringPass::CreateTemporalPipelineState()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr || !mTemporalRootSignature)
    {
        return false;
    }


    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },

        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        }
    };


    auto* vertexShader = Shader::getVShader(Shader::eVertexShader::VS_Copy);
    auto* pixelShader = Shader::getPShader(Shader::ePixelShader::PS_SSSTemporal);


    if (vertexShader == nullptr || pixelShader == nullptr)
    {
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = mTemporalRootSignature.Get();

    desc.InputLayout =
    {
        layout,
        _countof(layout)
    };


    desc.VS =
    {
        vertexShader->getCode(),
        static_cast<SIZE_T>(vertexShader->getLength())
    };


    desc.PS =
    {
        pixelShader->getCode(),
        static_cast<SIZE_T>(pixelShader->getLength())
    };

    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.NumRenderTargets = 1u;
    desc.RTVFormats[0] = RenderFormat::SceneHDR;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.SampleDesc.Count = 1u;


    const HRESULT hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(mTemporalPSO.ReleaseAndGetAddressOf())
    );


    return SUCCEEDED(hr);
}

void SubsurfaceScatteringPass::UpdateDebugSRV()
{
    if (mDebugView == DebugView::None || !mDebugSRVHeap)
    {
        return;
    }


    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return;
    }


    //============================================================
    // 表示対象
    //============================================================
    ID3D12Resource* resource = nullptr;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    bool replicateRedChannel = false;


    switch (mDebugView)
    {
        //============================================================
        // SSS Mask
        //============================================================
    case DebugView::Mask:

        resource = mMaskResource.Get();

        format = DXGI_FORMAT_R16_FLOAT;

        replicateRedChannel = true;

        break;


        //============================================================
        // Diffusion Source
        //============================================================
    case DebugView::DiffusionSource:

        resource = mDiffusionSourceResource.Get();

        format = RenderFormat::SceneHDR;

        break;


        //============================================================
        // Material Profile
        //============================================================
    case DebugView::MaterialProfile:

        resource = mDiffusionProfileResource.Get();

        format = DXGI_FORMAT_R16G16B16A16_FLOAT;

        break;


        //============================================================
        // Horizontal
        //============================================================
    case DebugView::HorizontalBlur:

        resource = mBlurResources[0].Get();

        format = RenderFormat::SceneHDR;

        break;


        //============================================================
        // Vertical
        //============================================================
    case DebugView::VerticalBlur:

        resource = mBlurResources[1].Get();

        format = RenderFormat::SceneHDR;

        break;


        //============================================================
        // Temporal
        //============================================================
    case DebugView::TemporalResolved:

        if (mTemporalEnabled && mTemporalHistoryValid)
        {
            resource = mTemporalHistoryResources[mTemporalReadIndex].Get();
        }
        else
        {
            //================================================
            // Temporal OFF / History未作成なら
            // Vertical Blurを表示する
            //================================================
            resource = mBlurResources[1].Get();
        }

        format = RenderFormat::SceneHDR;

        break;


    default:

        return;
    }


    if (resource == nullptr)
    {
        return;
    }


    //============================================================
    // SRV
    //============================================================
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0u;
    srvDesc.Texture2D.MipLevels = 1u;


    //============================================================
    // MaskだけRをRGBへ展開する
    //============================================================
    if (replicateRedChannel)
    {
        srvDesc.Shader4ComponentMapping =
            D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
                D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
                D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
                D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
                D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1
            );
    }
    else
    {
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    }


    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        mDebugSRVHeap->GetCPUDescriptorHandleForHeapStart();


    //============================================================
    // t0
    //============================================================
    device->CreateShaderResourceView(
        resource,
        &srvDesc,
        handle
    );


    //============================================================
    // t1
    //============================================================
    handle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize);


    device->CreateShaderResourceView(
        resource,
        &srvDesc,
        handle
    );
}

void SubsurfaceScatteringPass::UpdateDiffusionConstantBuffers()
{
    //============================================================
    // Current VP
    //============================================================
    const XMMATRIX viewProjection = *vnCamera::getScreen();


    const XMMATRIX inverseViewProjection =
        XMMatrixInverse(
            nullptr,
            viewProjection
        );


    XMFLOAT4 cameraPosition = {};

    XMStoreFloat4(
        &cameraPosition,
        *vnCamera::getPosition()
    );


    for (UINT index = 0u; index < 2u; ++index)
    {
        if (mDiffusionMapped[index] == nullptr)
        {
            continue;
        }


        SSSDiffusionBuffer& buffer = *mDiffusionMapped[index];


        //========================================================
        // HLSL mul(vector, matrix)
        //========================================================
        XMStoreFloat4x4(&buffer.InverseViewProjection, XMMatrixTranspose(inverseViewProjection));


        buffer.CameraPosition = cameraPosition;

        //============================================================
        // Working Resolution
        //============================================================
        const float workWidth = static_cast<float>((std::max)(mWorkWidth, 1u));
        const float workHeight = static_cast<float>((std::max)(mWorkHeight, 1u));


        buffer.ScreenParameters =
            XMFLOAT4(
                workWidth,
                workHeight,
                1.0f / workWidth,
                1.0f / workHeight
            );


        //============================================================
        // Full Resolution
        //============================================================
        const float fullWidth = static_cast<float>((std::max)(mWidth, 1u));
        const float fullHeight = static_cast<float>((std::max)(mHeight, 1u));


        buffer.FullScreenParameters =
            XMFLOAT4(
                fullWidth,
                fullHeight,
                1.0f / fullWidth,
                1.0f / fullHeight
            );


        //========================================================
        // Horizontal / Vertical
        //========================================================
        if (index == 0u)
        {
            buffer.BlurParameters =
                XMFLOAT4(
                    1.0f,
                    0.0f,
                    static_cast<float>(mBlurRadius),
                    mDepthFalloff
                );
        }
        else
        {
            buffer.BlurParameters =
                XMFLOAT4(
                    0.0f,
                    1.0f,
                    static_cast<float>(mBlurRadius),
                    mDepthFalloff
                );
        }


        buffer.DiffusionParameters =
            XMFLOAT4(
                mDiffusionStrength,
                mMaskThreshold,
                0.999999f,
                mWorldSpaceRadius
            );


        buffer.ScatteringProfile =
            XMFLOAT4(
                mScatteringProfile.x,
                mScatteringProfile.y,
                mScatteringProfile.z,
                mSampleQuality
            );
    }
}

void SubsurfaceScatteringPass::UpdateTemporalConstantBuffer()
{
    if (mTemporalMapped == nullptr)
    {
        return;
    }

    //============================================================
    // Current VP
    //============================================================
    const XMMATRIX viewProjection = *vnCamera::getScreen();


    const XMMATRIX inverseViewProjection =
        XMMatrixInverse(
            nullptr,
            viewProjection
        );


    XMStoreFloat4x4(
        &mTemporalMapped->InverseViewProjection,
        XMMatrixTranspose(inverseViewProjection)
    );


    //============================================================
    // Camera Position
    //============================================================
    XMStoreFloat4(
        &mTemporalMapped->CameraPosition,
        *vnCamera::getPosition()
    );

    const float width = static_cast<float>((std::max)(mWorkWidth, 1u));

    const float height = static_cast<float>((std::max)(mWorkHeight, 1u));


    mTemporalMapped->ScreenParameters =
        XMFLOAT4(
            width,
            height,
            1.0f / width,
            1.0f / height
        );


    //============================================================
    // Temporal Parameters
    //============================================================
    mTemporalMapped->TemporalParameters =
        XMFLOAT4(
            mTemporalHistoryWeight,
            mTemporalDepthThreshold,
            mTemporalMotionResponse,
            mTemporalHistoryValid ? 1.0f : 0.0f
        );
}

void SubsurfaceScatteringPass::TransitionBlurResource(UINT index, D3D12_RESOURCE_STATES newState)
{
    if (index >= 2u ||
        !mBlurResources[index] ||
        mBlurStates[index] == newState)
    {
        return;
    }


    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mBlurResources[index].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mBlurStates[index];
    barrier.Transition.StateAfter = newState;

    Direct3D::getCommandList()->ResourceBarrier(
        1u,
        &barrier
    );


    mBlurStates[index] = newState;
}

void SubsurfaceScatteringPass::TransitionDiffusionSource(D3D12_RESOURCE_STATES newState)
{
    if (!mDiffusionSourceResource || mDiffusionSourceState == newState)
    {
        return;
    }


    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mDiffusionSourceResource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mDiffusionSourceState;
    barrier.Transition.StateAfter = newState;

    Direct3D::getCommandList()->ResourceBarrier(
        1u,
        &barrier
    );

    mDiffusionSourceState = newState;
}

void SubsurfaceScatteringPass::TransitionDiffusionProfile(D3D12_RESOURCE_STATES newState)
{
    if (!mDiffusionProfileResource || mDiffusionProfileState == newState)
    {
        return;
    }


    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mDiffusionProfileResource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mDiffusionProfileState;
    barrier.Transition.StateAfter = newState;


    Direct3D::getCommandList()->ResourceBarrier(
        1u,
        &barrier
    );

    mDiffusionProfileState = newState;
}

void SubsurfaceScatteringPass::TransitionTemporalHistory(UINT index, D3D12_RESOURCE_STATES newState)
{
    if (index >= 2u || !mTemporalHistoryResources[index] || mTemporalHistoryStates[index] == newState)
    {
        return;
    }


    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mTemporalHistoryResources[index].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mTemporalHistoryStates[index];
    barrier.Transition.StateAfter = newState;


    Direct3D::getCommandList()->ResourceBarrier(
        1u,
        &barrier
    );

    mTemporalHistoryStates[index] = newState;
}