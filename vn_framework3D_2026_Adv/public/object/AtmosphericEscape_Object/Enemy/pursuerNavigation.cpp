#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "pursuerNavigation.h"

//============================================================
// CSVの1行をCommaで分割
//============================================================
static std::vector<std::string> SplitCSVLine(const std::string& line)
{
    std::vector<std::string> result;

    std::stringstream stream(line);

    std::string value;

    while (std::getline(stream, value, ','))
    {
        result.push_back(value);
    }

    return result;
}


//============================================================
// "|" 区切り
// Nav01|Nav02|Nav03
// のようなLinksを分解する
//============================================================
static std::vector<std::string> SplitLinks(const std::string& text)
{
    std::vector<std::string> result;

    std::stringstream stream(text);

    std::string value;

    while (std::getline(stream, value, '|'))
    {
        if (!value.empty())
        {
            result.push_back(value);
        }
    }

    return result;
}


//============================================================
// Load
//============================================================
bool PursuerNavigation::Load(const wchar_t* filePath)
{
    Clear();


    if (filePath == nullptr)
    {
        return false;
    }

    std::ifstream file
    {
        std::filesystem::path
        {
            filePath
        }
    };

    if (!file.is_open())
    {
        OutputDebugStringW(
            L"[PursuerNavigation] "
            L"Navigation CSV open failed.\n"
        );

        return false;
    }


    //========================================================
    // 一時データ
    // Linksはまだ文字列IDのまま保持する
    //========================================================
    struct TemporaryNode
    {
        PursuerNavNode Node;

        std::vector<std::string> LinkIds;
    };


    std::vector<TemporaryNode> temporaryNodes;


    std::string line;


    //========================================================
    // Header skip
    //========================================================
    if (!std::getline(file, line))
    {
        return false;
    }


    //========================================================
    // Rows
    //========================================================
    while (std::getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }


        const auto values = SplitCSVLine(line);


        //====================================================
        // Id,X,Y,Z,Links
        //====================================================
        if (values.size() < 5)
        {
            continue;
        }


        TemporaryNode temp;


        temp.Node.Id = values[0];


        temp.Node.Position =
        {
            std::stof(values[1]),
            std::stof(values[2]),
            std::stof(values[3])
        };


        temp.LinkIds = SplitLinks(values[4]);


        temporaryNodes.push_back(std::move(temp));
    }


    if (temporaryNodes.empty())
    {
        return false;
    }


    //========================================================
    // ID → Index
    //========================================================
    std::unordered_map<std::string, std::size_t > indexMap;

    for (std::size_t i = 0; i < temporaryNodes.size(); ++i)
    {
        const std::string& nodeId = temporaryNodes[i].Node.Id;

        //========================================================
        // Duplicate ID Check
        //========================================================
        if (indexMap.find(nodeId) != indexMap.end())
        {
#if defined(_DEBUG)

            std::wstring message =
                L"[PursuerNavigation] "
                L"Duplicate navigation node ID : ";

            message += std::wstring(nodeId.begin(), nodeId.end());

            message += L"\n";

            OutputDebugStringW(
                message.c_str()
            );

#endif

            return false;
        }

        indexMap.emplace(nodeId, i );
    }

    //========================================================
    // 実Node作成
    //========================================================
    mNodes.resize(temporaryNodes.size());


    for (std::size_t i = 0; i < temporaryNodes.size(); ++i)
    {
        mNodes[i] = temporaryNodes[i].Node;


        //====================================================
        // Link ID → Node Index
        //====================================================
        for (const std::string& linkId : temporaryNodes[i].LinkIds)
        {
            const auto found = indexMap.find(linkId);


            //================================================
            // 存在しないNode IDは無視
            // ※Debug時はログを出してもよい
            //================================================
            if (found == indexMap.end())
            {
                continue;
            }


            mNodes[i].Links.push_back(found->second);
        }
    }


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[PursuerNavigation] "
        L"Navigation loaded.\n"
    );

#endif


    return true;
}

std::size_t PursuerNavigation::FindNearestNode(const XMFLOAT3& position) const
{
    if (mNodes.empty())
    {
        return SIZE_MAX;
    }

    //============================================================
    // Floor判定用Vertical Tolerance
    //============================================================
    constexpr float VerticalTolerance = 1.6f;

    std::size_t bestIndex = SIZE_MAX;

    float bestDistanceSq = FLT_MAX;


    for (std::size_t i = 0; i < mNodes.size(); ++i)
    {
        const XMFLOAT3& nodePosition = mNodes[i].Position;
        const float dx = nodePosition.x - position.x;
        const float dy = nodePosition.y - position.y;
        const float dz = nodePosition.z - position.z;

        //========================================================
        // 別Floorと思われるNodeを除外
        //========================================================
        if (std::fabs(dy) > VerticalTolerance)
        {
            continue;
        }

        const float distanceSq = dx * dx + dy * dy + dz * dz;

        if (distanceSq < bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            bestIndex = i;
        }
    }

    //============================================================
    // 通常はこちらで終了
    //============================================================
    if (bestIndex != SIZE_MAX)
    {
        return bestIndex;
    }

    bestDistanceSq = FLT_MAX;

    //============================================================
    // Fallback
    // Save Dataの特殊位置などで、
    // 近い高さのNodeが存在しなかった場合だけ
    // 全Nodeから3D最寄りを探す
    //============================================================
    for (std::size_t i = 0; i < mNodes.size(); ++i)
    {
        const XMFLOAT3& nodePosition = mNodes[i].Position;
        const float dx = nodePosition.x - position.x;
        const float dy = nodePosition.y - position.y;
        const float dz = nodePosition.z - position.z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;


        if (distanceSq < bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            bestIndex = i;
        }
    }


    return bestIndex;
}

bool PursuerNavigation::FindPath(const XMFLOAT3& startPosition, const XMFLOAT3& goalPosition, std::vector<XMFLOAT3>& outPath) const
{
    outPath.clear();


    //========================================================
    // Graphなし
    //========================================================
    if (mNodes.empty())
    {
        return false;
    }


    //========================================================
    // Start / Goal Node
    //========================================================
    const std::size_t start = FindNearestNode(startPosition);
    const std::size_t goal = FindNearestNode(goalPosition);


    if (start == SIZE_MAX || goal == SIZE_MAX)
    {
        return false;
    }


    //========================================================
    // 同じNode
    //========================================================
    if (start == goal)
    {
        //========================================================
        // 同じNavigation Nodeに属していても
        // YはNav Mesh側の歩行高さへ合わせる
        //========================================================
        XMFLOAT3 finalGoal = goalPosition;
        finalGoal.y = mNodes[goal].Position.y;
        outPath.push_back(finalGoal);

        return true;
    }


    const std::size_t nodeCount = mNodes.size();


    std::vector<float> distance(nodeCount, FLT_MAX);
    std::vector<std::size_t> previous(nodeCount, SIZE_MAX);


    //========================================================
    // Priority Queue
    //========================================================
    using QueueItem = std::pair<float, std::size_t>;

    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem> > open;

    distance[start] = 0.0f;

    open.push({ 0.0f, start });


    //========================================================
    // Search
    //========================================================
    while (!open.empty())
    {
        const auto [currentDistance, current] = open.top();

        open.pop();

        if (current == goal)
        {
            break;
        }


        if (currentDistance > distance[current])
        {
            continue;
        }

        const PursuerNavNode& node = mNodes[current];

        for (std::size_t next : node.Links)
        {
            if (next >= nodeCount)
            {
                continue;
            }


            //================================================
            // Edge Cost
            // Node間のXZ距離
            //================================================
            const XMFLOAT3& a = node.Position;
            const XMFLOAT3& b = mNodes[next].Position;

            //========================================================
            // Dynamic Navigation Constraint
            // Locked Doorなどによって
            // 現在使用できないEdgeならDijkstraから除外する
            //========================================================
            if (!IsEdgeTraversable(a, b))
            {
                continue;
            }

            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float dz = b.z - a.z;
            const float edgeCost = sqrtf(dx * dx + dy * dy + dz * dz);
            const float candidate = currentDistance + edgeCost;


            if (candidate < distance[next])
            {
                distance[next] = candidate;

                previous[next] = current;

                open.push({ candidate, next });
            }
        }
    }

    //========================================================
    // Goalへ到達できなかった
    //========================================================
    if (previous[goal] == SIZE_MAX)
    {
        return false;
    }


    //========================================================
    // Goal → Startへ逆順で復元
    //========================================================
    std::vector<std::size_t> reversePath;

    std::size_t current = goal;

    while (current != SIZE_MAX)
    {
        reversePath.push_back(current);

        if (current == start)
        {
            break;
        }

        current = previous[current];
    }


    //========================================================
    // Start → Goalへ戻す
    //========================================================
    std::reverse(reversePath.begin(), reversePath.end());


    //========================================================
    // Start Nodeそのものは、
    // Monsterが既にその付近にいるため除外
    //========================================================
    for (std::size_t i = 1; i < reversePath.size(); ++i)
    {
        outPath.push_back(mNodes[reversePath[i]].Position);
    }

    //========================================================
    // 最後に本当の目的地を追加
    //========================================================
    XMFLOAT3 finalGoal = goalPosition;
    finalGoal.y = mNodes[goal].Position.y;
    outPath.push_back(finalGoal);

    return true;
}

bool PursuerNavigation::IsEdgeTraversable(const XMFLOAT3& from, const XMFLOAT3& to) const
{
    //========================================================
    // Door情報が無いなら通常通行可能
    //========================================================
    if (mNavigationDoors == nullptr)
    {
        return true;
    }


    //========================================================
    // Edge Vector
    // Doorway判定は基本的にXZ平面で行う
    //========================================================
    const float segmentX = to.x - from.x;
    const float segmentZ = to.z - from.z;

    const float segmentLengthSq =
        segmentX * segmentX + segmentZ * segmentZ;


    //========================================================
    // ほぼ長さ0のEdge
    // 通常CSVでは発生しないがSafetyとして通す
    //========================================================
    if (segmentLengthSq <= 0.000001f)
    {
        return true;
    }


    //========================================================
    // Navigation Door確認
    //========================================================
    for (const INavigationDoor* door : *mNavigationDoors)
    {
        if (door == nullptr)
        {
            continue;
        }

        //====================================================
        // AIが通れるDoor
        // ・Open
        // ・Unlocked Closed
        // はEdgeを塞がない
        //====================================================
        if (door->IsPassableForAI())
        {
            continue;
        }

        //====================================================
        // ここから先は
        // Locked Door
        // AI通行禁止Door
        // の判定
        //====================================================
        const XMFLOAT3& doorPosition = door->GetDoorPosition();

        //====================================================
        // DoorをEdgeへProjection
        // t:
        // 0 = from
        // 1 = to
        //====================================================
        const float fromToDoorX =
            doorPosition.x - from.x;

        const float fromToDoorZ =
            doorPosition.z - from.z;

        const float t =
            (fromToDoorX * segmentX + fromToDoorZ * segmentZ) / segmentLengthSq;


        //====================================================
        // DoorがこのEdge線分上の範囲外
        //====================================================
        if (t < 0.0f || t > 1.0f)
        {
            continue;
        }


        //====================================================
        // Edge上でDoorに最も近いXZ位置
        //====================================================
        const float closestX = from.x + segmentX * t;
        const float closestZ = from.z + segmentZ * t;


        //====================================================
        // Door中心からEdgeまでの横距離
        //====================================================
        const float dx =
            doorPosition.x - closestX;

        const float dz =
            doorPosition.z - closestZ;

        const float horizontalDistanceSq =
            dx * dx + dz * dz;

        const float doorWidthSq =
            mDoorEdgeWidth * mDoorEdgeWidth;


        //====================================================
        // DoorがEdgeから離れている
        // このEdgeとは無関係
        //====================================================
        if (horizontalDistanceSq > doorWidthSq)
        {
            continue;
        }


        //====================================================
        // Multi-Floor判定
        //====================================================
        const float edgeY =
            from.y + (to.y - from.y) * t;

        const float verticalDifference =
            std::fabs(doorPosition.y - edgeY);


        if (verticalDifference > mDoorVerticalTolerance)
        {
            continue;
        }
        //====================================================
        // Locked DoorがこのEdge上に存在する
        // このEdgeは現在使用不可
        //====================================================
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[PursuerNavigation] "
            L"Blocked edge by navigation door.\n"
        );

#endif

        return false;
    }


    //========================================================
    // BlockするDoorなし
    //========================================================
    return true;
}

//============================================================
// Clear
//============================================================
void PursuerNavigation::Clear()
{
    mNodes.clear();
}