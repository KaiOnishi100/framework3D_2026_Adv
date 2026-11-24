#pragma once

#include "facilityDoor.h"
#include "facilityTerminal.h"
#include "facilityPowerLight.h"
#include "facilityAccessKey.h"

#include "../puzzle/sequencePuzzle.h"

//============================================================
// FacilityGraybox
// Atmospheric Escape用の仮施設
// 目的:
// ・ゲーム進行確認
// ・Fog / Light確認
// ・Player Collision確認
//============================================================
class FacilityGraybox
{
public:

    FacilityGraybox() = default;

    ~FacilityGraybox() = default;


    //========================================================
    // Initialize
    // origin: Facility全体の基準位置
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const XMFLOAT3& origin,
        const std::string& terminalPersistentId,
        const XMFLOAT3& terminalPosition,
        const XMFLOAT3& terminalScale,
        const XMFLOAT3& innerDoorPosition
    );

    //============================================================
    // Access Key初期化
    //============================================================
    bool InitializeAccessKey(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& terminalPersistentId,
        const XMFLOAT3& position,
        const XMFLOAT3& scale
    );

    //============================================================
    // Exit Door Initialize
    // Facility本体のInitialize引数を増やし続けないため
    // Exit Doorは個別に初期化する
    //============================================================
    bool InitializeExitDoor(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const XMFLOAT3& position
    );

    //============================================================
    // Sequence Puzzle
    //============================================================
    bool InitializeSequencePuzzle(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const std::array<XMFLOAT3, 3>& switchPositions,
        const std::string& rewardDoorId
    );

    //========================================================
    // Terminate
    //========================================================
    void Terminate(vnScene* scene, InteractionSystem* interactionSystem);

    //========================================================
    // Update
    // Facility内部の動的Objectを更新する
    //========================================================
    void Update(float dt);

    //============================================================
    // Facility Power Light追加
    // CSVに書かれたFacilityLightを登録する
    //============================================================
    bool AddPowerLight(
        vnScene* scene,
        const XMFLOAT3& position,
        const XMFLOAT3& scale
    );

    //============================================================
    // CSVからFacility Blockを追加
    //============================================================
    bool AddStaticBlock(
        vnScene* scene,
        const XMFLOAT3& position,
        const XMFLOAT3& scale
    );

    //============================================================
    // Interactive Door追加
    //============================================================
    bool AddInteractiveDoor(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const XMFLOAT3& position
    );

    //========================================================
    // CollisionManagerへColliderを渡す
    // FacilityGraybox側がColliderを所有し
    // Scene側vectorには非所有Pointerだけを追加する
    //========================================================
    void AppendColliders(std::vector<Collider*>& destination) const;

    //============================================================
    // PlayerなどのWorld Positionが
    // Facility内部に入っているか判定する
    //============================================================
    bool ContainsPosition(const XMVECTOR& worldPosition) const;

    //============================================================
    // Entrance Door
    //============================================================
    bool IsEntranceDoorOpen() const
    {
        return mEntranceDoor.IsOpen();
    }

    //============================================================
    // Inner Door State
    //============================================================
    bool IsInnerDoorOpen() const
    {
        return mInnerDoor.IsOpen();
    }

    bool IsInnerDoorLocked() const
    {
        return mInnerDoor.IsLocked();
    }

    //============================================================
    // Exit Door Lock
    //============================================================
    void SetExitDoorLocked(bool locked)
    {
        mExitDoor.SetLocked(locked);
    }

    //============================================================
    // Exit Door State
    //============================================================
    bool IsExitDoorOpen() const
    {
        return mExitDoor.IsOpen();
    }

    bool IsExitDoorLocked() const
    {
        return mExitDoor.IsLocked();
    }

    //============================================================
    // Terminal
    //============================================================
    bool IsTerminalActivated() const
    {
        return mTerminal.IsActivated();
    }

    //============================================================
    // Facility全体の電源
    //============================================================
    void SetPowerEnabled(bool enabled);

    //============================================================
    // Power State
    //============================================================
    bool IsPowerEnabled() const
    {
        return mPowerEnabled;
    }

    //============================================================
    // Access Key State
    //============================================================
    bool IsAccessKeyCollected() const
    {
        return mAccessKey.IsCollected();
    }

    //============================================================
    // Noise System
    //============================================================
    void SetNoiseSystem(NoiseSystem* noiseSystem)
    {
        mNoiseSystem = noiseSystem;

        //========================================================
        // Entrance Door
        //========================================================
        mEntranceDoor.SetNoiseSystem(noiseSystem);
        //========================================================
        // Terminal
        //========================================================
        mTerminal.SetNoiseSystem(noiseSystem);
        mInnerDoor.SetNoiseSystem(noiseSystem);
        mExitDoor.SetNoiseSystem(noiseSystem);

        //========================================================
        // Interactive Doors
        //========================================================
        for (auto& door : mInteractiveDoors)
        {
            if (door != nullptr)
            {
                door->SetNoiseSystem(noiseSystem);
            }
        }
    }

    //============================================================
    // AIが使用できるDoorを収集
    //============================================================
    void AppendAIDoors(std::vector<INavigationDoor*>& destination)
    {
        destination.push_back(&mEntranceDoor);

        for (auto& door : mInteractiveDoors)
        {
            if (door != nullptr)
            {
                destination.push_back(door.get());
            }
        }
    }

public:

    //============================================================
    // Persistent World Objects
    //============================================================
    void AppendPersistentObjects(std::vector<IPersistentWorldObject*>& destination);

    void AppendPersistentObjects(std::vector<const IPersistentWorldObject*>& destination) const;

    //============================================================
    // Persistent Object復元後の依存状態再構築
    //============================================================
    void RebuildPersistentDerivedState();

public:

    SequencePuzzle* GetSequencePuzzle()
    {
        return mSequencePuzzle.get();
    }

    FacilityDoor* FindInteractiveDoorById(const std::string& persistentId);

private:

    //========================================================
    // Graybox Block生成Helper
    // Cube ModelとBoxColliderを同時生成する
    //========================================================
    bool CreateBlock(
        vnScene* scene,
        const XMFLOAT3& position,
        const XMFLOAT3& halfExtents,
        const XMFLOAT4& color,
        bool castShadow = true
    );


private:

    //========================================================
    // Facilityを構成するModel
    //========================================================
    std::vector<ModelBase*> mObjects;

    //========================================================
    // Static Collider
    //========================================================
    std::vector<std::unique_ptr<BoxCollider>> mColliders;

    //========================================================
    // Entrance Door
    //========================================================
    FacilityDoor mEntranceDoor;

    //============================================================
    // Facility Terminal
    //============================================================
    FacilityTerminal mTerminal;

    //============================================================
    // Inner Security Door
    // Terminalで電源を復旧するまではLocked
    //============================================================
    FacilityDoor mInnerDoor;

    //============================================================
    // Exit Door
    // Access Key取得後にUnlockされる最終出口
    //============================================================
    FacilityDoor mExitDoor;

    //============================================================
    // Access Key
    //============================================================
    FacilityAccessKey mAccessKey;

    //============================================================
    // Sequence Puzzle Test
    //============================================================
    std::unique_ptr<SequencePuzzle> mSequencePuzzle;

    //============================================================
    // Noise System
    //============================================================
    NoiseSystem* mNoiseSystem = nullptr;

    //============================================================
    // Facility Power Lights
    // unique_ptr:
    // FacilityPowerLight内部にvnModel*を持つため、
    // vector再配置時のObject Copyを避ける
    //============================================================
    std::vector<std::unique_ptr<FacilityPowerLight>> mPowerLights;

    //============================================================
    // Interactive Doors
    // unique_ptr:
    // FacilityDoorはModel/Colliderなどを内部に持つため、
    // vector再配置によるCopyを避ける
    //============================================================
    std::vector<std::unique_ptr<FacilityDoor>> mInteractiveDoors;

    //============================================================
    // Facility Power State
    //============================================================
    bool mPowerEnabled = false;

    //========================================================
    // Facility基準位置
    //========================================================
    XMFLOAT3 mOrigin =
    {
        0.0f,
        0.0f,
        0.0f
    };

};