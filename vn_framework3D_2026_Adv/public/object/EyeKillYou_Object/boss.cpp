#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "boss.h"

Boss* Boss::sInstance = nullptr;

Boss::Boss(const WCHAR* folder, const WCHAR* file) : Character(folder,file)
{
    setPosition(0.0f,8.0f,20.0f);

    useQuaternion(true);
    mDefaultHeight = 8.f;
    mRadius = 7.5f;

    mCollider = new SphereCollider(this,mRadius);
    mCollider->SetMass(10.0f);
    sInstance = this;
    mState = nullptr;
    mStage = nullptr;
}

void Boss::init()
{
    mHP = 1000;
    mMaxHP = mHP;
    ChangeState(new BossStateIntro());
}

void Boss::update(float deltaTime)
{
    if (vnScene::IsHitStop())
        return;
    if (!mStage) return;

    UpdateInvincible(deltaTime);

    UpdateDamageFlash(deltaTime);
    //vnFont::print(100.0f, 140.0f, L"%f,%f,%f,%f",
    //    XMVectorGetX(*getPosition()), XMVectorGetY(*getPosition()), XMVectorGetZ(*getPosition()), XMVectorGetW(*getPosition()));

    //エフェクト更新
    const bool slow = SlowSystem::Get().IsJustAvoidSlow();
    mEffectManager.SetAllOpacity(slow ? 0.1f : 1.0f);
    mEffectManager.Update(deltaTime);

    UpdateJustAvoid(deltaTime);

    if (IsFreezeGameplay())
    {
        //フィールド制限
        XMVECTOR pos = *getPosition();
        mStage->ClampPosition(pos);
        setPosition(&pos);
        return;
    }

    //侵入チェック
    if (!mStage->IsActivated())
    {
        XMVECTOR playerPos = *Player::GetInstance()->getPosition();
        mStage->CheckPlayerInside(playerPos);

        if (!mStage->IsActivated())
            return; // 侵入前は何もしない
    }
    //vnDebugDraw::Sphere(*this->getPosition(), 7.5f, 0xffffff00); // 黄色
    //状態更新
    if (mState)
        mState->Update(this, deltaTime);

    //フィールド制限
    XMVECTOR pos = *getPosition();
    mStage->ClampPosition(pos);
    setPosition(&pos);
}

void Boss::TakeDamage(const AttackInfo& info)
{
    // ノックバック開始
    if (IsInvincible()) return;

    mIsKnockback = true;

    mHP -= info.damage;
    StartDamageFlash(0.25f);
    StartInvincible(1.0f,false);
    if (auto* snd = GetSound())
    {
        snd->PlaySfx(SoundID::Player_AttackHit, 1.0f);
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

void Boss::OnDeath()
{
    AddEffect(std::make_unique<ExplosionEffect>(this, 3.0f));
    vnCamera::StartShake(1.2f, 0.8f);
    if (mSound)
    {
        mSound->PlaySfx(SoundID::Character_Dead, 1.0f);
    }
    if (GetCollider()) GetCollider()->SetEnabled(false);
}

void Boss::StartDamageFlash(float durationSec)
{
    // すでに点滅中でも「残り時間を上書きして延長」したいので代入
    mDamageFlashTimer = durationSec;

    // トグル管理をリセット
    mDamageFlashAcc = 0.0f;
    mDamageFlashOn = true; // 最初の1フレームは赤くして「当たった感」を出す

    // ここで即反映（1フレーム遅れを防ぐ）
    ApplyDamageFlashColor(true);
}

void Boss::UpdateDamageFlash(float dt)
{
    if (mDamageFlashTimer <= 0.0f)
        return;

    mDamageFlashTimer -= dt;

    // まだ点滅中なら、一定間隔でON/OFFを切り替える
    mDamageFlashAcc += dt;
    if (mDamageFlashAcc >= mDamageFlashInterval)
    {
        // interval を超えたぶんを引いて、dtが大きいフレームでも破綻しにくくする
        mDamageFlashAcc -= mDamageFlashInterval;

        mDamageFlashOn = !mDamageFlashOn;
        ApplyDamageFlashColor(mDamageFlashOn);
    }

    // 終了処理：最後は必ず通常色に戻す
    if (mDamageFlashTimer <= 0.0f)
    {
        mDamageFlashTimer = 0.0f;
        mDamageFlashOn = false;
        ApplyDamageFlashColor(false);
    }
}

void Boss::ApplyDamageFlashColor(bool on)
{
    if (on)
    {
        const float r = 1.0f;
        const float g = 0.2f;
        const float b = 0.2f;

        this->setAmbientR(r);
        this->setAmbientG(g);
        this->setAmbientB(b);
    }
    else
    {
        const float r = mBaseAmbientR;
        const float g = mBaseAmbientG;
        const float b = mBaseAmbientB;

        this->setAmbientR(r);
        this->setAmbientG(g);
        this->setAmbientB(b);
    }

}

void Boss::LookAt(const XMVECTOR& target)
{
    XMVECTOR pos = *getPosition();

    XMVECTOR forward = XMVectorSubtract(target, pos);
    forward = XMVectorSetY(forward, 0.0f);

    if (XMVector3LengthSq(forward).m128_f32[0] < 0.0001f)
        return;

    forward = XMVector3Normalize(forward);

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    //LH座標系なので反転
    XMVECTOR right = XMVector3Cross(forward, up);
    right = XMVector3Normalize(right);

    up = XMVector3Cross(right, forward);

    XMMATRIX look =
    {
        right,
        up,
        forward,
        XMVectorSet(0,0,0,1)
    };

    XMVECTOR q = XMQuaternionRotationMatrix(look);
    Quaternion = XMQuaternionNormalize(q);
}

void Boss::LookAtSmooth(const XMVECTOR& target, float speed)
{
    XMVECTOR pos = Position;

    XMVECTOR forward = XMVectorSubtract(target, pos);
    //forward = XMVectorSetY(forward, 0.0f);

    if (XMVector3LengthSq(forward).m128_f32[0] < 0.0001f)
        return;

    forward = XMVector3Normalize(forward);

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    //ビュー行列を作る
    XMMATRIX view = XMMatrixLookToLH(pos, forward, up);

    //逆行列にする※重要
    XMMATRIX world = XMMatrixInverse(nullptr, view);

    XMVECTOR targetQ = XMQuaternionRotationMatrix(world);

    Quaternion = XMQuaternionSlerp(Quaternion, targetQ, speed);
    Quaternion = XMQuaternionNormalize(Quaternion);
}

void Boss::LookAtSmoothFlat(const XMVECTOR& target, float speed)
{
    XMVECTOR pos = *getPosition();

    XMVECTOR flatTarget = target;
    flatTarget = XMVectorSetY(flatTarget, XMVectorGetY(pos));

    XMVECTOR forward = XMVectorSubtract(flatTarget, pos);
    forward = XMVectorSetY(forward, 0.0f);

    if (XMVectorGetX(XMVector3LengthSq(forward)) < 0.0001f)
        return;

    forward = XMVector3Normalize(forward);

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMMATRIX view = XMMatrixLookToLH(pos, forward, up);
    XMMATRIX world = XMMatrixInverse(nullptr, view);

    XMVECTOR targetQ = XMQuaternionRotationMatrix(world);

    Quaternion = XMQuaternionSlerp(Quaternion, targetQ, speed);
    Quaternion = XMQuaternionNormalize(Quaternion);
}

void Boss::ChangeState(BossStateBase* newState)
{
    if (mState)
        mState->Exit(this);

    mState = newState;

    if (mState)
        mState->Enter(this);
}


void Boss::BeginJustAvoidWindow(AttackKind k, float sec)
{
    mJustAttackKind = k;
    mJustTimer = sec;
}

void Boss::UpdateJustAvoid(float dt)
{
    if (mJustTimer > 0.0f) mJustTimer -= dt;
    if (mJustTimer <= 0.0f)
    {
        mJustTimer = 0.0f;
        mJustAttackKind = AttackKind::None;
    }
}

JustAvoidKind Boss::GetJustAvoidKind() const
{
    if (mJustTimer <= 0.0f) return JustAvoidKind::None;
    switch (mJustAttackKind)
    {
    case AttackKind::Roll:  return JustAvoidKind::Roll;
    case AttackKind::Press: return JustAvoidKind::Press;
    default:                return JustAvoidKind::None;
    }
}

Boss* Boss::GetInstance()
{
    return sInstance;
}

void Boss::AlignToGround(const XMVECTOR& groundNormal)
{
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    float dot = XMVectorGetX(XMVector3Dot(up, groundNormal));

    dot = std::clamp(dot, -1.0f, 1.0f);

    if (fabs(dot - 1.0f) < 0.001f)
        return; // ほぼ平地

    XMVECTOR axis = XMVector3Normalize(
        XMVector3Cross(up, groundNormal));

    float angle = acosf(dot);

    XMVECTOR q = XMQuaternionRotationAxis(axis, angle);

    setQuaternion(&q);
}

