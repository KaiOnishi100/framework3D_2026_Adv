#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "doorDustEffect.h"

DoorDustEffect::DoorDustEffect(const XMVECTOR& thresholdWorldPos, float scale) : Effect(nullptr)
{
    SetRenderPath(EffectRenderPath::FullResolution);

    Emitter::stEmitterDesc desc = {};
    swprintf_s(desc.Texture, L"%s", L"data/image/effect/smoke.png");

    // 少量だけ足元に広げ、1秒前後で消える埃にする
    desc.Shape = EmitShape::Dust;
    desc.Circle.Radius = 0.24f * scale;
    desc.EmitCount = 10;
    desc.Life.Min = 0.55f;
    desc.Life.Max = 1.10f;
    desc.Size.Min = 0.055f * scale;
    desc.Size.Max = 0.14f * scale;
    desc.Speed.Min = 0.16f * scale;
    desc.Speed.Max = 0.40f * scale;
    desc.RandomVelocity.Min = XMVectorSet(-0.05f * scale, 0.01f * scale, -0.05f * scale, 0.0f);
    desc.RandomVelocity.Max = XMVectorSet( 0.05f * scale, 0.05f * scale,  0.05f * scale, 0.0f);

    // 壁や床の質感を透かす程度の薄い煙。通常の半透明で描画する
    desc.Color.Min = XMVectorSet(0.23f, 0.19f, 0.15f, 0.07f);
    desc.Color.Max = XMVectorSet(0.36f, 0.30f, 0.23f, 0.18f);
    desc.Visual.Additive = false;
    desc.Visual.Dark = false;
    desc.Visual.SizeFade = SizeFadeMode::KeepMin;
    desc.Visual.SizeFadeMin = 0.70f;
    desc.Attract.Enable = false;
    desc.emit = false; // 継続放出は使わず最初のUpdateでEmitOnceする

    mEmitter = std::make_unique<Emitter>(&desc);
    mEmitter->setPositionX(XMVectorGetX(thresholdWorldPos));
    mEmitter->setPositionY(XMVectorGetY(thresholdWorldPos) + 0.07f * scale);
    mEmitter->setPositionZ(XMVectorGetZ(thresholdWorldPos));
    mEmitter->SyncTransform();

    mEmitter->setReceiveShadow(true);
    mEmitter->SetLocalLightStrength(0.45f);
    mEmitter->SetLocalShadowEnabled(true);
}

DoorDustEffect::~DoorDustEffect() = default;

void DoorDustEffect::Update(float dt)
{
    // ポーズなどで dt が 0 の間は放出を次の有効な更新まで待つ
    if (!mEmitted && !IsStopped())
    {
        if (dt <= 0.0f)
            return;

        mEmitter->EmitOnce();
        mEmitted = true;
    }

    // 放出後は寿命まで粒を進め最後の粒が消えたらManagerが回収する
    mEmitter->SetAlphaMul(mOpacity);
    mEmitter->execute(dt);
    if (!mEmitter->HasAliveParticles())
        mIsAlive = false;
}

void DoorDustEffect::Render()
{
    // 初回の更新前は、空のエミッターを描画しない
    if (mEmitted)
        mEmitter->render();
}

void DoorDustEffect::Stop()
{
    Effect::Stop();

    // 放出前ならキャンセル 放出済みの埃は自然に消える
    if (!mEmitted)
        mIsAlive = false;
}
