#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "ambientDustEffect.h"

AmbientDustEffect::AmbientDustEffect(const XMVECTOR& worldPos, float scale) : Effect(nullptr)
{
    SetRenderPath(EffectRenderPath::FullResolution);

    Emitter::stEmitterDesc desc = {};
    swprintf_s(desc.Texture, L"%s", L"data/image/effect/glow.png");

    // 目の高さに近い位置を中心に、小粒がゆっくり流れる
    // scale は配置に合わせて範囲・粒径・移動速度を一緒に調整する
    desc.Shape = EmitShape::Dust;
    desc.Circle.Radius = 0.85f * scale;
    desc.EmitCount = 1;
    desc.EmitRate = 6.0f;
    desc.Life.Min = 2.0f;
    desc.Life.Max = 3.5f;
    desc.Size.Min = 0.009f * scale;
    desc.Size.Max = 0.023f * scale;
    desc.Speed.Min = 0.04f * scale;
    desc.Speed.Max = 0.12f * scale;
    desc.RandomVelocity.Min = XMVectorSet(-0.03f * scale, -0.07f * scale, -0.03f * scale, 0.0f);
    desc.RandomVelocity.Max = XMVectorSet( 0.03f * scale,  0.07f * scale,  0.03f * scale, 0.0f);

    // 少し暖かい灰色。加算量を抑え、暗い廊下で粒が主張しすぎないようにする
    desc.Color.Min = XMVectorSet(0.28f, 0.25f, 0.20f, 0.10f);
    desc.Color.Max = XMVectorSet(0.42f, 0.38f, 0.31f, 0.24f);
    desc.Visual.Additive = true;
    desc.Visual.Dark = false;
    desc.Visual.SizeFade = SizeFadeMode::KeepMin;
    desc.Visual.SizeFadeMin = 0.70f;
    desc.Attract.Enable = false;
    desc.emit = true;

    mEmitter = std::make_unique<Emitter>(&desc);
    mEmitter->setPositionX(XMVectorGetX(worldPos));
    mEmitter->setPositionY(XMVectorGetY(worldPos));
    mEmitter->setPositionZ(XMVectorGetZ(worldPos));
    mEmitter->SyncTransform();

    // 既存の懐中電灯やろうそくの光を粒子側で受けるための設定
    mEmitter->setReceiveShadow(true);
    mEmitter->SetLocalLightStrength(0.80f);
    mEmitter->SetLocalShadowEnabled(true);
}

AmbientDustEffect::~AmbientDustEffect() = default;

void AmbientDustEffect::Update(float dt)
{
    // 頂点データの更新前に、環境エフェクト共通の不透明度を反映する
    mEmitter->SetAlphaMul(mOpacity);
    mEmitter->execute(dt);

    // 停止後も、すでに漂っている粒が消えるまで描画を続ける
    if (IsStopped() && !mEmitter->HasAliveParticles())
        mIsAlive = false;
}

void AmbientDustEffect::Render()
{
    mEmitter->render();
}

void AmbientDustEffect::Stop()
{
    Effect::Stop();
    mEmitter->setEmit(false);
}
