#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "pursuerSystem.h"
#include <cmath>

//============================================================
// Initialize
//============================================================
bool PursuerSystem::Initialize(
    vnScene* scene, 
    const std::vector<Collider*>& sightColliders,
    const NoiseSystem* noiseSystem,
    const std::vector<INavigationDoor*>& navigationDoors
)
{
    //========================================================
    // Safety
    //========================================================
    if (scene == nullptr)
    {
        return false;
    }


    //========================================================
    // 二重Initialize防止
    //========================================================
    if (mInitialized)
    {
        return true;
    }


    //========================================================
    // AI Placement読み込み
    //========================================================
    if (!LoadPlacement())
    {
        return false;
    }


    //========================================================
    // Pursuer生成
    //========================================================
    if (!CreatePursuer(scene))
    {
        return false;
    }


    //========================================================
    // 起動演出の完了まで、生成済みモデルとコライダーを隠しておく。
    // SetActive(false)は同じ値で早期returnするため、初期状態はここで直接反映する。
    mActive = false;
    mPursuer.SetPresenceEnabled(false);

    RegisterPatrolPoints();

    //========================================================
    // Search Point登録
    //========================================================
    RegisterSearchPoints();

    //========================================================
    // Line Of Sight Collider
    //========================================================
    mPursuer.SetSightColliders(&sightColliders);

    //============================================================
    // Hearing
    //============================================================
    mPursuer.SetNoiseSystem(noiseSystem);

    //============================================================
    // Navigation
    //============================================================
    const bool navigationLoaded =
        mNavigation.Load(L"data/csv/ai/AtmosphericEscapeNavigation.csv");


    if (!navigationLoaded)
    {
        OutputDebugStringW(
            L"[PursuerSystem] "
            L"Navigation load failed.\n"
        );

        return false;
    }

    //============================================================
    // MonsterへNavigationを渡す
    //============================================================
    mPursuer.SetNavigation(&mNavigation);

    mNavigation.SetNavigationDoors(&navigationDoors);

    mPursuer.SetNavigationDoors(&navigationDoors);

    //========================================================
    // Initialize Complete
    //========================================================
    mInitialized = true;


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[PursuerSystem] "
        L"Initialize complete.\n"
    );

#endif


    return true;
}

bool PursuerSystem::InitializeAnimations(const PursuerAnimationSet& animations)
{
    //========================================================
    // 実際のMotion管理はPursuerMonsterへ委譲
    //========================================================
    return mPursuer.InitializeAnimations(animations);
}

//============================================================
// Terminate
//============================================================
void PursuerSystem::Terminate(vnScene* scene)
{
    if (!mInitialized)
    {
        return;
    }


    //========================================================
    // Pursuer破棄
    //========================================================
    mPursuer.Terminate(scene);


    //========================================================
    // 状態Reset
    //========================================================
    mPlacementTable.Clear();
    mNavigation.Clear();

    mActive = false;
    mInitialized = false;
}

//============================================================
// Load Placement
//============================================================
bool PursuerSystem::LoadPlacement()
{
    //========================================================
    // AI専用CSV
    //========================================================
    const bool loaded =
        mPlacementTable.Load(L"data/csv/ai/AtmosphericEscapePursuer.csv" );


    if (!loaded)
    {
        OutputDebugStringW(
            L"[PursuerSystem] "
            L"Failed to load "
            L"AtmosphericEscapePursuer.csv\n"
        );


        return false;
    }


    return true;
}


//============================================================
// Create Pursuer
//============================================================
bool PursuerSystem::CreatePursuer(vnScene* scene)
{
    //========================================================
    // Spawn Point
    // CSV:
    // PursuerSpawn,PursuerSpawn,...
    // のIdを探す
    //========================================================
    const WorldObjectPlacement* spawn =
        mPlacementTable.Find("PursuerSpawn");


    if (spawn == nullptr)
    {
        OutputDebugStringW(
            L"[PursuerSystem] "
            L"PursuerSpawn not found.\n"
        );


        return false;
    }


    //========================================================
    // Monster本体生成
    //========================================================
    if (!mPursuer.Initialize(scene,spawn->Position))
    {
        OutputDebugStringW(
            L"[PursuerSystem] "
            L"Pursuer initialization failed.\n"
        );


        return false;
    }


    return true;
}


//============================================================
// Register Patrol Points
//============================================================
void PursuerSystem::RegisterPatrolPoints()
{
    //========================================================
    // Prefab == PursuerPatrol
    // の配置情報をすべて取得する
    //========================================================
    const auto patrolPoints =
        mPlacementTable.FindByPrefab("PursuerPatrol");


    //========================================================
    // Pursuerへ登録
    //========================================================
    for (const WorldObjectPlacement* placement : patrolPoints)
    {
        if (placement == nullptr)
        {
            continue;
        }

        mPursuer.AddPatrolPoint(placement->Position);
    }


#if defined(_DEBUG)

    //========================================================
    // Patrol Pointが0の場合は
    // AI自体は生成できるが巡回できない
    //========================================================
    if (patrolPoints.empty())
    {
        OutputDebugStringW(
            L"[PursuerSystem] "
            L"No patrol points found.\n"
        );
    }

#endif
}

//============================================================
// Search Point
//============================================================
void PursuerSystem::RegisterSearchPoints()
{
    //========================================================
    // AI CSVからSearch Point取得
    //========================================================
    const auto searchPoints = mPlacementTable.FindByPrefab("PursuerSearch");

    //========================================================
    // Monsterへ登録
    //========================================================
    for (const WorldObjectPlacement* placement : searchPoints)
    {
        if (placement == nullptr)
        {
            continue;
        }

        mPursuer.AddSearchPoint(placement->Position);
    }


#if defined(_DEBUG)

    if (searchPoints.empty())
    {
        OutputDebugStringW(
            L"[PursuerSystem] "
            L"No search points found.\n"
        );
    }

#endif
}

void PursuerSystem::AppendColliders(std::vector<Collider*>& destination)
{
    //========================================================
    // Pursuer Body
    //========================================================
    Collider* bodyCollider =  mPursuer.GetBodyCollider();

    if (bodyCollider != nullptr)
    {
        destination.push_back(bodyCollider);
    }

    //========================================================
    // Attack Collider
    //========================================================
    Collider* attackCollider = mPursuer.GetAttackCollider();

    if (attackCollider != nullptr)
    {
        destination.push_back(attackCollider);
    }
}

//============================================================
// Update
//============================================================
void PursuerSystem::Update(float dt, ExplorationPlayer& player)
{
    if (!mInitialized)
    {
        return;
    }

    if (!mActive)
    {
        return;
    }

    //========================================================
    // AI本体Update
    //========================================================
    mPursuer.Update(dt, player);
}

void PursuerSystem::UpdateAudio(float dt, const ExplorationPlayer& player)
{
    if (mInitialized && mActive)
    {
        mPursuer.UpdateAudio(dt, player);
    }
}


void PursuerSystem::BeginFinalChase(const ExplorationPlayer& player)
{
    if (!mInitialized || !mActive || mPursuer.IsFinalChase())
    {
        return;
    }

    XMFLOAT3 playerPosition{};
    XMStoreFloat3(&playerPosition, *player.getPosition());

    // 旧セーブなどで別の場所から脱出フェーズに入った場合は、
    // 最終端末近くへの強制移動を避けて、現在位置から追跡を開始する。
    XMFLOAT3 startPosition = mPursuer.GetPosition();
    const auto* spawn = mPlacementTable.Find("PursuerFinalSpawn");

    if (spawn != nullptr)
    {
        const XMFLOAT3& candidate = spawn->Position;
        const float dx = candidate.x - playerPosition.x;
        const float dz = candidate.z - playerPosition.z;
        const float distanceSq = dx * dx + dz * dz;
        constexpr float minDistance = 6.0f;
        constexpr float maxDistance = 18.0f;

        // 別フロアや至近距離での出現を避ける。施錠中の扉は既存のFindPathが判定する。
        if (std::fabs(candidate.y - playerPosition.y) <= 1.6f &&
            distanceSq >= minDistance * minDistance &&
            distanceSq <= maxDistance * maxDistance)
        {
            std::vector<XMFLOAT3> path;
            if (mNavigation.FindPath(candidate, playerPosition, path))
            {
                startPosition = candidate;
            }
        }
    }

    mPursuer.BeginFinalChase(startPosition, playerPosition);
}
