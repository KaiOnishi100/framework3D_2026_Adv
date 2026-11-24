#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "player.h"

Player* Player::sInstance = nullptr;

Player::Player(const WCHAR* folder, const WCHAR* file) : Character(folder , file)
{
    //=========================
    // 基本ステータス
    //=========================
    mHP = 100.0f;
    mMaxHP = mHP;
    mAtk = 20;

    mRadius = 1.0f;
    mSpeed = 8.0f;
    mVerticalSpeed = 0.0f;
    mGravity = -0.02f;
    mJumpPower = 0.5f;
    mBounce = 0.4f;
    mKnockbackDuration = 1.0f;

    //=========================
    // 状態
    //=========================
    mIsGrounded = false;
    mIsLookOn = false;
    mIsJumpJustSlow = false;

    //=========================
    // ベクトル
    //=========================
    mMoveVector = XMVectorZero();
    mLastVec = XMVectorZero();
    mAvoidDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    //=========================
    // サブシステム初期化
    //=========================
    InitAttack();

    //=========================
    // その他
    //=========================
    mAttackInfo.knockbackPower = 1.0f;

    setPosition(0.0f, 10.0f, 0.0f);

    useQuaternion(true);

    sInstance = this;

    setRenderFlag(ObjectBase::eRenderFlag::Lighting, true);
    setRenderFlag(ObjectBase::eRenderFlag::Transparent, true);
    setRenderFlag(ObjectBase::eRenderFlag::ZWrite, true);

    mCollider = new SphereCollider(this, mRadius);
    mCollider->SetMass(1.0f);
}

//各メンバ変数の初期化処理
void Player::init()
{
    mHP = 100.0f;
    mMaxHP = mHP;
}

void Player::update(float dt)
{

    if (vnScene::IsHitStop())
        return;

    UpdateTimers(dt);
    UpdateStatus(dt);

    if (IsFreezeGameplay())return;

    HandleInput();
    UpdateAction(dt);
    UpdateTransform();
    ClampToStage();

    ////プレイヤーのコライダー可視化
    //vnDebugDraw::Sphere(*this->getPosition(), 0.5f, 0xffffff00); // 黄色
        //vnFont::print(100.0f, 100.0f, L"%f,%f,%f,%f",
    //    XMVectorGetX(*getPosition()), XMVectorGetY(*getPosition()), XMVectorGetZ(*getPosition()), XMVectorGetW(*getPosition()));

}

void Player::OnDeath()
{
    AddEffect(std::make_unique<ExplosionEffect>(this, 2.0f));
    if (mSound)
    {
        mSound->PlaySfx(SoundID::Character_Dead, 1.0f);
    }
    vnCamera::StartShake(0.8f, 0.5f);
    if (GetCollider()) GetCollider()->SetEnabled(false);
}

float Player::GetCameraSpeedRate() const
{
    //============================================================
    // 通常移動
    //============================================================
    float speed = mSpeed;

    //============================================================
    // 回避中は少し速く見せる
    //============================================================
    if (mIsAvoiding)
    {
        speed = mAvoidSpeed;
    }

    //============================================================
    // 攻撃中はかなり速く見せる
    // mAttackSpeed はチャージ攻撃やジャスト回避攻撃で大きくなる
    //============================================================
    if (mAttackState == AttackState::Attacking)
    {
        speed = mAttackSpeed;
    }

    //============================================================
    // 速度を 0.0 ～ 1.0 に変換
    // 8.0f くらいなら通常
    // 110.0f 以上なら最大演出
    //============================================================
    float rate = (speed - 8.0f) / (110.0f - 8.0f);

    return std::clamp(rate, 0.0f, 1.0f);
}

void Player::ToggleLockOn()
{
    if (!mIsLookOn)
    {
        mIsLookOn = true;
    }
    else {
        mIsLookOn = false;
        vnCamera::EndLockOn();
    }
}

void Player::TakeDamage(const AttackInfo& info)
{
    // ノックバック開始
    if (IsInvincible()) return;

    mIsKnockback = true;

    mHP -= info.damage;
    StartInvincible(1.0f);
    if (auto* snd = GetSound())
    {
        snd->PlaySfx(SoundID::Hit, 1.0f);
    }

    vnCamera::StartShake(0.3f, 0.2f);

    if (CanBeKnockback())
    {
        mIsKnockback = true;
        ApplyKnockback(info.knockbackDirection, info.knockbackPower);

        mIsGrounded = false;
    }

    if (mHP <= 0 && !mIsDead)
    {
        mHP = 0;
        mIsDead = true;
        OnDeath();
    }
}
void Player::StartJumpJustSlow()
{
    //====================================================
    // 既にスロー中なら二重開始しない
    //====================================================
    if (SlowSystem::Get().IsJustAvoidSlow())
        return;

    //====================================================
    // ジャンプ成功版のジャストスロー開始
    //====================================================
    SlowSystem::Get().StartJustAvoidSlow(0.01f);

    // ジャンプ成功版フラグを立てる
    mIsJumpJustSlow = true;

    // スロー中は被弾で壊れないように無敵
    StartInvincible(9999.0f, false);

    mIsLookOn = true;
    if (Boss::GetInstance())
    {
        vnCamera::StartLockOn(*Boss::GetInstance()->getPosition());
    }
}
void Player::UpdateJumpJustSlowLanding()
{
    //====================================================
    // 前フレーム false → 今フレーム true なら着地した
    //====================================================
    const bool landedThisFrame = (!mWasGroundedLastFrame && mIsGrounded);

    if (mIsJumpJustSlow && landedThisFrame)
    {
        //================================================
        // 着地したのでスロー解除
        //================================================
        SlowSystem::Get().EndJustAvoidByHit();
        EndInvincible();

        mIsJumpJustSlow = false;
    }

    // 次フレーム用に保存
    mWasGroundedLastFrame = mIsGrounded;
}

Player* Player::GetInstance()
{
    return sInstance;
}

void Player::UpdateTimers(float dt)
{
    // ジャスト回避猶予
    if (mJustAvoidTimer > 0.0f)
    {
        mJustAvoidTimer -= dt;
        if (mJustAvoidTimer < 0.0f)
            mJustAvoidTimer = 0.0f;
    }

    // 回避クールタイム
    if (mAvoidCooldownTimer > 0.0f)
    {
        mAvoidCooldownTimer -= dt;
        if (mAvoidCooldownTimer < 0.0f)
            mAvoidCooldownTimer = 0.0f;
    }
}

void Player::UpdateStatus(float dt)
{
    UpdateJumpJustSlowLanding();

    // 無敵時間
    UpdateInvincible(dt);

    // ノックバック
    UpdateKnockback(dt);

    // エフェクト
    mEffectManager.Update(dt);

    // スロー中はロックオン強制
    if (SlowSystem::Get().IsJustAvoidSlow())
    {
        mIsLookOn = true;

        if (Boss::GetInstance())
            vnCamera::StartLockOn(*Boss::GetInstance()->getPosition());
    }
}

void Player::HandleInput()
{
    if (!mIsKnockback)
    {
        mInputHandler.HandleInput(this);
    }    
}

void Player::UpdateAction(float dt)
{
    if (mAttackState != AttackState::None)
    {
        UpdateAttack(dt);
    }
    else
    {
        if (!mIsKnockback && !SlowSystem::Get().IsAttackOnly())
        {
            Move(dt);
        }
    }

    if (mIsAvoiding)
    {
        UpdateAvoid(dt);
    }
}

void Player::UpdateTransform()
{
    if (mIsLookOn)
    {
        if (Boss::GetInstance())
            vnCamera::StartLockOn(*Boss::GetInstance()->getPosition());
    }
}

void Player::ClampToStage()
{
    XMVECTOR pos = *getPosition();
    mStage->ClampPosition(pos);
    setPosition(&pos);
}

