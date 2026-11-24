#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"chargeEffect.h"

ChargeEffect::ChargeEffect(Character* owner)
    : Effect(owner)
{
    //========================================================
    // 外側リング
    //========================================================
    Emitter::stEmitterDesc outer = {};
    swprintf_s(outer.Texture, L"%s", L"data/image/effect/glow.png");

    outer.Shape = EmitShape::Circle;
    outer.Circle.Radius = 34.0f;

    outer.EmitCount = 10;

    outer.Life.Min = 1.6f;
    outer.Life.Max = 2.4f;

    outer.Size.Min = 1.8f;
    outer.Size.Max = 3.5f;

    outer.Speed.Min = 0.0f;
    outer.Speed.Max = 0.0f;

    outer.Color.Min = XMVectorSet(1.0f, 0.10f, 0.55f, 0.18f);
    outer.Color.Max = XMVectorSet(1.0f, 0.05f, 1.00f, 0.32f);

    outer.Visual.Dark = false;
    outer.Visual.Additive = true;
    outer.Visual.SizeFade = SizeFadeMode::KeepMin;
    outer.Visual.SizeFadeMin = 0.65f;

    outer.Attract.Enable = true;
    outer.Attract.Power = 45.0f;
    outer.Attract.KillDistance = 6.0f;
    outer.Attract.SwirlPower = 18.0f;
    outer.Attract.MaxSpeed = 75.0f;

    outer.emit = false;

    mOuterEmitter = std::make_unique<Emitter>(&outer);

    //========================================================
    // 内側リング
    //========================================================
    Emitter::stEmitterDesc inner = {};
    swprintf_s(inner.Texture, L"%s", L"data/image/effect/glow.png");

    inner.Shape = EmitShape::Circle;
    inner.Circle.Radius = 18.0f;

    inner.EmitCount = 14;

    inner.Life.Min = 1.0f;
    inner.Life.Max = 1.8f;

    inner.Size.Min = 1.0f;
    inner.Size.Max = 2.2f;

    inner.Speed.Min = 0.0f;
    inner.Speed.Max = 0.0f;

    inner.Color.Min = XMVectorSet(1.0f, 0.45f, 0.85f, 0.40f);
    inner.Color.Max = XMVectorSet(1.0f, 0.75f, 1.00f, 0.75f);

    inner.Visual.Dark = false;
    inner.Visual.Additive = true;
    inner.Visual.SizeFade = SizeFadeMode::KeepMin;
    inner.Visual.SizeFadeMin = 0.50f;

    inner.Attract.Enable = true;
    inner.Attract.Power = 70.0f;
    inner.Attract.KillDistance = 5.0f;
    inner.Attract.SwirlPower = 26.0f;
    inner.Attract.MaxSpeed = 95.0f;

    inner.emit = false;

    mInnerEmitter = std::make_unique<Emitter>(&inner);
}

void ChargeEffect::Update(float dt)
{
    if (!mOwner || !mOuterEmitter || !mInnerEmitter) return;

    mLifeTime += dt;

    //========================================================
    // オーナー位置に追従
    //========================================================
    XMMATRIX ownerW = *mOwner->getWorld();

    // 少し上に寄せると胸元や口元のチャージ感が出しやすい
    XMVECTOR localOffset = XMVectorSet(0.0f, 8.0f, 0.0f, 1.0f);
    XMVECTOR worldPos = XMVector3TransformCoord(localOffset, ownerW);

    mOuterEmitter->setPosition(&worldPos);
    mInnerEmitter->setPosition(&worldPos);

    //========================================================
    // 最初の 1.0 秒だけ発生
    //========================================================
    const bool emitNow = (mLifeTime < 1.0f);

    mOuterEmitter->setEmit(emitNow);
    mInnerEmitter->setEmit(emitNow);

    //========================================================
    // チャージ進行で明るさを強くする
    //========================================================
    float t = std::clamp(mLifeTime / 1.0f, 0.0f, 1.0f);

    // 外は控えめ、内は強め
    mOuterEmitter->SetAlphaMul(0.25f + t * 0.45f);
    mInnerEmitter->SetAlphaMul(0.35f + t * 0.65f);

    mOuterEmitter->execute(dt);
    mInnerEmitter->execute(dt);

    //========================================================
    // 全体寿命を過ぎたら、残粒子が消えるまで待つ
    //========================================================
    if (mLifeTime > mDuration)
    {
        mOuterEmitter->setEmit(false);
        mInnerEmitter->setEmit(false);

        if (!mOuterEmitter->HasAliveParticles() &&
            !mInnerEmitter->HasAliveParticles())
        {
            mIsAlive = false;
        }
    }
}

void ChargeEffect::Render()
{
    if (mOuterEmitter) mOuterEmitter->render();
    if (mInnerEmitter) mInnerEmitter->render();
}