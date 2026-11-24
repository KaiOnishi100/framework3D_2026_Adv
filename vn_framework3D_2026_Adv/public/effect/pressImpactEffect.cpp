#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"pressImpactEffect.h"

PressImpactEffect::PressImpactEffect(Character* owner, float durationSec)
    : Effect(owner), mDuration(durationSec)
{
    //========================================================
    // Ring：輪状の土煙
    //========================================================
    Emitter::stEmitterDesc ring = {};
    swprintf_s(ring.Texture, L"%s", L"data/image/effect/smoke.png");

    // 基本
    ring.Shape = EmitShape::Press;
    ring.Circle.Radius = owner ? owner->GetRadius() * 1.25f : 2.0f;

    ring.EmitCount = 36;

    // 寿命
    ring.Life.Min = 0.7f;
    ring.Life.Max = 1.1f;

    // サイズ
    ring.Size.Min = 2.2f;
    ring.Size.Max = 5.2f;

    // 速度
    ring.Speed.Min = 18.0f;
    ring.Speed.Max = 38.0f;

    // Press 専用の上方向
    ring.Press.Up.Min = 14.0f;
    ring.Press.Up.Max = 26.0f;

    // 色
    ring.Color.Min = XMVectorSet(0.25f, 0.20f, 0.16f, 0.75f);
    ring.Color.Max = XMVectorSet(0.30f, 0.24f, 0.18f, 0.85f);

    // 見た目
    ring.Visual.Dark = true;
    ring.Visual.Additive = false;
    ring.Visual.SizeFade = SizeFadeMode::KeepMin;
    ring.Visual.SizeFadeMin = 0.75f;

    // 吸引なし
    ring.Attract.Enable = false;

    // 最初は emit しない
    ring.emit = false;

    mRing = std::make_unique<Emitter>(&ring);

    //========================================================
    // Chunks：土塊
    //========================================================
    Emitter::stEmitterDesc chunk = {};
    swprintf_s(chunk.Texture, L"%s", L"data/image/effect/smoke.png");

    // 基本
    chunk.Shape = EmitShape::Cone;
    chunk.Direction = XMVectorSet(0, 1, 0, 0);
    chunk.EmitCount = 20;

    // 寿命
    chunk.Life.Min = 0.4f;
    chunk.Life.Max = 0.8f;

    // サイズ
    chunk.Size.Min = 1.2f;
    chunk.Size.Max = 3.0f;

    // 速度
    chunk.Speed.Min = 20.0f;
    chunk.Speed.Max = 50.0f;

    // 色
    chunk.Color.Min = XMVectorSet(0.12f, 0.09f, 0.06f, 0.95f);
    chunk.Color.Max = XMVectorSet(0.25f, 0.20f, 0.14f, 0.90f);

    // Cone 設定
    chunk.Cone.SpreadAngle = XMConvertToRadians(65.0f);
    chunk.Cone.YawScale = 1.0f;
    chunk.Cone.PitchScale = 0.8f;

    // 少し上方向を強める
    chunk.Cone.UpBias.Min = 6.0f;
    chunk.Cone.UpBias.Max = 14.0f;

    // 多少バラつかせる
    chunk.RandomVelocity.Min = XMVectorSet(-3.0f, -1.0f, -3.0f, 0.0f);
    chunk.RandomVelocity.Max = XMVectorSet(3.0f, 1.0f, 3.0f, 0.0f);

    // 見た目
    chunk.Visual.Dark = true;
    chunk.Visual.Additive = false;
    chunk.Visual.SizeFade = SizeFadeMode::KeepMin;
    chunk.Visual.SizeFadeMin = 0.55f;

    // 吸引なし
    chunk.Attract.Enable = false;

    // 最初は emit しない
    chunk.emit = false;

    mChunks = std::make_unique<Emitter>(&chunk);
}

void PressImpactEffect::Update(float dt)
{
    if (!mOwner || !mRing || !mChunks) return;

    mLife += dt;

    //========================================================
     // 着地点
     // ボス中心 - 半径分下
     //========================================================
    XMVECTOR center = *mOwner->getPosition();

    XMVECTOR impactPos = center + XMVectorSet(
        0.0f,
        -mOwner->GetRadius(),
        0.0f,
        0.0f
    );

    mRing->setPosition(&impactPos);
    mChunks->setPosition(&impactPos);

    //========================================================
    // 最初の短時間だけ放出
    //========================================================
    const bool emitting = (mLife <= mEmitTime);
    mRing->setEmit(emitting);
    mChunks->setEmit(emitting);

    mRing->execute(dt);
    mChunks->execute(dt);

    //========================================================
    // 全体時間を過ぎたら新規放出停止
    // 残粒子が全部消えたら終了
    //========================================================
    if (mLife >= mDuration)
    {
        mRing->setEmit(false);
        mChunks->setEmit(false);

        const bool ringAlive = mRing->HasAliveParticles();
        const bool chunkAlive = mChunks->HasAliveParticles();

        if (!ringAlive && !chunkAlive)
        {
            mIsAlive = false;
        }
    }
}

void PressImpactEffect::Render()
{
    if (mRing)   mRing->render();
    if (mChunks) mChunks->render();
}