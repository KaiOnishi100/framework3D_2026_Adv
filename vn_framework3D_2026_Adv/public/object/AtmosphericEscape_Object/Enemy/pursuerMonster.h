#pragma once

#include "pursuerAnimationController.h"

#include <array>
#include <cstdint>
#include <random>

class vnScene;
class ExplorationPlayer;
class PursuerNavigation;
class INavigationDoor;

//============================================================
// PursuerMonster
// ホラーゲーム用の追跡者AI
//============================================================
class PursuerMonster
{
public:

    //========================================================
    // AI State
    //========================================================
    enum class State
    {
        Patrol,
        Investigate,
        Chase,
        Search,
        Attack
    };

    //============================================================
    // Search Phase
    // State::Search の内部状態
    //============================================================
    enum class SearchPhase
    {
        //========================================================
        // Playerを最後に見た位置へ移動
        //========================================================
        MoveToLastKnown,

        //========================================================
        // 最後に見た位置で周囲を確認
        //========================================================
        LookAroundLastKnown,

        //========================================================
        // 近くのSearch Pointへ移動
        //========================================================
        MoveToSearchPoint,

        //========================================================
        // Search Pointで周囲を確認
        //========================================================
        LookAroundSearchPoint
    };

public:

    PursuerMonster() = default;

    ~PursuerMonster() = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(vnScene* scene, const XMFLOAT3& position);

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
    // Patrol Point追加
    //========================================================
    void AddPatrolPoint(const XMFLOAT3& point);

    // 描画対象の子モデル、影、物理接触を進行フラグに合わせる。
    void SetPresenceEnabled(bool enabled);

    void BeginFinalChase(const XMFLOAT3& startPosition, const XMFLOAT3& target);
    bool IsFinalChase() const { return mFinalChase; }

    //============================================================
    // Search Point追加
    //============================================================
    void AddSearchPoint(const XMFLOAT3& point)
    {
        mSearchPoints.push_back(point);
    }

    //============================================================
    // Line Of Sight用Collider設定
    //============================================================
    void SetSightColliders(const std::vector<Collider*>* colliders)
    {
        mSightColliders = colliders;
    }

    //============================================================
    // Body Collider
    //============================================================
    Collider* GetBodyCollider() const
    {
        return mBodyCollider.get();
    }

    //============================================================
    // Attack Collider
    //============================================================
    Collider* GetAttackCollider() const
    {
        return mAttackCollider.get();
    }
    //========================================================
    // Debug
    //========================================================
    State GetState() const
    {
        return mState;
    }


    const XMFLOAT3& GetPosition() const
    {
        return mPosition;
    }

    //============================================================
    // Noise System設定
    //============================================================
    void SetNoiseSystem(const NoiseSystem* system)
    {
        mNoiseSystem = system;
    }

    //============================================================
    // Navigation
    //============================================================
    void SetNavigation(const PursuerNavigation* navigation )
    {
        mNavigation = navigation;
    }

    //============================================================
    // Navigation Door設定
    // 所有権は持たない
    //============================================================
    void SetNavigationDoors(const std::vector<INavigationDoor*>* doors)
    {
        mNavigationDoors = doors;
    }

    //============================================================
    // Collision後のTransform同期
    // CollisionManagerがmModelを押し戻した結果を
    // AI側のmPositionへ戻す
    //============================================================
    void SyncPositionAfterCollision();


private:

    //========================================================
    // State Update
    //========================================================
    void UpdatePatrol(float dt, const ExplorationPlayer& player);

    void UpdateChase(float dt, const ExplorationPlayer& player);

    void UpdateSearch(float dt, const ExplorationPlayer& player);

    void UpdateAttack(float dt, ExplorationPlayer& player);

    void UpdateAttackCollider();


    //============================================================
    // Hearing
    //============================================================
    void UpdateInvestigate(float dt, const ExplorationPlayer& player);


    //========================================================
    // Detection
    //========================================================
    bool CanDetectPlayer(const ExplorationPlayer& player,float maxDistance) const;

    bool CanSeePlayer(const ExplorationPlayer& player, float maxDistance) const;

    //============================================================
    // Chase継続判定
    //============================================================
    bool CanMaintainChase(const ExplorationPlayer& player, float maxDistance) const;

    //========================================================
    // Movement
    //========================================================
    void MoveTowards(
        const XMFLOAT3& target,
        float speed,
        float dt
    );

    //============================================================
    // Navigation Waypoint専用3D移動
    //============================================================
    void MoveTowardsNavigationWaypoint(
        const XMFLOAT3& target,
        float speed,
        float dt
    );


    //============================================================
    // Nav Waypoint 3D到着判定
    //============================================================
    bool IsNearNavigationWaypoint(const XMFLOAT3& target, float distance) const;

    bool TryInsertOpenDoorDetour(const XMFLOAT3& waypoint);

    //========================================================
    // State Change
    //========================================================
    void ChangeState(State state);

    //============================================================
    // Line Of Sight
    //============================================================
    bool HasLineOfSightToPlayer(const ExplorationPlayer& player) const;

private:
    //============================================================
    // Search Helpers
    //============================================================
    bool IsNearPosition(const XMFLOAT3& target,float distance ) const;

    bool SelectSearchPoint();

    void BeginLookAround();

    bool UpdateLookAround(float dt);

    void SetFacingYaw(float yaw);

    //============================================================
    // Hearing
    //============================================================
    bool TryHearNoise(XMFLOAT3& outPosition) const;

    bool BuildNavigationPath(const XMFLOAT3& target);

    void MoveTowardsNavigated(const XMFLOAT3& target, float speed, float dt, bool forceRepath = false);

    void ClearNavigationPath();

    //============================================================
    // Door Handling
    // true: Door待ち中なので、このFrameは移動しない
    // false: Doorによる停止は不要
    //============================================================
    bool HandleNearbyDoor(const XMFLOAT3& waypoint);

    //============================================================
    // Patrol Point選択
    // 現在のPatrol PointへPathが存在するか確認する
    // 到達不能: 次のPatrol PointへSkip
    // 到達可能: Navigation Pathをそのまま保持してtrue
    // 全地点到達不能: false
    //============================================================
    bool SelectReachablePatrolPoint();

private:

    //========================================================
    // Temporary Model
    //========================================================
    SkeletalModel* mModel = nullptr;

    //========================================================
    // アニメーション管理
    //========================================================
    PursuerAnimationController mAnimation;

    //============================================================
    // Collider
    //============================================================
    std::unique_ptr<CapsuleCollider> mBodyCollider;
    std::unique_ptr<SphereCollider> mAttackCollider;

    //========================================================
    // World Position
    //========================================================
    XMFLOAT3 mPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };

    //============================================================
    // Facing Direction
    // Monsterが現在向いている方向
    // XZ平面上の正規化Vectorとして保持する
    //============================================================
    XMFLOAT3 mForward =
    {
        0.0f,
        0.0f,
        1.0f
    };


    //========================================================
    // AI State
    //========================================================
    State mState = State::Patrol;


    //========================================================
    // Patrol
    //========================================================
    std::vector<XMFLOAT3> mPatrolPoints;

    std::size_t mPatrolIndex = 0;

    // 到着済みの直近3地点 長い往復を繰り返しにくくする
    std::array<std::size_t, 3> mRecentPatrolPoints{SIZE_MAX, SIZE_MAX, SIZE_MAX};
    std::mt19937 mPatrolRandom{std::random_device{}()};
    float mPatrolRetryTimer = 0.0f;

    bool mFinalChase = false;
    float mFinalChasePathRetry = 0.0f;

    //============================================================
    // Search Point
    // Search Point: Playerを見失った時に確認する候補地点
    //============================================================
    std::vector<XMFLOAT3> mSearchPoints;

    //============================================================
    // Search内部状態
    //============================================================
    SearchPhase mSearchPhase = SearchPhase::MoveToLastKnown;

    //============================================================
    // 現在調査しているSearch Point
    //============================================================
    std::size_t mCurrentSearchPointIndex = 0;

    //============================================================
    // 有効なSearch Pointが選ばれているか
    //============================================================
    bool mHasSearchPoint = false;

    //============================================================
    // Look Around
    //============================================================
    float mSearchLookTimer = 0.0f;

    //============================================================
    // Look開始時の向き
    //============================================================
    float mSearchBaseYaw = 0.0f;

    //============================================================
    // Search Point / LastKnown到着判定
    //============================================================
    float mSearchArrivalDistance = 0.40f;

    //============================================================
    // 周囲を確認する時間
    //============================================================
    float mSearchLookDuration = 2.5f;

    //============================================================
    // LookAround最大角度
    // 左右70°程度見る
    //============================================================
    float mSearchLookAngleDegrees = 70.0f;


    //============================================================
    // LastKnownからSearch Pointを探す範囲
    //============================================================
    float mSearchPointSearchRadius = 8.0f;

    //========================================================
    // 最後にPlayerを確認した位置
    //========================================================
    XMFLOAT3 mLastKnownPlayerPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };

    //========================================================
    // Parameters
    //========================================================

    //============================================================
    // Rotation
    //============================================================

    // 現在の見た目上のYaw
    float mCurrentYaw = 0.0f;
    // 旋回速度 [rad/sec]
    float mTurnSpeed = 8.0f;

    //============================================================
    // Sight Occluders
    //============================================================
    const std::vector<Collider*>* mSightColliders = nullptr;

    // Patrol速度
    float mPatrolSpeed = 1.2f;

    // Chase速度
    float mChaseSpeed = 4.0f;

    // 最終脱出中の追跡速度 [m/s]
    // 近距離ではプレイヤーが走って距離を作れる余地を残す
    // 離れている間は走り続けるプレイヤーにも少しずつ追いつく
    float mFinalChaseNearSpeed = 4.7f;
    float mFinalChaseFarSpeed = 5.3f;

    // Player発見距離
    float mDetectionDistance = 10.0f;

    // Playerを見失う距離
    // DetectionDistanceより大きくして
    // Stateが境界でバタつくのを防ぐ
    float mLoseDistance = 15.0f;

    // Patrol Point到着判定
    float mPatrolArrivalDistance = 0.35f;

    //============================================================
    // View Angle
    // Monsterの視野角
    //============================================================
    float mViewAngleDegrees = 110.0f;

    //============================================================
    // Hearing
    //============================================================
    
    const NoiseSystem* mNoiseSystem = nullptr;

    //============================================================
    // 最後に聞こえた音の位置
    //============================================================
    XMFLOAT3 mInvestigatePosition =
    {
        0.0f,
        0.0f,
        0.0f
    };

    //============================================================
    // Investigate地点への到着距離
    //============================================================
    float mInvestigateArrivalDistance = 0.45f;

    //============================================================
    // Navigation
    //============================================================
    const PursuerNavigation* mNavigation = nullptr;

    //============================================================
    // Current Path
    //============================================================
    std::vector<XMFLOAT3> mNavigationPath;

    //============================================================
    // 現在向かっているPath Point
    //============================================================
    std::size_t mNavigationPathIndex =  0;

    //============================================================
    // 現在Pathが向かっている最終目的地
    //============================================================
    XMFLOAT3 mNavigationGoal =
    {
        0.0f,
        0.0f,
        0.0f
    };

    //============================================================
    // Pathを持っているか
    //============================================================
    bool mHasNavigationPath = false;

    //============================================================
    // Waypoint到着距離
    //============================================================
    float mNavigationArrivalDistance = 0.30f;

    //============================================================
    // Navigation Doors
    // Pursuerが通路上で閉じたDoorを発見し
    // Opening要求を出すために使用
    //============================================================
    const std::vector<INavigationDoor*>* mNavigationDoors = nullptr;

    //============================================================
    // Doorを操作する距離
    //============================================================
    float mDoorInteractionDistance = 1.25f;

    // 回り込み中は角をショートカットしないよう到着判定を小さくする
    std::size_t mDoorDetourRemaining = 0;
    float mDoorDetourRetry = 0.0f;

    //============================================================
    // Chase Perception Memory
    // Chase中に一瞬Playerが見えなくなっても
    // 即Searchへ遷移しないための猶予時間
    //============================================================

    // Playerを見失っている時間
    float mChaseLostSightTimer = 0.0f;

    // 見失ってからSearchへ移行するまでの時間
    float mChaseLostSightGraceTime = 1.75f;

    //============================================================
    // Door Path Width
    //============================================================
    float mDoorPathWidth = 0.90f;

    //============================================================
    // Attack Parameters
    //============================================================
    
    // Attack Stateへ入る距離
    float mAttackStartDistance = 1.45f;

    // 実際にDamageが届く距離
    float mAttackHitDistance =  1.60f;

    // 1回のDamage
    float mAttackDamage = 25.0f;

    //============================================================
    // Attack Timeline
    // 0.00s : Attack開始
    // 0.35s : Hit判定
    // 0.90s : Attack終了
    //============================================================
    float mAttackTimer = 0.0f;
    float mAttackHitTime = 0.30f;
    float mAttackEndTime = 0.90f;
    float mAttackHitStartTime = 0.30f;
    float mAttackHitEndTime = 0.45f;
    //============================================================
    // 1Attackにつき1Hitだけ
    //============================================================
    bool mAttackHitProcessed = false;

    //============================================================
    // Attack後Cooldown
    //============================================================
    float mAttackCooldownTimer = 0.0f;
    float mAttackCooldownDuration = 1.0f;

    //============================================================
    // AudioParameter
    //============================================================
    XMFLOAT3 mAudioPreviousPosition{};
    float mStepSoundTimer = 0.0f;
    float mBreathSoundTimer = 1.2f;
    int mStepSoundVariant = 0;
    bool mAttackSoundPending = false;
};

