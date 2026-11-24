
#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "mansionGraybox.h"

//============================================================
// Destructor
//============================================================
MansionGraybox::~MansionGraybox() = default;

//============================================================
// Initialize
//============================================================
bool MansionGraybox::Initialize(vnScene* scene)
{
    //========================================================
    // 既存の呼び出しインターフェースを維持する
    //========================================================
    if (scene == nullptr)
    {
        return false;
    }

    return true;
}

//============================================================
// Terminate
//============================================================
void MansionGraybox::Terminate(vnScene* scene)
{
    (void)scene;

    //========================================================
    // 検索用ポインタを破棄
    //========================================================
    mBlockById.clear();

    //========================================================
    // Colliderを破棄
    //========================================================
    mColliders.clear();
}

//============================================================
// Static Block追加
//============================================================
bool MansionGraybox::AddStaticBlock(
    vnScene* scene,
    const std::string& id,
    const XMFLOAT3& position,
    const XMFLOAT3& scale
)
{
    //========================================================
    // sceneは既存APIとの互換性のため保持する
    // ※この関数内ではScene登録を行わない
    //========================================================
    (void)scene;

    if (id.empty())
    {
        return false;
    }

    //========================================================
    // 同じIDのColliderを重複して作らない
    //========================================================
    if (mBlockById.find(id) != mBlockById.end())
    {
#if defined(_DEBUG)
        const std::string message =
            "[MansionGraybox] Duplicate Block ID : " +
            id + "\n";

        OutputDebugStringA(message.c_str());
#endif

        return false;
    }

    //========================================================
    // CSVのScaleはFull Size
    // BoxColliderに渡すのはHalf Extents
    //========================================================
    const XMVECTOR halfExtents = XMVectorSet(
        scale.x * 0.5f,
        scale.y * 0.5f,
        scale.z * 0.5f,
        0.0f
    );

    //========================================================
    // 静的Collider生成
    // 第一引数は固定World座標
    // vnModelもvnObjectも作成しない
    //========================================================
    auto collider = std::make_unique<BoxCollider>(position, halfExtents);

    //========================================================
    // 通常のBody Collider
    //========================================================
    collider->SetType(ColliderType::Body);

    //========================================================
    // World Collider Tag
    // CollisionManager側で静的World Collider同士を除外するため使用
    //========================================================
    collider->SetTag(ColliderTag::World);

    //========================================================
    // Surface
    //========================================================
    collider->SetSurfaceType(SurfaceType::Default);

#if defined(_DEBUG)

    //============================================================
    // DEBUG - 床Colliderの座標確認
    // CSVのPositionが固定中心に設定されているか確認
    // 最初の床Colliderだけ表示する
    //============================================================
    static bool sLoggedFloor = false;

    if (!sLoggedFloor && id.find("Floor") != std::string::npos)
    {
        sLoggedFloor = true;

        const XMVECTOR center = collider->GetCenter();
        const XMVECTOR half = collider->GetHalfSize();

        char buffer[512];

        sprintf_s(
            buffer,
            "[MansionFloor] ID=%s Center=(%.2f, %.2f, %.2f) Half=(%.2f, %.2f, %.2f)\n",
            id.c_str(),
            XMVectorGetX(center),
            XMVectorGetY(center),
            XMVectorGetZ(center),
            XMVectorGetX(half),
            XMVectorGetY(half),
            XMVectorGetZ(half)
        );

        OutputDebugStringA(buffer);
    }

#endif


    //========================================================
    // IDから参照できるようポインタを保存する
    // unique_ptr本体はvector内で管理
    //========================================================
    BoxCollider* colliderPtr = collider.get();

    mColliders.push_back(std::move(collider));

    mBlockById.emplace(id, colliderPtr );

    return true;
}

//============================================================
// SetBlockVisualEnabled
//============================================================
bool MansionGraybox::SetBlockVisualEnabled(const std::string& id,bool enabled)
{
    (void)enabled;

    return mBlockById.find(id) != mBlockById.end();
}

//============================================================
// Append Colliders
//============================================================
void MansionGraybox::AppendColliders(std::vector<Collider*>& destination) const
{
    for (const auto& collider : mColliders)
    {
        if (collider == nullptr)
        {
            continue;
        }

        //====================================================
        // Colliderの所有権はWorld側に残す
        // Scene / CollisionManagerには非所有ポインタだけ渡す
        //====================================================
        destination.push_back(collider.get());
    }
}
