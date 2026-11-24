#pragma once

class InputHandler;
class BossStage;
class Boss;

enum class AttackState
{
	None,
	Charging,
	Attacking
};

class  Player : public Character , public IControllable
{
public:
	Player(const WCHAR* folder, const WCHAR* file);

	void init();
	void update(float dt);
	void OnDeath()override; //死亡演出

	float GetCameraSpeedRate() const;
	void ToggleLockOn() override;               //カメラロックオン処理

	void TakeDamage(const AttackInfo& info)override;

	void SetStage(BossStage* stage) { mStage = stage; }

	InputHandler& GetInputHandler() { return mInputHandler; }

	//インスタンスを生成
	static Player* GetInstance();

public:
	// ジャスト回避アクション処理
	void StartJumpJustSlow();
	void UpdateJumpJustSlowLanding();
	bool IsJumpJustSlow() const { return mIsJumpJustSlow; }
	bool IsJustAvoidWindow() const { return (mJustAvoidTimer > 0.0f); }

	//外部から触る予定
	void EndAttack();                           //攻撃終了
	void ResetAttackInfo();

private:

	void UpdateTimers(float dt);
	void UpdateStatus(float dt);
	void HandleInput();
	void UpdateAction(float dt);
	void UpdateTransform();
	void ClampToStage();

	// 攻撃処理
	void StartAttack()override;                 //攻撃実装(IControllable)

	void AttackRelease()override;               //攻撃ボタンリリース処理(IControllable)
	void UpdateAttack(float dt);

	// 攻撃内部処理
	void UpdateAttackCharge(float dt);
	void UpdateAttackActive(float dt);
	void UpdateJustAvoidDashAttack(float dt);
	void UpdateNormalAttack(float dt);

	//攻撃初期化処理
	void InitAttack();
	void StartJustAvoidDashAttack();

	void ApplyAttackRecoil(float power);

	//移動処理
	void Move(float dt);
	void AddMoveInput(Direction dir) override;  //移動処理(IControllable)
	void AddMoveAxis(float x, float y) override;//スティックでの移動処理(IControllable)
	void Jump() override;                       //ジャンプ処理(IControllable)

	//回避処理
	void Avoid() override;                     //回避処理(IControllable)
	void UpdateAvoid(float dt);
	void OnAvoidTriggeredForJust(float windowSec);
	void UpdateJustAvoidTimer(float dt);

private:
	static Player* sInstance;

    //=========================
    // 回避（Avoid）
    //=========================
    
    // 設定値
	float mAvoidSpeed = 12.0f;
	float mAvoidDuration = 0.5f;
	float mAvoidCooldown = 0.5f;

	// 実行時
	float mAvoidElapsedTime = 0.0f;  // 経過時間
	float mAvoidCooldownTimer = 0.0f;  // クールタイム残り

	// 状態
	bool mIsAvoiding = false;

	// 移動
	XMVECTOR mAvoidDir = XMVectorSet(0, 0, 1, 0);

	//エフェクト用
	float mMoveDustTimer = 0.0f;

	//=========================
    // 攻撃（Attack）
    //=========================
    
    // 状態
	AttackState mAttackState = AttackState::None;

	float mChargeSpinAngle = 0.0f;

	// チャージ
	float mChargeElapsedTime = 0.0f;
	float mChargeMaxTime = 2.0f;

	// 攻撃実行
	float mAttackElapsedTime = 0.0f;
	float mAttackDuration = 0.4f;

	// 移動
	float mAttackMoveDistance = 0.0f;
	float mAttackMaxDistance = 70.0f;

	// 速度
	float mAttackSpeed = 40.0f;

	// 回転
	float mAttackSpinAngle = 0.0f;
	float mAttackSpinSpeed = 0.0f;

	// 方向
	XMVECTOR mAttackDir = XMVectorZero();
	XMVECTOR mAttackAxis = XMVectorSet(1, 0, 0, 0);

	// 距離補正
	float mBaseAttackDistance = 20.0f;
	float mMaxBonusDistance = 40.0f;

	// SE制御
	bool  mChargeCompletePlayed = false;
	float mChargeSoundStartTime = 0.10f;
	bool  mChargeSoundStarted = false;

	//=========================
    // ジャスト回避
    //=========================
    
    // 設定
	float mJustAvoidWindow = 0.12f;       // 判定受付時間

	// 実行時
	float mJustAvoidTimer = 0.0f;        // 残り時間

	float mJustAvoidHitRadius = 2.0f;   // ボス近くで吸着終了する距離

	float mJustAvoidSnapOffset = 1.0f;   // ボスにめり込まない距離


	// 状態
	bool  mIsJustAvoidDash = false;
	Boss* mJustAvoidTarget = nullptr;

	//=========================
    // ジャンプ
    //=========================
	float mJumpPower = 0.5f;

	bool mIsJumpJustSlow = false;          // ジャンプ成功で入ったスロー中か
	bool mWasGroundedLastFrame = false;    // 前フレームの接地状態
	
	bool mIsLookOn = false;           //カメラロックオンフラグ

	BossStage* mStage = nullptr;

	//入力用変数
	InputHandler mInputHandler;

	XMVECTOR mMoveVector = XMVectorZero();
	XMVECTOR mLastVec = XMVectorZero();

	//ノックバックの有無
	bool CanBeKnockback() const override { return true; }

};
