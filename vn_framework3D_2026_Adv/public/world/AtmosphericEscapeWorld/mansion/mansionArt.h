#pragma once

class vnScene;
class ModelBase;

//============================================================
// MansionArtPlacement
// 高品質な屋敷モデル1個分の配置データ
//============================================================
struct MansionArtPlacement
{
    // Art Object固有ID
    std::string Id;

    // VNMの保存先
    std::wstring Folder;
    std::wstring File;

    // World Transform
    XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 Rotation = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };

    // Static Shadowを落とすか
    bool CastShadow = true;
};

//============================================================
// MansionArt
// 屋敷の静的なVisual Modelを管理する
//============================================================
class MansionArt
{
public:
    MansionArt() = default;
    ~MansionArt() = default;

    // 静的Art Modelを1個追加
    bool AddStaticModel(
        vnScene* scene,
        const MansionArtPlacement& placement,
        ModelBase** createdModel = nullptr
    );

    // 登録済みArt Modelをすべて削除
    void Terminate(vnScene* scene);

private:
    // IDからModelを参照するための非所有Map
    std::unordered_map<std::string, ModelBase*> mModels;
};
