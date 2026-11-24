#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"breathEffect.h"

BreathEffect::BreathEffect(Character* owner, Type type, float range, float angleRad, float lifeTime)
    : Effect(owner), mType(type), mRange(range), mAngle(angleRad), mDuration(lifeTime), mLife(0.0f)
{
    SetRenderPath(EffectRenderPath::HalfResolutionAdditive);

    //============================================================
    // ブレス中心層
    //============================================================
    Emitter::stEmitterDesc core = {};
    swprintf_s(core.Texture, L"%s", L"data/image/effect/glow.png");

    //============================================================
    // 基本設定
    //============================================================
    core.Shape = EmitShape::Cone;
    core.EmitCount = 4;

    core.Life.Min = 0.9f;
    core.Life.Max = 1.4f;

    core.Size.Min = 1.2f;
    core.Size.Max = 2.8f;

    core.Speed.Min = 85.0f;
    core.Speed.Max = 120.0f;

    //============================================================
    // 色
    //============================================================
    core.Color.Min = XMVectorSet(1.0f, 0.95f, 0.55f, 0.30f);
    core.Color.Max = XMVectorSet(1.0f, 0.55f, 0.10f, 0.55f);

    //============================================================
    // コーン設定
    //============================================================
    core.Cone.SpreadAngle = mAngle * 0.45f;
    core.Cone.YawScale = 1.0f;
    core.Cone.PitchScale = 0.12f;     // 縦にはあまり広げない
    core.Cone.UpBias.Min = 0.0f;
    core.Cone.UpBias.Max = 1.0f;

    //============================================================
    // ランダム揺らぎ
    //============================================================
    core.RandomVelocity.Min = XMVectorSet(-1.5f, -0.4f, -1.5f, 0.0f);
    core.RandomVelocity.Max = XMVectorSet(1.5f, 0.4f, 1.5f, 0.0f);

    //============================================================
    // 見た目設定
    //============================================================
    core.Visual.Dark = false;
    core.Visual.Additive = true;
    core.Visual.SizeFade = SizeFadeMode::KeepMin;
    core.Visual.SizeFadeMin = 0.45f;

    //============================================================
    // 吸引なし
    //============================================================
    core.Attract.Enable = false;

    //============================================================
    // 初期状態では emit しない
    //============================================================
    core.emit = false;
    core.EmitRate = 120.0f;

    mCoreEmitter = std::make_unique<Emitter>(&core);

    mCoreEmitter->setReceiveShadow(true);

    mCoreEmitter->SetShadowReceiveStrength(1.0f);

    mCoreEmitter->SetLocalLightStrength(0.08f);

    mCoreEmitter->SetLocalShadowEnabled(false);

    //============================================================
    // ブレス外側層
    // ・広め
    // ・薄め
    // ・炎のふくらみ担当
    //============================================================
    Emitter::stEmitterDesc outer = {};
    swprintf_s(outer.Texture, L"%s", L"data/image/effect/glow.png");

    //============================================================
    // 基本設定
    //============================================================
    outer.Shape = EmitShape::Cone;
    outer.EmitCount = 2;

    outer.Life.Min = 1.2f;
    outer.Life.Max = 1.9f;

    outer.Size.Min = 2.8f;
    outer.Size.Max = 5.8f;

    outer.Speed.Min = 60.0f;
    outer.Speed.Max = 95.0f;

    //============================================================
    // 色
    //============================================================
    outer.Color.Min = XMVectorSet(1.0f, 0.40f, 0.10f, 0.16f);
    outer.Color.Max = XMVectorSet(1.0f, 0.18f, 0.04f, 0.28f);

    //============================================================
    // コーン設定
    //============================================================
    outer.Cone.SpreadAngle = mAngle * 0.95f;
    outer.Cone.YawScale = 1.0f;
    outer.Cone.PitchScale = 0.18f;
    outer.Cone.UpBias.Min = 0.0f;
    outer.Cone.UpBias.Max = 1.8f;

    //============================================================
    // ランダム揺らぎ  
    //============================================================
    outer.RandomVelocity.Min = XMVectorSet(-2.5f, -0.7f, -2.5f, 0.0f);
    outer.RandomVelocity.Max = XMVectorSet(2.5f, 0.7f, 2.5f, 0.0f);

    //============================================================
    // 見た目設定
    //============================================================
    outer.Visual.Dark = false;
    outer.Visual.Additive = true;
    outer.Visual.SizeFade = SizeFadeMode::KeepMin;
    outer.Visual.SizeFadeMin = 0.55f;

    //============================================================
    // 吸引なし
    //============================================================
    outer.Attract.Enable = false;

    //============================================================
    // 初期状態では emit しない
    //============================================================
    outer.emit = false;
    outer.EmitRate = 60.0f;

    mOuterEmitter = std::make_unique<Emitter>(&outer);

    mOuterEmitter->setReceiveShadow(false);

    mOuterEmitter->SetShadowReceiveStrength(1.0f);

    mOuterEmitter->SetLocalLightStrength(0.20f);

    mOuterEmitter->SetLocalShadowEnabled(true);

    //============================================================
    // 氷ブレス対応
    //============================================================
    if (mType == Type::Ice)
    {
        // 今は未実装
    }

}

void BreathEffect::Update(float dt)
{
    if (!mOwner || !mCoreEmitter || !mOuterEmitter)
        return;

    //============================================================
    // ボスの口元に追従
    //============================================================
    XMMATRIX ownerW = *mOwner->getWorld();

    // 口元オフセット
    XMVECTOR localMouth = XMVectorSet(0.0f, 2.0f, 7.5f, 1.0f);
    XMVECTOR mouthWorldPos = XMVector3TransformCoord(localMouth, ownerW);

    // 平行移動だけ口元へ
    mCoreEmitter->setPosition(&mouthWorldPos);
    mOuterEmitter->setPosition(&mouthWorldPos);

    //============================================================
    // 停止済みなら新規発生なし
    //============================================================
    if (mStopped)
    {
        mCoreEmitter->setEmit(false);
        mOuterEmitter->setEmit(false);

        mCoreEmitter->execute(dt);
        mOuterEmitter->execute(dt);

        if (!mCoreEmitter->HasAliveParticles() &&
            !mOuterEmitter->HasAliveParticles())
        {
            mIsAlive = false;
        }
        return;
    }

    //============================================================
    // 生存時間カウント
    //============================================================
    mLife += dt;

    //============================================================
    // 発射中かどうか
    //============================================================
    const bool active = (mLife < mDuration);

    mCoreEmitter->setEmit(active);
    mOuterEmitter->setEmit(active);

    // 更新
    mCoreEmitter->execute(dt);
    mOuterEmitter->execute(dt);

    //============================================================
    // 寿命を過ぎたら、残留粒子が消えた時点で終了
    //============================================================
    if (!active)
    {
        if (!mCoreEmitter->HasAliveParticles() && !mOuterEmitter->HasAliveParticles())
        {
            mIsAlive = false;
        }
        return;
    }
}

void BreathEffect::Render()
{
    if (mOuterEmitter) mOuterEmitter->render();
    if (mCoreEmitter)  mCoreEmitter->render();
}

void BreathEffect::Stop()
{
    Effect::Stop();

    if (mCoreEmitter)  mCoreEmitter->setEmit(false);
    if (mOuterEmitter) mOuterEmitter->setEmit(false);
}
