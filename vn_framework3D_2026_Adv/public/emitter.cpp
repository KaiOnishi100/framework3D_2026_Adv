#include "../framework.h"
#include "../framework/vn_environment.h"
#include "emitter.h"

//============================================================
// コンストラクタ
//============================================================
Emitter::Emitter(stEmitterDesc* desc)
{
    //========================================================
    // 設定コピー
    //========================================================
    if (desc != nullptr)
    {
        Desc = *desc;
    }

    //========================================================
    // パラメータの同期
    //========================================================
    Desc.Circle.Radius = (Desc.Circle.Radius > 0.0f) ? Desc.Circle.Radius : Desc.Radius;
    Desc.Attract.Enable = Desc.Attract.Enable || Desc.UseAttract;
    if (Desc.UseAttract)
    {
        Desc.Attract.Power = Desc.AttractPower;
    }

    //========================================================
    // パーティクル配列の作成
    //========================================================
    pParticle = new Particle[ParticleMax];

    //============================================================
    // Particle管理List初期化
    //============================================================
    mActiveParticleCount = 0;
    mFreeParticleCount = ParticleMax;


    for (int i = 0; i < ParticleMax; i++)
    {
        pParticle[i].Life = 0.0f;
        pParticle[i].StartLife = 0.0f;
        pParticle[i].Pos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
        pParticle[i].Vel = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
        pParticle[i].color = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
        pParticle[i].Size = 1.0f;
        mFreeParticleIndices[i] = ParticleMax - 1 - i;
    }

    HRESULT hr = S_OK;

    TexMetadata metadata = {};
    ScratchImage scratchImg = {};

    //========================================================
    // マテリアル作成
    //========================================================
    pMaterials = new stMaterial[1];
    pMaterials[0].texbuff = nullptr;
    pMaterials[0].basicDescHeap = nullptr;
    pMaterials[0].materialBuff = nullptr;
    pMaterials[0].pMaterialBuffer = nullptr;

    //============================================================
    // Normal Map用Resource初期値
    //============================================================
    pMaterials[0].normalTexBuff = nullptr;

    //============================================================
    // Normal Map設定
    // x : Normal Mapping有効フラグ
    //     0.0fなので無効
    // y : Normal強度
    // z : Y方向符号
    // w : 未使用
    //============================================================
    pMaterials[0].NormalParam = XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f);
    //========================================================
    // テクスチャ読み込み
    //========================================================
    WCHAR texture_file[64] = L"";
    swprintf_s(texture_file, L"%s", Desc.Texture);

    const WCHAR* ext = wcsrchr(texture_file, L'.');

    if (ext != nullptr)
    {
        if (wcscmp(ext, L".tga") == 0 || wcscmp(ext, L".TGA") == 0)
        {
            hr = LoadFromTGAFile(texture_file, TGA_FLAGS_NONE, &metadata, scratchImg);
        }
        else
        {
            hr = LoadFromWICFile(texture_file, WIC_FLAGS_NONE, &metadata, scratchImg);
        }

        if (hr == S_OK)
        {
            const Image* img = scratchImg.GetImage(0, 0, 0);

            D3D12_HEAP_PROPERTIES texHeapProp = {};
            texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
            texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
            texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
            texHeapProp.CreationNodeMask = 0;
            texHeapProp.VisibleNodeMask = 0;

            D3D12_RESOURCE_DESC resDesc = {};
            resDesc.Format = metadata.format;
            resDesc.Width = metadata.width;
            resDesc.Height = (UINT)metadata.height;
            resDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
            resDesc.SampleDesc.Count = 1;
            resDesc.SampleDesc.Quality = 0;
            resDesc.MipLevels = (UINT16)metadata.mipLevels;
            resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
            resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            hr = Direct3D::getDevice()->CreateCommittedResource(
                &texHeapProp,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                nullptr,
                IID_PPV_ARGS(&pMaterials[0].texbuff)
            );

            if (hr == S_OK && pMaterials[0].texbuff != nullptr)
            {
                pMaterials[0].texbuff->SetName(L"vnEffect::texbuff");
                pMaterials[0].texbuff->WriteToSubresource(
                    0,
                    nullptr,
                    img->pixels,
                    (UINT)img->rowPitch,
                    (UINT)img->slicePitch
                );
            }
        }
    }

    //========================================================
    // ディスクリプタヒープ + マテリアル定数バッファ
    //========================================================
    {


        D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
        descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descHeapDesc.NodeMask = 0;
        descHeapDesc.NumDescriptors = MaterialDescriptor::Count;
        descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        hr = Direct3D::getDevice()->CreateDescriptorHeap(
            &descHeapDesc,
            IID_PPV_ARGS(&pMaterials[0].basicDescHeap)
        );

        assert(hr == S_OK);

        D3D12_HEAP_PROPERTIES constHeapProp = {};
        constHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
        constHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        constHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        constHeapProp.CreationNodeMask = 1;
        constHeapProp.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC constDesc = {};
        constDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        constDesc.Width = ((sizeof(stMaterialBuffer) + 0xff) & ~0xff);
        constDesc.Height = 1;
        constDesc.DepthOrArraySize = 1;
        constDesc.MipLevels = 1;
        constDesc.Format = DXGI_FORMAT_UNKNOWN;
        constDesc.SampleDesc.Count = 1;
        constDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        constDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = Direct3D::getDevice()->CreateCommittedResource(
            &constHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &constDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pMaterials[0].materialBuff)
        );

        assert(hr == S_OK);

        pMaterials[0].materialBuff->SetName(L"vnEffect::materialBuff");

        D3D12_RANGE readRange = { 0, 0 };

        // CPUから読み取る予定はなく、書き込み専用なので、
        // 読み取り範囲は0～0を指定する。
        hr = pMaterials[0].materialBuff->Map(
            0,
            &readRange,
            reinterpret_cast<void**>(&pMaterials[0].pMaterialBuffer)
        );

        assert(SUCCEEDED(hr));
        assert(pMaterials[0].pMaterialBuffer != nullptr);

        //============================================================
        // Descriptorサイズ
        //============================================================
        const UINT descriptorSize =
            Direct3D::getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        //============================================================
        // DescriptorHeapの先頭
        //============================================================
        D3D12_CPU_DESCRIPTOR_HANDLE basicHeapHandle =
            pMaterials[0].basicDescHeap->GetCPUDescriptorHandleForHeapStart();

        //============================================================
        // Descriptor番号からCPU Handleを取得する補助関数
        //============================================================
        auto GetCPUHandle = [basicHeapHandle, descriptorSize](UINT descriptorIndex)-> D3D12_CPU_DESCRIPTOR_HANDLE
            {
                D3D12_CPU_DESCRIPTOR_HANDLE handle = basicHeapHandle;

                handle.ptr +=
                    static_cast<SIZE_T>(descriptorIndex) *
                    static_cast<SIZE_T>(descriptorSize);

                return handle;
            };

        //============================================================
        // [0] Diffuse Texture SRV
        // HLSL: Texture2D txDiffuse : register(t0);
        //============================================================
        const D3D12_CPU_DESCRIPTOR_HANDLE diffuseHandle = GetCPUHandle(MaterialDescriptor::Diffuse);

        if (pMaterials[0].texbuff != nullptr)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = metadata.format;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

            Direct3D::getDevice()->CreateShaderResourceView(
                pMaterials[0].texbuff,
                &srvDesc,
                diffuseHandle
            );
        }
        else
        {
            //========================================================
            // エフェクト画像が読み込めなかった場合は白テクスチャ
            //========================================================
            Direct3D::getDevice()->CreateShaderResourceView(
                Direct3D::getWhiteTexture(),
                Direct3D::getWhiteTextueViewDesc(),
                diffuseHandle
            );
        }


        //============================================================
        // ShadowMap用ダミーSRV
        //
        // Descriptor配置:
        //
        //   [1] Static  Cascade 0
        //   [2] Static  Cascade 1
        //   [3] Static  Cascade 2
        //
        //   [5] Dynamic Cascade 0
        //   [6] Dynamic Cascade 1
        //   [7] Dynamic Cascade 2
        //============================================================
        const UINT shadowDummyDescriptorIndices[] =
        {
            MaterialDescriptor::StaticShadow0,
            MaterialDescriptor::StaticShadow1,
            MaterialDescriptor::StaticShadow2,

            MaterialDescriptor::DynamicShadow0,
            MaterialDescriptor::DynamicShadow1,
            MaterialDescriptor::DynamicShadow2
        };

        //============================================================
        // Shadow用6スロットすべてを白Textureで初期化
        //============================================================
        for (UINT shadowIndex = 0; shadowIndex < _countof(shadowDummyDescriptorIndices); ++shadowIndex)
        {
            //========================================================
            // 今回初期化するDescriptor番号
            //========================================================
            const UINT descriptorIndex = shadowDummyDescriptorIndices[shadowIndex];

            //========================================================
            // Heap先頭から対象Descriptor位置まで進める
            //========================================================
            D3D12_CPU_DESCRIPTOR_HANDLE shadowHandle = basicHeapHandle;

            shadowHandle.ptr +=
                static_cast<SIZE_T>(descriptorIndex) *
                static_cast<SIZE_T>(descriptorSize);

            //========================================================
            // ダミーとして白テクスチャSRVを登録
            //========================================================
            Direct3D::getDevice()->CreateShaderResourceView(
                Direct3D::getWhiteTexture(),
                Direct3D::getWhiteTextueViewDesc(),
                shadowHandle
            );
        }


        //============================================================
        // [4] MaterialBuffer CBV
        // HLSL: cbuffer MaterialBuffer : register(b1)
        //============================================================
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};

        cbvDesc.BufferLocation =
            pMaterials[0].materialBuff->GetGPUVirtualAddress();

        cbvDesc.SizeInBytes =
            static_cast<UINT>(pMaterials[0].materialBuff->GetDesc().Width);

        const D3D12_CPU_DESCRIPTOR_HANDLE materialHandle =
            GetCPUHandle(MaterialDescriptor::Material);

        Direct3D::getDevice()->CreateConstantBufferView(
            &cbvDesc,
            materialHandle
        );

        //============================================================
        // [8] t7 : Normal Map Dummy
        //============================================================
        const D3D12_CPU_DESCRIPTOR_HANDLE normalHandle =
            GetCPUHandle(MaterialDescriptor::Normal);

        Direct3D::getDevice()->CreateShaderResourceView(
            Direct3D::getWhiteTexture(),
            Direct3D::getWhiteTextueViewDesc(),
            normalHandle
        );

        //============================================================
        //   t13 : Local Light StructuredBuffer
        //   t14 : Spot Light Shadow
        //   t15 : Point Light Shadow
        //============================================================
        {
            ID3D12Device* device = Direct3D::getDevice();

            assert(device != nullptr);

            //========================================================
            // Descriptor 1個分のサイズ
            //========================================================
            const UINT descriptorSize =
                device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            //========================================================
            // [14] t13 : Local Light StructuredBuffer
            //
            // Point / Spot Light本体の情報。
            //========================================================
            LocalLightManager::WriteSRV(GetCPUHandle(MaterialDescriptor::LocalLightBuffer));


            //========================================================
            // [15] t14 : Spot Light Shadow Texture2DArray
            //========================================================
            SpotLightShadowManager::WriteSRV(GetCPUHandle(MaterialDescriptor::SpotLightShadow));


            //========================================================
            // [16] t15 : Point Light Shadow Texture2DArray
            //========================================================
            PointLightShadowManager::WriteSRV(GetCPUHandle(MaterialDescriptor::PointLightShadow));
        }


    }

    //========================================================
    //マテリアル値初期化
    //========================================================
    XMStoreFloat4(&pMaterials[0].pMaterialBuffer->Diffuse, XMVectorSet(1, 1, 1, 1));
    XMStoreFloat4(&pMaterials[0].pMaterialBuffer->Ambient, XMVectorSet(0, 0, 0, 0));
    XMStoreFloat4(&pMaterials[0].pMaterialBuffer->Specular, XMVectorSet(0, 0, 0, 1));
    XMStoreFloat4(&pMaterials[0].pMaterialBuffer->TilingOffset, XMVectorSet(1, 1, 0, 0));
    XMStoreFloat4(&pMaterials[0].pMaterialBuffer->ShadowParam, XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));
    XMStoreFloat4(&pMaterials[0].pMaterialBuffer->NormalParam, pMaterials[0].NormalParam);
    //========================================================
    // マトリクス / ライト用定数バッファ
    //========================================================
    {
        D3D12_HEAP_PROPERTIES heapProp = {};
        heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProp.CreationNodeMask = 1;
        heapProp.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Width = ((sizeof(stConstantBuffer) + 0xff) & ~0xff);
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.Format = DXGI_FORMAT_UNKNOWN;
        resDesc.SampleDesc.Count = 1;
        resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = Direct3D::getDevice()->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&constBuff)
        );
        assert(hr == S_OK);

        constBuff->SetName(L"vnEffect::constBuff");

        hr = constBuff->Map(0, nullptr, (void**)&pConstBuffer);
        assert(hr == S_OK);
    }

    //========================================================
    // 頂点バッファ
    //========================================================
    {
        const int vnum = ParticleMax * 4;
        const UINT vertexBufferSize = sizeof(vnVertex3D) * vnum;

        D3D12_HEAP_PROPERTIES heapprop = {};
        heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC resdesc = {};
        resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resdesc.Width = vertexBufferSize;
        resdesc.Height = 1;
        resdesc.DepthOrArraySize = 1;
        resdesc.MipLevels = 1;
        resdesc.Format = DXGI_FORMAT_UNKNOWN;
        resdesc.SampleDesc.Count = 1;
        resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = Direct3D::getDevice()->CreateCommittedResource(
            &heapprop,
            D3D12_HEAP_FLAG_NONE,
            &resdesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertBuff)
        );
        assert(hr == S_OK);

        vertBuff->SetName(L"vnEffect::vertBuff");
        hr = vertBuff->Map(0, nullptr, reinterpret_cast<void**>(&vtx));
        assert(hr == S_OK);

        vertexBufferView.BufferLocation = vertBuff->GetGPUVirtualAddress();
        vertexBufferView.StrideInBytes = sizeof(vnVertex3D);
        vertexBufferView.SizeInBytes = vertexBufferSize;
    }

    //========================================================
    // インデックスバッファ
    //========================================================
    {
        const int inum = ParticleMax * 6;
        const UINT indexBufferSize = sizeof(WORD) * inum;

        D3D12_HEAP_PROPERTIES heapprop = {};
        heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC resdesc = {};
        resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resdesc.Width = indexBufferSize;
        resdesc.Height = 1;
        resdesc.DepthOrArraySize = 1;
        resdesc.MipLevels = 1;
        resdesc.Format = DXGI_FORMAT_UNKNOWN;
        resdesc.SampleDesc.Count = 1;
        resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = Direct3D::getDevice()->CreateCommittedResource(
            &heapprop,
            D3D12_HEAP_FLAG_NONE,
            &resdesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&idxBuffer)
        );
        assert(hr == S_OK);

        idxBuffer->SetName(L"vnEffect::idxBuffer");
        hr = idxBuffer->Map(0, nullptr, (void**)&idx);
        assert(hr == S_OK);

        indexBufferView.BufferLocation = idxBuffer->GetGPUVirtualAddress();
        indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        indexBufferView.SizeInBytes = indexBufferSize;

        //============================================================
        // Particle Quad Indexを一度だけ作成
        // Index Bufferの内容はParticleの生死によって変わらない
        // ※毎フレーム書き換える必要はない
        //============================================================
        for (int particleIndex = 0; particleIndex < ParticleMax; ++particleIndex)
        {
            const WORD baseVertex = static_cast<WORD>(particleIndex * 4);

            const int indexOffset = particleIndex * 6;


            //========================================================
            // Triangle 1
            //========================================================
            idx[indexOffset + 0] = baseVertex + 0;
            idx[indexOffset + 1] = baseVertex + 1;
            idx[indexOffset + 2] = baseVertex + 2;

            //========================================================
            // Triangle 2
            //========================================================
            idx[indexOffset + 3] = baseVertex + 1;
            idx[indexOffset + 4] = baseVertex + 3;
            idx[indexOffset + 5] = baseVertex + 2;
        }
    }


    //========================================================
    // 頂点データ初期化
    //========================================================
    for (int i = 0; i < ParticleMax; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            vtx[i * 4 + j].x = 0.0f;
            vtx[i * 4 + j].y = 0.0f;
            vtx[i * 4 + j].z = 0.0f;
            vtx[i * 4 + j].nx = 0.0f;
            vtx[i * 4 + j].ny = 0.0f;
            vtx[i * 4 + j].nz = 0.0f;
            vtx[i * 4 + j].r = 1.0f;
            vtx[i * 4 + j].g = 1.0f;
            vtx[i * 4 + j].b = 1.0f;
            vtx[i * 4 + j].a = 1.0f;
            vtx[i * 4 + j].tx = 1.0f;
            vtx[i * 4 + j].ty = 0.0f;
            vtx[i * 4 + j].tz = 0.0f;
            vtx[i * 4 + j].tw = 1.0f;
        }

        vtx[i * 4 + 0].u = 0.0f; vtx[i * 4 + 0].v = 0.0f;
        vtx[i * 4 + 1].u = 1.0f; vtx[i * 4 + 1].v = 0.0f;
        vtx[i * 4 + 2].u = 0.0f; vtx[i * 4 + 2].v = 1.0f;
        vtx[i * 4 + 3].u = 1.0f; vtx[i * 4 + 3].v = 1.0f;
    }

    //========================================================
    // 描画フラグ
    //========================================================
    setRenderFlag(eRenderFlag::Transparent, true);
    setRenderFlag(eRenderFlag::Lighting, false);
    setRenderFlag(eRenderFlag::ZWrite, false);

    if (Desc.Visual.Additive)
    {
        setRenderFlag(eRenderFlag::Addition, true);
    }

    IndexNum = 0;
    renderParticleNum = 0;

    setCastShadow(false);
    setReceiveShadow(false);
}

//============================================================
// デストラクタ
//============================================================
Emitter::~Emitter()
{
    delete[] pParticle;
    pParticle = nullptr;

    if (pMaterials != nullptr)
    {
        //========================================================
        // MaterialBufferがMapされている場合は解除
        //========================================================
        if (pMaterials[0].materialBuff != nullptr &&
            pMaterials[0].pMaterialBuffer != nullptr)
        {
            pMaterials[0].materialBuff->Unmap(
                0,
                nullptr
            );

            pMaterials[0].pMaterialBuffer = nullptr;
        }

        SAFE_RELEASE(pMaterials[0].materialBuff);
        SAFE_RELEASE(pMaterials[0].texbuff);
        SAFE_RELEASE(pMaterials[0].normalTexBuff);
        SAFE_RELEASE(pMaterials[0].basicDescHeap);

        delete[] pMaterials;
        pMaterials = nullptr;
    }

    //============================================================
    // 行列定数バッファもMapしているので、
    // 必要ならUnmapしてから解放する
    //============================================================
    if (constBuff != nullptr &&
        pConstBuffer != nullptr)
    {
        constBuff->Unmap(
            0,
            nullptr
        );

        pConstBuffer = nullptr;
    }

    SAFE_RELEASE(constBuff);
    SAFE_RELEASE(vertBuff);
    SAFE_RELEASE(idxBuffer);
}

//============================================================
// emit 操作
//============================================================
void Emitter::setEmit(bool flag)
{
    if (!flag)
    {
        mEmitAccumulator = 0.0f;
    }
    Desc.emit = flag;
}

bool Emitter::isEmit()
{
    return Desc.emit;
}

void Emitter::EmitOnce()
{
    Desc.Mode = EmitMode::Once;
    Desc.emit = true;
}

//============================================================
// 外部調整用
//============================================================
void Emitter::SetEmitCount(int n)
{
    Desc.EmitCount = (std::max)(0, n);
}

void Emitter::SetDirection(const XMVECTOR& dir)
{
    Desc.Direction = dir;
}

void Emitter::SetSpreadAngle(float rad)
{
    Desc.SpreadAngle = rad;
    Desc.Cone.SpreadAngle = rad;
}

void Emitter::SetCircleRadius(float r)
{
    r = (std::max)(0.0f, r);
    Desc.Radius = r;
    Desc.Circle.Radius = r;
}

float Emitter::GetCircleRadius() const
{
    return GetCircleRadiusInternal();
}

void Emitter::SetSizeRange(float minS, float maxS)
{
    Desc.Size.Min = (std::max)(0.0f, minS);
    Desc.Size.Max = (std::max)(Desc.Size.Min, maxS);
}

void Emitter::SetPressUp(float minUp, float maxUp)
{
    Desc.Press.Up.Min = minUp;
    Desc.Press.Up.Max = maxUp;
}

void Emitter::SetEmitRate(float particlesPerSecond)
{
    Desc.EmitRate = (std::max)(particlesPerSecond, 0.0f);
}

void Emitter::SetAlphaMul(float a)
{
    mAlphaMul = std::clamp(a, 0.0f, 1.0f);
}

float Emitter::GetAlphaMul() const
{
    return mAlphaMul;
}

void Emitter::SetShadowReceiveStrength(float strength)
{
    //========================================================
    // 0～1へ制限
    //========================================================
    mShadowReceiveStrength = (std::clamp)(strength, 0.0f, 1.0f);

    //========================================================
    // 強度0ならReceive Shadow自体も無効にする
    //========================================================
    setReceiveShadow(mShadowReceiveStrength > 0.0f);
}

float Emitter::GetShadowReceiveStrength() const
{
    return 	mShadowReceiveStrength;
}

void Emitter::SetLocalLightStrength(float strength)
{
    mLocalLightStrength = (std::clamp)(strength, 0.0f, 1.0f);
}

float Emitter::GetLocalLightStrength() const
{
    return mLocalLightStrength;
}

void Emitter::SetLocalShadowEnabled(bool enabled)
{
    mLocalShadowEnabled = enabled;
}

bool Emitter::IsLocalShadowEnabled() const
{
    return mLocalShadowEnabled;
}

bool Emitter::HasAliveParticles() const
{
    return mActiveParticleCount > 0;
}

void Emitter::SyncTransform()
{
    calculateLocalMatrix();
    calculateWorldMatrix();
}

int Emitter::AcquireParticleSlot()
{
    //========================================================
    // 空きなし
    //========================================================
    if (mFreeParticleCount <= 0)
    {
        return -1;
    }

    //========================================================
    // Free Stack末尾から取得
    //========================================================
    const int freeListIndex = mFreeParticleCount - 1;

    const int particleIndex = mFreeParticleIndices[freeListIndex];


    //========================================================
    // Free数を減らす
    //========================================================
    --mFreeParticleCount;

    //========================================================
    // Active Listへ追加
    //========================================================
    mActiveParticleIndices[mActiveParticleCount] = particleIndex;

    ++mActiveParticleCount;

    return particleIndex;
}

//============================================================
// ランダム
//============================================================
float Emitter::randFloat(float min, float max)
{
    // NaN 保険
    if (!(min == min) || !(max == max)) return 0.0f;

    if (min > max) std::swap(min, max);
    if (min == max) return min;

    static std::random_device rd;
    static std::mt19937 rng(rd());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

XMVECTOR Emitter::randVec(
    float minX, float minY, float minZ, float minW,
    float maxX, float maxY, float maxZ, float maxW
)
{
    return XMVectorSet(
        randFloat(minX, maxX),
        randFloat(minY, maxY),
        randFloat(minZ, maxZ),
        randFloat(minW, maxW)
    );
}

XMVECTOR Emitter::randVecBetween(const XMVECTOR& minV, const XMVECTOR& maxV)
{
    return XMVectorSet(
        randFloat(XMVectorGetX(minV), XMVectorGetX(maxV)),
        randFloat(XMVectorGetY(minV), XMVectorGetY(maxV)),
        randFloat(XMVectorGetZ(minV), XMVectorGetZ(maxV)),
        randFloat(XMVectorGetW(minV), XMVectorGetW(maxV))
    );
}

void Emitter::ReleaseParticleSlot(int activeListIndex)
{
    //========================================================
    // 不正Index
    //========================================================
    if (activeListIndex < 0 ||
        activeListIndex >= mActiveParticleCount)
    {
        return;
    }


    //========================================================
    // 実際のParticle Index
    //========================================================
    const int particleIndex = mActiveParticleIndices[activeListIndex];

    //========================================================
    // Particleを死亡状態へ
    //========================================================
    pParticle[particleIndex].Life = 0.0f;

    //========================================================
    // Free Listへ戻す
    //========================================================
    mFreeParticleIndices[mFreeParticleCount] = particleIndex; ++mFreeParticleCount;


    //========================================================
    // Active Listから削除
    // ※eraseのように後ろ全部を移動するとO(N)になる
    // ※最後の要素を現在位置へ持ってくる
    // ※Swap & Pop方式にする
    // 例:
    // [5, 8, 12, 20]
    //      ↑8を削除
    // [5, 20, 12]
    //============================================================
    const int lastActiveIndex = mActiveParticleCount - 1;


    mActiveParticleIndices[activeListIndex] = mActiveParticleIndices[lastActiveIndex];

    --mActiveParticleCount;
}

//============================================================
// 内部値取得
//============================================================
float Emitter::GetConeSpreadAngle() const
{
    return Desc.Cone.SpreadAngle;
}

float Emitter::GetCircleRadiusInternal() const
{
    if (Desc.Circle.Radius > 0.0f) return Desc.Circle.Radius;
    return Desc.Radius;
}

//============================================================
// サイズフェード
//============================================================
float Emitter::CalcSizeScale(float overLifeTime) const
{
    switch (Desc.Visual.SizeFade)
    {
    case SizeFadeMode::Normal:
        return overLifeTime;

    case SizeFadeMode::KeepMin:
        return Desc.Visual.SizeFadeMin + overLifeTime * (1.0f - Desc.Visual.SizeFadeMin);

    case SizeFadeMode::Fixed:
        return 1.0f;
    }

    return overLifeTime;
}

//============================================================
// 1粒生成
//============================================================
void Emitter::SpawnOneParticle(int index, const XMVECTOR& worldPos)
{
    Particle& p = pParticle[index];

    //========================================================
    // 基本パラメータ
    //========================================================
    p.Life = randFloat(Desc.Life.Min, Desc.Life.Max);
    p.StartLife = p.Life;
    p.Size = randFloat(Desc.Size.Min, Desc.Size.Max);

    p.color = randVec(
        XMVectorGetX(Desc.Color.Min),
        XMVectorGetY(Desc.Color.Min),
        XMVectorGetZ(Desc.Color.Min),
        XMVectorGetW(Desc.Color.Min),
        XMVectorGetX(Desc.Color.Max),
        XMVectorGetY(Desc.Color.Max),
        XMVectorGetZ(Desc.Color.Max),
        XMVectorGetW(Desc.Color.Max)
    );

    //========================================================
    // 形状別発生
    //========================================================
    switch (Desc.Shape)
    {
    case EmitShape::Sphere:
    {
        XMVECTOR dir = randVec(-1, -1, -1, 0, 1, 1, 1, 0);

        // ゼロベクトル保険
        if (XMVectorGetX(XMVector3LengthSq(dir)) < 0.0001f)
        {
            dir = XMVectorSet(0, 1, 0, 0);
        }

        dir = XMVector3Normalize(dir);
        float speed = randFloat(Desc.Speed.Min, Desc.Speed.Max);

        p.Pos = worldPos;
        p.Vel = dir * speed;
    }
    break;

    case EmitShape::Direction:
    {
        XMVECTOR dir = Desc.Direction;
        if (XMVectorGetX(XMVector3LengthSq(dir)) < 0.0001f)
        {
            dir = XMVectorSet(0, 0, 1, 0);
        }

        dir = XMVector3Normalize(dir);
        float speed = randFloat(Desc.Speed.Min, Desc.Speed.Max);

        p.Pos = worldPos;
        p.Vel = dir * speed;
    }
    break;

    case EmitShape::Cone:
    {
        XMVECTOR dirCenter = Desc.Direction;
        if (XMVectorGetX(XMVector3LengthSq(dirCenter)) < 0.0001f)
        {
            dirCenter = XMVectorSet(0, 0, 1, 0);
        }
        dirCenter = XMVector3Normalize(dirCenter);

        const float spread = GetConeSpreadAngle();

        // 横方向・縦方向にばらけさせる
        float yaw = randFloat(-spread * Desc.Cone.YawScale, spread * Desc.Cone.YawScale);
        float pitch = randFloat(-spread * Desc.Cone.PitchScale, spread * Desc.Cone.PitchScale);

        XMMATRIX rot = XMMatrixRotationX(pitch) * XMMatrixRotationY(yaw);
        XMVECTOR dir = XMVector3TransformNormal(dirCenter, rot);
        dir = XMVector3Normalize(dir);

        // エミッタ自身の回転も反映したいので World の回転成分を使う
        dir = XMVector3TransformNormal(dir, World);
        if (XMVectorGetX(XMVector3LengthSq(dir)) > 0.0001f)
        {
            dir = XMVector3Normalize(dir);
        }

        float speed = randFloat(Desc.Speed.Min, Desc.Speed.Max);
        float upBias = randFloat(Desc.Cone.UpBias.Min, Desc.Cone.UpBias.Max);

        p.Pos = worldPos;
        p.Vel = dir * speed + XMVectorSet(0, upBias, 0, 0);
        p.Vel += randVecBetween(Desc.RandomVelocity.Min, Desc.RandomVelocity.Max);
    }
    break;

    case EmitShape::Circle:
    {
        float angle = randFloat(0.0f, XM_2PI);
        float r = GetCircleRadiusInternal();

        XMVECTOR localOffset = XMVectorSet(
            cosf(angle) * r,
            0.0f,
            sinf(angle) * r,
            0.0f
        );

        p.Pos = worldPos + localOffset;
        p.Vel = XMVectorZero();
    }
    break;

    case EmitShape::Dust:
    {
        float angle = randFloat(0.0f, XM_2PI);
        float r = GetCircleRadiusInternal();

        XMVECTOR offset = XMVectorSet(
            cosf(angle) * r,
            0.0f,
            sinf(angle) * r,
            0.0f
        );

        XMVECTOR dir = offset;
        if (XMVectorGetX(XMVector3LengthSq(dir)) < 0.0001f)
        {
            dir = XMVectorSet(1, 0, 0, 0);
        }
        dir = XMVector3Normalize(dir);

        float speed = randFloat(Desc.Speed.Min, Desc.Speed.Max);

        p.Pos = worldPos + offset;
        p.Vel = dir * speed + XMVectorSet(0, randFloat(8.0f, 14.0f), 0, 0);
        p.Vel += randVecBetween(Desc.RandomVelocity.Min, Desc.RandomVelocity.Max);
    }
    break;

    case EmitShape::Press:
    {
        float angle = randFloat(0.0f, XM_2PI);
        float r = GetCircleRadiusInternal();

        XMVECTOR offset = XMVectorSet(
            cosf(angle) * r,
            0.0f,
            sinf(angle) * r,
            0.0f
        );

        XMVECTOR dir = offset;
        if (XMVectorGetX(XMVector3LengthSq(dir)) < 0.0001f)
        {
            dir = XMVectorSet(1, 0, 0, 0);
        }
        dir = XMVector3Normalize(dir);

        float speed = randFloat(Desc.Speed.Min, Desc.Speed.Max);
        float up = randFloat(Desc.Press.Up.Min, Desc.Press.Up.Max);

        p.Pos = worldPos + offset;
        p.Vel = dir * speed + XMVectorSet(0, up, 0, 0);
        p.Vel += randVecBetween(Desc.RandomVelocity.Min, Desc.RandomVelocity.Max);
    }
    break;
    }
}

//============================================================
// 吸引処理
//============================================================
void Emitter::ApplyAttract(Particle& p, float dt)
{
    if (!Desc.Attract.Enable && !Desc.UseAttract) return;

    XMVECTOR center;
    getWorldPosition(&center);

    XMVECTOR dir = XMVectorSubtract(center, p.Pos);
    float dist = XMVectorGetX(XMVector3Length(dir));

    const float killDistance = Desc.Attract.KillDistance;

    //========================================================
    // 中心に近づいたら消す
    //========================================================
    if (dist <= killDistance)
    {
        p.Life = 0.0f;
        return;
    }

    //========================================================
    // 中心方向へ吸引
    //========================================================
    if (dist > 0.0001f)
    {
        dir = XMVector3Normalize(dir);
    }
    else
    {
        dir = XMVectorSet(0, 0, 1, 0);
    }

    float power = Desc.UseAttract ? Desc.AttractPower : Desc.Attract.Power;
    p.Vel += dir * power * dt;

    //========================================================
    // 渦回転
    //========================================================
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR tangent = XMVector3Cross(up, dir);

    if (XMVectorGetX(XMVector3LengthSq(tangent)) > 0.0001f)
    {
        tangent = XMVector3Normalize(tangent);

        float radius = GetCircleRadiusInternal();
        float swirlScale = 1.0f;
        if (radius > 0.0001f)
        {
            swirlScale = dist / radius;
        }

        p.Vel += tangent *
            (Desc.Attract.SwirlPower * Desc.Attract.SwirlSign * swirlScale) * dt;
    }

    //========================================================
    // 最大速度制限
    //========================================================
    float speed = XMVectorGetX(XMVector3Length(p.Vel));
    if (speed > Desc.Attract.MaxSpeed)
    {
        p.Vel = XMVector3Normalize(p.Vel) * Desc.Attract.MaxSpeed;
    }
}

//============================================================
// 頂点書き込み
//============================================================
void Emitter::WriteParticleVertex(const Particle& particle, float overLifeTime, const XMMATRIX& billboard)
{
    //============================================================
    // Size Fade
    // Normal: 寿命に応じて0まで縮む
    // KeepMin: 最低サイズを残す
    // Fixed: サイズ固定
    //============================================================
    const float sizeScale =
        CalcSizeScale(overLifeTime);


    const float width =
        particle.Size * sizeScale;

    const float height =
        particle.Size * sizeScale;


    //============================================================
    // Billboard Quad
    //============================================================
    XMVECTOR vertices[4];

    vertices[0] =
        XMVectorSet(
            -width,
            height,
            0.0f,
            0.0f
        );

    vertices[1] =
        XMVectorSet(
            width,
            height,
            0.0f,
            0.0f
        );

    vertices[2] =
        XMVectorSet(
            -width,
            -height,
            0.0f,
            0.0f
        );

    vertices[3] =
        XMVectorSet(
            width,
            -height,
            0.0f,
            0.0f
        );


    //============================================================
    // Particle Color
    //============================================================
    float red = XMVectorGetX(particle.color);
    float green = XMVectorGetY(particle.color);
    float blue = XMVectorGetZ(particle.color);
    float alpha = XMVectorGetW(particle.color);


    //============================================================
    // Dark Fade
    //============================================================
    if (Desc.Visual.Dark)
    {
        const float dark = overLifeTime * overLifeTime;
        red *= dark;
        green *= dark;
        blue *= dark;
    }


    //============================================================
    // Alpha Fade
    //============================================================
    alpha *= overLifeTime * mAlphaMul;


    //============================================================
    // 4頂点生成
    //============================================================
    for (int vertexIndex = 0; vertexIndex < 4; ++vertexIndex)
    {
        //========================================================
        // Camera方向へBillboard化
        //========================================================
        vertices[vertexIndex] =
            XMVector3TransformNormal(
                vertices[vertexIndex],
                billboard
            );


        //========================================================
        // Particle位置へ移動
        //========================================================
        vertices[vertexIndex] += particle.Pos;

        //========================================================
        // 描画用ParticleはVertex Bufferの先頭から
        // 隙間なく詰めて配置する。
        //
        // Active Particle Slot番号とは無関係。
        //========================================================
        vnVertex3D& outputVertex = vtx[renderParticleNum * 4 + vertexIndex];

        outputVertex.x = XMVectorGetX(vertices[vertexIndex]);
        outputVertex.y = XMVectorGetY(vertices[vertexIndex]);
        outputVertex.z = XMVectorGetZ(vertices[vertexIndex]);

        outputVertex.r = red;
        outputVertex.g = green;
        outputVertex.b = blue;
        outputVertex.a = alpha;
    }
    //============================================================
    // 描画Particle数を1増やす
    //============================
    renderParticleNum++;
}

//============================================================
// 更新
//============================================================
void Emitter::execute(float dt)
{
    // dt が異常に大きい時の暴走防止
    dt = std::clamp(dt, 0.0f, 0.1f);

    calculateLocalMatrix();
    calculateWorldMatrix();

    XMVECTOR world;
    getWorldPosition(&world);

    //========================================================
    // 新規発生
    //========================================================
    if (Desc.emit)
    {
        // 今フレームに実際に生成するParticle数
        int spawnCount = 0;

        if (Desc.Mode == EmitMode::Continuous)
        {
            // 1秒あたりのParticle数の経過時間をAccumulatorへ蓄積
            mEmitAccumulator += Desc.EmitRate * dt;
            // 整数部分だけ今フレーム生成する
            spawnCount = static_cast<int>(mEmitAccumulator);
            // 今回使った整数部分だけ減らす
            mEmitAccumulator -= static_cast<float>(spawnCount);
        }
        else if (Desc.Mode == EmitMode::Once)
        {
            spawnCount = (std::max)(Desc.EmitCount, 0);
        }

        // 異常な大量生成を防止
        spawnCount = (std::min)(spawnCount, ParticleMax);

        //============================================================
        // Particle生成
        //============================================================
        for (int spawnIndex = 0; spawnIndex < spawnCount; ++spawnIndex)
        {
            //========================================================
            // O(1)で空きSlot取得
            //========================================================
            const int particleIndex = AcquireParticleSlot();

            //========================================================
            // 最大Particle数へ到達
            //========================================================
            if (particleIndex < 0)
            {
                break;
            }

            //========================================================
            // Particle初期化
            //========================================================
            SpawnOneParticle(particleIndex, world);
        }
    }

    //========================================================
    // ビルボード行列
    //========================================================
    XMMATRIX billboard = *vnCamera::getView();

    // 平行移動を消して、カメラの回転だけ使う
    billboard.r[3] = XMVectorSet(0, 0, 0, 1);

    // 回転のみなら逆行列は転置でOK
    billboard = XMMatrixTranspose(billboard);

    //============================================================
    // 生存Particleのみ更新
    //============================================================
    renderParticleNum = 0;

    //============================================================
    // activeListIndexはfor末尾で必ず++しない
    // Particle削除時はSwapによって別Particleが
    // 同じ位置へ移動してくるため
    //============================================================
    int activeListIndex = 0;

    while (activeListIndex < mActiveParticleCount)
    {
        //========================================================
        // 実際のParticle Index
        //========================================================
        const int particleIndex = mActiveParticleIndices[activeListIndex];


        Particle& particle = pParticle[particleIndex];


        //========================================================
        // Life更新
        //========================================================
        particle.Life -= dt;


        //========================================================
        // 寿命終了
        // Free Listへ返し、このactiveListIndexは進めない
        // ※ReleaseParticleSlot()によって
        // ※最後のActive Particleがこの位置へ移動してくる
        //========================================================
        if (particle.Life <= 0.0f)
        {
            ReleaseParticleSlot(activeListIndex);
            continue;
        }


        //========================================================
        // 0～1の残り寿命
        //========================================================
        const float overLifeTime =
            particle.Life / (std::max)(particle.StartLife, 0.00001f);


        //========================================================
        // Attract
        //========================================================
        if (Desc.UseAttract)
        {
            XMVECTOR center = world;

            XMVECTOR directionToCenter =
                XMVectorSubtract(
                    center,
                    particle.Pos
                );


            const float distance = XMVectorGetX(XMVector3Length(directionToCenter));


            if (distance > 7.5f)
            {
                directionToCenter = XMVector3Normalize(directionToCenter);


                particle.Vel += directionToCenter * Desc.AttractPower * dt;


                const XMVECTOR up =
                    XMVectorSet(
                        0.0f,
                        1.0f,
                        0.0f,
                        0.0f
                    );


                XMVECTOR tangent =
                    XMVector3Cross(
                        up,
                        directionToCenter
                    );

                tangent = XMVector3Normalize(tangent);

                const float safeRadius = (std::max)(Desc.Radius, 0.001f);


                const float swirlPower = 10.0f * (distance / safeRadius);


                particle.Vel += tangent * swirlPower * dt;


                //================================================
                // 最大速度
                //================================================
                constexpr float MaxSpeed = 50.0f;

                const float speed = XMVectorGetX(XMVector3Length(particle.Vel));

                if (speed > MaxSpeed)
                {
                    particle.Vel = XMVector3Normalize(particle.Vel) * MaxSpeed;
                }
            }
            else
            {
                //================================================
                // 中心到達
                //================================================
                ReleaseParticleSlot(activeListIndex);
                continue;
            }
        }


        //========================================================
        // Particle移動
        //========================================================
        particle.Pos += particle.Vel * dt;

        WriteParticleVertex(
            particle,
            overLifeTime,
            billboard
        );

        //========================================================
        // Particleが正常に更新された場合だけ次へ進む
        //========================================================
        ++activeListIndex;
    }

    IndexNum = renderParticleNum * 6;

    //========================================================
    // Once モードは1フレームだけ emit
    //========================================================
    if (Desc.Mode == EmitMode::Once)
    {
        Desc.emit = false;
    }
}

//============================================================
// 頂点設定
//============================================================
void Emitter::setVertexPosition()
{
    // execute()側で動的頂点を書き込むので空でOK
}

//============================================================
// 描画
//============================================================
void Emitter::render()
{
    if (IndexNum <= 0) return;

    calculateLocalMatrix();
    calculateWorldMatrix();

    // Effect Material Shadow Parameter更新
    XMStoreFloat4(
        &pMaterials[0].pMaterialBuffer->ShadowParam,
        XMVectorSet(isReceiveShadow() ? 1.0f : 0.0f,
            mShadowReceiveStrength,
            mLocalLightStrength,
            mLocalShadowEnabled ? 1.0f : 0.0f
        ));

    //========================================================
    // パーティクル頂点は execute() でワールド座標化済み
    // World は Identity 扱いでスクリーンへ送る
    //========================================================
    XMMATRIX W = XMMatrixIdentity();
    XMMATRIX WVP = XMMatrixMultiply(W, *vnCamera::getScreen());
    XMMATRIX worldView = XMMatrixMultiply(W, *vnCamera::getView());
    setVertexPosition();

    //========================================================
    // 行列定数バッファ
    //========================================================
    XMStoreFloat4x4(&pConstBuffer->WVP, XMMatrixTranspose(WVP));
    XMStoreFloat4x4(&pConstBuffer->World, XMMatrixTranspose(W));
    XMStoreFloat4x4(&pConstBuffer->WorldView, XMMatrixTranspose(worldView));

    if (ShadowMapManager::IsValid())
    {
        for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
        {
            //====================================================
            // CascadeごとのLight View Projection取得
            //====================================================
            const XMMATRIX lightViewProjection = ShadowMapManager::GetLightViewProj(cascadeIndex);
            XMStoreFloat4x4(&pConstBuffer->LightWVP[cascadeIndex], XMMatrixTranspose(lightViewProjection));
        }
    }
    else
    {
        //========================================================
        // Shadow Mapが存在しないScene
        //========================================================
        for (UINT cascadeIndex = 0u; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
        {
            XMStoreFloat4x4(&pConstBuffer->LightWVP[cascadeIndex], XMMatrixIdentity());
        }
    }

    //========================================================
    // ・Root 1 : 行列/ライト用CBV
    // ・Root 2 : SRV + MaterialCBV のディスクリプタテーブル
    //========================================================
    Direct3D::getCommandList()->SetGraphicsRootConstantBufferView(
        1,
        constBuff->GetGPUVirtualAddress()
    );

    Direct3D::getCommandList()->SetPipelineState(pPipelineState[renderFlag]);
    Direct3D::getCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


    //============================================================
    // Directional CSM Descriptor更新
    // Static / Dynamic Shadow Mapはフレームによって
    // 更新内容が変化するため、描画時に実際のSRVをコピーする
    //============================================================
    if (isReceiveShadow() && ShadowMapManager::IsValid())
    {
        ID3D12Device* device = Direct3D::getDevice();

        assert(device != nullptr);


        //========================================================
        // Descriptor 1個分のサイズ
        //========================================================
        const UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //========================================================
        // Effect Material Heap先頭
        //========================================================
        const D3D12_CPU_DESCRIPTOR_HANDLE heapStart = pMaterials[0].basicDescHeap->GetCPUDescriptorHandleForHeapStart();


        //========================================================
        // Static CSM
        //
        // Destination:
        //
        //   [1] StaticShadow0
        //   [2] StaticShadow1
        //   [3] StaticShadow2
        //
        // Source: ShadowMapManager Static Layer
        //========================================================
        {
            D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle =
                heapStart;

            destinationHandle.ptr +=
                static_cast<SIZE_T>(MaterialDescriptor::StaticShadow0) *
                static_cast<SIZE_T>(descriptorSize);


            const D3D12_CPU_DESCRIPTOR_HANDLE sourceHandle = ShadowMapManager::GetSRVCopyCPUHandle(0, ShadowMapLayer::Static);


            //====================================================
            // Cascade 3枚をまとめてコピー
            //====================================================
            device->CopyDescriptorsSimple(
                SHADOW_CASCADE_COUNT,
                destinationHandle,
                sourceHandle,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );
        }


        //========================================================
        // Dynamic CSM
        //
        // Destination:
        //
        //   [5] DynamicShadow0
        //   [6] DynamicShadow1
        //   [7] DynamicShadow2
        //========================================================
        {
            D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle = heapStart;

            destinationHandle.ptr +=
                static_cast<SIZE_T>(MaterialDescriptor::DynamicShadow0) *
                static_cast<SIZE_T>(descriptorSize);


            const D3D12_CPU_DESCRIPTOR_HANDLE sourceHandle = ShadowMapManager::GetSRVCopyCPUHandle(0, ShadowMapLayer::Dynamic);

            device->CopyDescriptorsSimple(
                SHADOW_CASCADE_COUNT,
                destinationHandle,
                sourceHandle,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );
        }
    }


    Direct3D::getCommandList()->SetDescriptorHeaps(1, &pMaterials[0].basicDescHeap);
    Direct3D::getCommandList()->SetGraphicsRootDescriptorTable(
        2,
        pMaterials[0].basicDescHeap->GetGPUDescriptorHandleForHeapStart()
    );

    Direct3D::getCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
    Direct3D::getCommandList()->IASetIndexBuffer(&indexBufferView);

    Direct3D::getCommandList()->DrawIndexedInstanced(IndexNum, 1, 0, 0, 0);
}
