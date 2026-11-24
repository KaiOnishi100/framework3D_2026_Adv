#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"smokeEffect.h"

SmokeEffect::SmokeEffect(Character* owner, float durationSec)
    : Effect(owner), mDuration(durationSec)
{
    Emitter::stEmitterDesc desc = {};
    swprintf_s(desc.Texture, L"%s", L"data/image/effect/smoke.png");

    //========================================================
    // 基本設定
    //========================================================
    desc.Shape = EmitShape::Cone;
    desc.EmitCount = 1;

    //========================================================
    // 煙は広めのコーン
    //========================================================
    desc.Cone.SpreadAngle = XM_PIDIV4 * 0.8f;
    desc.Cone.YawScale = 1.0f;
    desc.Cone.PitchScale = 0.35f;

    // 上昇感
    desc.Cone.UpBias.Min = 6.0f;
    desc.Cone.UpBias.Max = 12.0f;

    //========================================================
    // 煙は長生き
    //========================================================
    desc.Life.Min = 1.8f;
    desc.Life.Max = 3.2f;

    //========================================================
    // かなり大きめ
    //========================================================
    desc.Size.Min = 3.5f;
    desc.Size.Max = 7.5f;

    //========================================================
    // 速度は遅め
    //========================================================
    desc.Speed.Min = 8.0f;
    desc.Speed.Max = 18.0f;

    //========================================================
    // 色は低彩度の紫煙
    //========================================================
    desc.Color.Min = XMVectorSet(0.20f, 0.20f, 0.22f, 0.10f);
    desc.Color.Max = XMVectorSet(0.36f, 0.36f, 0.40f, 0.20f);

    //========================================================
    // ランダム揺らぎ
    //========================================================
    desc.RandomVelocity.Min = XMVectorSet(-2.0f, -1.0f, -2.0f, 0.0f);
    desc.RandomVelocity.Max = XMVectorSet(2.0f, 1.0f, 2.0f, 0.0f);

    //========================================================
    // 見た目設定
    //========================================================
    desc.Visual.Additive = false;                       // 煙は通常ブレンド
    desc.Visual.Dark = true;                           // 時間で少し暗くなる
    desc.Visual.SizeFade = SizeFadeMode::KeepMin;      // 最低サイズを残す
    desc.Visual.SizeFadeMin = 0.75f;                   // かなり残す

    //========================================================
    // 吸引なし
    //========================================================
    desc.Attract.Enable = false;

    //========================================================
    // 初期状態では emit しない
    //========================================================
    desc.emit = false;

    mEmitter = std::make_unique<Emitter>(&desc);

}

void SmokeEffect::Update(float dt)
{
    if (!mOwner || !mEmitter) return;

    //========================================================
    // 口元に追従
    //========================================================
    XMMATRIX ownerW = *mOwner->getWorld();

    XMVECTOR localMouth = XMVectorSet(0.0f, 2.0f, 7.5f, 1.0f);
    XMVECTOR mouthWorldPos = XMVector3TransformCoord(localMouth, ownerW);

    mEmitter->setPosition(&mouthWorldPos);

    //========================================================
    // 発生時間管理
    //========================================================
    mLife += dt;

    const bool emitNow = (mLife < mDuration);

    mEmitter->setEmit(emitNow);
    mEmitter->execute(dt);

    //========================================================
    // 発生停止後も、残っている煙が全部消えるまで残す
    //========================================================
    if (!emitNow && !mEmitter->HasAliveParticles())
    {
        mIsAlive = false;
    }
}

void SmokeEffect::Render()
{
    if (mEmitter) mEmitter->render();
}