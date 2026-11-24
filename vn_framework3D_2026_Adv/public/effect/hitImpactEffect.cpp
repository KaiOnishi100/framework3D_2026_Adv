#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"hitImpactEffect.h"

HitImpactEffect::HitImpactEffect(Character* owner,
    float sizeScale,
    const XMVECTOR& colorMin,
    const XMVECTOR& colorMax)
    : Effect(owner)
{
    //========================================================
    // 発生位置を固定
    // 壁に当たった瞬間の場所に残したい
    //========================================================
    if (mOwner)
    {
        mSpawnPos = *mOwner->getPosition();
    }

    //========================================================
    // Burst（球状バースト）
    //========================================================
    Emitter::stEmitterDesc burst = {};
    swprintf_s(burst.Texture, L"%s", L"data/image/effect/glow.png");

    // 基本
    burst.Shape = EmitShape::Sphere;
    burst.EmitCount = 24;

    // 寿命
    burst.Life.Min = 0.12f;
    burst.Life.Max = 0.24f;

    // サイズ
    burst.Size.Min = 0.8f * sizeScale;
    burst.Size.Max = 1.8f * sizeScale;

    // 速度
    burst.Speed.Min = 30.0f * sizeScale;
    burst.Speed.Max = 60.0f * sizeScale;

    // 色
    burst.Color.Min = colorMin;
    burst.Color.Max = colorMax;

    // 見た目
    burst.Visual.Dark = false;
    burst.Visual.Additive = true;
    burst.Visual.SizeFade = SizeFadeMode::KeepMin;
    burst.Visual.SizeFadeMin = 0.35f;

    // 吸引なし
    burst.Attract.Enable = false;

    // 最初は emit しない
    burst.emit = false;

    mBurst = std::make_unique<Emitter>(&burst);

    if (mBurst)
    {
        mBurst->setPositionX(XMVectorGetX(mSpawnPos));
        mBurst->setPositionY(XMVectorGetY(mSpawnPos));
        mBurst->setPositionZ(XMVectorGetZ(mSpawnPos));
        mBurst->SyncTransform();
    }

    //========================================================
    // Ring（衝撃波リング）
    // ・外へ一瞬だけ広がる輪
    // ・Dust を使って円周から外向きへ飛ばす
    // ・Burst より少し薄くして補助演出にする
    //========================================================
    Emitter::stEmitterDesc ring = {};
    swprintf_s(ring.Texture, L"%s", L"data/image/effect/glow.png");

    //========================================================
    // 形状
    // 外へ広げるため Dust を使う
    //========================================================
    ring.Shape = EmitShape::Dust;
    ring.Circle.Radius = 0.8f * sizeScale;

    //========================================================
    // 発生数
    // リングとして見えるくらいの数
    //========================================================
    ring.EmitCount = 18;

    //========================================================
    // 寿命
    // Burst より少しだけ長くして余韻を残す
    //========================================================
    ring.Life.Min = 0.16f;
    ring.Life.Max = 0.28f;

    //========================================================
    // サイズ
    // 粒は小さめ
    //========================================================
    ring.Size.Min = 0.45f * sizeScale;
    ring.Size.Max = 0.95f * sizeScale;

    //========================================================
    // 速度
    // Burst より少し速めにすると衝撃波感が出る
    //========================================================
    ring.Speed.Min = 36.0f * sizeScale;
    ring.Speed.Max = 68.0f * sizeScale;

    //========================================================
    // 色
    //========================================================
    XMVECTOR ringColorMin = XMVectorSet(
        (std::min)(1.0f, XMVectorGetX(colorMin) + 0.20f),
        (std::min)(1.0f, XMVectorGetY(colorMin) + 0.20f),
        (std::min)(1.0f, XMVectorGetZ(colorMin) + 0.20f),
        0.75f
    );

    XMVECTOR ringColorMax = XMVectorSet(
        (std::min)(1.0f, XMVectorGetX(colorMax) + 0.25f),
        (std::min)(1.0f, XMVectorGetY(colorMax) + 0.25f),
        (std::min)(1.0f, XMVectorGetZ(colorMax) + 0.25f),
        0.95f
    );

    ring.Color.Min = ringColorMin;
    ring.Color.Max = ringColorMax;

    //========================================================
    // 見た目
    //========================================================
    ring.Visual.Dark = false;
    ring.Visual.Additive = true;
    ring.Visual.SizeFade = SizeFadeMode::KeepMin;
    ring.Visual.SizeFadeMin = 0.50f;

    //========================================================
    // 吸引なし
    //========================================================
    ring.Attract.Enable = false;

    //========================================================
    // 最初は emit しない
    //========================================================
    ring.emit = false;

    mRing = std::make_unique<Emitter>(&ring);

    if (mRing)
    {
        mRing->setPositionX(XMVectorGetX(mSpawnPos));
        mRing->setPositionY(XMVectorGetY(mSpawnPos));
        mRing->setPositionZ(XMVectorGetZ(mSpawnPos));
        mRing->SyncTransform();
    }
}

void HitImpactEffect::Update(float dt)
{
    if (!mBurst || !mRing) return;

    mLife += dt;

    //========================================================
    // 初回だけ1回放出
    //========================================================
    if (!mEmitted)
    {
        mBurst->EmitOnce();
        mRing->EmitOnce();
        mEmitted = true;
    }

    //========================================================
    // 更新
    // 発生位置は固定のまま
    //========================================================
    mBurst->execute(dt);
    mRing->execute(dt);

    //========================================================
    // 目安時間を過ぎて、両方の粒が消えたら終了
    //========================================================
    if (mLife >= mDuration)
    {
        const bool burstAlive = mBurst->HasAliveParticles();
        const bool ringAlive = mRing->HasAliveParticles();

        if (!burstAlive && !ringAlive)
        {
            mIsAlive = false;
        }
    }
}

void HitImpactEffect::Render()
{
    if (mRing)  mRing->render();
    if (mBurst) mBurst->render();
}
