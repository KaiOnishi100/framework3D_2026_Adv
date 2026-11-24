#pragma once

class BossStateBase;

enum class JustAvoidKind { 
    None,
    Roll,
    Press 
};
class Boss : public Character
{
public:
    enum class AttackKind {
        None,
        Roll,
        Press,
    };

	Boss(const WCHAR* folder, const WCHAR* file);

	void init();
	void update(float deltaTime);

    void TakeDamage(const AttackInfo& info)override;
    void OnDeath()override;
    // ステート取得用
    BossStateBase* GetState() { return mState; }
    BossStage* GetStage() { return mStage; }


    int GetLastAttack() { return mLastAttack; }
    void SetLastAttack(int lastAttack) { mLastAttack = lastAttack; }

    void StartDamageFlash(float durationSec);
    void UpdateDamageFlash(float dt);
    void ApplyDamageFlashColor(bool on);

    // プレイヤー方向を向く
    void LookAt(const XMVECTOR& target);
    void LookAtSmooth(const XMVECTOR& target, float speed);//補間用
    void LookAtSmoothFlat(const XMVECTOR& target, float speed);

    void SetStage(BossStage* stage) { mStage = stage; }
    void ChangeState(BossStateBase* newState);
   
    void  SetBaseHeight(float h) { mBaseHeight = h; }
    float GetBaseHeight() const { return mBaseHeight; }
    float GetDefaultHeight() const{ return mDefaultHeight; }

    bool IsIntroFinished() const { return mIntroFinished; }
    bool IsInGuaranteedRoll() const { return mGuaranteedRoll; }

    void SetIntroFinished(bool v) { mIntroFinished = v; }
    void SetGuaranteedRoll(bool v) { mGuaranteedRoll = v; }

    void SetAttackKind(AttackKind k) { mAttackKind = k; }
    AttackKind GetAttackKind() const { return mAttackKind; }

    void BeginJustAvoidWindow(AttackKind k, float sec);
    void UpdateJustAvoid(float dt);
    JustAvoidKind GetJustAvoidKind() const;

    void SetTutorialEnabled(bool v) { mTutorialEnabled = v; }
    bool IsTutorialEnabled() const { return mTutorialEnabled; }
    //インスタンスを生成
    static Boss* GetInstance();

private:
    static Boss* sInstance;

    BossStateBase* mState;//現在の状態
    BossStage* mStage;

  
    int   mLastAttack    = -1;

    float mDefaultHeight = 0.0f;
    float mBaseHeight    = 0.0f;
    float mStateTimer    = 0.0f;

    bool mIntroFinished = false;
    bool mGuaranteedRoll = false;
    bool mTutorialEnabled = true;
    
    AttackKind mAttackKind = AttackKind::None;
    float mJustTimer = 0.0f;

    // ジャスト回避専用
    AttackKind mJustAttackKind = AttackKind::None;

    //-------------------------------
    // ダメージ点滅（赤フラッシュ）用
    //-------------------------------
    float mDamageFlashTimer = 0.0f;     // 残り時間（0で終了）
    float mDamageFlashInterval = 0.05f; // 点滅の間隔（秒） 0.05=20Hzくらいでパキパキ点滅
    float mDamageFlashAcc = 0.0f;       // 点滅用の累積
    bool  mDamageFlashOn = false;       // 今赤くするフレームかどうか

    float mBaseAmbientR = 0.0f;
    float mBaseAmbientG = 0.0f;
    float mBaseAmbientB = 0.0f; 

    void AlignToGround(const XMVECTOR& groundNormal);
    bool CanBeKnockback() const override { return false; }
};