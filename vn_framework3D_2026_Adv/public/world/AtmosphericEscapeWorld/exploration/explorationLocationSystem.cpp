#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "explorationLocationSystem.h"


//============================================================
// Initialize
//============================================================
void ExplorationLocationSystem::Initialize()
{
    mRooms.clear();

    mDiscoveredRooms.clear();

    mCurrentLocation =
    {
        ExplorationArea::Unknown,
        ExplorationFloor::Unknown,
        ""
    };
}


//============================================================
// Terminate
//============================================================
void ExplorationLocationSystem::Terminate()
{
    mRooms.clear();

    mDiscoveredRooms.clear();

    mCurrentLocation =
    {
        ExplorationArea::Unknown,
        ExplorationFloor::Unknown,
        ""
    };
}


//============================================================
// Register Room
//============================================================
bool ExplorationLocationSystem::RegisterRoom(const RoomDefinition& room)
{
    //========================================================
    // ID必須
    //========================================================
    if (room.Id.empty())
    {
        return false;
    }


    //========================================================
    // Duplicate Check
    // Map / Objective / SaveのKeyになるため
    // Room ID重複は禁止
    //========================================================
    for (const RoomDefinition& existing : mRooms)
    {
        if (existing.Id == room.Id)
        {
#if defined(_DEBUG)

            std::string message =
                "[LocationSystem] "
                "Duplicate room ID : " +
                room.Id +
                "\n";

            OutputDebugStringA(
                message.c_str()
            );

#endif
            return false;
        }
    }


    mRooms.push_back(room);

    return true;
}


//============================================================
// Contains
//============================================================
bool ExplorationLocationSystem::Contains(const RoomDefinition& room, const XMVECTOR& position)
{
    const float x =
        XMVectorGetX(position);

    const float y =
        XMVectorGetY(position);

    const float z =
        XMVectorGetZ(position);


    return
        x >= room.MinBounds.x &&
        x <= room.MaxBounds.x &&
        y >= room.MinBounds.y &&
        y <= room.MaxBounds.y &&
        z >= room.MinBounds.z &&
        z <= room.MaxBounds.z;
}


//============================================================
// Update
//============================================================
void ExplorationLocationSystem::Update(const XMVECTOR& playerPosition)
{
    //========================================================
    // Default
    // どのRoomにもいなければUnknown
    //========================================================
    const RoomDefinition* foundRoom = nullptr;

    //========================================================
    // 現在Positionを含むRoomを探す
    //========================================================
    for (const RoomDefinition& room : mRooms)
    {
        if (Contains(room, playerPosition))
        {
            foundRoom = &room;
            break;
        }
    }


    //========================================================
    // Roomなし
    //========================================================
    if (foundRoom == nullptr)
    {
        mCurrentLocation =
        {
            ExplorationArea::Unknown,
            ExplorationFloor::Unknown,
            ""
        };

        return;
    }


    //========================================================
    // Room Changed?
    //========================================================
    const bool roomChanged = mCurrentLocation.RoomId != foundRoom->Id;

    //========================================================
    // Current Location更新
    //========================================================
    mCurrentLocation.Area = foundRoom->Area;
    mCurrentLocation.Floor = foundRoom->Floor;
    mCurrentLocation.RoomId = foundRoom->Id;

    //========================================================
    // Discovered
    //========================================================
    mDiscoveredRooms.insert(foundRoom->Id);


#if defined(_DEBUG)

    if (roomChanged)
    {
        std::string message =
            "[LocationSystem] Enter Room : " +
            foundRoom->Id +
            "\n";

        OutputDebugStringA(
            message.c_str()
        );
    }

#endif
}


//============================================================
// Find Room
//============================================================
const RoomDefinition* ExplorationLocationSystem::FindRoom(const std::string& roomId) const
{
    for (const RoomDefinition& room : mRooms)
    {
        if (room.Id == roomId)
        {
            return &room;
        }
    }

    return nullptr;
}


//============================================================
// Current Room
//============================================================
const RoomDefinition* ExplorationLocationSystem::GetCurrentRoom() const
{
    if (mCurrentLocation.RoomId.empty())
    {
        return nullptr;
    }

    return FindRoom(mCurrentLocation.RoomId);
}


//============================================================
// Discovery
//============================================================
bool ExplorationLocationSystem::IsRoomDiscovered(const std::string& roomId) const
{
    return  mDiscoveredRooms.find(roomId) != mDiscoveredRooms.end();
}


//============================================================
// Restore Discovery
//============================================================
void ExplorationLocationSystem::RestoreDiscoveredRooms(const std::vector<std::string>& roomIds)
{
    mDiscoveredRooms.clear();


    for (const std::string& roomId : roomIds)
    {
        //====================================================
        // 存在するRoomだけ復元
        // 古いSaveに削除済みRoomがあっても無視する
        //====================================================
        if (FindRoom(roomId) == nullptr)
        {
            continue;
        }

        mDiscoveredRooms.insert(roomId);
    }
}

const RoomDefinition* ExplorationLocationSystem::FindRoomAtPosition(const XMFLOAT3& worldPosition) const
{
    //========================================================
    // 既存Contains()がXMVECTORを受け取るため変換
    //========================================================
    const XMVECTOR position =
        XMLoadFloat3(&worldPosition);

    //========================================================
    // 光源位置を含むRoomをすべて確認する。
    //
    // 現在地判定用のAABBには床下の余裕があるため、地下の天井付近は
    // 地下Roomと1F／前庭Roomの両方に含まれることがある。
    // 最初の一致を返すとCSVの登録順で1Fを選び、RoomMin.yより下の
    // 地下の壁・床への照明がシェーダーで切り落とされてしまう。
    //
    // 上下階の範囲が重なった場合は、位置を含むAABBのうち
    // 上端Yが最も近い（低い）Roomを選び、床下の余裕を持つ上階を避ける。
    // 上端Yが同じ場合は従来どおり登録順を維持する。
    // Playerの現在地更新はUpdate()側の判定をそのまま使用する。
    //========================================================
    const RoomDefinition* foundRoom = nullptr;

    for (const RoomDefinition& room : mRooms)
    {
        if (!Contains(room, position))
        {
            continue;
        }

        if (foundRoom == nullptr || room.MaxBounds.y < foundRoom->MaxBounds.y)
        {
            foundRoom = &room;
        }
    }

    return foundRoom;
}
