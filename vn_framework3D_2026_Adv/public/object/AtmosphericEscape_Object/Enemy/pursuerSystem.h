#pragma once

#include "pursuerMonster.h"
#include "pursuerNavigation.h"
#include "../../../world/AtmosphericEscapeWorld/worldPlacement.h"


class vnScene;
class ExplorationPlayer;


//============================================================
// PursuerSystem
// AtmosphericEscapeの追跡者全体を管理する
//============================================================
class PursuerSystem
{
public:

    PursuerSystem() = default;

    ~PursuerSystem() = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(
        vnScene* scene,
        const std::vector<Collider*>& sightColliders, 
        const NoiseSystem* noiseSystem,
        const std::vector<INavigationDoor*>& navigationDoors
    );

    //========================================================
    // SceneからPursuerのAnimationを初期化する
    //========================================================
    bool InitializeAnimations(const PursuerAnimationSet& animations);

    //========================================================
    // Terminate
    //========================================================
    void Terminate(vnScene* scene);

    //========================================================
    // Update
    //========================================================
    void Update(float dt, ExplorationPlayer& player);

    void UpdateAudio(float dt, const ExplorationPlayer& player);

    //========================================================
    // Pursuer取得
    // Debug表示やGameProgress判定用
    //========================================================
    PursuerMonster* GetPursuer()
    {
        return &mPursuer;
    }


    const PursuerMonster* GetPursuer() const
    {
        return &mPursuer;
    }

    //============================================================
    // Pursuer State
    //============================================================
    PursuerMonster::State GetState() const
    {
        return mPursuer.GetState();
    }

    //============================================================
    // AI Active
    // false: 表示・影・接触を無効にして待機する
    // true: 通常のPatrol / Investigate / Chase等を開始する
    //============================================================
    void SetActive(bool active)
    {
        //========================================================
        // 同じ状態なら何もしない
        //========================================================
        if (mActive == active)
        {
            return;
        }

        mActive = active;

        // AIの更新可否と、実体の表示・接触を揃える
        mPursuer.SetPresenceEnabled(active);

#if defined(_DEBUG)

        OutputDebugStringW(
            mActive ? L"[PursuerSystem] Active = true\n" : L"[PursuerSystem] Active = false\n"
        );

#endif
    }


    // EscapeMansionに入ったときに一度だけ開始する
    void BeginFinalChase(const ExplorationPlayer& player);

    bool IsActive() const
    {
        return mActive;
    }

    void AppendColliders(std::vector<Collider*>& destination);

    //============================================================
    // Collision後同期
    //============================================================
    void SyncAfterCollision()
    {
        if (!mInitialized || !mActive)
        {
            return;
        }

        mPursuer.SyncPositionAfterCollision();
    }

private:

    //========================================================
    // Placement読み込み
    //========================================================
    bool LoadPlacement();


    //========================================================
    // Pursuer生成
    //========================================================
    bool CreatePursuer(vnScene* scene);


    //========================================================
    // Patrol Point登録
    //========================================================
    void RegisterPatrolPoints();

    //============================================================
    // Search Point登録
    //============================================================
    void RegisterSearchPoints();


private:

    //========================================================
    // AI Placement
    //========================================================
    WorldPlacementTable mPlacementTable;


    //========================================================
    // Pursuer
    //========================================================
    PursuerMonster mPursuer;

    //============================================================
    // Pursuer Navigation
    // PursuerMonsterとは分離してGraphを管理する
    //============================================================
    PursuerNavigation mNavigation;

    //========================================================
    // Initialize済みか
    //========================================================
    bool mInitialized = false;

    bool mActive = false;
};

