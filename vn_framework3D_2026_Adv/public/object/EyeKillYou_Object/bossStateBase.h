#pragma once

class LaserBeamEffect;

//ボスの攻撃管理用の基底ステートクラス
class BossStateBase
{
public:
    virtual ~BossStateBase() {}

    virtual void Enter(Boss* boss) {}
    virtual void Update(Boss* boss, float dt) {}
    virtual void Exit(Boss* boss) {}

    Effect* mFx = nullptr;
    bool mEffectOn = false;
};

//登場演出
class BossStateIntro : public BossStateBase
{
private:
    float mWait = 0.0f;     // 着地後に止める時間
    bool  mLanded = false;  // 1回だけ着地処理する用
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
};

class BossStateIdle : public BossStateBase
{
private:
    float mIdleTime = 0.0f;

public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
};

class BossStateSelect : public BossStateBase
{
    void Update(Boss* boss, float dt) override;
};

class BossStateCooldown : public BossStateBase
{
private:
    float mTimer;

public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
};

enum class RollPhase
{
    Landing,
    ChargeSpin,
    Rolling,
    End,
    None,
};

//物理反射型
class BossStateRollAttack :public BossStateBase
{
private:
    RollPhase mPhase = RollPhase::None;

    int         mBounceCount = 0;//反射数
    int         mMaxBoounceCount = 0;//最大反射数
    float       mTimer = 0.0f;
    float       mSpeed = 0.0f;
    float       mRollSpeed = 0.0f;
    bool        mHasHitPlayer = false;
    bool        mDustOn = false;
    bool        mJustWindowStarted = false;

    AttackInfo mAttackInfo;
    XMVECTOR mMoveDir = XMVectorZero();
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss)override;
};

//プレイヤー追尾型
class BossStateRollAttacktoPlayer : public BossStateBase
{
private:
    RollPhase mPhase = RollPhase::None;

    int         mBounceCount = 0;//反射数
    int         mMaxBoounceCount = 0;//最大反射数
    float       mTimer = 0.0f;
    float       mSpeed = 0.0f;
    float       mRollSpeed = 0.0f;
    bool        mHasHitPlayer = false;
    bool        mDustOn = false;
    bool        mJustWindowStarted = false;


    AttackInfo mAttackInfo;
    XMVECTOR mMoveDir = XMVectorZero();
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss)override;

};

enum class PressPhase
{
    JumpUp,
    AirChase,
    PressDown,
    Landing,
    None,
};

class BossStatePressAttack :public BossStateBase
{
private:
    int   mPressCount = 0;
    float mAttackRadius = 0.0f;
    float mTimer = 0.0f;
    bool        mJustWindowStarted = false;


    AttackInfo mPressAttackInfo;
    AttackInfo mAttackInfo;
    XMVECTOR   mTargetPos = XMVectorZero();
    PressPhase mPhase = PressPhase::None;

public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
};

enum class BreathPhase
{
    LookAt,
    Charge,
    Breathing,
    End,
    None,
};
class BossStateBreath : public BossStateBase
{
private:
    float mTimer = 0.0f;
    float mBreathRange = 0.0f;
    float mBreathAngle = 0.0f; // 扇半角
    float mSmokeSpawnTimer = 0.0f;

    bool mChargeEffectOn = false;
    bool mBreathEffectOn = false;

    // BreathEffect を止めたいなら保持
    BreathEffect* mBreathEffectPtr = nullptr;

    AttackInfo mAttackInfo;
    BreathPhase mPhase = BreathPhase::None;
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss)override;

};

enum class LaserPhase
{
    Charge,
    FiringFixed,
    Rotating,
    None,
};

class BossStateRollLaser : public BossStateBase
{
private:
    LaserPhase mPhase = LaserPhase::None;
    LaserBeamEffect* mLaserFx = nullptr;
    bool mChargeEffectOn = false;
    bool mLaserFxOn = false;    
    int mRotatedNum = 0; //回転数

    float mLaserTimer = 0.0f;
    float mLaserAngle = 0.0f;  //角度
    float mTotalRotated = 0.0f;
    float mLaserLength = 0.0f;
    bool  mLaserFiring = false;
public:
    float DistancePointToSegment(XMVECTOR point, XMVECTOR start, XMVECTOR end);//カプセルコライダー化予定

    void CheckLaserHit(Boss* boss);
    void DrawLaser(Boss* boss);
    float GetYawFromBossForward(Boss* boss);
    void UpdateLaserFxSegment(Boss* boss);
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss)override;

};

enum class ShockwavePhase
{
    JumpUp,     // 上昇
    Aim,        // 少し狙う
    SlamDown,   // 急降下
    Wave,       // 衝撃波展開中
    Recover,    // 次の行動まで少し待つ
    None,
};

class BossStateShockwave : public BossStateBase
{
private:
    ShockwavePhase mPhase = ShockwavePhase::None;

    int   mWaveCount = 0;          // 何回やるか
    int   mMaxWaveCount = 0;

    float mTimer = 0.0f;

    // 衝撃波の現在半径
    float mWaveRadius = 0.0f;

    // 衝撃波の広がる最大距離
    float mWaveMaxRadius = 0.0f;

    // 衝撃波の広がる速度
    float mWaveSpeed = 0.0f;

    // 衝撃波の厚み
    float mWaveThickness = 0.0f;

    // プレイヤーがこの1回の波で既に被弾したか
    bool mWaveHitPlayer = false;

    // 着地点
    XMVECTOR mTargetPos = XMVectorZero();

    // 直接踏みつけ本体のダメージ
    AttackInfo mPressAttackInfo;

    // 衝撃波ダメージ
    AttackInfo mWaveAttackInfo;

public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;

private:
    void UpdateShockwaveHit(Boss* boss);
};