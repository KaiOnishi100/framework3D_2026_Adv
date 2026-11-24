#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "bossStateBase.h"

void BossStateIntro::Enter(Boss* boss)
{
    //====================================================
    // 登場演出中だけロックオン（カメラの注視点がボスになる）
    //====================================================
    vnCamera::StartLockOn(*boss->getPosition());

    vnCamera::setFovY(XMConvertToRadians(70.0f));
    // 上空に配置
    XMVECTOR pos = *boss->getPosition();
    pos = XMVectorSetY(pos, 45.0f);
    boss->setPosition(&pos);

    boss->SetVerticalSpeed(0.0f);     // 落下開始
    // 落下中は攻撃判定しない
    if (boss->GetCollider()) boss->GetCollider()->SetType(ColliderType::Body);

    mWait = 0.0f;
    mLanded = false;
}

void BossStateIntro::Update(Boss* boss, float dt)
{
    // 落下処理（地面に当たったらIsGrounded = true）
    boss->executeGravity(dt);

    // プレイヤー方向をゆっくり向く
    boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.03f);

    vnCamera::StartLockOn(*boss->getPosition());

    if (boss->IsGrounded())
    {
        if (!mLanded)
        {
            mLanded = true;

            // 着地エフェクト
            boss->AddEffect(std::make_unique<PressImpactEffect>(boss, 1.6f));
            vnCamera::StartShake(1.0f, 0.6f);

            // 着地SE
            if (boss->GetSound())
                boss->GetSound()->PlaySfx(SoundID::Boss_PressLand, 1.0f);

            // ここから少し止める
            mWait = 1.0f;
        }
        else
        {
            mWait -= dt;
            if (mWait <= 0.0f)
            {
                // ------------------------------------------
                // Intro完了を通知
                // ------------------------------------------
                boss->SetIntroFinished(true);
                if (boss->IsTutorialEnabled())
                {
                    // チュートリアル：最初は確定ローリング
                    boss->SetGuaranteedRoll(true);
                    boss->ChangeState(new BossStateRollAttacktoPlayer());
                }
                else
                {
                    // 通常：待機→ランダム選択へ
                    boss->SetGuaranteedRoll(true);
                    boss->ChangeState(new BossStateIdle());
                }
            }
        }
    }
}

void BossStateIntro::Exit(Boss* boss)
{
    vnCamera::EndLockOn();

    vnCamera::setFovY(XMConvertToRadians(45.0f));
}

void BossStateIdle::Enter(Boss* boss)
{
    boss->SetBaseHeight(boss->getPositionY());
    mIdleTime = 2.0f + (rand() % 200) / 100.0f;  // 2～4秒待機
}

void BossStateIdle::Update(Boss* boss, float dt)
{
    mIdleTime -= dt;

    // ふよふよ上下させる
    static float floatTimer = 0.0f;
    floatTimer += dt;

    float base = boss->GetBaseHeight();
    float offsetY = sinf(floatTimer * 2.0f) * 0.5f;

    float newY = base + offsetY;

    // 今の位置を取得
    XMVECTOR pos = *boss->getPosition();
    pos = XMVectorSetY(pos, newY);
    boss->setPosition(&pos);

    // プレイヤーの方向を向く
    boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.05f);

    // 待機終了 → 攻撃決定
    if (mIdleTime <= 0.0f)
    {
        boss->ChangeState(new BossStateSelect());
    }
}

void BossStateSelect::Update(Boss* boss, float dt)
{
    XMVECTOR playerPos = *Player::GetInstance()->getPosition();
    XMVECTOR bossPos = *boss->getPosition();

    float dist = XMVectorGetX(XMVector3Length(playerPos - bossPos));
    int candidates[6];
    int count = 0;

    if (dist < 10.0f)
    {
        // 近距離：近接多め
        candidates[count++] = 0; // RollAttack
        candidates[count++] = 1; // Press
        candidates[count++] = 2; // RollToPlayer
        candidates[count++] = 4; // Breath
        candidates[count++] = 5; // Shockwave
    }
    else if (dist < 30.0f)
    {
        // 中距離：全部あり
        candidates[count++] = 0;
        candidates[count++] = 1;
        candidates[count++] = 2;
        candidates[count++] = 3; // Laser
        candidates[count++] = 4; // Breath
        candidates[count++] = 5; // Shockwave
    }
    else
    {
        // 遠距離：Shockwaveは少し減らす
        candidates[count++] = 3; // Laser
        candidates[count++] = 4; // Breath
        candidates[count++] = 2; // RollToPlayer   
    }
    int r = 4;//candidates[rand() % count];

    switch (r)
    {
    case 0: boss->ChangeState(new BossStateRollAttack());         break;
    case 1: boss->ChangeState(new BossStatePressAttack());        break;
    case 2: boss->ChangeState(new BossStateRollAttacktoPlayer()); break;
    case 3: boss->ChangeState(new BossStateRollLaser());          break;
    case 4: boss->ChangeState(new BossStateBreath());             break;
    case 5: boss->ChangeState(new BossStateShockwave());          break;
    }
}

void BossStateCooldown::Enter(Boss* boss)
{
    mTimer = 3.0f;
    boss->setPositionY(boss->GetDefaultHeight());
}

void BossStateCooldown::Update(Boss* boss, float dt)
{
    mTimer -= dt;
    boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.05f);

    if (mTimer <= 0.0f)
    {
        boss->ChangeState(new BossStateIdle());
    }
}

void BossStateRollAttack::Enter(Boss* boss)
{
    mPhase = RollPhase::Landing;
    mTimer = 0.0f;
    mRollSpeed = 30.0f;
    mSpeed = 70.0f;
    mBounceCount = 0;
    mMaxBoounceCount = 10;
    mHasHitPlayer = false;
    mDustOn = false;
    mJustWindowStarted = false;

    mAttackInfo.damage = 30;
    mAttackInfo.knockbackPower = 5.0f;
    if (boss->GetSound())
    {
        boss->GetSound()->StopLoop(SoundID::Boss_RollLoop);
    }
    if (boss) boss->SetHasHit(false);
}

void BossStateRollAttack::Update(Boss* boss, float dt)
{
    auto* snd = boss->GetSound();//ローカルへ移動
    switch (mPhase)
    {
    case RollPhase::Landing:
        //ボスに重力を適用
        boss->executeGravity(dt);

        if (boss->IsGrounded())
        {
            if (snd)
            {
                snd->PlaySfx(SoundID::Boss_RollCharge, 0.7f);
            }
            mPhase = RollPhase::ChargeSpin;
            mTimer = 3.0f;
            boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.05f);
            // プレイヤー方向記録
            XMVECTOR bossPos = *boss->getPosition();
            XMVECTOR playerPos = *Player::GetInstance()->getPosition();

            XMVECTOR dir = playerPos - bossPos;
            dir = XMVectorSetY(dir, 0);
            mMoveDir = XMVector3Normalize(dir);
        }
        break;
    case RollPhase::ChargeSpin:
        mTimer -= dt;

        // Y軸で高速回転
        XMVECTOR q = XMQuaternionRotationAxis(
            XMVectorSet(1, 0, 0, 0),
            dt * mRollSpeed);

        boss->addRotationQ(&q);
        vnCamera::StartShake(0.01f, 0.01f);

        if (!mDustOn)
        {
            boss->AddEffect(
                std::make_unique<RollingDustEffect>(
                    boss,
                    mTimer,   // Charge時間
                    RollingDustEffect::Mode::Spin
                )
            );
            mDustOn = true;
        }

        if (mTimer <= 0.0f)
        {
            if (snd)
            {
                snd->StartLoop(SoundID::Boss_RollLoop, 0.6f);
            }
            mPhase = RollPhase::Rolling;
            mDustOn = false;
        }
        break;
    case RollPhase::Rolling:
    {
        if (!mDustOn)
        {
            mFx = boss->AddEffect(std::make_unique<RollingDustEffect>
                (boss, 4.0f, RollingDustEffect::Mode::Rolling)); // 長め
            mDustOn = true;
        }

        //コライダーをAttackTypeに切り替える
        boss->GetCollider()->SetType(ColliderType::Attack);
        boss->SetAttackKind(Boss::AttackKind::Roll);

        if (!mJustWindowStarted)
        {
            boss->BeginJustAvoidWindow(Boss::AttackKind::Roll, 0.25f);
            mJustWindowStarted = true;
        }

        XMVECTOR bossPos = *boss->getPosition();
        XMVECTOR playerPos = *Player::GetInstance()->getPosition();

        XMVECTOR targetDir = playerPos - bossPos;
        targetDir = XMVectorSetY(targetDir, 0);
        targetDir = XMVector3Normalize(targetDir);

        mMoveDir = XMVector3Normalize(
            XMVectorLerp(mMoveDir, targetDir, dt * 0.5f));

        mAttackInfo.knockbackDirection = mMoveDir;

        boss->SetCurrentAttackInfo(mAttackInfo);

        //移動
        boss->addPosition(
            XMVectorGetX(mMoveDir) * mSpeed * dt,
            0,
            XMVectorGetZ(mMoveDir) * mSpeed * dt);

        // 回転
        XMVECTOR q = XMQuaternionRotationAxis(
            XMVectorSet(1, 0, 0, 0),
            dt * mRollSpeed);

        boss->addRotationQ(&q);

        XMVECTOR pos = *boss->getPosition();

        bool hitWall = boss->GetStage()->ClampPosition(pos);
        boss->setPosition(&pos);

        if (hitWall)
        {
            boss->AddEffect(
                std::make_unique<HitImpactEffect>(
                    boss,
                    2.5f, // デカい
                    XMVectorSet(1, 0.7f, 0.2f, 1),   // オレンジ
                    XMVectorSet(1, 0.3f, 0.1f, 1)
                )
            );
            if (snd)
            {
                snd->PlaySfx(SoundID::Boss_HitWall, 5.0f);
            }
            //スピードを上げていく
            mSpeed *= 1.15f;
            mDustOn = false;
            mBounceCount++;

            vnCamera::StartShake(0.5f, 0.4f);

            XMVECTOR center = boss->GetStage()->GetCenter();

            // 壁の法線
            XMVECTOR normal = XMVector3Normalize(pos - center);
            normal = XMVectorSetY(normal, 0);

            // 物理反射
            mMoveDir = XMVector3Reflect(mMoveDir, normal);
            mMoveDir = XMVector3Normalize(mMoveDir);

            // ±50度くらい
            float randomAngle = (float)XMConvertToRadians(
                (rand() % 100 - 50) // -50 ～ +50
            );

            // Y軸回転クォータニオン
            XMVECTOR rotQ = XMQuaternionRotationAxis(
                XMVectorSet(0, 1, 0, 0),
                randomAngle
            );

            // ベクトルを回転
            mMoveDir = XMVector3Rotate(mMoveDir, rotQ);
            mMoveDir = XMVector3Normalize(mMoveDir);

            //向きを更新
            boss->LookAtSmooth(mMoveDir, 1.0f);

            mHasHitPlayer = false;
            if (boss) boss->SetHasHit(false);
            if (mBounceCount >= mMaxBoounceCount)
            {
                if (snd) snd->StopLoop(SoundID::Boss_RollLoop);
                if (mFx) mFx->Stop();
                mFx = nullptr;
                vnCamera::StartShake(1.0f, 1.0f);
                boss->SetAttackKind(Boss::AttackKind::None);
                boss->GetCollider()->SetType(ColliderType::Body);
                boss->ChangeState(new BossStateCooldown());
                return;
            }
        }
    }
    break;
    case RollPhase::End:
        break;
    }

}

void BossStateRollAttack::Exit(Boss* boss)
{
    if (mFx) mFx->Stop();
    mFx = nullptr;
    boss->SetAttackKind(Boss::AttackKind::None);
    if (boss->GetCollider()) boss->GetCollider()->SetType(ColliderType::Body);
    if (boss->GetSound())
    {
        boss->GetSound()->StopLoop(SoundID::Boss_RollLoop);
    }
    boss->SetHasHit(false);
}

void BossStateRollAttacktoPlayer::Enter(Boss* boss)
{
    mPhase = RollPhase::Landing;
    mTimer = 0.0f;
    mRollSpeed = 20.0f;
    mSpeed = 70.0f;
    mBounceCount = 0;
    mMaxBoounceCount = 3;
    mHasHitPlayer = false;
    mDustOn = false;
    mJustWindowStarted = false;

    mAttackInfo.damage = 30;
    mAttackInfo.knockbackPower = 5.0f;
    boss->SetGuaranteedRoll(true);
    if (boss->GetSound())
    {
        boss->GetSound()->StopLoop(SoundID::Boss_RollLoop);
    }
    if (boss) boss->SetHasHit(false);
}

void BossStateRollAttacktoPlayer::Update(Boss* boss, float dt)
{
    auto* snd = boss->GetSound();
    switch (mPhase)
    {
    case RollPhase::Landing:
        //ボスに重力を適用
        boss->executeGravity(dt);

        if (boss->IsGrounded())
        {
            if (snd)
            {
                snd->PlaySfx(SoundID::Boss_RollCharge, 0.7f);
            }
            mPhase = RollPhase::ChargeSpin;
            mTimer = 3.0f;
            boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.05f);
            // プレイヤー方向記録
            XMVECTOR bossPos = *boss->getPosition();
            XMVECTOR playerPos = *Player::GetInstance()->getPosition();

            XMVECTOR dir = playerPos - bossPos;
            dir = XMVectorSetY(dir, 0);
            mMoveDir = XMVector3Normalize(dir);
        }
        break;
    case RollPhase::ChargeSpin:

        mTimer -= dt;

        // Y軸で高速回転（Quaternion）
        XMVECTOR q = XMQuaternionRotationAxis(
            XMVectorSet(1, 0, 0, 0),
            dt * mRollSpeed);

        boss->addRotationQ(&q);
        vnCamera::StartShake(0.01f, 0.01f);

        if (!mDustOn)
        {
           boss->AddEffect(
                std::make_unique<RollingDustEffect>(
                    boss,
                    mTimer,   // Charge時間
                    RollingDustEffect::Mode::Spin
                )
            );
            mDustOn = true;
        }

        if (mTimer <= 0.0f)
        {
            if (snd)
            {
                snd->StartLoop(SoundID::Boss_RollLoop, 0.6f);
            }
            mPhase = RollPhase::Rolling;
            mDustOn = false;
        }
        break;
    case RollPhase::Rolling:
    {
        if (!mDustOn)
        {
            mFx = boss->AddEffect(std::make_unique<RollingDustEffect>
                (boss, 4.0f, RollingDustEffect::Mode::Rolling)); // 長め
            mDustOn = true;
        }

        //コライダーをAttackTypeに切り替える
        boss->GetCollider()->SetType(ColliderType::Attack);
        boss->SetAttackKind(Boss::AttackKind::Roll);
        if (!mJustWindowStarted)
        {
            boss->BeginJustAvoidWindow(Boss::AttackKind::Roll, 0.25f);
            mJustWindowStarted = true;
        }

        XMVECTOR bossPos = *boss->getPosition();
        XMVECTOR playerPos = *Player::GetInstance()->getPosition();

        XMVECTOR targetDir = playerPos - bossPos;
        targetDir = XMVectorSetY(targetDir, 0);
        targetDir = XMVector3Normalize(targetDir);

        mMoveDir = XMVector3Normalize(
            XMVectorLerp(mMoveDir, targetDir, dt * 0.5f));

        mAttackInfo.knockbackDirection = mMoveDir;

        boss->SetCurrentAttackInfo(mAttackInfo);

        boss->addPosition(
            XMVectorGetX(mMoveDir) * mSpeed * dt,
            0,
            XMVectorGetZ(mMoveDir) * mSpeed * dt);

        // 回転
        XMVECTOR q = XMQuaternionRotationAxis(
            XMVectorSet(1, 0, 0, 0),
            dt * mRollSpeed);

        boss->addRotationQ(&q);

        XMVECTOR pos = *boss->getPosition();

        bool hitWall = boss->GetStage()->ClampPosition(pos);
        boss->setPosition(&pos);

        if (hitWall)
        {
            boss->AddEffect(
                std::make_unique<HitImpactEffect>(
                    boss,
                    2.5f, // デカい
                    XMVectorSet(1, 0.7f, 0.2f, 1),   // オレンジ
                    XMVectorSet(1, 0.3f, 0.1f, 1)
                )
            );

            if (snd)
            {
                snd->PlaySfx(SoundID::Boss_HitWall, 5.0f);
            }

            mDustOn = false;
            mBounceCount++;
            //カメラシェイク
            vnCamera::StartShake(0.5f, 0.4f);
            // 反射ベクトル計算
            XMVECTOR center = boss->GetStage()->GetCenter();
            XMVECTOR normal = XMVector3Normalize(pos - center);
            normal = XMVectorSetY(normal, 0);//念のためY成分を削除

            //プレイヤーに向けて再度方向調整
            XMVECTOR dir = playerPos - bossPos;
            dir = XMVectorSetY(dir, 0);
            mMoveDir = XMVector3Normalize(dir);

            boss->SetHasHit(false);
            mHasHitPlayer = false;

            if (mBounceCount >= mMaxBoounceCount)
            {
                boss->SetAttackKind(Boss::AttackKind::None);
                boss->GetCollider()->SetType(ColliderType::Body);
                if (snd) snd->StopLoop(SoundID::Boss_RollLoop);
                boss->ChangeState(new BossStateCooldown());
                return;
            }
            else
            {
                //壁にぶつかったあとプレイヤーのむきにあわせて回転する
                boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 1.0f);
            }
        }
    }
        break;
    case RollPhase::End:
        break;
    }
   
}

void BossStateRollAttacktoPlayer::Exit(Boss* boss)
{
    boss->SetAttackKind(Boss::AttackKind::None);
    if (mFx) mFx->Stop();
    mFx = nullptr;
    if (boss->GetCollider()) boss->GetCollider()->SetType(ColliderType::Body);
    boss->SetGuaranteedRoll(false);
    if (boss->GetSound())
    {
        boss->GetSound()->StopLoop(SoundID::Boss_RollLoop);
    }
    boss->SetHasHit(false);
}

void BossStatePressAttack::Enter(Boss* boss)
{
    mPhase = PressPhase::JumpUp;
    mPressCount = 0;
    mAttackRadius = boss->GetRadius() * 1.5f;
    boss->SetVerticalSpeed(0.3f);  // 上向き初速
    mJustWindowStarted = false;

    mAttackInfo.damage = 25.0f;
    mAttackInfo.knockbackPower = 7.0f;
    mPressAttackInfo.damage = 30.0f;
    mPressAttackInfo.knockbackPower = 5.0f;
}

void BossStatePressAttack::Update(Boss* boss, float dt)
{
    switch (mPhase)
    {
    case PressPhase::JumpUp: 
        boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.07f);
        //頂点に達したら
        if (boss->GetVerticalSpeed() <= 0.0f)
        {
            mTimer = 0.2f;  // 0.2秒溜め
            mPhase = PressPhase::AirChase;
        }
        break;
    case PressPhase::AirChase:
    
        mTimer -= dt;
        if (mTimer <= 0.0f)
        {
            mTargetPos = *Player::GetInstance()->getPosition();
            vnCamera::StartShake(0.3f, 0.2f);
            mPhase = PressPhase::PressDown;
        }
        break;
    case PressPhase::PressDown:
        {
        boss->GetCollider()->SetType(ColliderType::Attack);
        boss->SetAttackKind(Boss::AttackKind::Press);
        if (!mJustWindowStarted)
        {
            boss->BeginJustAvoidWindow(Boss::AttackKind::Press, 0.20f);
            mJustWindowStarted = true;
        }
        XMVECTOR bossPos = *boss->getPosition();
        boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.07f);

        // プレイヤー方向
        XMVECTOR dir = mTargetPos - bossPos;
        dir = XMVector3Normalize(dir);

        float pressSpeed = 100.0f;
        boss->SetCurrentAttackInfo(mAttackInfo);

        //プレイヤーの方向に向かって突進
        boss->addPosition(
            XMVectorGetX(dir) * pressSpeed * dt,
            XMVectorGetY(dir) * pressSpeed * dt,
            XMVectorGetZ(dir) * pressSpeed * dt);

        if (boss->IsGrounded())
        {
            if (boss->GetSound())
            {
                boss->GetSound()->PlaySfx(SoundID::Boss_PressLand, 1.0f);
            }
            boss->GetCollider()->SetType(ColliderType::Body);
            boss->SetAttackKind(Boss::AttackKind::None);
            boss->AddEffect(std::make_unique<PressImpactEffect>(boss, 1.2f));
            mPhase = PressPhase::Landing;
            vnCamera::StartShake(0.8f, 0.5f);

            XMVECTOR bossPos = *boss->getPosition();
            XMVECTOR playerPos = *Player::GetInstance()->getPosition();

            XMVECTOR diff = playerPos - bossPos;
            float distSq = XMVectorGetX(XMVector3LengthSq(diff));

            float radius = mAttackRadius;
            float radiusSq = radius * radius;

            if (distSq <= radiusSq)
            {
                // 外向きノックバック
                XMVECTOR dir = XMVector3Normalize(diff);
                mAttackInfo.knockbackDirection = dir;
                Player::GetInstance()->TakeDamage(mAttackInfo);
            }

            mTimer = 1.0f;//待機時間
        }
        }
        
        break;
    case PressPhase::Landing:
        mTimer -= dt;

        if (mTimer <= 0.0f)
        {
            mPressCount++;

            if (mPressCount >= 2)
            {
                boss->ChangeState(new BossStateCooldown());
                return;
            }

            boss->SetVerticalSpeed(0.3f);
            mPhase = PressPhase::JumpUp;
        }
        break;
    }
}

void BossStateBreath::Enter(Boss* boss)
{
    mPhase = BreathPhase::LookAt;
    mTimer = 0.0f;
    mBreathRange = 60.0f;
    mBreathAngle = XMConvertToRadians(45.0f);
    mSmokeSpawnTimer = 0.0f;

    mChargeEffectOn = false;

    mAttackInfo.damage = 10.0f;
    mAttackInfo.knockbackPower = 5.0f;

    if (boss->GetSound())
    {
        boss->GetSound()->StopLoop(SoundID::Boss_BreathLoop);
    }
}

void BossStateBreath::Update(Boss* boss, float dt)
{
    auto* snd = boss->GetSound();
    switch (mPhase)
    {
    case BreathPhase::LookAt:

        boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.2f);

        mTimer += dt;

        if (mTimer >= 0.5f) // 少し向く時間
        {
            mTimer = 3.0f;  // 溜め時間
            mPhase = BreathPhase::Charge;
        }
        break;
    case BreathPhase::Charge:
        mTimer -= dt;
        // 1回だけ生成
        if (!mChargeEffectOn)
        {
            snd->PlaySfx(SoundID::Boss_BeamCharge);
            boss->AddEffect(std::make_unique<ChargeEffect>(boss));
            mChargeEffectOn = true;
        }

        if (mTimer <= 0.0f)
        {
            //エフェクト発生
            //boss->AddEffect(std::make_unique<ChargeCompleteEffect>(boss));
            mTimer = 5.0f; // ブレス時間
            mPhase = BreathPhase::Breathing;
            mChargeEffectOn = false;
        }
        break;
    case BreathPhase::Breathing:
        {
        mTimer -= dt;
        if (!mBreathEffectOn)
        {
            if (snd) snd->StartLoop(SoundID::Boss_BreathLoop, 0.9f);
            mFx = boss->AddEffect(std::make_unique<BreathEffect>(boss, BreathEffect::Type::FireYellow, mBreathRange, mBreathAngle, mTimer));
            mBreathEffectOn = true;
        }
        mSmokeSpawnTimer -= dt;
        if (mSmokeSpawnTimer <= 0.0f)
        {
            boss->AddEffect(std::make_unique<SmokeEffect>(boss, 1.0f));
            mSmokeSpawnTimer = 0.2f;
        }

        //vnDebugDraw::DrawBreathDebug(boss, mBreathRange, mBreathAngle);
        // プレイヤー判定
        XMVECTOR bossPos = *boss->getPosition();
        XMVECTOR playerPos = *Player::GetInstance()->getPosition();

        XMVECTOR toPlayer = playerPos - bossPos;
        float distance = XMVectorGetX(XMVector3Length(toPlayer));

        if (distance <= mBreathRange)
        {
            toPlayer = XMVector3Normalize(toPlayer);

            // ボスの前方向を取得
            XMVECTOR forward = boss->getForward();

            float dot = XMVectorGetX(XMVector3Dot(forward, toPlayer));
            dot = std::clamp(dot, -1.0f, 1.0f);
            float angle = acosf(dot);

            if (angle <= mBreathAngle)
            {
                // forward はY成分を潰しておくと地面に沿った方向になる
                XMVECTOR flatForward = boss->getForward();
                flatForward = XMVectorSetY(flatForward, 0.0f);

                // 念のため正規化
                if (XMVectorGetX(XMVector3LengthSq(flatForward)) > 0.0001f)
                {
                    flatForward = XMVector3Normalize(flatForward);
                }
                else
                {
                    flatForward = XMVectorSet(0, 0, 1, 0);
                }

                // ボスの右方向ベクトル
                XMVECTOR up = XMVectorSet(0, 1, 0, 0);
                XMVECTOR right = XMVector3Cross(up, flatForward);

                if (XMVectorGetX(XMVector3LengthSq(right)) > 0.0001f)
                {
                    right = XMVector3Normalize(right);
                }
                else
                {
                    right = XMVectorSet(1, 0, 0, 0);
                }

                // プレイヤーが右側・左側どちらにいるか判定
                float side = XMVectorGetX(XMVector3Dot(right, toPlayer));

                // プレイヤーがいる側へ飛ばす
                XMVECTOR knockDir = (side >= 0.0f) ? right : -right;

                knockDir = XMVector3Normalize(knockDir + flatForward * 0.15f);

                mAttackInfo.knockbackDirection = knockDir;
                mAttackInfo.knockbackPower = 6.5f; 
                Player::GetInstance()->TakeDamage(mAttackInfo);
            }
        }

        if (mTimer <= 0.0f)
        {
            if (snd) snd->StopLoop(SoundID::Boss_BreathLoop);
            if (mFx) mFx->Stop();
            mFx = nullptr;
            boss->ChangeState(new BossStateCooldown());
            return;
        }
    }
        
        break;
    case BreathPhase::End:
        break;
    }
}
void BossStateBreath::Exit(Boss* boss)
{
    if (mFx) { mFx->Stop(); mFx = nullptr; }
    if (boss->GetSound())
    {
        boss->GetSound()->StopLoop(SoundID::Boss_BreathLoop);
    }
}
//カプセルコライダー化予定
float BossStateRollLaser::DistancePointToSegment(XMVECTOR point, XMVECTOR start, XMVECTOR end)
{
    XMVECTOR segment = XMVectorSubtract(end, start);
    XMVECTOR toPoint = XMVectorSubtract(point, start);

    float segmentLengthSq = XMVectorGetX(XMVector3LengthSq(segment));

    if (segmentLengthSq <= 0.0001f)
    {
        return XMVectorGetX(XMVector3Length(toPoint));
    }


    float t = XMVectorGetX(XMVector3Dot(toPoint, segment)) / segmentLengthSq;

    // 線分内にクランプ
    t = max(0.0f, min(1.0f, t));

    XMVECTOR closest = XMVectorAdd(start, XMVectorScale(segment, t));

    return XMVectorGetX(XMVector3Length(
        XMVectorSubtract(point, closest)
    ));
}

void BossStateRollLaser::CheckLaserHit(Boss* boss)
{
    if (!mLaserFiring) return;

    XMVECTOR start = *boss->getPosition();
    XMVECTOR forward = boss->getForward();
    forward = XMVectorSetY(forward, 0.0f);

    if (XMVectorGetX(XMVector3LengthSq(forward)) > 0.0001f)
    {
        forward = XMVector3Normalize(forward);
    }
    else
    {
        forward = XMVectorSet(0, 0, 1, 0);
    }

    XMVECTOR end = XMVectorAdd(start, XMVectorScale(forward, mLaserLength));

    XMVECTOR playerPos = *Player::GetInstance()->getPosition();

    // 判定は地面上だけで見る
    start = XMVectorSetY(start, 0.0f);
    end = XMVectorSetY(end, 0.0f);
    playerPos = XMVectorSetY(playerPos, 0.0f);

    float distance = DistancePointToSegment(playerPos, start, end);

    const float laserRadius = 5.0f;

    if (distance <= laserRadius + Player::GetInstance()->GetRadius())
    {
        XMVECTOR segment = XMVectorSubtract(end, start);
        XMVECTOR toPlayer = XMVectorSubtract(playerPos, start);

        float t = XMVectorGetX(XMVector3Dot(toPlayer, segment)) /
            XMVectorGetX(XMVector3LengthSq(segment));

        t = max(0.0f, min(1.0f, t));

        XMVECTOR closestPoint = XMVectorAdd(start, XMVectorScale(segment, t));

        XMVECTOR knockDir = XMVectorSubtract(playerPos, closestPoint);
        knockDir = XMVectorSetY(knockDir, 0.0f);

        if (XMVectorGetX(XMVector3LengthSq(knockDir)) > 0.0001f)
        {
            knockDir = XMVector3Normalize(knockDir);
        }
        else
        {
            knockDir = boss->getRight();
        }

        AttackInfo info;
        info.damage = 20.0f;
        info.knockbackDirection = knockDir;//最近傍距離を衝撃の発生源とする
        info.knockbackPower = 0.8f;// ノックバック強さ

        Player::GetInstance()->TakeDamage(info);
    }
}

void BossStateRollLaser::DrawLaser(Boss* boss)
{
    if (!mLaserFiring) return;

    XMVECTOR start = *boss->getPosition();

    XMVECTOR forward = boss->getForward(); // Y回転から作る
    XMVECTOR end = start + forward * mLaserLength;

    vnDebugDraw::Line(&start, &end, 0xFFFF0000); // 赤

    XMVECTOR right = boss->getRight();

    for (int i = -3; i <= 3; i++)
    {
        float offset = i * 0.5f;

        XMVECTOR offsetPos = start + right * offset;
        XMVECTOR end = offsetPos + forward * mLaserLength;

        vnDebugDraw::Line(&offsetPos, &end, 0xFFFF0000);
    }
}

float BossStateRollLaser::GetYawFromBossForward(Boss* boss)
{
    if (!boss) return 0.0f;

    //====================================================
    // 現在の forward を取得して、地面上の向きにする
    //====================================================
    XMVECTOR forward = boss->getForward();
    forward = XMVectorSetY(forward, 0.0f);

    // 長さがほぼ0だと atan2f に渡す前に壊れるので保険
    if (XMVectorGetX(XMVector3LengthSq(forward)) < 0.0001f)
    {
        return 0.0f;
    }

    forward = XMVector3Normalize(forward);

    //====================================================
    // XZ平面上の forward から yaw を作る
    //====================================================
    float x = XMVectorGetX(forward);
    float z = XMVectorGetZ(forward);

    return atan2f(x, z);
}

void BossStateRollLaser::UpdateLaserFxSegment(Boss* boss)
{
    if (!boss) return;
    if (!mLaserFiring) return;
    if (!mLaserFx) return;

    //====================================================
     // ボスのワールド行列
     //====================================================
    XMMATRIX world = *boss->getWorld();

    //====================================================
    // ボスローカル空間での目玉位置
    //====================================================
    XMVECTOR localEye = XMVectorSet(0.0f, 0.0f, 2.0f, 1.0f);

    // ワールドへ変換
    XMVECTOR start = XMVector3TransformCoord(localEye, world);

    // 前方
    XMVECTOR forward = boss->getForward();
    forward = XMVector3Normalize(forward);

    // 終点
    XMVECTOR end = start + forward * mLaserLength;

    mLaserFx->SetSegment(start, end);
}

void BossStateRollLaser::Enter(Boss* boss)
{
   mRotatedNum = 4;
   mLaserTimer = 0.0f;
   mLaserAngle = 0.0f;
   mTotalRotated = 0.0f;
   mLaserLength = 150.0f;
   mLaserFiring = false;
   mChargeEffectOn = false;
   mPhase = LaserPhase::Charge;

   if (boss->GetSound())
   {
       boss->GetSound()->StopLoop(SoundID::Boss_BeamLoop); 
   }
}

void BossStateRollLaser::Update(Boss* boss, float dt)
{
    auto* snd = boss ? boss->GetSound() : nullptr;
    if (!boss) return;

    switch (mPhase)
    {
    case LaserPhase::Charge:
    {
        mLaserTimer += dt;

        if (!mLaserFiring)
        {
            // プレイヤー方向へゆっくり向く
            if (Player::GetInstance())
            {
                boss->LookAtSmoothFlat(*Player::GetInstance()->getPosition(), 0.05f);
            }

            // チャージ演出は1回だけ
            if (!mChargeEffectOn)
            {
                if (snd) snd->PlaySfx(SoundID::Boss_BeamCharge, 1.0f);
                boss->AddEffect(std::make_unique<ChargeEffect>(boss));
                mChargeEffectOn = true;
            }

            // 一定時間チャージしたら発射開始
            if (mLaserTimer >= 2.0f)
            {
                if (snd) snd->StartLoop(SoundID::Boss_BeamLoop, 0.9f);

                mLaserFiring = true;
                mLaserTimer = 0.0f;

                mLaserAngle = GetYawFromBossForward(boss);

                // 角度を固定してレーザーをしばらく真正面に出す
                boss->setRotationQY(mLaserAngle);

                mPhase = LaserPhase::FiringFixed;

                //================================================
                // レーザー見た目生成
                //================================================
                LaserQuad::Desc qd;
                swprintf_s(qd.Texture, L"%s", L"data/image/effect/lazer.png");
                qd.Thickness = 12.0f;
                qd.Color = XMVectorSet(0.65f, 0.90f, 1.0f, 1.0f);
                qd.UvScrollSpeed = 1.5f;
                qd.ZWrite = false;
                qd.Additive = false;

                auto fx = std::make_unique<LaserBeamEffect>(boss, 7.0f, qd);
                mLaserFx = fx.get();
                boss->AddEffect(std::move(fx));
                mLaserFxOn = true;

                // 生成直後に1回反映しておく
                UpdateLaserFxSegment(boss);
            }
        }
        break;
    }

    //----------------------------------------------------
    // FiringFixed
    // ・一瞬だけ現在方向に固定照射
    // ・その後 Rotating へ移る
    //----------------------------------------------------
    case LaserPhase::FiringFixed:
    {
        mLaserTimer += dt;

        boss->setRotationQY(mLaserAngle);

        // 当たり判定と見た目更新
        CheckLaserHit(boss);
        UpdateLaserFxSegment(boss);

        // 固定照射時間が終わったら回転開始
        if (mLaserTimer >= 0.5f)
        {
            mLaserTimer = 0.0f;
            mTotalRotated = 0.0f;

            mLaserAngle = GetYawFromBossForward(boss);

            mPhase = LaserPhase::Rotating;
        }
        break;
    }

    //----------------------------------------------------
    // Rotating
    // ・徐々に加速しながら回転
    // ・一定回転数で終了
    //----------------------------------------------------
    case LaserPhase::Rotating:
    {
        // 1周 * mRotatedNum に対する進捗
        float totalTarget = XMConvertToRadians(360.0f * mRotatedNum);

        float progress = 0.0f;
        if (totalTarget > 0.0001f)
        {
            progress = mTotalRotated / totalTarget;
            progress = min(progress, 1.0f);
        }

        //================================================
        // 回転速度
        // ・最初はゆっくり
        // ・だんだん加速
        //================================================
        float baseSpeed = XMConvertToRadians(360.0f * 0.5f);
        float maxAccel = 1.2f;

        float rotateSpeed = baseSpeed * (1.0f + maxAccel * progress);
        float delta = rotateSpeed * dt;

        mLaserAngle += delta;
        mTotalRotated += delta;

        boss->setRotationQY(mLaserAngle);

        // 当たり判定と見た目更新
        CheckLaserHit(boss);
        UpdateLaserFxSegment(boss);

        // 回転終了
        if (mTotalRotated >= totalTarget)
        {
            mLaserFiring = false;
            mLaserFx = nullptr;
            mLaserFxOn = false;

            if (snd) snd->StopLoop(SoundID::Boss_BeamLoop);

            boss->ChangeState(new BossStateCooldown());
            return;
        }
        break;
    }

    default:
        break;
    }
}

void BossStateRollLaser::Exit(Boss* boss)
{
    if (mLaserFx) mLaserFx->Stop();

    if (boss->GetSound())
    {
        boss->GetSound()->StopLoop(SoundID::Boss_BeamLoop);
    }
}

void BossStateShockwave::Enter(Boss* boss)
{
    //====================================================
    // 初期化
    //====================================================
    mPhase = ShockwavePhase::JumpUp;

    mWaveCount = 0;
    mMaxWaveCount = 2;

    mTimer = 0.5f;

    mWaveRadius = 0.0f;
    mWaveMaxRadius = 85.0f;
    mWaveSpeed = 55.0f;
    mWaveThickness = 4.0f;
    mWaveHitPlayer = false;

    //====================================================
    // 着地点は「ジャンプ開始時点」で固定
    //====================================================
    mTargetPos = *Player::GetInstance()->getPosition();
    mTargetPos = XMVectorSetY(mTargetPos, boss->GetDefaultHeight());

    //====================================================
    // ダメージ設定
    //====================================================
    mPressAttackInfo.damage = 25.0f;
    mPressAttackInfo.knockbackPower = 6.0f;

    mWaveAttackInfo.damage = 20.0f;
    mWaveAttackInfo.knockbackPower = 8.0f;

    //====================================================
    // 上昇を速くする
    //====================================================
    boss->SetVerticalSpeed(1.1f);

    boss->SetAttackKind(Boss::AttackKind::None);
    if (boss->GetCollider())
    {
        boss->GetCollider()->SetType(ColliderType::Body);
    }
}

void BossStateShockwave::Update(Boss* boss, float dt)
{
    switch (mPhase)
    {
    case ShockwavePhase::JumpUp:
    {
        //====================================================
        // 上昇中（時間制御）
        //====================================================
        boss->LookAtSmooth(mTargetPos, 0.08f);

        // 重力は適用してOK
        // boss->executeGravity(dt);

        mTimer -= dt;

        // 時間で強制終了
        if (mTimer <= 0.0f)
        {
            //====================================================
            // ここで強制的に落下に切り替える
            //====================================================
            boss->SetVerticalSpeed(0.0f); // 一旦止める

            mTimer = 0.1f; // ほんの少しだけ溜め
            mPhase = ShockwavePhase::Aim;
        }
        break;
    }

    case ShockwavePhase::Aim:
    {
        //====================================================
        // プレイヤー位置を記録
        // ・PressAttackと違って完全追尾しすぎないよう、
        //   このタイミングで着地点を固定する
        //====================================================
        boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.10f);

        mTimer -= dt;
        if (mTimer <= 0.0f)
        {
            mTargetPos = *Player::GetInstance()->getPosition();

            // 急降下開始
            mPhase = ShockwavePhase::SlamDown;

            // 当たり判定ON
            if (boss->GetCollider())
            {
                boss->GetCollider()->SetType(ColliderType::Attack);
            }
            boss->SetAttackKind(Boss::AttackKind::Press);

            // カメラを少し揺らして予兆感
            vnCamera::StartShake(0.25f, 0.10f);
        }
        break;
    }

    case ShockwavePhase::SlamDown:
    {
        XMVECTOR bossPos = *boss->getPosition();

        //====================================================
        // XZだけターゲットへ向かう
        //====================================================
        XMVECTOR toTarget = mTargetPos - bossPos;
        toTarget = XMVectorSetY(toTarget, 0.0f);

        XMVECTOR moveXZ = XMVectorZero();
        if (XMVectorGetX(XMVector3LengthSq(toTarget)) > 0.0001f)
        {
            moveXZ = XMVector3Normalize(toTarget);
        }

        //====================================================
        // 落下速度
        //====================================================
        const float slamMoveSpeedXZ = 28.0f;   // 横移動
        const float slamDownSpeedY = 85.0f;   // 縦落下

        // ノックバック方向は見た目用にざっくり下向き混ぜる
        XMVECTOR attackDir = XMVectorSet(
            XMVectorGetX(moveXZ),
            -0.8f,
            XMVectorGetZ(moveXZ),
            0.0f
        );
        attackDir = XMVector3Normalize(attackDir);

        mPressAttackInfo.knockbackDirection = attackDir;
        boss->SetCurrentAttackInfo(mPressAttackInfo);

        boss->addPosition(
            XMVectorGetX(moveXZ) * slamMoveSpeedXZ * dt,
            -slamDownSpeedY * dt,
            XMVectorGetZ(moveXZ) * slamMoveSpeedXZ * dt);

        boss->LookAtSmooth(mTargetPos, 0.18f);

        if (boss->IsGrounded())
        {
            if (boss->GetSound())
            {
                boss->GetSound()->PlaySfx(SoundID::Boss_PressLand, 1.0f);
            }

            if (boss->GetCollider())
            {
                boss->GetCollider()->SetType(ColliderType::Body);
            }
            boss->SetAttackKind(Boss::AttackKind::None);

            mWaveRadius = boss->GetRadius() * 1.2f;
            mWaveHitPlayer = false;
            mTimer = 0.0f;

            vnCamera::StartShake(0.9f, 0.45f);

            boss->AddEffect(std::make_unique<ShockwaveEffect>(
                boss,
                -1.0f,          // groundY
                mWaveMaxRadius,
                mWaveSpeed,
                1.2f
            ));

            mPhase = ShockwavePhase::Wave;
        }
        break;
    }

    case ShockwavePhase::Wave:
    {
        //====================================================
        // 地面を走るリング衝撃波
        //====================================================
        mWaveRadius += mWaveSpeed * dt;

        // プレイヤーへの当たり判定
        UpdateShockwaveHit(boss);

        // 最大半径まで行ったら終了
        if (mWaveRadius >= mWaveMaxRadius)
        {
            mTimer = 0.8f;
            mPhase = ShockwavePhase::Recover;
        }
        break;
    }

    case ShockwavePhase::Recover:
    {
        mTimer -= dt;
        boss->LookAtSmooth(*Player::GetInstance()->getPosition(), 0.05f);

        if (mTimer <= 0.0f)
        {
            mWaveCount++;

            if (mWaveCount >= mMaxWaveCount)
            {
                boss->ChangeState(new BossStateCooldown());
                return;
            }

            //====================================================
            // 次のジャンプ開始時点で再度着地点を固定
            //====================================================
            mTargetPos = *Player::GetInstance()->getPosition();
            mTargetPos = XMVectorSetY(mTargetPos, boss->GetDefaultHeight());
            mTimer = 0.35f;
            boss->SetVerticalSpeed(1.1f);
            mPhase = ShockwavePhase::JumpUp;
        }
        break;
    }

    default:
        break;
    }
}

void BossStateShockwave::UpdateShockwaveHit(Boss* boss)
{
    Player* player = Player::GetInstance();
    if (!player || !boss) return;

    //====================================================
    // 1つの波につき多段ヒットしないようにする
    //====================================================
    if (mWaveHitPlayer) return;

    //====================================================
    // ボス中心からプレイヤーまでのXZ距離を見る
    // ・衝撃波は地面を走るのでYは無視
    //====================================================
    XMVECTOR bossPos = *boss->getPosition();
    XMVECTOR playerPos = *player->getPosition();

    XMVECTOR diff = playerPos - bossPos;
    diff = XMVectorSetY(diff, 0.0f);

    float dist = XMVectorGetX(XMVector3Length(diff));

    //====================================================
    // リング内にいるか
    // ・今広がっている円周部分だけ当てる
    //====================================================
    const float inner = mWaveRadius - mWaveThickness;
    const float outer = mWaveRadius + mWaveThickness;

    if (dist < inner || dist > outer)
    {
        return;
    }

    //====================================================
    // ジャンプ成功判定
    // ・地面近くではなく、十分浮いていたら成功
    //====================================================
    const float playerY = player->getPositionY();
    const float groundAvoidHeight = 1.2f; // 要調整

    if (!player->IsGrounded() && playerY > groundAvoidHeight)
    {
        //================================================
        // ジャンプ成功
        // ・通常のジャスト回避スローと同じ処理に入る
        // ・ただし二重開始は防ぐ
        //================================================
        if (!player->IsJumpJustSlow())
        {
            player->StartJumpJustSlow();
        }

        //================================================
        // この波ではもう判定しない
        // ・毎フレ何度も StartJumpJustSlow しないため
        //================================================
        mWaveHitPlayer = true;
        return;
    }

    //====================================================
    // 地上にいたので被弾
    // ・ノックバック方向はボス中心から外向き
    //====================================================
    XMVECTOR knockDir;
    if (XMVectorGetX(XMVector3LengthSq(diff)) > 0.0001f)
    {
        knockDir = XMVector3Normalize(diff);
    }
    else
    {
        knockDir = XMVectorSet(0, 0, 1, 0);
    }

    mWaveAttackInfo.knockbackDirection = knockDir;
    player->TakeDamage(mWaveAttackInfo);

    mWaveHitPlayer = true;
}

void BossStateShockwave::Exit(Boss* boss)
{
    if (!boss) return;

    boss->SetAttackKind(Boss::AttackKind::None);

    if (boss->GetCollider())
    {
        boss->GetCollider()->SetType(ColliderType::Body);
    }
}