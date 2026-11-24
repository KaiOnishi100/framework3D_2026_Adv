#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"invincibleAuraEffect.h"

InvincibleAuraEffect::InvincibleAuraEffect(Character* owner, float durationSec) : Effect(owner), mDuration(durationSec)
{
    Emitter::stEmitterDesc desc = {};
    swprintf_s(desc.Texture, L"%s", L"data/image/effect/glow.png");

    //========================================================
    // 発生形状
    // 円周から発生させて、中心へ吸わせる
    //========================================================
    desc.Shape = EmitShape::Circle;
    desc.Circle.Radius = owner ? owner->GetRadius() * 1.6f : 2.0f;

    //========================================================
    // 発生数
    //========================================================
    desc.EmitCount = 8;

    //========================================================
    // 寿命
    //========================================================
    desc.Life.Min = 0.25f;
    desc.Life.Max = 0.45f;

    //========================================================
    // サイズ
    //========================================================
    const float baseRadius = owner ? owner->GetRadius() : 1.0f;
    desc.Size.Min = baseRadius * 0.25f;
    desc.Size.Max = baseRadius * 0.45f;

    //========================================================
    // 初速ゼロ
    // 吸引で動かす
    //========================================================
    desc.Speed.Min = 0.0f;
    desc.Speed.Max = 0.0f;

    //========================================================
    // 色
    // 青白いオーラ
    //========================================================
    desc.Color.Min = XMVectorSet(0.55f, 0.85f, 1.0f, 0.85f);
    desc.Color.Max = XMVectorSet(1.00f, 1.00f, 1.0f, 0.95f);

    //========================================================
    // 見た目
    // 無敵オーラは加算合成と相性が良い
    //========================================================
    desc.Visual.Dark = false;
    desc.Visual.Additive = true;
    desc.Visual.SizeFade = SizeFadeMode::KeepMin;
    desc.Visual.SizeFadeMin = 0.55f;

    //========================================================
    // 吸引設定
    //========================================================
    desc.Attract.Enable = true;
    desc.Attract.Power = 30.0f;
    desc.Attract.KillDistance = 2.0f;
    desc.Attract.SwirlPower = 10.0f;
    desc.Attract.MaxSpeed = 45.0f;

    //========================================================
    // 初期状態
    // 常駐系なので最初から出す
    //========================================================
    desc.emit = true;

    mEmitter = std::make_unique<Emitter>(&desc);
}

void InvincibleAuraEffect::Update(float dt)
{
    if (!mOwner || !mEmitter) return;

    mLife += dt;

    //========================================================
    // オーナー中心に追従
    //========================================================
    XMVECTOR pos = *mOwner->getPosition();

    mEmitter->setPosition(&pos);

    const bool active = (mLife < mDuration);
    mEmitter->setEmit(active);

    //========================================================
    // 脈動
    //========================================================
    float pulse = 0.75f + 0.25f * sinf(mLife * 12.0f);
    mEmitter->SetAlphaMul(pulse);

    mEmitter->execute(dt);

    if (!active)
    {
        if (!mEmitter->HasAliveParticles())
        {
            mIsAlive = false;
        }
    }
}

void InvincibleAuraEffect::Render()
{
    if (mEmitter) mEmitter->render();
}
