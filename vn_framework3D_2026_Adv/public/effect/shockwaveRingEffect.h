#pragma once

//============================================================
// Shockwave用リング（地面固定クアッド）
// ・ビルボードしない
// ・XZ平面に固定
// ・サイズ拡大＋フェード用
//============================================================
class ShockwaveRingQuad : public ObjectBase
{
public:
    struct Desc
    {
        WCHAR Texture[64] = L"";     // リング画像
        float Size = 10.0f;          // 初期サイズ（直径）
        float Y = -1.5f;             // 地面高さ
        XMVECTOR Color = XMVectorSet(1, 1, 1, 1);
        bool Additive = true;
        bool ZWrite = false;
    };

public:
    ShockwaveRingQuad(const Desc& desc);
    ~ShockwaveRingQuad() = default;

    //====================================================
    // 設定
    //====================================================
    void SetCenter(const XMVECTOR& pos);
    void SetSize(float s);
    void SetAlpha(float a);
    void SetColor(const XMVECTOR& c);

    void render() override;

private:
    float mSize = 10.0f;
    float mY = -1.5f;
    XMVECTOR mColor = XMVectorSet(1, 1, 1, 1);

    // 中心位置（XZのみ使う）
    XMVECTOR mCenter = XMVectorZero();

    // 頂点
    vnVertex3D mVertices[4];
    WORD mIndices[6] = { 0,1,2, 1,3,2 };

    ID3D12Resource* mVB = nullptr;
    ID3D12Resource* mIB = nullptr;

    D3D12_VERTEX_BUFFER_VIEW mVBView = {};
    D3D12_INDEX_BUFFER_VIEW  mIBView = {};

    ID3D12Resource* mTex = nullptr;
    ID3D12DescriptorHeap* mDescHeap = nullptr;
    ID3D12Resource* mConstBuff = nullptr;
    stConstantBuffer* mCB = nullptr;
};
