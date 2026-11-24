#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"shockwaveRingEffect.h"

//============================================================
// コンストラクタ
//============================================================
ShockwaveRingQuad::ShockwaveRingQuad(const Desc& desc)
{
    //====================================================
    // 初期設定
    //====================================================
    mSize = desc.Size;
    mY = desc.Y;
    mColor = desc.Color;

    // 加算合成
    if (desc.Additive)
        setRenderFlag(eRenderFlag::Addition, true);

    setRenderFlag(eRenderFlag::Transparent, false);      // 透明OFF
    setRenderFlag(eRenderFlag::Lighting, true);          // ライト
    setRenderFlag(eRenderFlag::ZWrite, desc.ZWrite);

    //----------------------------------------------------
    // テクスチャロード（Emitterと同じ）
    //----------------------------------------------------
    TexMetadata metadata{};
    ScratchImage img{};

    LoadFromWICFile(desc.Texture, WIC_FLAGS_NONE, &metadata, img);

    const Image* image = img.GetImage(0, 0, 0);

    D3D12_HEAP_PROPERTIES heapProp{};
    heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
    heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Width = metadata.width;
    resDesc.Height = (UINT)metadata.height;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = metadata.format;
    resDesc.SampleDesc.Count = 1;

    Direct3D::getDevice()->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&mTex)
    );

    mTex->WriteToSubresource(0, nullptr,
        image->pixels,
        (UINT)image->rowPitch,
        (UINT)image->slicePitch);

    //----------------------------------------------------
    // 頂点バッファ
    //----------------------------------------------------
    D3D12_HEAP_PROPERTIES vbHeap{};
    vbHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC vbDesc{};
    vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width = sizeof(mVertices);
    vbDesc.Height = 1;
    vbDesc.DepthOrArraySize = 1;
    vbDesc.MipLevels = 1;
    vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Direct3D::getDevice()->CreateCommittedResource(
        &vbHeap, D3D12_HEAP_FLAG_NONE,
        &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&mVB));

    void* vmap;
    mVB->Map(0, nullptr, &vmap);
    memcpy(vmap, mVertices, sizeof(mVertices));

    mVBView.BufferLocation = mVB->GetGPUVirtualAddress();
    mVBView.SizeInBytes = sizeof(mVertices);
    mVBView.StrideInBytes = sizeof(vnVertex3D);

    //----------------------------------------------------
    // インデックス
    //----------------------------------------------------
    D3D12_RESOURCE_DESC ibDesc = vbDesc;
    ibDesc.Width = sizeof(mIndices);

    Direct3D::getDevice()->CreateCommittedResource(
        &vbHeap, D3D12_HEAP_FLAG_NONE,
        &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&mIB));

    void* imap;
    mIB->Map(0, nullptr, &imap);
    memcpy(imap, mIndices, sizeof(mIndices));

    mIBView.BufferLocation = mIB->GetGPUVirtualAddress();
    mIBView.SizeInBytes = sizeof(mIndices);
    mIBView.Format = DXGI_FORMAT_R16_UINT;
}

void ShockwaveRingQuad::SetCenter(const XMVECTOR& pos)
{
    mCenter = pos;
}

void ShockwaveRingQuad::SetSize(float s)
{
    mSize = s;
}

void ShockwaveRingQuad::SetAlpha(float a)
{
    mColor = XMVectorSetW(mColor, std::clamp(a, 0.0f, 1.0f));
}

void ShockwaveRingQuad::SetColor(const XMVECTOR& c)
{
    mColor = c;
}

void ShockwaveRingQuad::render()
{
    //====================================================
    // XZ平面に固定（ビルボードしない）
    //====================================================
    float half = mSize * 0.5f;

    float cx = XMVectorGetX(mCenter);
    float cz = XMVectorGetZ(mCenter);

    // 頂点（XZ平面）
    mVertices[0] = { cx - half, mY, cz - half, 0,0,0, 1,1,1,1, 0,0 };
    mVertices[1] = { cx + half, mY, cz - half, 0,0,0, 1,1,1,1, 1,0 };
    mVertices[2] = { cx - half, mY, cz + half, 0,0,0, 1,1,1,1, 0,1 };
    mVertices[3] = { cx + half, mY, cz + half, 0,0,0, 1,1,1,1, 1,1 };

    for (int i = 0; i < 4; i++)
    {
        mVertices[i].r = XMVectorGetX(mColor);
        mVertices[i].g = XMVectorGetY(mColor);
        mVertices[i].b = XMVectorGetZ(mColor);
        mVertices[i].a = XMVectorGetW(mColor);
    }

    // 頂点更新
    void* vmap;
    mVB->Map(0, nullptr, &vmap);
    memcpy(vmap, mVertices, sizeof(mVertices));

    //----------------------------------------------------
    // 描画
    //----------------------------------------------------
    Direct3D::getCommandList()->IASetVertexBuffers(0, 1, &mVBView);
    Direct3D::getCommandList()->IASetIndexBuffer(&mIBView);

    Direct3D::getCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
