#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "../player.h"

void Player::InitAttack()
{
    //=========================
   // 状態
   //=========================
    mAttackState = AttackState::None;

    //=========================
    // チャージ
    //=========================
    mChargeElapsedTime = 0.0f;
    mChargeMaxTime = 2.0f;

    //=========================
    // 攻撃
    //=========================
    mAttackElapsedTime = 0.0f;
    mAttackDuration = 0.4f;
    mAttackSpeed = 40.0f;

    //=========================
    // 距離
    //=========================
    mAttackMoveDistance = 0.0f;
    mBaseAttackDistance = 20.0f;
    mMaxBonusDistance = 40.0f;
    mAttackMaxDistance = mBaseAttackDistance;

    //=========================
    // 回転
    //=========================
    mAttackSpinAngle = 0.0f;
    mAttackSpinSpeed = 0.0f;

    //=========================
    // 状態系
    //=========================
    mIsJustAvoidDash = false;
    mJustAvoidTarget = nullptr;

    //=========================
    // SE制御
    //=========================
    mChargeCompletePlayed = false;
    mChargeSoundStarted = false;
}

//============================================================
// 攻撃開始
//============================================================
void Player::StartAttack()
{
    // ジャスト回避専用攻撃
    if (SlowSystem::Get().IsAttackOnly())
    {
        StartJustAvoidDashAttack();
        return;
    }

    if (mAttackState != AttackState::None) return;

    //=========================
    // チャージ開始
    //=========================
    mAttackState = AttackState::Charging;

    mChargeElapsedTime = 0.0f;
    mChargeCompletePlayed = false;
    mChargeSoundStarted = false;
}

//============================================================
// 攻撃終了
//============================================================
void Player::EndAttack()
{
    GetCollider()->SetType(ColliderType::Body);

    mAttackState = AttackState::None;

    mAttackMoveDistance = 0.0f;
    mAttackElapsedTime = 0.0f;

    mIsJustAvoidDash = false;
    mJustAvoidTarget = nullptr;

    SetHasHit(false);
}
//============================================================
// 攻撃リリース
//============================================================
void Player::AttackRelease()
{
    if (mAttackState != AttackState::Charging) return;

    if (mSound)
        mSound->PlaySfx(SoundID::Player_AttackRelese, 1.0f);

    //--------------------------------
    // チャージ率
    //--------------------------------
    float ratio = mChargeElapsedTime / mChargeMaxTime;
    ratio = std::clamp(ratio, 0.0f, 1.0f);

    //--------------------------------
    // パラメータ決定
    //--------------------------------
    float powerScale = 1.0f + ratio * 2.0f;

    mAttackSpinSpeed = 10.0f + ratio * 40.0f;

    float distRatio = ratio * ratio * ratio;

    mAttackMaxDistance =
        mBaseAttackDistance +
        distRatio * mMaxBonusDistance;

    mAttackDuration = 0.4f;
    mAttackElapsedTime = 0.0f;

    mAttackSpeed = 40.0f + ratio * 20.0f;

    //--------------------------------
    // フルチャージ強化
    //--------------------------------
    if (ratio >= 0.98f)
    {
        mAttackMaxDistance += 50.0f;
        mAttackSpeed = 110.0f;
        mAttackDuration = 0.65f;
        mAttackSpinSpeed += 20.0f;
    }

    //--------------------------------
    // 攻撃方向
    //--------------------------------
    if (mIsLookOn && Boss::GetInstance())
    {
        XMVECTOR bossPos = *Boss::GetInstance()->getPosition();
        XMVECTOR myPos = *getPosition();
        mAttackDir = XMVector3Normalize(bossPos - myPos);
    }
    else
    {
        mAttackDir = XMVector3Normalize(
            *vnCamera::getTarget() - *vnCamera::getPosition());
    }

    //--------------------------------
    // 回転軸
    //--------------------------------
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR axis = XMVector3Cross(up, mAttackDir);

    if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.0001f)
        axis = XMVectorSet(1, 0, 0, 0);
    else
        axis = XMVector3Normalize(axis);

    mAttackAxis = axis;

    //--------------------------------
    // 状態初期化
    //--------------------------------
    mAttackState = AttackState::Attacking;

    mAttackMoveDistance = 0.0f;
    mAttackSpinAngle = 0.0f;

    //--------------------------------
    // 攻撃情報
    //--------------------------------
    mAttackInfo.damage = mAtk * powerScale;
    mAttackInfo.knockbackPower = 3.0f * powerScale;
    mAttackInfo.knockbackDirection = mAttackDir;

    SetHasHit(false);
}

void Player::UpdateAttack(float dt)
{
    switch (mAttackState)
    {
    case AttackState::Charging:
        UpdateAttackCharge(dt);
        break;

    case AttackState::Attacking:
        UpdateAttackActive(dt);
        break;
    }
}

//============================================================
// チャージ更新
//============================================================
void Player::UpdateAttackCharge(float dt)
{
    // 時間更新
    mChargeElapsedTime += dt;

    if (mChargeElapsedTime > mChargeMaxTime)
        mChargeElapsedTime = mChargeMaxTime;

    //--------------------------------
    // SE
    //--------------------------------
    if (!mChargeSoundStarted && mChargeElapsedTime >= 0.1f)
    {
        mChargeSoundStarted = true;
    }

    if (mChargeElapsedTime >= mChargeMaxTime && !mChargeCompletePlayed)
    {
        if (mSound)
            mSound->PlaySfx(SoundID::Player_ChargeComplete, 1.0f);

        mChargeCompletePlayed = true;
    }

    //--------------------------------
    // 演出
    //--------------------------------
    float ratio = mChargeElapsedTime / mChargeMaxTime;

    float spin = 5.0f + ratio * 40.0f;

    mChargeSpinAngle += spin * dt;

    XMVECTOR camRight = vnCamera::getRight();
    XMVECTOR rot = XMQuaternionRotationAxis(camRight, mChargeSpinAngle);
    setQuaternion(&rot);
}

void Player::UpdateAttackActive(float dt)
{
    GetCollider()->SetType(ColliderType::Attack);
    SetCurrentAttackInfo(mAttackInfo);

    if (mIsJustAvoidDash)
    {
        UpdateJustAvoidDashAttack(dt);
    }
    else
    {
        UpdateNormalAttack(dt);
    }
}

//============================================================
// ジャスト回避専用ダッシュ攻撃
//============================================================
void Player::UpdateJustAvoidDashAttack(float dt)
{
    Boss* boss = mJustAvoidTarget ? mJustAvoidTarget : Boss::GetInstance();

    // ボスがいない or 死亡 → 即終了
    if (!boss || !boss->IsAlive())
    {
        EndAttack();
        return;
    }

    XMVECTOR myPos = *getPosition();
    XMVECTOR bossPos = *boss->getPosition();

    // ボス方向ベクトル（XZ平面）
    XMVECTOR toBoss = bossPos - myPos;
    toBoss = XMVectorSetY(toBoss, 0.0f);

    float distSq = XMVectorGetX(XMVector3LengthSq(toBoss));
    float dist = sqrtf(distSq);

    // 方向更新
    if (distSq > 0.0001f)
    {
        mAttackDir = XMVector3Normalize(toBoss);
        mAttackInfo.knockbackDirection = mAttackDir;
    }

    // 命中距離
    const float hitDist = boss->GetRadius() + GetRadius() + 2.5f;

    //====================================================
    // 命中処理
    //====================================================
    if (dist <= hitDist)
    {
        // ボス手前にスナップ
        XMVECTOR snapPos = bossPos - mAttackDir * (boss->GetRadius() + GetRadius() + 0.2f);
        snapPos = XMVectorSetY(snapPos, XMVectorGetY(*getPosition()));
        setPosition(&snapPos);

        if (!HasHit())
        {
            boss->TakeDamage(mAttackInfo);
            SetHasHit(true);

            ApplyAttackRecoil(5.0f);

            SlowSystem::Get().EndJustAvoidByHit();
            EndInvincible();
            ResetAttackInfo();
        }

        EndAttack();
        return;
    }

    //====================================================
    // 移動処理
    //====================================================
    float move = mAttackSpeed * dt;

    float remain = dist - hitDist;
    if (remain < 0.0f) remain = 0.0f;
    if (move > remain) move = remain;

    XMVECTOR delta = XMVectorScale(mAttackDir, move);
    addPosition(&delta);

    mAttackElapsedTime += dt;

    // 回転
    mAttackSpinAngle += mAttackSpinSpeed * dt;
    XMVECTOR spinRot = XMQuaternionRotationAxis(mAttackAxis, mAttackSpinAngle);
    setQuaternion(&spinRot);

    // タイムアウト保険
    if (mAttackElapsedTime >= mAttackDuration)
    {
        if (!HasHit())
        {
            boss->TakeDamage(mAttackInfo);
            SetHasHit(true);

            ApplyAttackRecoil(5.0f);

            SlowSystem::Get().EndJustAvoidByHit();
            EndInvincible();
            ResetAttackInfo();
        }

        EndAttack();
    }
}

//============================================================
// 通常攻撃更新
//============================================================
void Player::UpdateNormalAttack(float dt)
{
    float move = mAttackSpeed * dt;

    //--------------------------------
    // 移動
    //--------------------------------
    XMVECTOR delta = XMVectorScale(mAttackDir, move);
    addPosition(&delta);

    mAttackMoveDistance += move;
    mAttackElapsedTime += dt;

    //--------------------------------
    // 回転
    //--------------------------------
    mAttackSpinAngle += mAttackSpinSpeed * dt;

    XMVECTOR rot = XMQuaternionRotationAxis(mAttackAxis, mAttackSpinAngle);
    setQuaternion(&rot);

    //--------------------------------
    // 終了条件
    //--------------------------------
    if (mAttackMoveDistance >= mAttackMaxDistance ||
        mAttackElapsedTime >= mAttackDuration)
    {
        EndAttack();
    }
}

void Player::StartJustAvoidDashAttack()
{
    Boss* boss = Boss::GetInstance();
    if (!boss) return;
    if (mAttackState != AttackState::None) return;

    if (mSound)
        mSound->PlaySfx(SoundID::Player_JustAvoidAttack, 1.5f);

    XMVECTOR bossPos = *boss->getPosition();
    XMVECTOR myPos = *getPosition();

    XMVECTOR dir = bossPos - myPos;
    dir = XMVectorSetY(dir, 0.0f);

    if (XMVectorGetX(XMVector3LengthSq(dir)) < 0.0001f)
        dir = XMVectorSet(0, 0, 1, 0);
    else
        dir = XMVector3Normalize(dir);

    mIsJustAvoidDash = true;
    mJustAvoidTarget = boss;

    mAttackDir = dir;
    mAttackAxis = XMVectorSet(1, 0, 0, 0);
    mAttackSpinSpeed = 40.0f;

    mAttackState = AttackState::Attacking;
    mAttackElapsedTime = 0.0f;
    mAttackDuration = 0.3f;
    mAttackSpeed = 180.0f;
    mAttackMaxDistance = 9999.0f;
    mAttackMoveDistance = 0.0f;
    mAttackSpinAngle = 0.0f;

    mAttackInfo.damage = mAtk * 2.5f;
    mAttackInfo.knockbackPower = 6.0f;
    mAttackInfo.knockbackDirection = mAttackDir;

    SetHasHit(false);
}

void Player::ResetAttackInfo()
{
    AttackInfo info{};
    SetCurrentAttackInfo(info);

    SetHasHit(false);

    mAttackSpeed = 40.0f;
    mAttackDuration = 0.4f;
    mAttackElapsedTime = 0.0f;
    mAttackMoveDistance = 0.0f;
    mAttackMaxDistance = mBaseAttackDistance;

    mIsJustAvoidDash = false;
    mJustAvoidTarget = nullptr;
}

void Player::ApplyAttackRecoil(float power)
{
    XMVECTOR recoilDir = -mAttackInfo.knockbackDirection;
    recoilDir = XMVectorSetY(recoilDir, 0.0f);

    if (XMVectorGetX(XMVector3LengthSq(recoilDir)) > 0.0001f)
        recoilDir = XMVector3Normalize(recoilDir);
    else
        recoilDir = XMVectorSet(0, 0, 1, 0);

    AttackKnockback(recoilDir, power);
}
