#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "../player.h"

//============================================================
// 回避開始処理
//============================================================
void Player::Avoid()
{
    // スロー中は回避禁止（攻撃専用状態）
    if (SlowSystem::Get().IsJustAvoidSlow()) return;

    // 攻撃中は回避不可
    if (mAttackState != AttackState::None) return;

    // すでに回避中 or クールタイム中
    if (mIsAvoiding || mAvoidCooldownTimer > 0.0f) return;

    //--------------------------------------------------------
    // 回避開始
    //--------------------------------------------------------

    mIsAvoiding = true;
    mAvoidElapsedTime = 0.0f;
    mAvoidCooldownTimer = mAvoidCooldown;

    if (mSound)
        mSound->PlaySfx(SoundID::Player_Avoid, 1.0f);

    StartInvincible(1.0f);

    // 見た目変更（青くする）
    setAmbientB(0.5f);

    //--------------------------------------------------------
    // 回避方向決定
    //--------------------------------------------------------
    if (XMVectorGetX(XMVector3LengthSq(mLastVec)) < 0.0001f)
    {
        mAvoidDir = getForwardVector(*getQuaternion());
    }
    else
    {
        mAvoidDir = XMVector3Normalize(mLastVec);
    }

    //--------------------------------------------------------
    // ジャスト回避判定
    //--------------------------------------------------------
    if (Boss* boss = Boss::GetInstance())
    {
        Boss::AttackKind kind = boss->GetAttackKind();

        if (kind == Boss::AttackKind::Roll || kind == Boss::AttackKind::Press)
        {
            const float margin = (kind == Boss::AttackKind::Roll) ? 8.0f : 12.0f;

            XMVECTOR b = *boss->getPosition();
            XMVECTOR p = *getPosition();

            float dist = XMVectorGetX(XMVector3Length(p - b));

            if (dist <= (boss->GetRadius() + GetRadius() + margin))
            {
                if (!SlowSystem::Get().IsJustAvoidSlow())
                {
                    SlowSystem::Get().StartJustAvoidSlow(0.15f);

                    // スロー中は被弾無効
                    StartInvincible(9999.0f, false);
                }
            }
        }
    }
}

//============================================================
// 回避中の更新処理
//============================================================
void Player::UpdateAvoid(float dt)
{
    //====================================================
    // 経過時間更新
    //====================================================
    mAvoidElapsedTime += dt;

    //====================================================
    // 移動
    //====================================================
    float move = mAvoidSpeed * dt;
    XMVECTOR delta = XMVectorScale(mAvoidDir, move);
    addPosition(&delta);

    //====================================================
    // 回転（転がり）
    //====================================================
    float angle = move / mRadius;

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR rot = *getRotation();

    XMMATRIX invRot = XMMatrixRotationQuaternion(XMQuaternionInverse(rot));
    XMVECTOR localUp = XMVector3TransformNormal(up, invRot);

    XMVECTOR axis = XMVector3Normalize(XMVector3Cross(localUp, mAvoidDir));

    XMVECTOR q = XMQuaternionRotationAxis(axis, angle);

    rot = XMQuaternionMultiply(q, rot);
    rot = XMQuaternionNormalize(rot);

    setRotation(&rot);

    //====================================================
    // 時間更新
    //====================================================
    if (mAvoidElapsedTime >= mAvoidDuration)
    {
        mIsAvoiding = false;
        setAmbientB(0.0f);
    }
}

//============================================================
// ジャスト回避受付開始
//============================================================
void Player::OnAvoidTriggeredForJust(float windowSec)
{
    mJustAvoidTimer = windowSec;
}

//============================================================
// ジャスト回避タイマー更新
//============================================================
void Player::UpdateJustAvoidTimer(float dt)
{
    // ジャスト回避
    if (mJustAvoidTimer > 0.0f)
        mJustAvoidTimer -= dt;

    // 回避クールタイム
    if (mJustAvoidTimer < 0.0f)
        mJustAvoidTimer = 0.0f;
}