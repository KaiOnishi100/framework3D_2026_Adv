#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"rollingDustEffect.h"

RollingDustEffect::RollingDustEffect(Character* owner, float durationSec, Mode mode) : 
    Effect(owner), mDuration(durationSec), mMode(mode)
{
    Emitter::stEmitterDesc desc = {};
    swprintf_s(desc.Texture, L"%s", L"data/image/effect/smoke.png");

    //========================================================
    // 発生形状
    //========================================================
    desc.Shape = EmitShape::Dust;
    desc.Circle.Radius = 8.5f;   // ボス半径 7.5 より少し外

    //========================================================
    // 発生数
    //========================================================
    desc.EmitCount = 16;

    //========================================================
    // 寿命
    //========================================================
    desc.Life.Min = 0.6f;
    desc.Life.Max = 1.0f;

    //========================================================
    // サイズ
    //========================================================
    desc.Size.Min = 1.0f;
    desc.Size.Max = 2.0f;

    //========================================================
    // 速度
    //========================================================
    desc.Speed.Min = 10.0f;
    desc.Speed.Max = 22.0f;

    //========================================================
    // 色
    // 暗めの砂っぽい色
    //========================================================
    desc.Color.Min = XMVectorSet(0.20f, 0.16f, 0.12f, 0.70f);
    desc.Color.Max = XMVectorSet(0.22f, 0.18f, 0.14f, 0.55f);

    //========================================================
    // 見た目
    // 土煙なので通常ブレンド寄り
    //========================================================
    desc.Visual.Dark = true;
    desc.Visual.Additive = false;
    desc.Visual.SizeFade = SizeFadeMode::KeepMin;
    desc.Visual.SizeFadeMin = 0.70f;

    //========================================================
    // 吸引なし
    //========================================================
    desc.Attract.Enable = false;

    //========================================================
    // 常時放出型
    //========================================================
    desc.emit = true;

    mEmitter = std::make_unique<Emitter>(&desc);
}

void RollingDustEffect::Update(float dt)
{
    if (!mOwner || !mEmitter) return;

    mLife += dt;

    //========================================================
    // オーナー位置を取得
    //========================================================
    XMVECTOR pos = *mOwner->getPosition();

    // 地面付近に固定
    pos = XMVectorSetY(pos, mYOffset);

    if (mMode == Mode::Rolling)
    {
        XMVECTOR forward = mOwner->getForward();
        forward = XMVectorSetY(forward, 0.0f);

        if (XMVectorGetX(XMVector3LengthSq(forward)) > 0.0001f)
        {
            forward = XMVector3Normalize(forward);

            // 少し後ろに出す
            pos += forward * (-mBackwardBias);
        }
    }

    mEmitter->setPosition(&pos);

    const bool active = (!mStopped && mLife < mDuration);
    mEmitter->setEmit(active);

    mEmitter->SetAlphaMul(mOpacity);
    mEmitter->execute(dt);

    if (!active && !mEmitter->HasAliveParticles())
    {
        mIsAlive = false;
    }
}

void RollingDustEffect::Render()
{
    if (mEmitter) mEmitter->render();
}

void RollingDustEffect::Stop()
{
    Effect::Stop();
    if (mEmitter) mEmitter->setEmit(false);
}

void RollingDustEffect::SetOpacity(float a)
{
    mOpacity = std::clamp(a, 0.0f, 1.0f);
    if (mEmitter) mEmitter->SetAlphaMul(mOpacity);
}
