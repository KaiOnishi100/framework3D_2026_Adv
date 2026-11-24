#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"laserQuad.h"

LaserQuad::LaserQuad(const Desc& desc)
{
    mDesc = desc;

    setRenderFlag(eRenderFlag::Transparent ,true);      // 透明ON
    setRenderFlag(eRenderFlag::Lighting, false);// ライト不要
    setRenderFlag(eRenderFlag::ZWrite, mDesc.ZWrite);

    LoadTexture(mDesc.Texture);
    BuildBuffers();
}

LaserQuad::~LaserQuad()
{
    SAFE_RELEASE(basicDescHeap);
    SAFE_RELEASE(texbuff);
    SAFE_RELEASE(constBuff);
    SAFE_RELEASE(vertBuff);
    SAFE_RELEASE(idxBuff);
    SAFE_RELEASE(materialBuff);
}

void LaserQuad::SetSegment(const XMVECTOR& start, const XMVECTOR& end)
{
    mStart = start;
    mEnd = end;
}

void LaserQuad::execute(float dt)
{
    // UVスクロール
    if (mDesc.UvScrollSpeed != 0.0f)
    {
        mUvOffset += mDesc.UvScrollSpeed * dt;
        if (mUvOffset > 1.0f) mUvOffset -= 1.0f;
    }

    // ---- 4頂点を更新（太さのある線） ----
    XMVECTOR start = mStart;
    XMVECTOR end = mEnd;

    XMVECTOR dir = end - start;
    float len = XMVectorGetX(XMVector3Length(dir));
    if (len < 0.001f) return;

    dir = XMVector3Normalize(dir);

    // カメラ方向（レーザーがカメラに対して常に見えるように）
    XMVECTOR camPos = *vnCamera::getPosition();
    XMVECTOR toCam = XMVector3Normalize(camPos - start);

    // レーザー軸(dir) と カメラ方向(toCam) から “板の横方向” を作る
    XMVECTOR right = XMVector3Cross(dir, toCam);
    float rl = XMVectorGetX(XMVector3Length(right));
    if (rl < 0.001f)
    {
        // ほぼ平行ならUpで逃げる
        right = XMVector3Cross(dir, XMVectorSet(0, 1, 0, 0));
    }
    right = XMVector3Normalize(right);

    float half = mDesc.Thickness * 0.5f;
    XMVECTOR off = right * half;

    XMVECTOR p0 = start - off;
    XMVECTOR p1 = start + off;
    XMVECTOR p2 = end - off;
    XMVECTOR p3 = end + off;

    // 0..1 のUを使う。スクロールはUに足す。
    float u0 = mUvOffset;
    float u1 = 1.0f + mUvOffset;

    // 頂点書き換え
    auto setV = [&](int i, const XMVECTOR& p, float u, float v, const XMVECTOR& col)
        {
            vtx[i].x = XMVectorGetX(p);
            vtx[i].y = XMVectorGetY(p);
            vtx[i].z = XMVectorGetZ(p);
            vtx[i].u = u;
            vtx[i].v = v;
            vtx[i].r = XMVectorGetX(col);
            vtx[i].g = XMVectorGetY(col);
            vtx[i].b = XMVectorGetZ(col);
            vtx[i].a = XMVectorGetW(col);
            vtx[i].nx = vtx[i].ny = vtx[i].nz = 0.0f;
        };

    XMVECTOR edge = XMVectorSet(
        XMVectorGetX(mDesc.Color) * 0.9f,
        XMVectorGetY(mDesc.Color) * 0.9f,
        XMVectorGetZ(mDesc.Color) * 0.9f,
        0.75f
    );

    // V=0が下端、V=1が上端
    setV(0, p0, u0, 0.0f, edge);
    setV(1, p1, u0, 1.0f, edge);
    setV(2, p2, u1, 0.0f, edge);
    setV(3, p3, u1, 1.0f, edge);
}

void LaserQuad::render()
{
    //============================================================
    // LaserQuadの頂点は execute() でワールド座標として作っている
    // なので World は Identity 扱いで描画する
    //============================================================
    XMMATRIX W = XMMatrixIdentity();
    XMMATRIX WVP = XMMatrixMultiply(W, *vnCamera::getScreen());

    XMStoreFloat4x4(&pConstBuffer->WVP, XMMatrixTranspose(WVP));
    XMStoreFloat4x4(&pConstBuffer->World, XMMatrixTranspose(W));

    //============================================================
    // MaterialBuffer
    //============================================================
    XMStoreFloat4(&pMaterialBuffer->Diffuse, mDesc.Color);
    XMStoreFloat4(&pMaterialBuffer->Ambient, XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f));
    XMStoreFloat4(&pMaterialBuffer->Specular, XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));
    XMStoreFloat4(&pMaterialBuffer->TilingOffset, XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f));

    //============================================================
    // Root1 に行列CBVを渡す
    //============================================================
    Direct3D::getCommandList()->SetGraphicsRootConstantBufferView(
        1,
        constBuff->GetGPUVirtualAddress()
    );

    Direct3D::getCommandList()->SetPipelineState(pPipelineState[renderFlag]);
    Direct3D::getCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Direct3D::getCommandList()->SetDescriptorHeaps(1, &basicDescHeap);

    Direct3D::getCommandList()->SetGraphicsRootDescriptorTable(
        2,
        basicDescHeap->GetGPUDescriptorHandleForHeapStart()
    );

    Direct3D::getCommandList()->IASetVertexBuffers(0, 1, &vbView);
    Direct3D::getCommandList()->IASetIndexBuffer(&ibView);

    Direct3D::getCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void LaserQuad::BuildBuffers()
{
    HRESULT hr = S_OK;

    //============================================================
    // 共通ヒープ設定
    //============================================================
    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProp.CreationNodeMask = 1;
    heapProp.VisibleNodeMask = 1;

    //============================================================
    // Descriptor heap
    // [0] SRV
    // [1] MaterialCBV
    //============================================================
    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
    descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descHeapDesc.NumDescriptors = 2;
    descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    hr = Direct3D::getDevice()->CreateDescriptorHeap(
        &descHeapDesc,
        IID_PPV_ARGS(&basicDescHeap)
    );
    assert(SUCCEEDED(hr));

    //============================================================
    // constBuff
    // Root1 に直接セットする行列用CB
    //============================================================
    D3D12_RESOURCE_DESC cbDesc = {};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = ((sizeof(stConstantBuffer) + 0xff) & ~0xff);
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.Format = DXGI_FORMAT_UNKNOWN;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = Direct3D::getDevice()->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constBuff)
    );
    assert(SUCCEEDED(hr));
    assert(constBuff != nullptr);

    hr = constBuff->Map(0, nullptr, (void**)&pConstBuffer);
    assert(SUCCEEDED(hr));
    assert(pConstBuffer != nullptr);

    //============================================================
    // MaterialBuffer
    // DescriptorHeap[1] に入れる
    //============================================================
    D3D12_RESOURCE_DESC matDesc = {};
    matDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    matDesc.Width = ((sizeof(stMaterialBuffer) + 0xff) & ~0xff);
    matDesc.Height = 1;
    matDesc.DepthOrArraySize = 1;
    matDesc.MipLevels = 1;
    matDesc.Format = DXGI_FORMAT_UNKNOWN;
    matDesc.SampleDesc.Count = 1;
    matDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    matDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = Direct3D::getDevice()->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &matDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&materialBuff)
    );
    assert(SUCCEEDED(hr));
    assert(materialBuff != nullptr);

    hr = materialBuff->Map(0, nullptr, (void**)&pMaterialBuffer);
    assert(SUCCEEDED(hr));
    assert(pMaterialBuffer != nullptr);

    //============================================================
    // Descriptor[0] : Texture SRV
    //============================================================
    D3D12_CPU_DESCRIPTOR_HANDLE h =
        basicDescHeap->GetCPUDescriptorHandleForHeapStart();

    if (texbuff)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = mTexMeta.format;
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        Direct3D::getDevice()->CreateShaderResourceView(
            texbuff,
            &srvDesc,
            h
        );
    }
    else
    {
        Direct3D::getDevice()->CreateShaderResourceView(
            Direct3D::getWhiteTexture(),
            Direct3D::getWhiteTextueViewDesc(),
            h
        );
    }

    //============================================================
    // Descriptor[1] : Material CBV
    //============================================================
    h.ptr += Direct3D::getDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    );

    D3D12_CONSTANT_BUFFER_VIEW_DESC matCbvDesc = {};
    matCbvDesc.BufferLocation = materialBuff->GetGPUVirtualAddress();
    matCbvDesc.SizeInBytes = (UINT)materialBuff->GetDesc().Width;

    Direct3D::getDevice()->CreateConstantBufferView(
        &matCbvDesc,
        h
    );

    //============================================================
    // Vertex buffer
    //============================================================
    {
        const UINT vbSize = sizeof(vnVertex3D) * 4;

        D3D12_RESOURCE_DESC vbDesc = {};
        vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        vbDesc.Width = vbSize;
        vbDesc.Height = 1;
        vbDesc.DepthOrArraySize = 1;
        vbDesc.MipLevels = 1;
        vbDesc.Format = DXGI_FORMAT_UNKNOWN;
        vbDesc.SampleDesc.Count = 1;
        vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        vbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = Direct3D::getDevice()->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertBuff)
        );
        assert(SUCCEEDED(hr));
        assert(vertBuff != nullptr);

        hr = vertBuff->Map(0, nullptr, reinterpret_cast<void**>(&vtx));
        assert(SUCCEEDED(hr));
        assert(vtx != nullptr);

        vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
        vbView.StrideInBytes = sizeof(vnVertex3D);
        vbView.SizeInBytes = vbSize;
    }

    //============================================================
    // Index buffer
    //============================================================
    {
        const UINT ibSize = sizeof(WORD) * 6;

        D3D12_RESOURCE_DESC ibDesc = {};
        ibDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ibDesc.Width = ibSize;
        ibDesc.Height = 1;
        ibDesc.DepthOrArraySize = 1;
        ibDesc.MipLevels = 1;
        ibDesc.Format = DXGI_FORMAT_UNKNOWN;
        ibDesc.SampleDesc.Count = 1;
        ibDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ibDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = Direct3D::getDevice()->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &ibDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&idxBuff)
        );
        assert(SUCCEEDED(hr));
        assert(idxBuff != nullptr);

        hr = idxBuff->Map(0, nullptr, reinterpret_cast<void**>(&idx));
        assert(SUCCEEDED(hr));
        assert(idx != nullptr);

        idx[0] = 0; idx[1] = 1; idx[2] = 2;
        idx[3] = 1; idx[4] = 3; idx[5] = 2;

        ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
        ibView.Format = DXGI_FORMAT_R16_UINT;
        ibView.SizeInBytes = ibSize;
    }
}

void LaserQuad::LoadTexture(const WCHAR* path)
{
    HRESULT hr = S_OK;

    if (!path || path[0] == 0) return;

    const WCHAR* ext = wcsrchr(path, L'.');

    mScratch = ScratchImage{};
    mTexMeta = TexMetadata{};

    if (ext && (wcscmp(ext, L".tga") == 0 || wcscmp(ext, L".TGA") == 0))
        hr = LoadFromTGAFile(path, TGA_FLAGS_NONE, &mTexMeta, mScratch);
    else
        hr = LoadFromWICFile(path, WIC_FLAGS_NONE, &mTexMeta, mScratch);

    if (FAILED(hr)) return;

    const Image* img = mScratch.GetImage(0, 0, 0);

    D3D12_HEAP_PROPERTIES texHeapProp = {};
    texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
    texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Format = mTexMeta.format;
    resDesc.Width = mTexMeta.width;
    resDesc.Height = (UINT)mTexMeta.height;
    resDesc.DepthOrArraySize = (UINT16)mTexMeta.arraySize;
    resDesc.MipLevels = (UINT16)mTexMeta.mipLevels;
    resDesc.SampleDesc.Count = 1;
    resDesc.Dimension = (D3D12_RESOURCE_DIMENSION)mTexMeta.dimension;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    SAFE_RELEASE(texbuff);

    hr = Direct3D::getDevice()->CreateCommittedResource(
        &texHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&texbuff)
    );
    if (FAILED(hr) || !texbuff) return;

    texbuff->WriteToSubresource(
        0, nullptr,
        img->pixels,
        (UINT)img->rowPitch,
        (UINT)img->slicePitch
    );
}