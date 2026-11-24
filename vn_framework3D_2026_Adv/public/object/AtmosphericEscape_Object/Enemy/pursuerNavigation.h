#pragma once

class INavigationDoor;

//============================================================
// Pursuer Navigation Node
// Monsterが移動可能なWaypoint
//============================================================
struct PursuerNavNode
{
    //========================================================
    // Node ID
    //========================================================
    std::string Id;

    //========================================================
    // World Position
    //========================================================
    XMFLOAT3 Position =
    {
        0.0f,
        0.0f,
        0.0f
    };

    //========================================================
    // 接続先Node Index
    // CSV Load後にID → Indexへ変換する
    //========================================================
    std::vector<std::size_t> Links;
};


//============================================================
// PursuerNavigation
// 追跡者用Navigation Graph
//============================================================
class PursuerNavigation
{
public:

    //========================================================
    // Navigation CSV Load
    //========================================================
    bool Load(const wchar_t* filePath);


    //========================================================
    // Clear
    //========================================================
    void Clear();


    //========================================================
    // 最寄りNode
    // return: SIZE_MAX = Nodeなし
    //========================================================
    std::size_t FindNearestNode(const XMFLOAT3& position) const;


    //========================================================
    // Path Search
    // startPosition: Monster現在位置
    // goalPosition: Player / Noise / Patrolなどの目的地
    // outPath: 通過するWorld Position
    //========================================================
    bool FindPath(
        const XMFLOAT3& startPosition,
        const XMFLOAT3& goalPosition,
        std::vector<XMFLOAT3>& outPath
    ) const;


    //========================================================
    // Node数
    //========================================================
    std::size_t GetNodeCount() const
    {
        return mNodes.size();
    }

    //============================================================
    // Navigation Door
    // Dijkstra中にLocked Doorを通過するEdgeを
    // 無効化するために使用する。
    //============================================================
    void SetNavigationDoors(const std::vector<INavigationDoor*>* doors)
    {
        mNavigationDoors = doors;
    }

private:

    //============================================================
    // 指定Edgeを現在通行できるか
    //============================================================
    bool IsEdgeTraversable(const XMFLOAT3& from, const XMFLOAT3& to) const;

private:

    //========================================================
    // Nodes
    //========================================================
    std::vector<PursuerNavNode> mNodes;

    //============================================================
    // World側が所有するDoor一覧
    //============================================================
    const std::vector<INavigationDoor*>* mNavigationDoors = nullptr;

    //============================================================
    // DoorとNavigation Edgeが交差しているとみなす幅
    // HandleNearbyDoorのmDoorPathWidthとほぼ同等
    //============================================================
    float mDoorEdgeWidth = 0.90f;

    //============================================================
    // Multi Floor対策
    // 同じXZ座標でも別FloorのDoorなら
    // Edgeを塞がない
    //============================================================
    float mDoorVerticalTolerance = 1.5f;
};