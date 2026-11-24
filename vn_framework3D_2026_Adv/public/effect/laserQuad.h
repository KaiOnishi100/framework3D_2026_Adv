#pragma once

class LaserQuad : public ObjectBase
{
public:
    struct Desc
    {
        WCHAR Texture[64] = L"";
        float Thickness = 12.0f;          // レーザー太さ（世界単位）
        XMVECTOR Color = XMVectorSet(0.55f, 0.85f, 1.0f, 1.0f); // 青白
        float UvScrollSpeed = 0.0f;      // 0ならスクロール無し
        bool  Additive = true;          
        bool  ZWrite = false;            
    };

    LaserQuad(const Desc& desc);
    ~LaserQuad() override;

    // レーザーの開始・終了（ワールド座標）
    void SetSegment(const XMVECTOR& start, const XMVECTOR& end);

    void LoadTexture(const WCHAR* path);
    void execute(float dt);
    void render() override;

private:
    void BuildBuffers();

private:
    Desc mDesc;

    XMVECTOR mStart = XMVectorZero();
    XMVECTOR mEnd = XMVectorSet(0, 0, 1, 0);

    float mUvOffset = 0.0f;

    vnVertex3D* vtx = nullptr;
    WORD* idx = nullptr;

    ID3D12Resource* vertBuff = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vbView = {};

    ID3D12Resource* idxBuff = nullptr;
    D3D12_INDEX_BUFFER_VIEW ibView = {};

    ID3D12Resource* texbuff = nullptr;
    ID3D12Resource* constBuff = nullptr;
    ID3D12Resource* materialBuff = nullptr;
    ID3D12DescriptorHeap* basicDescHeap = nullptr;
    stConstantBuffer* pConstBuffer = nullptr;
    stMaterialBuffer* pMaterialBuffer = nullptr;

    TexMetadata mTexMeta = {};
    ScratchImage mScratch = {};
};