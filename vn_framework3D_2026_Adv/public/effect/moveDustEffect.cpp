#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"moveDustEffect.h"

MoveDustEffect::MoveDustEffect(Character* owner, float scale, float durationSec) : Effect(owner), mDuration(durationSec)
{
    Emitter::stEmitterDesc desc = {};
    swprintf_s(desc.Texture, L"%s", L"data/image/effect/smoke.png");

    //========================================================
    // 発生形状
    //========================================================
    desc.Shape = EmitShape::Dust;
    desc.Circle.Radius = owner ? owner->GetRadius() * 1.05f * scale : 1.0f * scale;

    //========================================================
    // 発生数
    //========================================================
    desc.EmitCount = (std::max)(1, (int)(6 * scale));

    //========================================================
    // 寿命
    //========================================================
    desc.Life.Min = 0.25f;
    desc.Life.Max = 0.45f;

    //========================================================
    // サイズ
    //========================================================
    desc.Size.Min = 0.6f * scale;
    desc.Size.Max = 1.4f * scale;

    //========================================================
    // 速度
    //========================================================
    desc.Speed.Min = 6.0f * scale;
    desc.Speed.Max = 14.0f * scale;

    //========================================================
    // 色
    // 暗めの砂色
    //========================================================
    desc.Color.Min = XMVectorSet(0.12f, 0.10f, 0.08f, 0.70f);
    desc.Color.Max = XMVectorSet(0.22f, 0.18f, 0.14f, 0.55f);

    //========================================================
    // 見た目
    // 土埃なので通常ブレンド寄り
    //========================================================
    desc.Visual.Dark = true;
    desc.Visual.Additive = false;
    desc.Visual.SizeFade = SizeFadeMode::KeepMin;
    desc.Visual.SizeFadeMin = 0.65f;

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

void MoveDustEffect::Update(float dt)
{
    if (!mOwner || !mEmitter) return;

    mLife += dt;

    //========================================================
    // 最初のごく短時間だけ放出
    //========================================================
    const bool emitNow = (mLife < 0.02f);
    mEmitter->setEmit(emitNow);

    //========================================================
    // 足元のワールド座標を作る
    //========================================================
    XMVECTOR foot = *mOwner->getPosition();

    //========================================================
    // 半径ぶん下げて、少しだけ上に戻す
    //========================================================
    foot += XMVectorSet(
        0.0f,
        -mOwner->GetRadius() + 0.2f,
        0.0f,
        0.0f
    );

    //========================================================
    // エミッター位置を足元へ移動
    //========================================================
    mEmitter->setPosition(&foot);

    //========================================================
    // エミッター更新
    //========================================================
    mEmitter->execute(dt);

    //========================================================
    // 発生時間を過ぎたら、新規放出は止める
    // 残粒子が全部消えたら終了
    //========================================================
    if (mLife >= mDuration)
    {
        mEmitter->setEmit(false);

        if (!mEmitter->HasAliveParticles())
        {
            mIsAlive = false;
        }
    }
}

void MoveDustEffect::Render()
{
    if (mEmitter) mEmitter->render();
}
