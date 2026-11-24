#pragma once

#include "explorationLocation.h"

//============================================================
// ExplorationLocationSystem
// PlayerのWorld Positionから
// ・Area
// ・Floor
// ・Room
// を判定する
//============================================================
class ExplorationLocationSystem
{
public:

    ExplorationLocationSystem() = default;

    ~ExplorationLocationSystem() = default;


    //========================================================
    // 初期化
    //========================================================
    void Initialize();


    //========================================================
    // 終了
    //========================================================
    void Terminate();


    //========================================================
    // Room登録
    //========================================================
    bool RegisterRoom(const RoomDefinition& room);

    //========================================================
    // Player Positionから現在地更新
    //========================================================
    void Update(const XMVECTOR& playerPosition);

    //========================================================
    // Current Location
    //========================================================
    const ExplorationLocation& GetCurrentLocation() const
    {
        return mCurrentLocation;
    }

    //============================================================
    // 全Room取得
    // Map UIが部屋配置を構築するときに使用する
    //============================================================
    const std::vector<RoomDefinition>& GetRooms() const
    {
        return mRooms;
    }

    //========================================================
    // Current Room
    //========================================================
    const RoomDefinition* GetCurrentRoom() const;

    //========================================================
    // ID検索
    //========================================================
    const RoomDefinition* FindRoom(const std::string& roomId ) const;

    //========================================================
    // Discovery
    //========================================================
    bool IsRoomDiscovered(const std::string& roomId ) const;

    const std::unordered_set<std::string>& GetDiscoveredRooms() const
    {
        return mDiscoveredRooms;
    }

    //========================================================
    // Save Load用
    //========================================================
    void RestoreDiscoveredRooms(const std::vector<std::string>& roomIds);

    //============================================================
    // 指定World Positionが属しているRoomを取得
    //============================================================
    const RoomDefinition* FindRoomAtPosition(const XMFLOAT3& worldPosition) const;

private:

    //========================================================
    // PointがRoomのAABB内部か
    //========================================================
    static bool Contains(const RoomDefinition& room, const XMVECTOR& position);


private:

    //========================================================
    // Room Definition
    //========================================================
    std::vector<RoomDefinition> mRooms;
    //========================================================
    // Current Location
    //========================================================
    ExplorationLocation mCurrentLocation;

    //========================================================
    // 一度訪れたRoom
    //========================================================
    std::unordered_set<std::string> mDiscoveredRooms;
};