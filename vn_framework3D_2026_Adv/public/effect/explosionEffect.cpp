#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"explosionEffect.h"

ExplosionEffect::ExplosionEffect(Character* owner, float scale, float lifetimeScale)
    : Effect(owner), mScale(scale), mLifeScale(lifetimeScale)
{
    //====================================================
   // 発生位置を死亡した瞬間で固定
   //====================================================
    if (mOwner)
    {
        mSpawnPos = *mOwner->getPosition();
    }

    //====================================================
    // Flash（閃光）
    // ・一瞬だけ大きく光る
    // ・加算合成
    //====================================================
    Emitter::stEmitterDesc flash = {};
    swprintf_s(flash.Texture, L"%s", L"data/image/effect/particle001.png");

    flash.Shape = EmitShape::Sphere;
    flash.EmitCount = 35;

    // 秒ベースに調整
    flash.Life.Min = 0.08f;
    flash.Life.Max = 0.14f;

    flash.Size.Min = 5.0f * mScale;
    flash.Size.Max = 10.0f * mScale;

    flash.Speed.Min = 1.0f * mScale;
    flash.Speed.Max = 4.0f * mScale;

    flash.Color.Min = XMVectorSet(1.0f, 0.98f, 0.85f, 1.0f);
    flash.Color.Max = XMVectorSet(1.0f, 1.00f, 1.00f, 1.0f);

    flash.Visual.Dark = false;
    flash.Visual.Additive = true;
    flash.Visual.SizeFade = SizeFadeMode::KeepMin;
    flash.Visual.SizeFadeMin = 0.75f;

    flash.Attract.Enable = false;
    flash.emit = false;

    mFlash = std::make_unique<Emitter>(&flash);
    mFlash->setPositionX(XMVectorGetX(mSpawnPos));
    mFlash->setPositionY(XMVectorGetY(mSpawnPos) + 1.2f);
    mFlash->setPositionZ(XMVectorGetZ(mSpawnPos));
    mFlash->SyncTransform();

    //====================================================
    // Core（爆発の核）
    // ・オレンジの中心
    // ・その場で膨らむ感じ
    // ・加算合成
    //====================================================
    Emitter::stEmitterDesc core = {};
    swprintf_s(core.Texture, L"%s", L"data/image/effect/particle001.png");

    core.Shape = EmitShape::Sphere;
    core.EmitCount = (int)(70 * mScale);

    core.Life.Min = 0.10f;
    core.Life.Max = 0.18f;

    core.Size.Min = 6.0f * mScale;
    core.Size.Max = 13.0f * mScale;

    core.Speed.Min = 0.2f * mScale;
    core.Speed.Max = 1.5f * mScale;

    core.Color.Min = XMVectorSet(1.0f, 0.55f, 0.10f, 1.0f);
    core.Color.Max = XMVectorSet(1.0f, 0.85f, 0.20f, 1.0f);

    core.Visual.Dark = false;
    core.Visual.Additive = true;
    core.Visual.SizeFade = SizeFadeMode::KeepMin;
    core.Visual.SizeFadeMin = 0.70f;

    core.Attract.Enable = false;
    core.emit = false;

    mCore = std::make_unique<Emitter>(&core);
    mCore->setPositionX(XMVectorGetX(mSpawnPos));
    mCore->setPositionY(XMVectorGetY(mSpawnPos) + 1.0f);
    mCore->setPositionZ(XMVectorGetZ(mSpawnPos));
    mCore->SyncTransform();

    //====================================================
    // Sparks（火花）
    // ・勢い担当
    // ・コーンで少し上方向へ
    // ・加算合成
    //====================================================
    Emitter::stEmitterDesc sparks = {};
    swprintf_s(sparks.Texture, L"%s", L"data/image/effect/particle001.png");

    sparks.Shape = EmitShape::Cone;
    sparks.Direction = XMVectorSet(0, 1, 0, 0);
    sparks.EmitCount = (int)(45 * mScale);

    sparks.Life.Min = 0.20f;
    sparks.Life.Max = 0.35f;

    sparks.Size.Min = 0.6f * mScale;
    sparks.Size.Max = 1.4f * mScale;

    sparks.Speed.Min = 18.0f * mScale;
    sparks.Speed.Max = 35.0f * mScale;

    sparks.Color.Min = XMVectorSet(1.0f, 0.85f, 0.25f, 1.0f);
    sparks.Color.Max = XMVectorSet(1.0f, 1.00f, 0.60f, 1.0f);

    // コーン設定
    sparks.Cone.SpreadAngle = XM_PIDIV2;    // 広め
    sparks.Cone.YawScale = 1.0f;
    sparks.Cone.PitchScale = 0.8f;
    sparks.Cone.UpBias.Min = 2.0f * mScale;
    sparks.Cone.UpBias.Max = 8.0f * mScale;

    // 少しバラつきを足す
    sparks.RandomVelocity.Min = XMVectorSet(-2.0f, -1.0f, -2.0f, 0.0f);
    sparks.RandomVelocity.Max = XMVectorSet(2.0f, 1.0f, 2.0f, 0.0f);

    sparks.Visual.Dark = false;
    sparks.Visual.Additive = true;
    sparks.Visual.SizeFade = SizeFadeMode::KeepMin;
    sparks.Visual.SizeFadeMin = 0.35f;

    sparks.Attract.Enable = false;
    sparks.emit = false;

    mSparks = std::make_unique<Emitter>(&sparks);
    mSparks->setPositionX(XMVectorGetX(mSpawnPos));
    mSparks->setPositionY(XMVectorGetY(mSpawnPos) + 0.8f);
    mSparks->setPositionZ(XMVectorGetZ(mSpawnPos));
    mSparks->SyncTransform();

    //====================================================
    // Smoke（煙）
    // ・余韻
    // ・上方向コーン
    // ・通常ブレンド
    //====================================================
    Emitter::stEmitterDesc smoke = {};
    swprintf_s(smoke.Texture, L"%s", L"data/image/effect/smoke.png");

    smoke.Shape = EmitShape::Cone;
    smoke.Direction = XMVectorSet(0, 1, 0, 0);
    smoke.EmitCount = (int)(130 * mScale);

    smoke.Life.Min = 1.0f * mLifeScale;
    smoke.Life.Max = 2.0f * mLifeScale;

    smoke.Size.Min = 2.2f * mScale;
    smoke.Size.Max = 5.0f * mScale;

    smoke.Speed.Min = 6.0f * mScale;
    smoke.Speed.Max = 14.0f * mScale;

    smoke.Color.Min = XMVectorSet(1.0f, 0.60f, 0.20f, 0.95f);
    smoke.Color.Max = XMVectorSet(0.55f, 0.55f, 0.55f, 0.85f);

    smoke.Cone.SpreadAngle = XM_PIDIV2;
    smoke.Cone.YawScale = 1.0f;
    smoke.Cone.PitchScale = 0.7f;
    smoke.Cone.UpBias.Min = 4.0f * mScale;
    smoke.Cone.UpBias.Max = 10.0f * mScale;

    smoke.RandomVelocity.Min = XMVectorSet(-1.5f, -0.5f, -1.5f, 0.0f);
    smoke.RandomVelocity.Max = XMVectorSet(1.5f, 0.5f, 1.5f, 0.0f);

    smoke.Visual.Dark = true;
    smoke.Visual.Additive = false;
    smoke.Visual.SizeFade = SizeFadeMode::KeepMin;
    smoke.Visual.SizeFadeMin = 0.80f;

    smoke.Attract.Enable = false;
    smoke.emit = false;

    mSmoke = std::make_unique<Emitter>(&smoke);
    mSmoke->setPositionX(XMVectorGetX(mSpawnPos));
    mSmoke->setPositionY(XMVectorGetY(mSpawnPos) + 0.5f);
    mSmoke->setPositionZ(XMVectorGetZ(mSpawnPos));
    mSmoke->SyncTransform();
}

void ExplosionEffect::Update(float dt)
{
    //====================================================
    // 生成直後に1回だけ EmitOnce（ワンショット爆発）
    //====================================================
    if (!mEmitted)
    {
        if (mFlash)  mFlash->EmitOnce();
        if (mCore)   mCore->EmitOnce();
        if (mSparks) mSparks->EmitOnce();
        if (mSmoke)  mSmoke->EmitOnce();

        mEmitted = true;
    }

    //====================================================
    // エミッタ更新
    //====================================================
    if (mFlash)  mFlash->execute(dt);
    if (mCore)   mCore->execute(dt);
    if (mSparks) mSparks->execute(dt);
    if (mSmoke)  mSmoke->execute(dt);

    //====================================================
    // 全部消えたら終了
    //====================================================
    const bool flashAlive = (mFlash && mFlash->HasAliveParticles());
    const bool coreAlive = (mCore && mCore->HasAliveParticles());
    const bool sparksAlive = (mSparks && mSparks->HasAliveParticles());
    const bool smokeAlive = (mSmoke && mSmoke->HasAliveParticles());

    if (!flashAlive && !coreAlive && !sparksAlive && !smokeAlive)
    {
        mIsAlive = false;
    }
}

void ExplosionEffect::Render()
{
    if (mFlash)  mFlash->render();
    if (mCore)   mCore->render();
    if (mSparks) mSparks->render();
    if (mSmoke)  mSmoke->render();
}