#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "ssgiPass.h"

bool SSGIPass::Initialize(UINT width, UINT height)
{
    mWidth = (std::max)(width, 1u);
    mHeight = (std::max)(height, 1u);

    mWorkWidth = (std::max)(static_cast<UINT>(std::ceil(static_cast<float>(mWidth) * mResolutionScale)), 1u);
    mWorkHeight = (std::max)(static_cast<UINT>(std::ceil(static_cast<float>(mHeight) * mResolutionScale)), 1u);

    ID3D12Device* device = Direct3D::getDevice();

    if (device == nullptr)
    {
        return false;
    }


    mSRVDescriptorSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


    //============================================================
    // GI Geometry Surface
    //============================================================
    if (!mSurfacePass.Initialize(mWidth, mHeight))
    {
        Terminate();
        return false;
    }


    //============================================================
    // Raw GI
    //============================================================
    if (!CreateResultResource())
    {
        Terminate();
        return false;
    }

    if (!CreateRootSignature())
    {
        Terminate();
        return false;
    }

    if (!CreateConstantBuffer())
    {
        Terminate();
        return false;
    }

    if (!CreatePipelineState())
    {
        Terminate();
        return false;
    }

    //============================================================
    // SSGI Spatial Filter
    //============================================================
    if (!CreateFilterResources())
    {
        Terminate();
        return false;
    }

    if (!CreateFilterRootSignature())
    {
        Terminate();
        return false;
    }

    if (!CreateFilterConstantBuffers())
    {
        Terminate();
        return false;
    }

    if (!CreateFilterPipelineState())
    {
        Terminate();
        return false;
    }

    //============================================================
    // SSGI Temporal
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

    ResetTemporalHistory();

    //============================================================
    // Full Resolution Upsampling
    //============================================================
    if (!CreateUpsampleResources())
    {
        Terminate();
        return false;
    }

    if (!CreateUpsampleRootSignature())
    {
        Terminate();
        return false;
    }

    if (!CreateUpsampleConstantBuffer())
    {
        Terminate();
        return false;
    }

    if (!CreateUpsamplePipelineState())
    {
        Terminate();
        return false;
    }

    //============================================================
    // SSGI HDR Composite
    //============================================================
    if (!CreateCompositeResources())
    {
        Terminate();
        return false;
    }

    if (!CreateCompositeRootSignature())
    {
        Terminate();
        return false;
    }

    if (!CreateCompositePipelineState())
    {
        Terminate();
        return false;
    }

    //============================================================
    // SSGI Debug
    //============================================================
    if (!CreateDebugResources())
    {
        Terminate();
        return false;
    }

    return true;
}

void SSGIPass::Terminate()
{
    //============================================================
    // Constant Buffer
    //============================================================
    if (mConstantBuffer && mMappedConstantBuffer != nullptr)
    {
        mConstantBuffer->Unmap(
            0u,
            nullptr
        );


        mMappedConstantBuffer = nullptr;
    }

    for (UINT index = 0u; index < 2u; ++index)
    {
        if ( mFilterConstantBuffers[index] && mFilterMapped[index] != nullptr )
        {
            mFilterConstantBuffers[index]-> Unmap(
                    0u,
                    nullptr
                );

            mFilterMapped[index] = nullptr;
        }


        mFilterConstantBuffers[index].Reset();
        mFilterResources[index].Reset();
        mFilterStates[index] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        mFilterRTV[index] = {};
    }

    if (mTemporalConstantBuffer &&mTemporalMappedBuffer != nullptr)
    {
        mTemporalConstantBuffer->Unmap(
                0u,
                nullptr
            );

        mTemporalMappedBuffer = nullptr;
    }

    if (mUpsampleConstantBuffer && mUpsampleMappedBuffer != nullptr)
    {
        mUpsampleConstantBuffer->Unmap(
            0u,
            nullptr
        );

        mUpsampleMappedBuffer = nullptr;
    }


    mConstantBuffer.Reset();
    mTemporalConstantBuffer.Reset();
    mUpsampleConstantBuffer.Reset();

    //============================================================
    // Pipeline
    //============================================================
    mRayMarchPSO.Reset();
    mFilterPSO.Reset();
    mTemporalPSO.Reset();
    mUpsamplePSO.Reset();
    mCompositePSO.Reset();

    mRootSignature.Reset();
    mFilterRootSignature.Reset();
    mTemporalRootSignature.Reset();
    mUpsampleRootSignature.Reset();
    mCompositeRootSignature.Reset();

    //============================================================
    // Descriptor
    //============================================================
    mRayMarchSRVHeap.Reset();
    mResultSRVHeap.Reset();
    mResultRTVHeap.Reset();
    mFilterSRVHeap.Reset();
    mFilterRTVHeap.Reset();
    mTemporalSRVHeap.Reset();
    mTemporalRTVHeap.Reset();
    mUpsampleSRVHeap.Reset();
    mUpsampleResultSRVHeap.Reset();
    mUpsampleRTVHeap.Reset();
    mCompositeSRVHeap.Reset();
    mDebugSRVHeap.Reset();

    //============================================================
    // Raw GI
    //============================================================
    mResultResource.Reset();

    mResultState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    mResultRTV = {};

    //============================================================
    // Temporal Resources
    //============================================================
    for (UINT index = 0u; index < 2u; ++index)
    {
        mTemporalGIResources[index].Reset();
        mTemporalGeometryResources[index].Reset();
        mTemporalGIStates[index] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        mTemporalGeometryStates[index] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        mTemporalGIRTV[index] = {};
        mTemporalGeometryRTV[index] = {};
    }

    //============================================================
    // Resource
    //============================================================
    mUpsampleResource.Reset();

    mUpsampleState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    mUpsampleRTV = {};

    //============================================================
    // Surface Pass
    //============================================================
    mSurfacePass.Terminate();

    //============================================================
    // State
    //============================================================
    mWidth = 0u;
    mHeight = 0u;
    mWorkWidth = 0u;
    mWorkHeight = 0u;
    mTemporalReadIndex = 0u;
    mTemporalWriteIndex = 1u;
    mTemporalHistoryValid = false;
    mFrameIndex = 0u;
    mSRVDescriptorSize = 0u;
    mDebugView = DebugView::None;
}

void SSGIPass::Generate(
    ID3D12Resource* inputSceneResource,
    ID3D12Resource* depthResource,
    ID3D12Resource* motionVectorResource,
    const XMMATRIX& previousViewProjection
)
{
    if (inputSceneResource == nullptr ||
        depthResource == nullptr ||
        !mResultResource ||
        !mRayMarchSRVHeap ||
        !mRootSignature ||
        !mRayMarchPSO ||
        !mConstantBuffer)
    {
        return;
    }


    ID3D12Device* device = Direct3D::getDevice();


    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


    if (device == nullptr || commandList == nullptr)
    {
        return;
    }


    //============================================================
    // GI Surface Buffer生成
    // Depth EQUALでGeometryを再描画する
    //============================================================
    mSurfacePass.Generate();


    //============================================================
    // Main DepthをRay Marchから読む
    //============================================================
    Direct3D::TransitionMainDepthToSRV();


    //============================================================
    // Raw GI出力先
    //============================================================
    TransitionResult(D3D12_RESOURCE_STATE_RENDER_TARGET);


    const float clearColor[4] =
    {
        0.0f,
        0.0f,
        0.0f,
        0.0f
    };


    commandList->ClearRenderTargetView(
        mResultRTV,
        clearColor,
        0u,
        nullptr
    );

    //============================================================
    // Constant Buffer
    //============================================================
    UpdateConstantBuffer();


    //============================================================
    // SRV Helper
    //============================================================
    D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
        mRayMarchSRVHeap->GetCPUDescriptorHandleForHeapStart();


    auto getHandle = [&](UINT index)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;

            handle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize) * index;

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
    // t0 Main HDR Scene
    //============================================================
    createSRV(
        inputSceneResource,
        RenderFormat::SceneHDR,
        0u
    );


    //============================================================
    // t1 Main Depth
    //============================================================
    createSRV(
        depthResource,
        DXGI_FORMAT_R32_FLOAT,
        1u
    );


    //============================================================
    // t2 Normal + Roughness
    //============================================================
    createSRV(
        mSurfacePass.GetSurfaceResource(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        2u
    );


    //============================================================
    // t3 BaseColor + Metallic
    //============================================================
    createSRV(
        mSurfacePass.GetMaterialResource(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        3u
    );


    //============================================================
    // t4 Previous Frame Full Resolution SSGI
    //============================================================
    createSRV(
        mUpsampleResource.Get(),
        RenderFormat::SceneHDR,
        4u
    );

    //============================================================
    // Full Resolution
    //============================================================
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
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
    commandList->OMSetRenderTargets(
        1u,
        &mResultRTV,
        FALSE,
        nullptr
    );


    commandList->SetGraphicsRootSignature(
        mRootSignature.Get()
    );


    commandList->SetPipelineState(
        mRayMarchPSO.Get()
    );


    //============================================================
    // Heap
    //============================================================
    ID3D12DescriptorHeap* heaps[] =
    {
        mRayMarchSRVHeap.Get()
    };


    commandList->SetDescriptorHeaps(
        1u,
        heaps
    );


    commandList->SetGraphicsRootDescriptorTable(
        0u,
        mRayMarchSRVHeap->
        GetGPUDescriptorHandleForHeapStart()
    );


    commandList->SetGraphicsRootConstantBufferView(
        1u,
        mConstantBuffer->
        GetGPUVirtualAddress()
    );


    //============================================================
    // Full Screen Quad
    //============================================================
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
    );


    commandList->IASetVertexBuffers(
        0u,
        1u,
        Direct3D::getScreenVBView()
    );


    commandList->DrawInstanced(
        4u,
        1u,
        0u,
        0u
    );


    //============================================================
    // Debug / 次Passから読める状態へ
    //============================================================
    TransitionResult(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    //============================================================
    // Spatial Bilateral Filter
    //============================================================
    ApplySpatialFilter(depthResource);

    //============================================================
    // Temporal Resolve
    //============================================================
    bool temporalResolvedThisFrame = false;


    if (mTemporalEnabled && motionVectorResource != nullptr)
    {
        ID3D12Resource* temporalResult =
            ResolveTemporal(
                motionVectorResource,
                depthResource,
                previousViewProjection
            );


        temporalResolvedThisFrame =
            temporalResult != nullptr &&
            temporalResult !=
            mFilterResources[1].Get();
    }


    //============================================================
    // Temporal History更新
    //============================================================
    if (temporalResolvedThisFrame)
    {
        CommitTemporalHistory();
    }


    //============================================================
    // Half Resolution Final Result
    // Temporalが有効ならTemporal
    // 無効ならSpatial Filter
    //============================================================
    ID3D12Resource* halfResolutionGI = GetHalfResolutionResultResource();


    //============================================================
    // Half Resolution
    //        ↓
    // Depth / Normal-aware Upsampling
    //        ↓
    // Full Resolution
    //============================================================
    ApplyUpsampling(
        halfResolutionGI,
        depthResource
    );

    mMultiBounceHistoryValid = true;

    //============================================================
    // Debug Descriptor更新
    //============================================================
    UpdateDebugSRV();

    //============================================================
    // 次Frame用Ray Pattern
    //============================================================
    ++mFrameIndex;
}

void SSGIPass::ApplyCompositeToRT(ID3D12Resource* sceneResource,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
    UINT width,
    UINT height)
{
    if (sceneResource == nullptr ||
        !mUpsampleResource ||
        !mCompositeSRVHeap ||
        !mCompositeRootSignature ||
        !mCompositePSO)
    {
        return;
    }


    ID3D12Device* device = Direct3D::getDevice();

    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


    if (device == nullptr || commandList == nullptr)
    {
        return;
    }


    //============================================================
    // Full Resolution SSGIはSRV状態にしておく。
    //============================================================
    TransitionUpsample(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


    //============================================================
    // Descriptor Helper
    //============================================================
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        mCompositeSRVHeap->GetCPUDescriptorHandleForHeapStart();


    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = RenderFormat::SceneHDR;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0u;
    srvDesc.Texture2D.MipLevels = 1u;


    //============================================================
    // t0:
    // HDR Scene
    //============================================================
    device->CreateShaderResourceView(
        sceneResource,
        &srvDesc,
        handle
    );


    //============================================================
    // t1へ
    //============================================================
    handle.ptr +=
        static_cast<SIZE_T>(mSRVDescriptorSize);


    //============================================================
    // t1:
    // Full Resolution SSGI
    //============================================================
    device->CreateShaderResourceView(
        mUpsampleResource.Get(),
        &srvDesc,
        handle
    );


    //============================================================
    // Full Resolution Viewport
    //============================================================
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;


    D3D12_RECT scissor =
    {
        0,
        0,
        static_cast<LONG>(width),
        static_cast<LONG>(height)
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
    commandList->OMSetRenderTargets(
        1u,
        &outputRTV,
        FALSE,
        nullptr
    );


    //============================================================
    // Pipeline
    //============================================================
    commandList->SetGraphicsRootSignature(
        mCompositeRootSignature.Get()
    );


    commandList->SetPipelineState(
        mCompositePSO.Get()
    );


    //============================================================
    // Descriptor Heap
    //============================================================
    ID3D12DescriptorHeap* heaps[] =
    {
        mCompositeSRVHeap.Get()
    };


    commandList->SetDescriptorHeaps(
        1u,
        heaps
    );


    commandList->SetGraphicsRootDescriptorTable(
        0u,

        mCompositeSRVHeap->GetGPUDescriptorHandleForHeapStart()
    );


    //============================================================
    // Full Screen Quad
    //============================================================
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
    );


    commandList->IASetVertexBuffers(
        0u,
        1u,
        Direct3D::getScreenVBView()
    );


    commandList->DrawInstanced(
        4u,
        1u,
        0u,
        0u
    );
}

void SSGIPass::ApplySpatialFilter(ID3D12Resource* depthResource)
{
    if (depthResource == nullptr ||
        !mResultResource ||
        !mFilterResources[0] ||
        !mFilterResources[1] ||
        !mFilterSRVHeap ||
        !mFilterRootSignature ||
        !mFilterPSO)
    {
        return;
    }


    ID3D12Device* device = Direct3D::getDevice();

    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


    if (device == nullptr || commandList == nullptr)
    {
        return;
    }


    //============================================================
    // Raw GIはSRV
    //============================================================
    TransitionResult(
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );


    //============================================================
    // DepthもSRV
    //============================================================
    Direct3D::TransitionMainDepthToSRV();


    //============================================================
    // Constant Buffer
    //============================================================
    UpdateFilterConstantBuffers();


    const D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
        mFilterSRVHeap->GetCPUDescriptorHandleForHeapStart();


    auto getCPUHandle =
        [&](UINT index)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;


            handle.ptr +=
                static_cast<SIZE_T>(mSRVDescriptorSize) * index;


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
                getCPUHandle(index)
            );
        };


    //============================================================
    // Horizontal
    //
    // t0 Raw GI
    // t1 Depth
    // t2 Normal
    //============================================================
    createSRV(
        mResultResource.Get(),
        RenderFormat::SceneHDR,
        0u
    );


    createSRV(
        depthResource,
        DXGI_FORMAT_R32_FLOAT,
        1u
    );


    createSRV(
        mSurfacePass.GetSurfaceResource(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        2u
    );


    //============================================================
    // Vertical
    //
    // t0 Horizontal Result
    // t1 Depth
    // t2 Normal
    //============================================================
    createSRV(
        mFilterResources[0].Get(),
        RenderFormat::SceneHDR,
        3u
    );


    createSRV(
        depthResource,
        DXGI_FORMAT_R32_FLOAT,
        4u
    );


    createSRV(
        mSurfacePass.GetSurfaceResource(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        5u
    );


    //============================================================
    // GPU Descriptor Handle
    //============================================================
    auto getGPUHandle = [&](UINT index)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE handle =
                mFilterSRVHeap->GetGPUDescriptorHandleForHeapStart();


            handle.ptr += static_cast<UINT64>(mSRVDescriptorSize) * index;


            return handle;
        };


    //============================================================
    // Half Resolution Viewport
    //============================================================
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


    commandList->SetGraphicsRootSignature(
        mFilterRootSignature.Get()
    );


    ID3D12DescriptorHeap* heaps[] =
    {
        mFilterSRVHeap.Get()
    };


    commandList->SetDescriptorHeaps(
        1u,
        heaps
    );


    commandList->SetPipelineState(
        mFilterPSO.Get()
    );


    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
    );


    commandList->IASetVertexBuffers(
        0u,
        1u,
        Direct3D::getScreenVBView()
    );


    //============================================================
    // Horizontal
    //============================================================
    TransitionFilterResource(
        0u,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );


    commandList->OMSetRenderTargets(
        1u,
        &mFilterRTV[0],
        FALSE,
        nullptr
    );


    commandList->SetGraphicsRootDescriptorTable(
        0u,
        getGPUHandle(0u)
    );


    commandList->SetGraphicsRootConstantBufferView(
        1u,
        mFilterConstantBuffers[0]->GetGPUVirtualAddress()
    );


    commandList->DrawInstanced(
        4u,
        1u,
        0u,
        0u
    );


    TransitionFilterResource(
        0u,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );


    //============================================================
    // Vertical
    //============================================================
    TransitionFilterResource(
        1u,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );


    commandList->OMSetRenderTargets(
        1u,
        &mFilterRTV[1],
        FALSE,
        nullptr
    );


    commandList->SetGraphicsRootDescriptorTable(
        0u,
        getGPUHandle(3u)
    );


    commandList->SetGraphicsRootConstantBufferView(
        1u,
        mFilterConstantBuffers[1]->GetGPUVirtualAddress()
    );


    commandList->DrawInstanced(
        4u,
        1u,
        0u,
        0u
    );


    TransitionFilterResource(
        1u,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
}

ID3D12Resource* SSGIPass::ResolveTemporal(ID3D12Resource* motionVectorResource, ID3D12Resource* depthResource, const XMMATRIX& previousViewProjection)
{
    //============================================================
    // Temporal OFFの場合はSpatial Filter結果をそのまま返す
    //============================================================
    if (!mTemporalEnabled)
    {
        return mFilterResources[1].Get();
    }


    if (motionVectorResource == nullptr ||
        depthResource == nullptr ||
        !mFilterResources[1] ||
        !mTemporalPSO ||
        !mTemporalRootSignature ||
        !mTemporalSRVHeap)
    {
        return mFilterResources[1].Get();
    }


    ID3D12Device* device = Direct3D::getDevice();

    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


    if (device == nullptr || commandList == nullptr)
    {
        return mFilterResources[1].Get();
    }


    //============================================================
    // Previous HistoryはSRV
    //============================================================
    TransitionTemporalGI(
        mTemporalReadIndex,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );


    TransitionTemporalGeometry(
        mTemporalReadIndex,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );


    //============================================================
    // Current History OutputはRT
    //============================================================
    TransitionTemporalGI(
        mTemporalWriteIndex,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );


    TransitionTemporalGeometry(
        mTemporalWriteIndex,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );


    Direct3D::TransitionMainDepthToSRV();


    UpdateTemporalConstantBuffer(
        previousViewProjection
    );


    //============================================================
    // SRV作成
    //============================================================
    D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
        mTemporalSRVHeap->GetCPUDescriptorHandleForHeapStart();


    auto getHandle = [&](UINT index)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;

            handle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize) * index;

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
    // t0 Current Filtered GI
    //============================================================
    createSRV(
        mFilterResources[1].Get(),
        RenderFormat::SceneHDR,
        0u
    );


    //============================================================
    // t1 Previous GI
    //============================================================
    createSRV(
        mTemporalGIResources[mTemporalReadIndex].Get(),
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
    // t3 Current Depth
    //============================================================
    createSRV(
        depthResource,
        DXGI_FORMAT_R32_FLOAT,
        3u
    );


    //============================================================
    // t4 Current Normal
    //============================================================
    createSRV(
        mSurfacePass.GetSurfaceResource(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        4u
    );


    //============================================================
    // t5 Previous Geometry
    //============================================================
    createSRV(
        mTemporalGeometryResources[mTemporalReadIndex].Get(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        5u
    );


    //============================================================
    // Half Resolution
    //============================================================
    D3D12_VIEWPORT viewport =
    {
        0.0f,
        0.0f,
        static_cast<float>(mWorkWidth),
        static_cast<float>(mWorkHeight),
        0.0f,
        1.0f
    };


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
    // MRT
    //============================================================
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2] =
    {
        mTemporalGIRTV[mTemporalWriteIndex],
        mTemporalGeometryRTV[mTemporalWriteIndex]
    };


    commandList->OMSetRenderTargets(
        2u,
        rtvs,
        FALSE,
        nullptr
    );


    commandList->SetGraphicsRootSignature(
        mTemporalRootSignature.Get()
    );


    commandList->SetPipelineState(
        mTemporalPSO.Get()
    );


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
        mTemporalSRVHeap->
        GetGPUDescriptorHandleForHeapStart()
    );


    commandList->SetGraphicsRootConstantBufferView(
        1u,
        mTemporalConstantBuffer->
        GetGPUVirtualAddress()
    );


    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
    );


    commandList->IASetVertexBuffers(
        0u,
        1u,
        Direct3D::getScreenVBView()
    );


    commandList->DrawInstanced(
        4u,
        1u,
        0u,
        0u
    );


    //============================================================
    // 次Frameと後続Passから読めるように戻す。
    //============================================================
    TransitionTemporalGI(
        mTemporalWriteIndex,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );


    TransitionTemporalGeometry(
        mTemporalWriteIndex,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );


    return mTemporalGIResources[mTemporalWriteIndex].Get();
}

void SSGIPass::ApplyUpsampling(ID3D12Resource* sourceGI, ID3D12Resource* depthResource)
{
    if (sourceGI == nullptr ||
        depthResource == nullptr ||
        !mUpsampleResource ||
        !mUpsampleSRVHeap ||
        !mUpsampleRootSignature ||
        !mUpsamplePSO ||
        !mUpsampleConstantBuffer)
    {
        return;
    }

    ID3D12Device* device = Direct3D::getDevice();

    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


    if (device == nullptr || commandList == nullptr)
    {
        return;
    }


    //============================================================
    // Full Resolution Depthを読む
    //============================================================
    Direct3D::TransitionMainDepthToSRV();


    //============================================================
    // Full Resolution Output
    //============================================================
    TransitionUpsample(D3D12_RESOURCE_STATE_RENDER_TARGET);


    const float clearColor[4] =
    {
        0.0f,
        0.0f,
        0.0f,
        0.0f
    };


    commandList->ClearRenderTargetView(
        mUpsampleRTV,
        clearColor,
        0u,
        nullptr
    );


    UpdateUpsampleConstantBuffer();


    //============================================================
    // Descriptor Helper
    //============================================================
    const D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
        mUpsampleSRVHeap->GetCPUDescriptorHandleForHeapStart();


    auto getHandle = [&](UINT index)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;

            handle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize) * index;

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
    // t0 Half Resolution Final GI
    //============================================================
    createSRV(
        sourceGI,
        RenderFormat::SceneHDR,
        0u
    );


    //============================================================
    // t1 Full Resolution Depth
    //============================================================
    createSRV(
        depthResource,
        DXGI_FORMAT_R32_FLOAT,
        1u
    );


    //============================================================
    // t2 Full Resolution Normal
    //============================================================
    createSRV(
        mSurfacePass.GetSurfaceResource(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        2u
    );


    //============================================================
    // Full Resolution Viewport
    //============================================================
    D3D12_VIEWPORT viewport =
    {
        0.0f,
        0.0f,
        static_cast<float>(mWidth),
        static_cast<float>(mHeight),
        0.0f,
        1.0f
    };


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


    commandList->OMSetRenderTargets(
        1u,
        &mUpsampleRTV,
        FALSE,
        nullptr
    );


    commandList->SetGraphicsRootSignature(
        mUpsampleRootSignature.Get()
    );


    commandList->SetPipelineState(
        mUpsamplePSO.Get()
    );


    ID3D12DescriptorHeap* heaps[] =
    {
        mUpsampleSRVHeap.Get()
    };


    commandList->SetDescriptorHeaps(
        1u,
        heaps
    );


    commandList->SetGraphicsRootDescriptorTable(
        0u,
        mUpsampleSRVHeap->GetGPUDescriptorHandleForHeapStart()
    );


    commandList->SetGraphicsRootConstantBufferView(
        1u,
        mUpsampleConstantBuffer->GetGPUVirtualAddress()
    );


    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
    );


    commandList->IASetVertexBuffers(
        0u,
        1u,
        Direct3D::getScreenVBView()
    );


    commandList->DrawInstanced(
        4u,
        1u,
        0u,
        0u
    );


    //============================================================
    // Composite / Debugから読めるようにする。
    //============================================================
    TransitionUpsample(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

ID3D12Resource* SSGIPass::GetHalfResolutionResultResource() const
{
    //============================================================
    // Temporal Result。
    //============================================================
    if (mTemporalEnabled &&
        mTemporalHistoryValid &&
        mTemporalGIResources[mTemporalReadIndex])
    {
        return mTemporalGIResources[mTemporalReadIndex].Get();
    }

    //============================================================
    // Spatial Filter Result
    //============================================================
    if (mFilterResources[1])
    {
        return mFilterResources[1].Get();
    }

    //============================================================
    // Fallback
    //============================================================
    return mResultResource.Get();
}

void SSGIPass::SetRayCount(int count)
{
    mRayCount = (std::clamp)(count, 1, 8);
}

void SSGIPass::SetStepCount(int count)
{
    mStepCount = (std::clamp)(count, 1, 32);
}

void SSGIPass::SetMaxDistance(float distance)
{
    mMaxDistance = (std::max)(distance, 0.01f);
}

void SSGIPass::SetThickness(float thickness)
{
    mThickness = (std::max)(thickness, 0.001f);
}

void SSGIPass::SetNormalBias(float bias)
{
    mNormalBias = (std::max)(bias, 0.0f);
}

void SSGIPass::SetIntensity(float intensity)
{
    mIntensity = (std::max)(intensity, 0.0f);
}

void SSGIPass::SetMaxRadiance(float value)
{
    mMaxRadiance = (std::max)(value, 0.1f);
}

void SSGIPass::SetFilterRadius(int radius)
{
    mFilterRadius = (std::clamp)(radius, 1, 8);
}

void SSGIPass::SetFilterDepthFalloff(float falloff)
{
    mFilterDepthFalloff = (std::max)(falloff, 0.0f);
}

void SSGIPass::SetFilterNormalPower(float power)
{
    mFilterNormalPower = (std::max)(power, 1.0f);
}

void SSGIPass::SetFilterConfidenceMin(float value)
{
    mFilterConfidenceMin = (std::clamp)(value, 0.0f, 1.0f);
}

void SSGIPass::SetTemporalEnabled(bool enabled)
{
    if (mTemporalEnabled == enabled)
    {
        return;
    }

    mTemporalEnabled = enabled;

    //============================================================
    // ON/OFF切り替え時は古いHistoryを使用しない
    //============================================================
    ResetTemporalHistory();
}


void SSGIPass::SetTemporalHistoryWeight(float weight)
{
    mTemporalHistoryWeight = (std::clamp)(weight, 0.0f, 0.99f);
}


void SSGIPass::SetTemporalWorldPositionThreshold(float threshold)
{
    mTemporalWorldPositionThreshold = (std::max)(threshold, 0.001f);
}


void SSGIPass::SetTemporalNormalThreshold(float threshold)
{
    mTemporalNormalThreshold = (std::clamp)(threshold, -1.0f, 1.0f);
}


void SSGIPass::SetTemporalMotionResponse(float response)
{
    mTemporalMotionResponse = (std::max)(response, 0.0f);
}


void SSGIPass::SetTemporalClampExpand(float expand)
{
    mTemporalClampExpand = (std::max)(expand, 0.0f);
}

void SSGIPass::ResetTemporalHistory()
{
    mTemporalHistoryValid = false;
    mTemporalReadIndex = 0u;
    mTemporalWriteIndex = 1u;
    //============================================================
    // Ray PatternもReset
    //============================================================
    mFrameIndex = 0u;
    mMultiBounceHistoryValid = false;
}

void SSGIPass::SetUpsampleDepthFalloff(float falloff)
{
    mUpsampleDepthFalloff = (std::max)(falloff, 0.0f);
}

void SSGIPass::SetUpsampleNormalPower(float power)
{
    mUpsampleNormalPower = (std::max)(power, 1.0f);
}

void SSGIPass::SetUpsampleConfidenceMin(float value)
{
    mUpsampleConfidenceMin = (std::clamp)(value, 0.0f, 1.0f);
}

void SSGIPass::SetMultiBounceEnabled(bool enabled)
{
    mMultiBounceEnabled = enabled;
}

void SSGIPass::SetMultiBounceStrength(float strength)
{
    mMultiBounceStrength = (std::clamp)(strength, 0.0f, 0.5f);
}

void SSGIPass::SetDebugView(DebugView view)
{
    mDebugView = view;
    UpdateDebugSRV();
}

void SSGIPass::SetQuality(Quality quality)
{
    mQuality = quality;


    switch (quality)
    {
        //============================================================
        // Low
        // 軽量優先。
        // Temporal蓄積を強めに使って少ないRayを補う
        //============================================================
    case Quality::Low:

        SetRayCount(2);
        SetStepCount(8);
        SetFilterRadius(3);
        SetTemporalHistoryWeight(0.94f);
        SetTemporalMotionResponse(0.08f);


        break;


        //============================================================
        // Medium
        // 標準設定
        //============================================================
    case Quality::Medium:

        SetRayCount(4);
        SetStepCount(12);
        SetFilterRadius(4);
        SetTemporalHistoryWeight(0.90f);
        SetTemporalMotionResponse(0.05f);

        break;


        //============================================================
        // High
        // Ray / Stepを増やしてRaw GI自体の品質を高める
        // Temporal依存は少し下げる
        //============================================================
    case Quality::High:

        SetRayCount(8);
        SetStepCount(20);
        SetFilterRadius(5);
        SetTemporalHistoryWeight(0.86f);
        SetTemporalMotionResponse(0.04f);

        break;
    }

    ResetTemporalHistory();
}

void SSGIPass::UpdateDebugSRV()
{
    if (!mDebugSRVHeap)
    {
        return;
    }

    ID3D12Device* device = Direct3D::getDevice();

    if (device == nullptr)
    {
        return;
    }


    //============================================================
    // 表示するResourceを決める
    //============================================================
    ID3D12Resource* debugResource = nullptr;


    switch (mDebugView)
    {
    case DebugView::Raw:

        //========================================================
        // Ray March直後
        // Half Resolution
        //========================================================
        debugResource = mResultResource.Get();

        break;


    case DebugView::Filtered:

        //========================================================
        // Bilateral Filter後
        // Half Resolution
        //========================================================
        debugResource = mFilterResources[1].Get();

        break;


    case DebugView::Temporal:

        //========================================================
        // Temporal Resolve後
        // Temporal無効時やHistory未完成時は
        // Filtered結果へFallbackする
        //========================================================
        if (mTemporalEnabled &&
            mTemporalHistoryValid &&
            mTemporalGIResources[mTemporalReadIndex])
        {
            debugResource =
                mTemporalGIResources[mTemporalReadIndex].Get();
        }
        else
        {
            debugResource = mFilterResources[1].Get();
        }

        break;


    case DebugView::Upsampled:

        //========================================================
        // Full Resolution Result
        //========================================================
        debugResource = mUpsampleResource.Get();

        break;


    case DebugView::FinalComposite:

        //========================================================
        // FinalCompositeはPingPongRT側なので、
        // SSGIPassではDescriptorを作らない
        // PostProcessManager側で表示する
        //========================================================
        return;


    case DebugView::None:

    default:

        return;
    }


    if (debugResource == nullptr)
    {
        return;
    }


    //============================================================
    // SceneHDR形式
    //============================================================
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = RenderFormat::SceneHDR;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0u;
    srvDesc.Texture2D.MipLevels = 1u;

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        mDebugSRVHeap->GetCPUDescriptorHandleForHeapStart();


    //============================================================
    // t0
    //============================================================
    device->CreateShaderResourceView(
        debugResource,
        &srvDesc,
        handle
    );


    //============================================================
    // t1 Dummy
    // Screen Root Signatureがt0～t1を要求するため
    //============================================================
    handle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize);

    device->CreateShaderResourceView(
        debugResource,
        &srvDesc,
        handle
    );
}

bool SSGIPass::CreateResultResource()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // Default Heap
    //============================================================
    D3D12_HEAP_PROPERTIES heapProperties = {};

    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


    //============================================================
    // Raw SSGI Texture
    // RGB: Indirect Radiance
    // A: Hit Confidence
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


    //============================================================
    // GIなし = Black
    //============================================================
    D3D12_CLEAR_VALUE clearValue = {};

    clearValue.Format = RenderFormat::SceneHDR;

    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;


    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(mResultResource.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr) || !mResultResource)
    {
        return false;
    }


    mResultResource->SetName(L"SSGI Raw Result");


    mResultState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


    //============================================================
    // RTV Heap
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1u;


    hr = device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(mResultRTVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    mResultRTV = mResultRTVHeap->GetCPUDescriptorHandleForHeapStart();


    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};

    rtvDesc.Format = RenderFormat::SceneHDR;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


    device->CreateRenderTargetView(
        mResultResource.Get(),
        &rtvDesc,
        mResultRTV
    );


    //============================================================
    // Ray March Inputs
    //
    // [0] Scene
    // [1] Depth
    // [2] GI Surface
    // [3] GI Material
    // [4] Previous GI
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC rayHeapDesc = {};
    rayHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    rayHeapDesc.NumDescriptors = 5u;
    rayHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    hr = device->CreateDescriptorHeap(
        &rayHeapDesc,
        IID_PPV_ARGS(mRayMarchSRVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    //============================================================
    // Debug用 SRV Heap
    //
    // Screen Root Signature用に2 Descriptor
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC resultHeapDesc = {};
    resultHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    resultHeapDesc.NumDescriptors = 2u;
    resultHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    hr = device->CreateDescriptorHeap(
        &resultHeapDesc,
        IID_PPV_ARGS(mResultSRVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    //============================================================
    // Raw GI SRVを2枚登録
    //============================================================
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = RenderFormat::SceneHDR;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1u;


    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        mResultSRVHeap->GetCPUDescriptorHandleForHeapStart();


    device->CreateShaderResourceView(
        mResultResource.Get(),
        &srvDesc,
        handle
    );


    handle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize);

    //============================================================
    // t1 Dummy
    //============================================================
    device->CreateShaderResourceView(
        mResultResource.Get(),
        &srvDesc,
        handle
    );


    return true;
}

bool SSGIPass::CreateRootSignature()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // t0 ～ t4
    //
    // t0 Scene HDR
    // t1 Depth
    // t2 Normal/Roughness
    // t3 BaseColor/Metallic
    // t4 Previous Frame GI
    //============================================================
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 5u;
    srvRange.BaseShaderRegister = 0u;
    srvRange.RegisterSpace = 0u;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    //============================================================
    // Root[0]
    //
    // t0 ～ t2
    //============================================================
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;


    //============================================================
    // Root[1]
    //
    // b0
    //============================================================
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0u;


    //============================================================
    // Sampler
    //============================================================
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};

    //============================================================
    // s0:
    // Scene Color用Linear
    //============================================================
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].ShaderRegister = 0u;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;

    //============================================================
    // s1:
    // Depth / Surface用Point
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
        IID_PPV_ARGS(mRootSignature.ReleaseAndGetAddressOf()
        )
    );


    return SUCCEEDED(hr);
}

bool SSGIPass::CreateConstantBuffer()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // CBVは256byte Alignment必須
    //============================================================
    const UINT bufferSize = (sizeof(SSGIRayMarchBuffer) + 255u) & ~255u;


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
        IID_PPV_ARGS(mConstantBuffer.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    hr = mConstantBuffer->Map(
        0u,
        nullptr,
        reinterpret_cast<void**>(&mMappedConstantBuffer)
    );


    return SUCCEEDED(hr);
}

bool SSGIPass::CreatePipelineState()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr || !mRootSignature)
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


    auto* vertexShader =
        Shader::getVShader(
            Shader::eVertexShader::VS_Copy
        );


    auto* pixelShader =
        Shader::getPShader(
            Shader::ePixelShader::PS_SSGIRayMarch
        );


    if (vertexShader == nullptr || pixelShader == nullptr)
    {
        return false;
    }


    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = mRootSignature.Get();


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


    //============================================================
    // Full Screen
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
        IID_PPV_ARGS(mRayMarchPSO.ReleaseAndGetAddressOf())
    );


    return SUCCEEDED(hr);
}

bool SSGIPass::CreateFilterResources()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // Half Resolution HDR Texture
    //
    // Raw SSGIと同じFormatを使用する。
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

    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;


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
            IID_PPV_ARGS(mFilterResources[index].ReleaseAndGetAddressOf())
        );


        if (FAILED(hr))
        {
            return false;
        }


        mFilterStates[index] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }


    mFilterResources[0]->SetName(L"SSGI Horizontal Filter");
    mFilterResources[1]->SetName(L"SSGI Vertical Filter");


    //============================================================
    // RTV Heap
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 2u;


    HRESULT hr = device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(mFilterRTVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    const UINT rtvDescriptorSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        mFilterRTVHeap->GetCPUDescriptorHandleForHeapStart();


    for (UINT index = 0u; index < 2u; ++index)
    {
        mFilterRTV[index] = rtvHandle;


        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = RenderFormat::SceneHDR;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


        device->CreateRenderTargetView(
            mFilterResources[index].Get(),
            &rtvDesc,
            rtvHandle
        );


        rtvHandle.ptr +=
            static_cast<SIZE_T>(rtvDescriptorSize);
    }


    //============================================================
    // Filter SRV Heap
    //
    // 3 Descriptor × 2 Pass
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 6u;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    hr = device->CreateDescriptorHeap(
        &srvHeapDesc,
        IID_PPV_ARGS(mFilterSRVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }

    //============================================================
    // Debug用Result SRVをFiltered SSGIへ変更
    //============================================================
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = RenderFormat::SceneHDR;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0u;
    srvDesc.Texture2D.MipLevels = 1u;


    D3D12_CPU_DESCRIPTOR_HANDLE debugHandle =
        mResultSRVHeap->GetCPUDescriptorHandleForHeapStart();


    device->CreateShaderResourceView(
        mFilterResources[1].Get(),
        &srvDesc,
        debugHandle
    );


    debugHandle.ptr +=
        static_cast<SIZE_T>(mSRVDescriptorSize);


    device->CreateShaderResourceView(
        mFilterResources[1].Get(),
        &srvDesc,
        debugHandle
    );

    return true;
}

bool SSGIPass::CreateFilterRootSignature()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // t0: SSGI Input
    // t1: Full Resolution Depth
    // t2: Full Resolution Normal + Roughness
    //============================================================
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 3u;
    srvRange.BaseShaderRegister = 0u;
    srvRange.RegisterSpace = 0u;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    //============================================================
    // Root[0] t0 ～ t2
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
    // s0 Linear
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
    // s1 Point
    //
    // Depth / Normal用。
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
        IID_PPV_ARGS(mFilterRootSignature.ReleaseAndGetAddressOf())
    );

    return SUCCEEDED(hr);
}

bool SSGIPass::CreateFilterConstantBuffers()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    const UINT bufferSize = (sizeof(SSGIFilterBuffer) + 255u) & ~255u;


    for (UINT index = 0u; index < 2u; ++index)
    {
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
            IID_PPV_ARGS(mFilterConstantBuffers[index].ReleaseAndGetAddressOf())
        );


        if (FAILED(hr))
        {
            return false;
        }


        hr = mFilterConstantBuffers[index]->
            Map(
                0u,
                nullptr,
                reinterpret_cast<void**>(&mFilterMapped[index])
            );

        if (FAILED(hr))
        {
            return false;
        }
    }

    return true;
}

bool SSGIPass::CreateFilterPipelineState()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr || !mFilterRootSignature)
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


    auto* vertexShader =
        Shader::getVShader(
            Shader::eVertexShader::VS_Copy
        );


    auto* pixelShader =
        Shader::getPShader(
            Shader::ePixelShader::PS_SSGIFilter
        );


    if (vertexShader == nullptr || pixelShader == nullptr)
    {
        return false;
    }


    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = mFilterRootSignature.Get();


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
        IID_PPV_ARGS(mFilterPSO.ReleaseAndGetAddressOf())
    );


    return SUCCEEDED(hr);
}

bool SSGIPass::CreateTemporalResources()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


    //============================================================
    // GI History
    // RGB: Resolved SSGI
    // A: Confidence
    //============================================================
    D3D12_RESOURCE_DESC giDesc = {};
    giDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    giDesc.Width = mWorkWidth;
    giDesc.Height = mWorkHeight;
    giDesc.DepthOrArraySize = 1u;
    giDesc.MipLevels = 1u;
    giDesc.Format = RenderFormat::SceneHDR;
    giDesc.SampleDesc.Count = 1u;
    giDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    giDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;


    D3D12_CLEAR_VALUE giClear = {};

    giClear.Format = RenderFormat::SceneHDR;


    //============================================================
    // Geometry History
    // RGB: World Normal
    // A: Device Depth
    //============================================================
    D3D12_RESOURCE_DESC geometryDesc = giDesc;

    geometryDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;


    D3D12_CLEAR_VALUE geometryClear = {};
    geometryClear.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    geometryClear.Color[0] = 0.0f;
    geometryClear.Color[1] = 0.0f;
    geometryClear.Color[2] = 0.0f;

    //============================================================
    // DepthのClear値
    //============================================================
    geometryClear.Color[3] = 1.0f;


    //============================================================
    // Ping-Pong History生成
    //============================================================
    for (UINT index = 0u; index < 2u; ++index)
    {
        HRESULT hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &giDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &giClear,
            IID_PPV_ARGS(mTemporalGIResources[index].ReleaseAndGetAddressOf())
        );


        if (FAILED(hr))
        {
            return false;
        }


        hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &geometryDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &geometryClear,
            IID_PPV_ARGS(mTemporalGeometryResources[index].ReleaseAndGetAddressOf())
        );


        if (FAILED(hr))
        {
            return false;
        }


        mTemporalGIStates[index] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        mTemporalGeometryStates[index] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }


    mTemporalGIResources[0]->SetName(L"SSGI Temporal GI History 0");
    mTemporalGIResources[1]->SetName(L"SSGI Temporal GI History 1");
    mTemporalGeometryResources[0]->SetName(L"SSGI Temporal Geometry History 0");
    mTemporalGeometryResources[1]->SetName(L"SSGI Temporal Geometry History 1");


    //============================================================
    // RTV Heap
    //
    // 2 History × 2 MRT
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 4u;


    HRESULT hr = device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(mTemporalRTVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    const UINT rtvSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        mTemporalRTVHeap->GetCPUDescriptorHandleForHeapStart();


    for (UINT index = 0u; index < 2u; ++index)
    {
        //========================================================
        // GI
        //========================================================
        mTemporalGIRTV[index] = handle;


        D3D12_RENDER_TARGET_VIEW_DESC giRTVDesc = {};
        giRTVDesc.Format = RenderFormat::SceneHDR;
        giRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


        device->CreateRenderTargetView(
            mTemporalGIResources[index].Get(),
            &giRTVDesc,
            handle
        );


        handle.ptr +=
            static_cast<SIZE_T>(rtvSize);


        //========================================================
        // Geometry
        //========================================================
        mTemporalGeometryRTV[index] = handle;


        D3D12_RENDER_TARGET_VIEW_DESC geometryRTVDesc = {};
        geometryRTVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        geometryRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


        device->CreateRenderTargetView(
            mTemporalGeometryResources[index].Get(),
            &geometryRTVDesc,
            handle
        );


        handle.ptr +=
            static_cast<SIZE_T>(rtvSize);
    }


    //============================================================
    // Temporal Input Heap
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 6u;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    hr = device->CreateDescriptorHeap(
        &srvHeapDesc,
        IID_PPV_ARGS(mTemporalSRVHeap.ReleaseAndGetAddressOf())
    );

    return SUCCEEDED(hr);
}

bool SSGIPass::CreateTemporalRootSignature()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }

    //============================================================
    // t0 ～ t5
    //============================================================
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 6u;
    srvRange.BaseShaderRegister = 0u;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    //============================================================
    // Root[0]
    //
    // t0 ～ t5
    //============================================================
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;

    //============================================================
    // Root[1]
    //
    // b0
    //============================================================
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0u;


    //============================================================
    // Sampler
    //============================================================
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
    // Linear
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].ShaderRegister = 0u;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;

    // Point
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
        IID_PPV_ARGS(mTemporalRootSignature.ReleaseAndGetAddressOf())
    );


    return SUCCEEDED(hr);
}

bool SSGIPass::CreateTemporalConstantBuffer()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    const UINT bufferSize = (sizeof(SSGITemporalBuffer) + 255u) & ~255u;


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
        reinterpret_cast<void**>(&mTemporalMappedBuffer)
    );


    return SUCCEEDED(hr);
}

bool SSGIPass::CreateTemporalPipelineState()
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


    auto* vertexShader =
        Shader::getVShader(
            Shader::eVertexShader::VS_Copy
        );


    auto* pixelShader =
        Shader::getPShader(
            Shader::ePixelShader::PS_SSGITemporal
        );


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

    //============================================================
    // MRT
    //============================================================
    desc.NumRenderTargets = 2u;
    // Temporal GI
    desc.RTVFormats[0] = RenderFormat::SceneHDR;
    // Normal + Depth History
    desc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;


    for (UINT index = 0u; index < 2u; ++index)
    {
        desc.BlendState.RenderTarget[index].BlendEnable = FALSE;
        desc.BlendState.RenderTarget[index].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.SampleDesc.Count = 1u;


    const HRESULT hr =
        device->CreateGraphicsPipelineState(
            &desc,
            IID_PPV_ARGS(mTemporalPSO.ReleaseAndGetAddressOf())
        );


    return SUCCEEDED(hr);
}

bool SSGIPass::CreateUpsampleResources()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // Full Resolution HDR Texture
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


    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;


    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = RenderFormat::SceneHDR;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;


    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(mUpsampleResource.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr) || !mUpsampleResource)
    {
        return false;
    }


    mUpsampleResource->SetName(L"SSGI Full Resolution Upsample");


    mUpsampleState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


    //============================================================
    // RTV Heap
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1u;


    hr = device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(mUpsampleRTVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    mUpsampleRTV = mUpsampleRTVHeap->GetCPUDescriptorHandleForHeapStart();


    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = RenderFormat::SceneHDR;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


    device->CreateRenderTargetView(
        mUpsampleResource.Get(),
        &rtvDesc,
        mUpsampleRTV
    );


    //============================================================
    // Upsampling Input SRV Heap
    //
    // t0 Half SSGI
    // t1 Depth
    // t2 Normal
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 3u;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    hr = device->CreateDescriptorHeap(
        &srvHeapDesc,
        IID_PPV_ARGS(mUpsampleSRVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    //============================================================
    // 後続Pass / Debug表示用
    // Screen Root Signature用に2個
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC resultHeapDesc = {};
    resultHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    resultHeapDesc.NumDescriptors = 2u;
    resultHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    hr = device->CreateDescriptorHeap(
        &resultHeapDesc,
        IID_PPV_ARGS(mUpsampleResultSRVHeap.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    //============================================================
    // Full-res SSGI SRV
    //============================================================
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = RenderFormat::SceneHDR;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0u;
    srvDesc.Texture2D.MipLevels = 1u;


    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        mUpsampleResultSRVHeap->GetCPUDescriptorHandleForHeapStart();


    //============================================================
    // t0
    //============================================================
    device->CreateShaderResourceView(
        mUpsampleResource.Get(),
        &srvDesc,
        handle
    );


    //============================================================
    // t1 Dummy
    //============================================================
    handle.ptr += static_cast<SIZE_T>(mSRVDescriptorSize);

    device->CreateShaderResourceView(
        mUpsampleResource.Get(),
        &srvDesc,
        handle
    );

    return true;
}

bool SSGIPass::CreateUpsampleRootSignature()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // t0 ～ t2
    //============================================================
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 3u;
    srvRange.BaseShaderRegister = 0u;
    srvRange.RegisterSpace = 0u;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    //============================================================
    // Root[0]
    // t0 ～ t2
    //============================================================
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1u;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;

    //============================================================
    // Root[1]
    // b0
    //============================================================
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0u;

    //============================================================
    // Sampler
    //============================================================
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};

    //============================================================
    // s0 Linear
    //============================================================
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].ShaderRegister = 0u;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;

    //============================================================
    // s1 Point
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
        IID_PPV_ARGS(mUpsampleRootSignature.ReleaseAndGetAddressOf())
    );

    return SUCCEEDED(hr);
}

bool SSGIPass::CreateUpsampleConstantBuffer()
{
    ID3D12Device* device = Direct3D::getDevice();

    if (device == nullptr)
    {
        return false;
    }

    const UINT bufferSize = (sizeof(SSGIUpsampleBuffer) + 255u) & ~255u;


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
        IID_PPV_ARGS(mUpsampleConstantBuffer.ReleaseAndGetAddressOf())
    );


    if (FAILED(hr))
    {
        return false;
    }


    hr = mUpsampleConstantBuffer->Map(
        0u,
        nullptr,
        reinterpret_cast<void**>(&mUpsampleMappedBuffer)
    );

    return SUCCEEDED(hr);
}

bool SSGIPass::CreateUpsamplePipelineState()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr || !mUpsampleRootSignature)
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


    auto* vertexShader =
        Shader::getVShader(
            Shader::eVertexShader::VS_Copy
        );


    auto* pixelShader =
        Shader::getPShader(
            Shader::ePixelShader::PS_SSGIUpsample
        );


    if (vertexShader == nullptr || pixelShader == nullptr)
    {
        return false;
    }


    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = mUpsampleRootSignature.Get();


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
        IID_PPV_ARGS(mUpsamplePSO.ReleaseAndGetAddressOf())
    );

    return SUCCEEDED(hr);
}

bool SSGIPass::CreateCompositeResources()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // Composite Input
    // t0: HDR Scene
    // t1: Full Resolution SSGI
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 2u;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    const HRESULT hr =
        device->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(mCompositeSRVHeap.ReleaseAndGetAddressOf()
            )
        );

    if (FAILED(hr))
    {
        return false;
    }

    mCompositeSRVHeap->SetName(L"SSGI Composite SRV Heap");

    return true;
}

bool SSGIPass::CreateCompositeRootSignature()
{
    ID3D12Device* device = Direct3D::getDevice();

    if (device == nullptr)
    {
        return false;
    }

    //============================================================
    // t0 ～ t1
    //============================================================
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 2u;
    srvRange.BaseShaderRegister = 0u;
    srvRange.RegisterSpace = 0u;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


    //============================================================
    // Root Parameter
    //============================================================
    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameter.DescriptorTable.NumDescriptorRanges = 1u;
    rootParameter.DescriptorTable.pDescriptorRanges = &srvRange;

    //============================================================
    // Linear Clamp Sampler
    // SSGIはすでにFull Resolutionだが、
    // Scene / GIとも通常のScreen Textureとして扱う
    //============================================================
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister = 0u;
    sampler.RegisterSpace = 0u;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;


    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = 1u;
    rootDesc.pParameters = &rootParameter;
    rootDesc.NumStaticSamplers = 1u;
    rootDesc.pStaticSamplers = &sampler;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


    //============================================================
    // Root Signature Serialize
    //============================================================
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


    //============================================================
    // Root Signature生成
    //============================================================
    hr = device->CreateRootSignature(
        0u,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(mCompositeRootSignature.ReleaseAndGetAddressOf())
    );


    return SUCCEEDED(hr);
}

bool SSGIPass::CreateCompositePipelineState()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr || !mCompositeRootSignature)
    {
        return false;
    }


    //============================================================
    // Full Screen Quad Layout
    //============================================================
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


    auto* vertexShader =
        Shader::getVShader(
            Shader::eVertexShader::VS_Copy
        );


    auto* pixelShader =
        Shader::getPShader(
            Shader::ePixelShader::PS_SSGIComposite
        );


    if (vertexShader == nullptr || pixelShader == nullptr)
    {
        return false;
    }


    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

    desc.pRootSignature = mCompositeRootSignature.Get();


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


    //============================================================
    // Rasterizer
    //============================================================
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;

    //============================================================
    // FullscreenなのでDepthなし
    //============================================================
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    //============================================================
    // Blendなし
    // Shader内部でScene + GIを計算する
    //============================================================
    desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    //============================================================
    // HDR
    //============================================================
    desc.NumRenderTargets = 1u;
    desc.RTVFormats[0] = RenderFormat::SceneHDR;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.SampleDesc.Count = 1u;


    const HRESULT hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(mCompositePSO.ReleaseAndGetAddressOf())
    );


    return SUCCEEDED(hr);
}

bool SSGIPass::CreateDebugResources()
{
    ID3D12Device* device = Direct3D::getDevice();


    if (device == nullptr)
    {
        return false;
    }


    //============================================================
    // Screen Root Signature:
    // t0 ～ t1
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 2u;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;


    const HRESULT hr =
        device->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(mDebugSRVHeap.ReleaseAndGetAddressOf())
        );


    if (FAILED(hr))
    {
        return false;
    }

    mDebugSRVHeap->SetName(L"SSGI Debug SRV Heap");

    return true;
}

void SSGIPass::UpdateConstantBuffer()
{
    if (mMappedConstantBuffer == nullptr)
    {
        return;
    }


    //============================================================
    // Current ViewProjection
    //============================================================
    const XMMATRIX viewProjection = *vnCamera::getScreen();


    const XMMATRIX inverseViewProjection =
        XMMatrixInverse(
            nullptr,
            viewProjection
        );

    XMStoreFloat4x4(
        &mMappedConstantBuffer->ViewProjection,
        XMMatrixTranspose(viewProjection)
    );


    XMStoreFloat4x4(
        &mMappedConstantBuffer->InverseViewProjection,
        XMMatrixTranspose(inverseViewProjection)
    );


    //============================================================
    // Camera
    //============================================================
    XMStoreFloat4(
        &mMappedConstantBuffer->CameraPosition,
        *vnCamera::getPosition()
    );

    //============================================================
    // Working Resolution
    // Ray March Shaderで使用する
    //============================================================
    const float workWidth =
        static_cast<float>(
            (std::max)(
                mWorkWidth,
                1u
                )
            );


    const float workHeight =
        static_cast<float>(
            (std::max)(
                mWorkHeight,
                1u
                )
            );


    mMappedConstantBuffer->ScreenParameters =
        XMFLOAT4(
            workWidth,
            workHeight,
            1.0f / workWidth,
            1.0f / workHeight
        );


    //============================================================
    // Full Resolution
    // Depth / Surface / Scene HDRで使用する
    //============================================================
    const float fullWidth =
        static_cast<float>(
            (std::max)(
                mWidth,
                1u
                )
            );


    const float fullHeight =
        static_cast<float>(
            (std::max)(
                mHeight,
                1u
                )
            );


    mMappedConstantBuffer->FullScreenParameters =
        XMFLOAT4(
            fullWidth,
            fullHeight,
            1.0f / fullWidth,
            1.0f / fullHeight
        );


    //============================================================
    // Ray
    //============================================================
    mMappedConstantBuffer->RayParameters =
        XMFLOAT4(
            mMaxDistance,
            static_cast<float>(mStepCount),
            mThickness,
            mNormalBias
        );


    //============================================================
    // Sampling
    //============================================================
    mMappedConstantBuffer->SamplingParameters =
        XMFLOAT4(
            static_cast<float>(mRayCount),
            mIntensity,
            0.999999f,
            static_cast<float>(mFrameIndex & 1023u) // Temporal用Ray Pattern Index
        );

    mMappedConstantBuffer->RadianceParameters =
        XMFLOAT4(
            mMaxRadiance,
            0.0f,
            0.0f,
            0.0f
        );

    //============================================================
    // Multi Bounce
    //============================================================
    mMappedConstantBuffer->MultiBounceParameters =
        XMFLOAT4(
            mMultiBounceEnabled ? 1.0f : 0.0f,
            mMultiBounceStrength,
            mMultiBounceHistoryValid ? 1.0f : 0.0f,
            0.0f
        );
}

void SSGIPass::UpdateFilterConstantBuffers()
{
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


    const float width =
        static_cast<float>(
            (std::max)(
                mWorkWidth,
                1u
                )
            );


    const float height =
        static_cast<float>(
            (std::max)(
                mWorkHeight,
                1u
                )
            );


    for (UINT index = 0u; index < 2u; ++index)
    {
        if (mFilterMapped[index] == nullptr)
        {
            continue;
        }


        SSGIFilterBuffer& buffer = *mFilterMapped[index];


        //========================================================
        // HLSL:
        // mul(vector, matrix)
        //========================================================
        XMStoreFloat4x4(
            &buffer.InverseViewProjection,
            XMMatrixTranspose(inverseViewProjection)
        );


        buffer.CameraPosition = cameraPosition;

        buffer.ScreenParameters =
            XMFLOAT4(
                width,
                height,
                1.0f / width,
                1.0f / height
            );


        //========================================================
        // Horizontal / Vertical
        //========================================================
        if (index == 0u)
        {
            buffer.FilterParameters =
                XMFLOAT4(
                    1.0f,
                    0.0f,
                    static_cast<float>(mFilterRadius),
                    mFilterDepthFalloff
                );
        }
        else
        {
            buffer.FilterParameters =
                XMFLOAT4(
                    0.0f,
                    1.0f,
                    static_cast<float>(mFilterRadius),
                    mFilterDepthFalloff
                );
        }


        buffer.EdgeParameters =
            XMFLOAT4(
                mFilterNormalPower,
                mFilterConfidenceMin,
                0.999999f,
                0.0f
            );
    }
}

void SSGIPass::UpdateTemporalConstantBuffer(const XMMATRIX& previousViewProjection)
{
    if (mTemporalMappedBuffer == nullptr)
    {
        return;
    }


    //============================================================
    // Current ViewProjection
    //============================================================
    const XMMATRIX currentViewProjection = *vnCamera::getScreen();


    const XMMATRIX currentInverseViewProjection =
        XMMatrixInverse(
            nullptr,
            currentViewProjection
        );


    //============================================================
    // Previous Inverse VP
    // Previous DepthからPrevious World Positionを復元する
    //============================================================
    const XMMATRIX previousInverseViewProjection =
        XMMatrixInverse(
            nullptr,
            previousViewProjection
        );


    XMStoreFloat4x4(
        &mTemporalMappedBuffer->InverseViewProjection,
        XMMatrixTranspose(currentInverseViewProjection)
    );


    XMStoreFloat4x4(
        &mTemporalMappedBuffer->PreviousInverseViewProjection,
        XMMatrixTranspose(previousInverseViewProjection)
    );


    //============================================================
    // Work Resolution
    //============================================================
    const float workWidth =
        static_cast<float>(
            (std::max)(
                mWorkWidth,
                1u
                )
            );


    const float workHeight =
        static_cast<float>(
            (std::max)(
                mWorkHeight,
                1u
                )
            );


    mTemporalMappedBuffer->ScreenParameters =
        XMFLOAT4(
            workWidth,
            workHeight,
            1.0f / workWidth,
            1.0f / workHeight
        );


    //============================================================
    // Full Resolution
    //============================================================
    const float fullWidth =
        static_cast<float>(
            (std::max)(
                mWidth,
                1u
                )
            );


    const float fullHeight =
        static_cast<float>(
            (std::max)(
                mHeight,
                1u
                )
            );


    mTemporalMappedBuffer->FullScreenParameters =
        XMFLOAT4(
            fullWidth,
            fullHeight,
            1.0f / fullWidth,
            1.0f / fullHeight
        );


    //============================================================
    // Temporal
    //============================================================
    mTemporalMappedBuffer->TemporalParameters =
        XMFLOAT4(
            mTemporalHistoryWeight,
            mTemporalWorldPositionThreshold,
            mTemporalNormalThreshold,
            mTemporalMotionResponse
        );


    mTemporalMappedBuffer->HistoryParameters =
        XMFLOAT4(
            mTemporalHistoryValid ? 1.0f : 0.0f,
            mTemporalClampExpand,
            0.0f,
            0.0f
        );
}

void SSGIPass::UpdateUpsampleConstantBuffer()
{
    if (mUpsampleMappedBuffer == nullptr)
    {
        return;
    }


    //============================================================
    // Depth → World Position用
    //============================================================
    const XMMATRIX viewProjection = *vnCamera::getScreen();


    const XMMATRIX inverseViewProjection =
        XMMatrixInverse(
            nullptr,
            viewProjection
        );


    XMStoreFloat4x4(
        &mUpsampleMappedBuffer->InverseViewProjection,
        XMMatrixTranspose(inverseViewProjection)
    );


    //============================================================
    // Camera Position
    //============================================================
    XMStoreFloat4(
        &mUpsampleMappedBuffer->CameraPosition,
        *vnCamera::getPosition()
    );


    //============================================================
    // Half Resolution
    //============================================================
    const float workWidth =
        static_cast<float>(
            (std::max)(
                mWorkWidth,
                1u
                )
            );


    const float workHeight =
        static_cast<float>(
            (std::max)(
                mWorkHeight,
                1u
                )
            );


    mUpsampleMappedBuffer->WorkScreenParameters =
        XMFLOAT4(
            workWidth,
            workHeight,
            1.0f / workWidth,
            1.0f / workHeight
        );


    //============================================================
    // Full Resolution
    //============================================================
    const float fullWidth =
        static_cast<float>(
            (std::max)(
                mWidth,
                1u
                )
            );


    const float fullHeight =
        static_cast<float>(
            (std::max)(
                mHeight,
                1u
                )
            );


    mUpsampleMappedBuffer->FullScreenParameters =
        XMFLOAT4(
            fullWidth,
            fullHeight,
            1.0f / fullWidth,
            1.0f / fullHeight
        );


    //============================================================
    // Edge-aware Parameters
    //============================================================
    mUpsampleMappedBuffer->UpsampleParameters =
        XMFLOAT4(
            mUpsampleDepthFalloff,
            mUpsampleNormalPower,
            mUpsampleConfidenceMin,
            0.999999f
        );
}

void SSGIPass::TransitionResult(D3D12_RESOURCE_STATES newState)
{
    if (!mResultResource || mResultState == newState)
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

    if (commandList == nullptr)
    {
        return;
    }


    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mResultResource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mResultState;
    barrier.Transition.StateAfter = newState;


    commandList->ResourceBarrier(
        1u,
        &barrier
    );


    mResultState = newState;
}

void SSGIPass::TransitionFilterResource(UINT index, D3D12_RESOURCE_STATES newState)
{
    if (index >= 2u ||
        !mFilterResources[index] ||
        mFilterStates[index] == newState)
    {
        return;
    }


    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


    if (commandList == nullptr)
    {
        return;
    }


    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mFilterResources[index].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mFilterStates[index];
    barrier.Transition.StateAfter = newState;


    commandList->ResourceBarrier(
        1u,
        &barrier
    );


    mFilterStates[index] = newState;
}

void SSGIPass::TransitionTemporalGI(UINT index, D3D12_RESOURCE_STATES newState)
{
    if (index >= 2u ||
        !mTemporalGIResources[index] ||
        mTemporalGIStates[index] == newState)
    {
        return;
    }


    ID3D12GraphicsCommandList* commandList =
        Direct3D::getCommandList();


    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mTemporalGIResources[index].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mTemporalGIStates[index];
    barrier.Transition.StateAfter = newState;


    commandList->ResourceBarrier(
        1u,
        &barrier
    );


    mTemporalGIStates[index] = newState;
}


void SSGIPass::TransitionTemporalGeometry(UINT index, D3D12_RESOURCE_STATES newState)
{
    if (index >= 2u ||
        !mTemporalGeometryResources[index] ||
        mTemporalGeometryStates[index] == newState)
    {
        return;
    }


    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mTemporalGeometryResources[index].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mTemporalGeometryStates[index];
    barrier.Transition.StateAfter = newState;


    commandList->ResourceBarrier(
        1u,
        &barrier
    );

    mTemporalGeometryStates[index] = newState;
}

void SSGIPass::TransitionUpsample(D3D12_RESOURCE_STATES newState)
{
    if (!mUpsampleResource || mUpsampleState == newState)
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

    if (commandList == nullptr)
    {
        return;
    }


    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mUpsampleResource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = mUpsampleState;
    barrier.Transition.StateAfter = newState;

    commandList->ResourceBarrier(
        1u,
        &barrier
    );


    mUpsampleState = newState;
}

void SSGIPass::CommitTemporalHistory()
{
    //============================================================
    // 今回書いたHistoryを次FrameのReadへする
    //============================================================
    std::swap(mTemporalReadIndex, mTemporalWriteIndex);
    mTemporalHistoryValid = true;
}