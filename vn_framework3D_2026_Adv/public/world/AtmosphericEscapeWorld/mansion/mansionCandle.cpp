#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "../../../effect/AtomosphericEscape/candleFlameEffect.h"
#include "mansionCandle.h"

MansionCandle::MansionCandle(
    const std::string& placementId,
    const XMFLOAT3& wickPosition,
    float scale)
    : mWickPosition(wickPosition)
    , mScale(std::isfinite(scale) && scale > 0.0f ? scale : 1.0f)
    , mPhase(static_cast<float>(std::hash<std::string>{}(placementId) % 1024u) * (XM_2PI / 1024.0f))
{
}

std::unique_ptr<Effect> MansionCandle::CreateFlame() const 
{
    Emitter::stEmitterDesc desc = {};
    swprintf_s(desc.Texture, L"%s", L"data/image/effect/candle_flame.png");

    // 芯から上へ、短寿命の小さな炎を少しずつ重ねる
    desc.Shape = EmitShape::Cone;
    desc.Direction = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    desc.Cone.SpreadAngle = 0.06f;
    desc.Cone.YawScale = 1.0f;
    desc.Cone.PitchScale = 1.0f;
    desc.Cone.UpBias.Min = 0.0f;
    desc.Cone.UpBias.Max = 0.0f;
    desc.EmitCount = 1;
    desc.EmitRate = 22.0f;
    desc.Life.Min = 0.18f;
    desc.Life.Max = 0.28f;
    desc.Size.Min = 0.046f * mScale;
    desc.Size.Max = 0.062f * mScale;
    desc.Speed.Min = 0.025f * mScale;
    desc.Speed.Max = 0.075f * mScale;
    desc.RandomVelocity.Min = XMVectorSet(-0.018f * mScale, 0.0f, -0.018f * mScale, 0.0f);
    desc.RandomVelocity.Max = XMVectorSet( 0.018f * mScale, 0.0f,  0.018f * mScale, 0.0f);

    // テクスチャの色を使い、アルファで重なった部分の明るさを抑える
    desc.Color.Min = XMVectorSet(1.0f, 0.91f, 0.76f, 0.30f);
    desc.Color.Max = XMVectorSet(1.0f, 1.00f, 0.94f, 0.52f);
    desc.Visual.Additive = true;
    desc.Visual.Dark = false;
    desc.Visual.SizeFade = SizeFadeMode::KeepMin;
    desc.Visual.SizeFadeMin = 0.72f;
    desc.Attract.Enable = false;
    desc.emit = true;

    auto emitter = std::make_unique<Emitter>(&desc);

    // ビルボードの中心を芯より上に置き、炎の根元を芯に近づける
    emitter->setPositionX(mWickPosition.x);
    emitter->setPositionY(mWickPosition.y + 0.044f * mScale);
    emitter->setPositionZ(mWickPosition.z);
    emitter->SyncTransform();

    // 自発光の炎なので、懐中電灯が当たっても粒子自体は増光させない
    emitter->setReceiveShadow(false);
    emitter->SetLocalLightStrength(0.0f);
    emitter->SetLocalShadowEnabled(false);

    return std::make_unique<CandleFlameEffect>(std::move(emitter));
}

void MansionCandle::UpdateLight(float dt)
{
    // 時間で連続的に変化させる。配置 ID ごとの位相で同時に明滅するのを避ける
    mTime = std::fmod(mTime + dt, XM_2PI);

    const float flicker = 1.0f
        + 0.09f * std::sin(7.0f * mTime + mPhase) + 0.05f * std::sin(13.0f * mTime + 2.0f * mPhase);

    AtmosphericEscapeLightBudget::PointLightRequest request{};
    request.Position = mWickPosition;
    request.Position.y += 0.06f * mScale;

    // 近くの壁や燭台だけを暖色で照らす。炎のサイズと光量は個別に調整する
    request.Color = XMFLOAT3(1.0f, 0.51f, 0.22f);
    request.Range = 2.6f;
    request.Intensity = 1.4f * flicker;

    // 初期設定では Point Shadow の枠を消費しない
    request.CastsShadow = false;
    AtmosphericEscapeLightBudget::SubmitPointLight(request);
}
