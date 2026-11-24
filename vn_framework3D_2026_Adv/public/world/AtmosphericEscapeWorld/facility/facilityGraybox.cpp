#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "facilityGraybox.h"


//============================================================
// Initialize
//============================================================
bool FacilityGraybox::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const XMFLOAT3& origin,
    const std::string& terminalPersistentId,
    const XMFLOAT3& terminalPosition,
    const XMFLOAT3& terminalScale,
    const XMFLOAT3& innerDoorPosition
)
{
    if (scene == nullptr)
    {
        return false;
    }


    mOrigin = origin;


    //========================================================
    // Graybox Color
    // 暗いConcreteを想定
    //========================================================
    const XMFLOAT4 wallColor =
    {
        0.18f,
        0.19f,
        0.18f,
        1.0f
    };


    const XMFLOAT4 floorColor =
    {
        0.10f,
        0.11f,
        0.10f,
        1.0f
    };


    //========================================================
    // Facility Size
    // Width  = 8
    // Depth  = 8
    // Height = 3
    // originは入口中央付近とする
    //========================================================

    //========================================================
    // Floor
    //========================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x, origin.y - 0.10f, origin.z + 4.0f),
        XMFLOAT3(4.0f, 0.10f, 4.0f),
        floorColor,
        false))
    {
        return false;
    }


    //========================================================
    // Left Wall
    //========================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x - 4.0f, origin.y + 1.5f, origin.z + 4.0f),
        XMFLOAT3(0.20f, 1.5f, 4.0f),
        wallColor))
    {
        return false;
    }


    //========================================================
    // Right Wall
    //========================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x + 4.0f, origin.y + 1.5f, origin.z + 4.0f),
        XMFLOAT3(0.20f, 1.5f, 4.0f),
        wallColor))
    {
        return false;
    }


    //============================================================
    // Back Wall
    // 中央にSecurity Door用の開口を作る
    // [ Left ][ Door ][ Right ]
    //      [ Upper ]
    //============================================================
    //============================================================
    // Back Left
    //============================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x - 2.5f, origin.y + 1.5f, origin.z + 8.0f),
        XMFLOAT3(1.5f, 1.5f, 0.20f),
        wallColor
    ))
    {
        return false;
    }


    //============================================================
    // Back Right
    //============================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x + 2.5f, origin.y + 1.5f, origin.z + 8.0f),
        XMFLOAT3(1.5f, 1.5f, 0.20f),
        wallColor
    ))
    {
        return false;
    }


    //============================================================
    // Back Upper
    //
    // Door:
    // Width  約1.8m
    // Height 約2.1m
    //============================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x, origin.y + 2.60f, origin.z + 8.0f),
        XMFLOAT3(1.0f, 0.40f, 0.20f),
        wallColor
    ))
    {
        return false;
    }

    //========================================================
    // Front Wall
    // 中央に入口を作るため
    // [ 左壁 ][ 入口 ][ 右壁 ]
    // の2Blockで作る
    //========================================================

    //========================================================
    // Front Left
    //========================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x - 2.5f, origin.y + 1.5f, origin.z),
        XMFLOAT3(1.5f, 1.5f, 0.20f),
        wallColor))
    {
        return false;
    }


    //========================================================
    // Front Right
    //========================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x + 2.5f, origin.y + 1.5f, origin.z),
        XMFLOAT3(1.5f, 1.5f, 0.20f),
        wallColor))
    {
        return false;
    }


    //========================================================
    // Entrance Upper Wall
    // Door上部だけ塞ぐ
    // Door Width: 約2m
    // Door Height: 約2.2m
    //========================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x, origin.y + 2.60f, origin.z),
        XMFLOAT3(1.0f, 0.40f, 0.20f),
        wallColor))
    {
        return false;
    }


    //========================================================
    // Ceiling
    //========================================================
    if (!CreateBlock(
        scene,
        XMFLOAT3(origin.x, origin.y + 3.0f, origin.z + 4.0f),
        XMFLOAT3(4.0f, 0.10f, 4.0f),
        wallColor))
    {
        return false;
    }

    //============================================================
    // Entrance Door
    //============================================================
    const bool doorInitialized =
        mEntranceDoor.Initialize(
            scene,
            interactionSystem,
            "FacilityEntranceDoor",
            XMFLOAT3(origin.x, origin.y + 1.05f, origin.z)
        );

    if (!doorInitialized)
    {
        return false;
    }

    //============================================================
    // Facility Terminal
    //============================================================
    const bool terminalInitialized =
        mTerminal.Initialize(
            scene,
            interactionSystem,
            terminalPersistentId,
            terminalPosition,
            terminalScale
        );


    if (!terminalInitialized)
    {
        return false;
    }

    //============================================================
    // Inner Security Door
    //============================================================
    const bool innerDoorInitialized =
        mInnerDoor.Initialize(
            scene,
            interactionSystem,
            "FacilityInnerDoor",
            innerDoorPosition
        );


    if (!innerDoorInitialized)
    {
        return false;
    }

    //============================================================
    // 初期状態ではLocked
    // Terminalで施設電源が復旧するとUnlockされる
    //============================================================
    mInnerDoor.SetLocked(true);

    return true;
}

bool FacilityGraybox::InitializeAccessKey(vnScene* scene, InteractionSystem* interactionSystem, const std::string& terminalPersistentId, const XMFLOAT3& position, const XMFLOAT3& scale)
{
    return
        mAccessKey.Initialize(
            scene,
            interactionSystem,
            terminalPersistentId,
            position,
            scale
        );
}


bool FacilityGraybox::InitializeExitDoor(vnScene* scene, InteractionSystem* interactionSystem, const std::string& persistentId, const XMFLOAT3& position)
{
    //========================================================
    // Door生成
    //========================================================
    if (!mExitDoor.Initialize(
        scene,
        interactionSystem,
        persistentId,
        position
    ))
    {
        return false;
    }


    //========================================================
    // 初期状態ではLocked
    // Access Key取得後にUnlockする
    //========================================================
    mExitDoor.SetLocked(true);


    return true;
}

bool FacilityGraybox::InitializeSequencePuzzle(vnScene* scene, InteractionSystem* interactionSystem, const std::string& persistentId, const std::array<XMFLOAT3, 3>& switchPositions, const std::string& rewardDoorId)
{
    //========================================================
   // Reward Door
   //========================================================
    FacilityDoor* rewardDoor = FindInteractiveDoorById(rewardDoorId);


    if (rewardDoor == nullptr)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[FacilityGraybox] "
            L"Sequence Puzzle reward door not found.\n"
        );

#endif

        return false;
    }


    //========================================================
    // Puzzle解決まではLocked
    //========================================================
    rewardDoor->SetLocked(true);

    //========================================================
    // Puzzle
    //========================================================
    mSequencePuzzle = std::make_unique<SequencePuzzle>();

    const std::vector<std::wstring> switchTexts =
    {
        L"Press Moon",
        L"Press Sun",
        L"Press Bird"
    };

    //if (!mSequencePuzzle->Initialize(
    //    scene,
    //    interactionSystem,
    //    persistentId,
    //    switchPositions,
    //    switchTexts
    //))
    //{
    //    mSequencePuzzle.reset();

    //    return false;
    //}


    //========================================================
    // Puzzle Reward
    // 通常SolveでもSave Loadでも
    // 最終的にはDoorをUnlockする
    //========================================================
    mSequencePuzzle->SetSolvedCallback([rewardDoor](bool restored)
        {
            (void)restored;

            if (rewardDoor != nullptr)
            {
                rewardDoor->SetLocked(false);
            }
        }
    );

    return true;
}


//============================================================
// Terminate
//============================================================
void FacilityGraybox::Terminate(vnScene* scene, InteractionSystem* interactionSystem)
{
    if (scene == nullptr)
    {
        return;
    }

    mAccessKey.Terminate(
        scene,
        interactionSystem
    );

    mInnerDoor.Terminate(
        scene,
        interactionSystem
    );

    mEntranceDoor.Terminate(
        scene,
        interactionSystem
    );

    mTerminal.Terminate(
        scene,
        interactionSystem
    );

    mExitDoor.Terminate(
        scene,
        interactionSystem
    );
    //============================================================
    // Interactive Doors
    //============================================================
    for (auto& door : mInteractiveDoors)
    {
        if (door != nullptr)
        {
            door->Terminate(scene, interactionSystem);
        }
    }

    //============================================================
    // Sequence Puzzle
    //============================================================
    if (mSequencePuzzle != nullptr)
    {
        mSequencePuzzle->Terminate(scene, interactionSystem);

        mSequencePuzzle.reset();
    }

    mInteractiveDoors.clear();

    mColliders.clear();

    for (const auto& light : mPowerLights)
    {
        if (light != nullptr)
        {
            light->Terminate(scene);
        }
    }

    mPowerLights.clear();

    //========================================================
    // Models
    //========================================================
    for (ModelBase* object : mObjects)
    {
        if (object != nullptr)
        {
            scene->deleteObject(object);
        }
    }

    mObjects.clear();
}

//============================================================
// Update
//============================================================
void FacilityGraybox::Update(float dt)
{
    //========================================================
    // Entrance Door
    //========================================================
    mEntranceDoor.Update(dt);

    //========================================================
    // Inner Door
    //========================================================
    mInnerDoor.Update(dt);

    //========================================================
    // Final Exit Door
    //========================================================
    mExitDoor.Update(dt);

    //========================================================
    // Interactive Doors
    //========================================================
    for (auto& door : mInteractiveDoors)
    {
        if (door != nullptr)
        {
            door->Update(dt);
        }
    }

    //========================================================
    // Facility Lights
    //========================================================
    for (const auto& light : mPowerLights)
    {
        if (light != nullptr)
        {
            light->Update();
        }
    }
}

//============================================================
// Append Colliders
//============================================================
void FacilityGraybox::AppendColliders(std::vector<Collider*>& destination) const
{
    for (const auto& collider : mColliders)
    {
        if (collider != nullptr)
        {
            destination.push_back(collider.get());
        }
    }

    Collider* doorCollider = mEntranceDoor.GetCollider();

    if (doorCollider != nullptr)
    {
        destination.push_back(doorCollider);
    }

    Collider* terminalCollider = mTerminal.GetCollider();

    if (terminalCollider != nullptr)
    {
        destination.push_back(terminalCollider);
    }

    Collider* innerDoorCollider = mInnerDoor.GetCollider();

    if (innerDoorCollider != nullptr)
    {
        destination.push_back(innerDoorCollider);
    }

    Collider* exitDoorCollider = mExitDoor.GetCollider();

    if (exitDoorCollider != nullptr)
    {
        destination.push_back(exitDoorCollider);
    }

    //============================================================
    // Interactive Door Colliders
    //============================================================
    for (const auto& door : mInteractiveDoors)
    {
        if (door == nullptr)
        {
            continue;
        }

        Collider* collider = door->GetCollider();

        if (collider != nullptr)
        {
            destination.push_back(collider);
        }
    }
}

bool FacilityGraybox::ContainsPosition(const XMVECTOR& worldPosition) const
{
    //========================================================
    // Player World Position
    //========================================================
    const float x = XMVectorGetX(worldPosition);
    const float z = XMVectorGetZ(worldPosition);


    //========================================================
    // Facility内部範囲
    // 左右壁が±4m、
    // 奥壁がorigin.z + 8m
    // ※壁Colliderとの境界付近で判定が
    // ※ちらつかないように少しMarginを取る
    //========================================================
    constexpr float margin = 0.30f;
    const float minX = mOrigin.x - 4.0f + margin;
    const float maxX = mOrigin.x + 4.0f - margin;
    const float minZ = mOrigin.z + margin;
    const float maxZ = mOrigin.z + 8.0f - margin;


    //========================================================
    // Inside
    //========================================================
    return
        x >= minX &&
        x <= maxX &&
        z >= minZ &&
        z <= maxZ;
}

bool FacilityGraybox::AddPowerLight(vnScene* scene, const XMFLOAT3& position, const XMFLOAT3& scale)
{
    if (scene == nullptr)
    {
        return false;
    }


    //========================================================
    // Light Object生成
    //========================================================
    auto light = std::make_unique<FacilityPowerLight>();


    if (!light->Initialize(
        scene,
        position,
        scale
    ))
    {
        return false;
    }


    //========================================================
    // 現在のFacility電源状態を反映
    //========================================================
    light->SetPowered(mPowerEnabled);

    //========================================================
    // Facilityが所有
    //========================================================
    mPowerLights.push_back(std::move(light));

    return true;
}

bool FacilityGraybox::AddStaticBlock(vnScene* scene, const XMFLOAT3& position, const XMFLOAT3& scale)
{
    if (scene == nullptr)
    {
        return false;
    }

    //========================================================
    // CSV Scale
    //========================================================
    const XMFLOAT3 halfExtents =
    {
        scale.x * 0.5f,
        scale.y * 0.5f,
        scale.z * 0.5f
    };

    //========================================================
    // Graybox Material
    //========================================================
    const XMFLOAT4 color =
    {
        0.16f,
        0.17f,
        0.17f,
        1.0f
    };

    return
        CreateBlock(
            scene,
            position,
            halfExtents,
            color,
            true
        );
}

bool FacilityGraybox::AddInteractiveDoor(vnScene* scene, InteractionSystem* interactionSystem, const std::string& persistentId, const XMFLOAT3& position)
{
    //========================================================
    // Safety
    //========================================================
    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }


    //========================================================
    // Door生成
    //========================================================
    auto door = std::make_unique<FacilityDoor>();


    //========================================================
    // Initialize
    //========================================================
    if (!door->Initialize(
        scene,
        interactionSystem,
        persistentId,
        position
    ))
    {
        return false;
    }

    //========================================================
    // 一般Doorは開閉可能
    //========================================================
    door->SetCanClose(true);


    //========================================================
    // Noise System
    //========================================================
    door->SetNoiseSystem(mNoiseSystem);

    //========================================================
    // Facilityが所有
    //========================================================
    mInteractiveDoors.push_back(std::move(door));

    return true;
}

void FacilityGraybox::SetPowerEnabled(bool enabled)
{
    //============================================================
    // 状態が変化したかだけ記録
    //============================================================
    const bool changed = mPowerEnabled != enabled;

    //============================================================
    // Current State
    //============================================================
    mPowerEnabled = enabled;

    //========================================================
    // 全照明へ状態を反映
    //========================================================
    for (const auto& light :
        mPowerLights)
    {
        if (light != nullptr)
        {
            light->SetPowered(enabled);
        }
    }

    mInnerDoor.SetLocked(!enabled);

    //============================================================
    // 状態変化がなければDebug Log等は不要
    //============================================================
    if (!changed)
    {
        return;
    }

#if defined(_DEBUG)

    if (enabled)
    {
        OutputDebugStringW(L"[FacilityGraybox] Power ON.\n");
    }
    else
    {
        OutputDebugStringW(L"[FacilityGraybox] Power OFF.\n");
    }

#endif
}

void FacilityGraybox::AppendPersistentObjects(std::vector<IPersistentWorldObject*>& destination)
{
    //========================================================
    // Entrance Door
    //========================================================
    destination.push_back(&mEntranceDoor);

    //========================================================
    // Inner Door
    //========================================================
    destination.push_back(&mInnerDoor);

    //========================================================
    // Exit Door
    //========================================================
    destination.push_back(&mExitDoor);

    //========================================================
    // Terminal
    //========================================================
    destination.push_back(&mTerminal);

    //========================================================
    // Access Key
    //========================================================
    destination.push_back(&mAccessKey);

    //========================================================
    // Generic Interactive Doors
    //========================================================
    for (auto& door : mInteractiveDoors)
    {
        if (door == nullptr)
        {
            continue;
        }

        destination.push_back(door.get());
    }

    if (mSequencePuzzle != nullptr)
    {
        destination.push_back(mSequencePuzzle.get());
    }
}

void FacilityGraybox::AppendPersistentObjects(std::vector<const IPersistentWorldObject*>& destination) const
{
    destination.push_back(&mEntranceDoor);
    destination.push_back(&mInnerDoor);
    destination.push_back(&mExitDoor);
    destination.push_back(&mTerminal);
    destination.push_back(&mAccessKey);

    for (const auto& door : mInteractiveDoors)
    {
        if (door == nullptr)
        {
            continue;
        }

        destination.push_back(door.get());
    }

    if (mSequencePuzzle != nullptr)
    {
        destination.push_back(mSequencePuzzle.get());
    }
}

void FacilityGraybox::RebuildPersistentDerivedState()
{
    //============================================================
    // Terminal
    //      ↓
    // Facility Power
    //      ↓
    // Inner Door / Power Lights
    //============================================================
    SetPowerEnabled(mTerminal.IsActivated());

    //============================================================
    // Access Key
    //      ↓
    // Exit Door Lock
    //============================================================
    mExitDoor.SetLocked(!mAccessKey.IsCollected());
}

bool FacilityGraybox::CreateBlock(
    vnScene* scene,
    const XMFLOAT3& position,
    const XMFLOAT3& halfExtents,
    const XMFLOAT4& color,
    bool castShadow
)
{
    if (scene == nullptr)
    {
        return false;
    }


    //========================================================
    // Cube Model生成
    //========================================================
    ModelBase* model = new ModelBase(L"data/model/primitive/", L"cube.vnm");


    if (model == nullptr)
    {
        return false;
    }


    //========================================================
    // Position
    //========================================================
    model->setPosition(
        position.x,
        position.y,
        position.z
    );


    //========================================================
    // Scale
    //========================================================
    model->setScale(
        halfExtents.x * 2.0f,
        halfExtents.y * 2.0f,
        halfExtents.z * 2.0f
    );


    //========================================================
    // PBR
    //========================================================
    model->SetPBREnabled(true);


    //========================================================
    // Graybox Material
    //
    // 少し暗いConcrete系Color
    //========================================================
    model->setDiffuse(
        color.x,
        color.y,
        color.z,
        color.w
    );


    model->SetMetallic(0.0f);
    model->SetRoughness(0.85f);


    //========================================================
    // Shadow
    //========================================================
    model->setCastShadow(castShadow);
    model->setReceiveShadow(true);
    model->SetShadowMobility(ShadowMobility::Static);


    //========================================================
    // Box Collider
    //========================================================
    auto collider =
        std::make_unique<BoxCollider>(
            model,
            XMVectorSet(halfExtents.x, halfExtents.y, halfExtents.z, 0.0f)
        );


    //========================================================
    // Sceneへ登録
    //========================================================
    scene->registerObject(model);

    //========================================================
    // Facility側で所有
    //========================================================
    mObjects.push_back(model);

    mColliders.push_back(std::move(collider));

    return true;
}

FacilityDoor* FacilityGraybox::FindInteractiveDoorById(const std::string& persistentId)
{
    for (auto& door : mInteractiveDoors)
    {
        if (door == nullptr)
        {
            continue;
        }

        //====================================================
        // FacilityDoorのGetPersistentId()がprivateでも
        // publicなIPersistentWorldObject経由なら取得可能
        //====================================================
        IPersistentWorldObject* persistent = door.get();

        if (persistent->GetPersistentId() == persistentId)
        {
            return door.get();
        }
    }

    return nullptr;
}
