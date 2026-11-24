
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

class vnScene;
class Collider;
class BoxCollider;

//============================================================
// MansionGraybox
// CSVのMansionBlockをColliderへ変換する管理クラス
// ・Colliderのみ保持する
// ・静的AABBとしてCollisionManagerへ渡す
//============================================================
class MansionGraybox
{
public:

    MansionGraybox() = default;

    //========================================================
    // BoxColliderの完全な定義があるcpp側で定義する
    //========================================================
    ~MansionGraybox();

    bool Initialize(vnScene* scene);

    void Terminate(vnScene* scene);

    //========================================================
    // CSVのMansionBlockから静的Collider生成
    //========================================================
    bool AddStaticBlock(
        vnScene* scene,
        const std::string& id,
        const XMFLOAT3& position,
        const XMFLOAT3& scale
    );

    //========================================================
    // 旧CSVとの互換性
    // Grayboxの描画は存在しないためID確認のみ行う
    //========================================================
    bool SetBlockVisualEnabled(
        const std::string& id,
        bool enabled
    );

    //========================================================
    // CollisionManagerへColliderを提供する
    // 所有権は移動しない
    //========================================================
    void AppendColliders(std::vector<Collider*>& destination) const;

private:

    //========================================================
    // Colliderを所有する
    //========================================================
    std::vector<std::unique_ptr<BoxCollider>> mColliders;

    //========================================================
    // ID検索用
    // BoxColliderの所有権はmColliders側にある
    //========================================================
    std::unordered_map<std::string, BoxCollider*> mBlockById;
};
