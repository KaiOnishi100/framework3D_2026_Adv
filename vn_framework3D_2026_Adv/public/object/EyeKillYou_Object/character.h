#pragma once

struct AttackInfo
{
	float damage;
	float knockbackPower;
	bool  mHasHit = false;
	XMVECTOR knockbackDirection;
};

class Character : public Object
{
public:
	virtual ~Character() = default;

	bool IsAlive() const { return mHP > 0; }

	virtual void TakeDamage(const AttackInfo& info) = 0;

	void executeGravity(float dt);//重力適用
	void CheckFallOut();
	void ClampToGround(const MeshCollider* ground);
	void UpdateKnockback(float dt);
	void ApplyKnockback(XMVECTOR dir, float power);     //ノックバック用
	void AttackKnockback(XMVECTOR dir, float power);    //アタック後のノックバック用
	void StartInvincible(float time);                   //無敵時間セット
	void StartInvincible(float time, bool spawnEffect);	// エフェクト出す/出さないを選べる

	void EndInvincible();

	void SetVerticalSpeed(float v) { mVerticalSpeed = v; }
	void SetGravity(float g) { mGravity = g; }
	void SetGrounded(bool flag) { mIsGrounded = flag; }
	void SetHasHit(bool flag) { mAttackInfo.mHasHit = flag; }

	Effect* AddEffect(std::unique_ptr<Effect> effect);

	// HP関連
	float GetHP() const { return mHP; }
	float GetMaxHP() const { return mMaxHP; }

	float GetVerticalSpeed() { return mVerticalSpeed; }
	float GetGravity() { return mGravity; }
	float GetRadius() { return mRadius; }

	bool IsGrounded() { return mIsGrounded; }
	bool IsKnockback() { return mIsKnockback; }
	bool HasHit() { return mAttackInfo.mHasHit; }

	//============================================================
    // ゲームプレイ停止（演出だけ動かす）用
    //============================================================
	void SetFreezeGameplay(bool v) { mFreezeGameplay = v; }
	bool IsFreezeGameplay() const { return mFreezeGameplay; }

	//============================================================
    // 攻撃情報を返す関数
    //============================================================
	void SetCurrentAttackInfo(AttackInfo info) { mCurrentAttack = info; }
	AttackInfo GetCurrentAttackInfo() { return mCurrentAttack; }

	//============================================================
    // 音声関連
    //============================================================
	SoundBank* GetSound() const { return mSound; }
	void SetSound(SoundBank* s) { mSound = s; }

	//============================================================
    // エフェクト管理用
    //============================================================
	EffectManager& GetEffectManager() { return mEffectManager; }
	const EffectManager& GetEffectManager() const { return mEffectManager; }

	Character* AsCharacter() override { return this; }

	// Character クラスの public セクションに宣言を追加
public:
	virtual void render();
	// vnObject のフックをオーバーライドして Effect を描画する
	virtual void renderEffects() override;

protected:
	Character(const WCHAR* folder, const WCHAR* file);

	void UpdateInvincible(float dt);
	bool IsInvincible() const { return mInvincibleTime > 0.0f; }

	virtual bool CanBeKnockback() const { return true; }
	virtual void OnDeath() {};

	AttackInfo mAttackInfo;
	AttackInfo mCurrentAttack;
	XMVECTOR mVelocity;
	EffectManager mEffectManager;
	SoundBank* mSound = nullptr;
protected:
	float   mMaxHP;
	float   mHP;
	int     mAtk;

	bool    mIsGrounded;    // 地面にいるか
	bool    mIsKnockback;   //ノックバック中か
	bool    mIsDead;
	bool    mFreezeGameplay = false;

	float   mRadius;
	float   mBounce;        // 着地時の跳ね返り
	float   mSpeed;
	float   mInvincibleTime = 0.0f;
	float   mVerticalSpeed; // 上下速度
	float   mGravity;       // 重力
	float   mKnockbackFriction;
	float   mKnockbackTimer = 0.0f;
	float   mKnockbackDuration = 0.0f;
	float   mTakeDamageTime = 0.0f;
};