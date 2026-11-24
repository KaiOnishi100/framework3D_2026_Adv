#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"chargeCompleteEffect.h"

ChargeCompleteEffect::ChargeCompleteEffect(Character* owner)
    : Effect(owner)
{
    //============================================================
    // Flash（中心フラッシュ）
    //============================================================
    Emitter::stEmitterDesc flash = {};
    swprintf_s(flash.Texture, L"%s", L"data/image/effect/blueCircle.png");

    // 形状
    flash.Shape = EmitShape::Sphere;

    // 発生数
    flash.EmitCount = 40;

    // 寿命
    flash.Life.Min = 0.08f;
    flash.Life.Max = 0.12f;

    // サイズ
    flash.Size.Min = 6.0f;
    flash.Size.Max = 10.0f;

    // 速度
    // ほぼ中心フラッシュなので動かさない
    flash.Speed.Min = 0.0f;
    flash.Speed.Max = 0.0f;

    // 色
    flash.Color.Min = XMVectorSet(1.0f, 0.3f, 1.0f, 1.0f);
    flash.Color.Max = XMVectorSet(1.0f, 0.6f, 1.0f, 1.0f);

    // 見た目
    flash.Visual.Dark = false;
    flash.Visual.Additive = true;
    flash.Visual.SizeFade = SizeFadeMode::KeepMin;
    flash.Visual.SizeFadeMin = 0.70f;

    // 吸引なし
    flash.Attract.Enable = false;

    // 初回だけ出す
    flash.emit = true;

    mFlash = std::make_unique<Emitter>(&flash);

    //============================================================
    // Ring（衝撃波リング）
    //============================================================
    Emitter::stEmitterDesc ring = {};
    swprintf_s(ring.Texture, L"%s", L"data/image/effect/ring.png");

    // 形状
    // Circle だと今の実装では速度ゼロ固定なので
    // 衝撃波として外へ飛ばすため Dust を使う
    ring.Shape = EmitShape::Dust;
    ring.Circle.Radius = 1.0f;

    // 発生数
    ring.EmitCount = 24;

    // 寿命
    ring.Life.Min = 0.40f;
    ring.Life.Max = 0.55f;

    // サイズ
    ring.Size.Min = 1.2f;
    ring.Size.Max = 2.0f;

    // 速度
    ring.Speed.Min = 25.0f;
    ring.Speed.Max = 35.0f;

    // 色
    ring.Color.Min = XMVectorSet(1.0f, 0.8f, 1.0f, 0.8f);
    ring.Color.Max = XMVectorSet(1.0f, 0.8f, 1.0f, 1.0f);

    // 見た目
    ring.Visual.Dark = false;
    ring.Visual.Additive = true;
    ring.Visual.SizeFade = SizeFadeMode::KeepMin;
    ring.Visual.SizeFadeMin = 0.55f;

    // 吸引なし
    ring.Attract.Enable = false;

    // 初回だけ出す
    ring.emit = true;

    mRing = std::make_unique<Emitter>(&ring);
}

void ChargeCompleteEffect::Update(float dt)
{
    if (!mOwner || !mFlash || !mRing) return;

    mTime += dt;

    //============================================================
    // 発生位置
    //============================================================
    XMMATRIX ownerW = *mOwner->getWorld();

    XMVECTOR localOffset = XMVectorSet(0.0f, 8.0f, 0.0f, 1.0f);
    XMVECTOR worldPos = XMVector3TransformCoord(localOffset, ownerW);

    //============================================================
    // World行列ではなく位置だけ渡す
    //============================================================
    mFlash->setPosition(&worldPos);
    mRing->setPosition(&worldPos);

    //============================================================
    // 1フレームだけ emit
    //============================================================
    if (mTime > dt)
    {
        mFlash->setEmit(false);
        mRing->setEmit(false);
    }

    //============================================================
    // 更新
    //============================================================
    mFlash->execute(dt);
    mRing->execute(dt);

    //============================================================
    // 残粒子が消えたら終了
    // mTotalDuration を過ぎても粒が残っていたら待つ
    //============================================================
    if (mTime > mTotalDuration)
    {
        if (!mFlash->HasAliveParticles() &&
            !mRing->HasAliveParticles())
        {
            mIsAlive = false;
        }
    }
}

void ChargeCompleteEffect::Render()
{
    if (mFlash) mFlash->render();
    if (mRing)  mRing->render();
}