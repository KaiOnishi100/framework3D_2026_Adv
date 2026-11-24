#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "../player.h"

//============================================================
// 移動処理
//============================================================
void Player::Move(float dt)
{
    //====================================================
   // 入力ベクトルをXZ平面に制限
   //====================================================
    mMoveVector = XMVectorSetY(mMoveVector, 0.0f);

    //====================================================
    // 正規化（斜め移動で速度が上がるのを防ぐ）
    //====================================================
    float lenSq = XMVectorGetX(XMVector3LengthSq(mMoveVector));
    if (lenSq > 1.0f)
    {
        mMoveVector = XMVector3Normalize(mMoveVector);
    }

    //====================================================
    // 移動量計算
    //====================================================
    XMVECTOR moveDelta = XMVectorScale(mMoveVector, mSpeed * dt);
    mLastVec = moveDelta;

    // 入力なしなら何もしない
    if (XMVectorGetX(XMVector3LengthSq(moveDelta)) < 0.00001f)
    {
        mMoveVector = XMVectorZero();
        return;
    }

    //====================================================
    // 実際に移動
    //====================================================
    addPosition(&moveDelta);

    //====================================================
    // 移動エフェクト（砂埃）
    //====================================================
    float dist = XMVectorGetX(XMVector3Length(moveDelta));

    if (mIsGrounded && dist > 0.001f && !mIsKnockback && mAttackState == AttackState::None)
    {
        mMoveDustTimer -= dt;

        if (mMoveDustTimer <= 0.0f)
        {
            AddEffect(std::make_unique<MoveDustEffect>(this, 1.0f));
            mMoveDustTimer = 0.08f;
        }
    }
    else
    {
        mMoveDustTimer = 0.0f;
    }

    //====================================================
    // 回転処理（転がり）
    //====================================================
    float distance = XMVectorGetX(XMVector3Length(moveDelta));
    XMVECTOR dir = XMVector3Normalize(moveDelta);

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMVECTOR rot = *getQuaternion();
    XMMATRIX invRotMat = XMMatrixRotationQuaternion(XMQuaternionInverse(rot));

    XMVECTOR localUp = XMVector3TransformNormal(up, invRotMat);
    XMVECTOR localDir = XMVector3TransformNormal(dir, invRotMat);

    XMVECTOR axis = XMVector3Normalize(XMVector3Cross(localUp, localDir));

    float angle = distance / mRadius;

    XMVECTOR q = XMQuaternionRotationAxis(axis, angle);

    rot = XMQuaternionMultiply(q, rot);
    rot = XMQuaternionNormalize(rot);

    setQuaternion(&rot);

    //====================================================
    // 入力リセット
    //====================================================
    mMoveVector = XMVectorZero(); 
}

//============================================================
// キー入力による移動ベクトル加算
//============================================================
void Player::AddMoveInput(Direction dir)
{
    if (mAttackState != AttackState::None) return;
    if (mIsKnockback) return;

    // カメラ基準方向
    XMVECTOR camPos = *vnCamera::getPosition();
    XMVECTOR camTarget = *vnCamera::getTarget();

    XMVECTOR forward = XMVector3Normalize(camTarget - camPos);
    forward = XMVectorSetY(forward, 0);
    forward = XMVector3Normalize(forward);

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));

    XMVECTOR inputDirection = XMVectorZero();

    switch (dir)
    {
    case Forward:  inputDirection = forward; break;
    case Backward: inputDirection = -forward; break;
    case Right:    inputDirection = right; break;
    case Left:     inputDirection = -right; break;
    }

    inputDirection = XMVectorSetY(inputDirection, 0);
    mMoveVector += inputDirection;
}

void Player::AddMoveAxis(float x, float y)
{
    if (mAttackState != AttackState::None) return;
    if (mIsKnockback) return;
    //----------------------------
    // 入力がほぼ無いなら無視
    //----------------------------
    const float dead = 0.20f;
    if (fabsf(x) < dead) x = 0.0f;
    if (fabsf(y) < dead) y = 0.0f;

    if (x == 0.0f && y == 0.0f)
        return;

    //----------------------------
    // カメラの forward/right を作る（AddMoveInput と同じ）
    //----------------------------
    XMVECTOR camPos = *vnCamera::getPosition();
    XMVECTOR camTarget = *vnCamera::getTarget();

    // 前方向：ターゲット - 位置
    XMVECTOR forward = camTarget - camPos;

    // 地面移動なのでYを落とす
    forward = XMVectorSetY(forward, 0.0f);

    // 長さが0に近いなら事故るので保険
    if (XMVectorGetX(XMVector3LengthSq(forward)) < 0.0001f)
    {
        forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }
    else
    {
        forward = XMVector3Normalize(forward);
    }

    // 右方向： up × forward（※あなたの AddMoveInput と同じ計算）
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));

    //----------------------------
    // スティック入力で合成
    //----------------------------
    // 斜め入力（x=1,y=1）だと長さが √2 になるので、
    // アナログは「正規化してから」足すと、斜めだけ速くなる問題を防げる
    XMVECTOR v =
        XMVectorScale(right, x) +
        XMVectorScale(forward, y);

    // 地面移動なのでYは0
    v = XMVectorSetY(v, 0.0f);

    //----------------------------
    // 斜め加速防止：長さが1を超えるなら正規化
    //----------------------------
    float lenSq = XMVectorGetX(XMVector3LengthSq(v));
    if (lenSq > 1.0f)
    {
        v = XMVector3Normalize(v);
    }

    mMoveVector += v;
}

//============================================================
// ジャンプ処理
//============================================================
void Player::Jump()
{
    if (mAttackState != AttackState::None) return;
    if (!mIsGrounded) return;

    if (mSound)
    {
        mSound->PlaySfx(SoundID::Player_Jump, 1.0f);
    }

    mVerticalSpeed = mJumpPower;
    mIsGrounded = false;
}