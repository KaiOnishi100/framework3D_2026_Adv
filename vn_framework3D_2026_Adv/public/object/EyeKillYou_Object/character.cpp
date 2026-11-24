#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "character.h"


Character::Character(const WCHAR* folder, const WCHAR* file) : Object(folder, file)
{
    SetCharacter(this);
    setCastShadow(true);
    mVerticalSpeed = 0.0f;
    mGravity = -0.001f;    // 下向き
    mBounce = 0.5f;
    mIsKnockback = false;
    mKnockbackDuration = 2.0f;
    mKnockbackFriction = 0.9f;
    mIsDead = false;
}

void Character::render()
{
    __super::render();
    renderEffects();
}

void Character::renderEffects()
{
    // Character が保持する EffectManager に描画を委譲
    mEffectManager.Render();
}

Effect* Character::AddEffect(std::unique_ptr<Effect> effect)
{
    Effect* raw = effect.get();
    mEffectManager.AddEffect(std::move(effect));
    return raw;
}

void Character::StartInvincible(float time)
{
    mInvincibleTime = time;
    mTakeDamageTime = time / 2;
    AddEffect(std::make_unique<InvincibleAuraEffect>(this, time));
}

void Character::StartInvincible(float time, bool spawnEffect)
{
    mInvincibleTime = time;
    mTakeDamageTime = time * 0.5f;

    if (spawnEffect)
    {
        //AddEffect(std::make_unique<InvincibleAuraEffect>(this, time));
    }
}

void Character::EndInvincible()
{
    mInvincibleTime = 0.0f;
    mTakeDamageTime = 0.0f;
    setDiffuse(1, 1, 1, 1, 0);
}


void Character::UpdateInvincible(float dt)
{
    if (mInvincibleTime > 0.0f)
    {
        mInvincibleTime -= dt;
        mTakeDamageTime -= dt;
        setDiffuse(1.0f, 1.0f, 1.0f, 0.2f, 0);
    }
    else {
        mInvincibleTime = 0.0f;
        mTakeDamageTime = 0.0f;
        setDiffuse(1.0f, 1.0f, 1.0f, 1.0f, 0);
    }

}

void Character::executeGravity(float dt)
{

     // 重力加算
    if (mIsKnockback)
        mVerticalSpeed += mGravity * 1.8f;
    else
        mVerticalSpeed += mGravity;

    // 落下上限
    if (mVerticalSpeed < -0.5f)
        mVerticalSpeed = -0.5f;

    addPosition(0.0f, mVerticalSpeed, 0.0f);
}

void Character::CheckFallOut()
{
    const float killY = -20.0f;   // この高さより下に行ったら復帰

    if (getPositionY() < killY)
    {
        setPosition(0.0f, 10.0f, 0.0f);

        // 落下速度リセット
        mVerticalSpeed = 0.0f;

        // 地面扱い
        mIsGrounded = true;
    }
}

void Character::ClampToGround(const MeshCollider* ground)
{
    if (!ground) return;

    //XMVECTOR pos = *getPosition();

    //// 足元から少し上からRayを落とす
    //XMVECTOR rayStart = pos + XMVectorSet(0, 2.0f, 0, 0);
    //XMVECTOR rayDir = XMVectorSet(0, -1, 0, 0);

    //float hitDist;

    //if (ground->RayCast(rayStart, rayDir, hitDist,60.0f))
    //{
    //    float groundY = XMVectorGetY(rayStart) - hitDist;

    //    if (pos.m128_f32[1] < groundY + mRadius)
    //    {
    //        pos.m128_f32[1] = groundY + mRadius;
    //        setPosition(&pos);

    //        mVerticalSpeed = 0.0f;
    //        mIsGrounded = true;
    //    }
    //}
}

void Character::UpdateKnockback(float dt)
{
    if (!mIsKnockback) return;

    mKnockbackTimer -= dt;

    // 横移動
    XMVECTOR delta = XMVectorScale(mVelocity, dt);
    addPosition(&delta);

    // 減衰
    float decay = 4.0f * dt;
    float scale = max(0.0f, 1.0f - decay);
    mVelocity = XMVectorScale(mVelocity, scale);

    // タイマー終了で解除
    if (mKnockbackTimer <= 0.0f)
    {
        mIsKnockback = false;
        GetCollider()->SetType(ColliderType::Body);
        mVelocity = XMVectorZero();
    }
}

void Character::ApplyKnockback(XMVECTOR dir, float power)
{
    mKnockbackTimer = mKnockbackDuration;

    dir = XMVector3Normalize(dir);

    float horizontalPower = power * 15.0f;
    float verticalPower = power * 0.2f;

    mVerticalSpeed = 0.0f;   // ジャンプの勢いを消す

    mVelocity = XMVectorScale(dir, horizontalPower);
    mVerticalSpeed = verticalPower;

    mIsGrounded = false;

    GetCollider()->SetType(ColliderType::Knockback);
}

void Character::AttackKnockback(XMVECTOR dir, float power)
{
    mKnockbackTimer = mKnockbackDuration;
    this->AddEffect(
        std::make_unique<HitImpactEffect>(
            this,
            0.4f,
            XMVectorSet(1, 0.7f, 0.2f, 1),  // 白
            XMVectorSet(1, 0.8f, 0.2f, 1)   // 黄
        )
    );

    dir = XMVector3Normalize(dir);

    float horizontalPower = power * 15.0f;
    float verticalPower = power * 0.1f;

    mVerticalSpeed = 0.0f;   // ジャンプの勢いを消す

    mVelocity = XMVectorScale(dir, horizontalPower);
    mVerticalSpeed = verticalPower;

    mIsGrounded = false;
    mIsKnockback = true;
    GetCollider()->SetType(ColliderType::Knockback);
}

