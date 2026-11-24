#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"shockwaveEffect.h"

ShockwaveEffect::ShockwaveEffect(
    Character* owner,
    float groundY,
    float radiusMax,
    float expandSpeed,
    float scale)
    : Effect(owner)
    , mGroundY(groundY)
    , mRadiusMax(radiusMax)
    , mExpandSpeed(expandSpeed)
    , mScale(scale)
{
    //========================================================
    // 発生位置を固定
    // ・ボスが後から動いても、衝撃波の中心はズレない
    //========================================================
    if (mOwner)
    {
        mSpawnPos = *mOwner->getPosition();
    }

    // 地面に少しだけ近づける
    mSpawnPos = XMVectorSetY(mSpawnPos, mGroundY + 0.05f);

    //========================================================
    // 中心フラッシュ
    //========================================================
    {
        Emitter::stEmitterDesc desc = {};
        swprintf_s(desc.Texture, L"%s", L"data/image/effect/particle001.png");

        desc.Life.Min = 8.0f;
        desc.Life.Max = 12.0f;

        desc.Size.Min = 2.5f * mScale;
        desc.Size.Max = 4.5f * mScale;

        desc.Speed.Min = 0.0f;
        desc.Speed.Max = 0.0f;

        desc.Color.Min = XMVectorSet(0.85f, 0.95f, 1.00f, 0.65f);
        desc.Color.Max = XMVectorSet(1.00f, 1.00f, 1.00f, 0.90f);

        desc.Shape = EmitShape::Circle;
        desc.Circle.Radius = 0.5f * mScale;

        desc.EmitCount = 20;

        desc.Visual.Additive = true;
        desc.Visual.Dark = false;
        desc.Visual.SizeFade = SizeFadeMode::KeepMin;
        desc.Visual.SizeFadeMin = 0.35f;

        mFlash = std::make_unique<Emitter>(&desc);
        mFlash->setPosition(&mSpawnPos);
        mFlash->setEmit(false);
    }

    //========================================================
    // 外周ダストリング
    // ・広がる円周上から粉塵を立てる
    //========================================================
    {
        Emitter::stEmitterDesc desc = {};
        swprintf_s(desc.Texture, L"%s", L"data/image/effect/smoke.png");

        desc.Life.Min = 0.25f;
        desc.Life.Max = 0.45f;

        desc.Size.Min = 1.2f * mScale;
        desc.Size.Max = 2.2f * mScale;

        // Dust / Press は速度が見た目にかなり効く
        desc.Speed.Min = 5.0f;
        desc.Speed.Max = 10.0f;

        // 白すぎず、土煙すぎず
        desc.Color.Min = XMVectorSet(0.70f, 0.78f, 0.85f, 0.28f);
        desc.Color.Max = XMVectorSet(0.92f, 0.96f, 1.00f, 0.42f);

        //====================================================
        // 円周から外へ少し飛ばしつつ上にも上げる
        //====================================================
        desc.Shape = EmitShape::Press;
        desc.Circle.Radius = 1.0f; // 毎フレ Update で上書き
        desc.Press.Up.Min = 2.5f;
        desc.Press.Up.Max = 5.5f;

        desc.EmitCount = 6;

        desc.Visual.Additive = true;
        desc.Visual.Dark = false;
        desc.Visual.SizeFade = SizeFadeMode::KeepMin;
        desc.Visual.SizeFadeMin = 0.45f;

        mDustRing = std::make_unique<Emitter>(&desc);
        mDustRing->setPosition(&mSpawnPos);
        mDustRing->setEmit(false);
    }

}

//============================================================
// 更新
//============================================================
void ShockwaveEffect::Update(float dt)
{
    if (!mIsAlive) return;

    //====================================================
    // 停止中は余韻だけ流す
    //====================================================
    if (mStopped)
    {
        if (mFlash)
        {
            mFlash->setEmit(false);
            mFlash->execute(dt);
        }

        if (mDustRing)
        {
            mDustRing->setEmit(false);
            mDustRing->execute(dt);
        }

        const bool flashAlive = (mFlash && mFlash->HasAliveParticles());
        const bool dustAlive = (mDustRing && mDustRing->HasAliveParticles());

        if (!flashAlive && !dustAlive)
        {
            mIsAlive = false;
        }
        return;
    }

    //====================================================
    // 発生位置は毎フレ固定
    // ・Owner を追わない
    //====================================================
    mSpawnPos = XMVectorSetY(mSpawnPos, mGroundY + 0.05f);

    mLife += dt;

    //====================================================
    // 半径を広げる
    // ・最大半径に到達したらそこで止める
    //====================================================
    mRadius += mExpandSpeed * dt;
    if (mRadius > mRadiusMax)
    {
        mRadius = mRadiusMax;
    }

    //====================================================
    // 中心フラッシュ
    //====================================================
    if (!mFlashEmitted && mFlash)
    {
        mFlash->EmitOnce();
        mFlashEmitted = true;
    }

    if (mFlash)
    {
        float t = 1.0f - std::clamp(mLife / 0.30f, 0.0f, 1.0f);
        mFlash->SetAlphaMul(t);
        mFlash->setPosition(&mSpawnPos);
        mFlash->execute(dt);
    }

    //====================================================
    // ダストリング
    // ・広がる円周上に粉塵を出す
    // ・半径が大きいほどまばらに見えるので、
    //   EmitCount / Size を半径連動で補正する
    //====================================================
    if (mDustRing)
    {
        //--------------------------------------------
        // 円周半径を更新
        //--------------------------------------------
        mDustRing->SetCircleRadius(mRadius);

        //--------------------------------------------
        // 円周の長さから EmitCount を決める
        // ・約 7.0f ごとに 1粒 くらいの感覚
        // ・下限と上限を付けて重くなりすぎないようにする
        //--------------------------------------------
        float circumference = XM_2PI * mRadius;

        int emitCount = static_cast<int>(circumference / 7.0f);

        // 出しすぎ防止
        emitCount = std::clamp(emitCount, 6, 12);
        mDustRing->SetEmitCount(emitCount);

        //--------------------------------------------
        // 半径が大きいほど粒サイズを少しだけ上げる
        // ・後半のスカスカ感を軽減する
        //--------------------------------------------
        float radiusT = 0.0f;
        if (mRadiusMax > 0.0001f)
        {
            radiusT = std::clamp(mRadius / mRadiusMax, 0.0f, 1.0f);
        }

        // 後半ほど 1.0 → 1.55 倍
        float sizeBoost = 1.0f + radiusT * 0.55f;

        mDustRing->SetSizeRange(
            1.2f * mScale * sizeBoost,
            2.2f * mScale * sizeBoost
        );

        //--------------------------------------------
        // α
        // ・時間で減らしつつ、後半でも少し見えるように補正
        //--------------------------------------------
        float lifeT = std::clamp(mLife / mMaxLife, 0.0f, 1.0f);
        float alpha = 1.0f - lifeT;

        // 後半でも完全に弱くなりすぎないよう少し底上げ
        float alphaBoost = 0.75f + radiusT * 0.20f;

        mDustRing->SetAlphaMul(alpha * alphaBoost);

        //--------------------------------------------
        // 実行
        //--------------------------------------------
        mDustRing->setPosition(&mSpawnPos);
        mDustRing->setEmit(true);
        mDustRing->execute(dt);
    }

    //====================================================
    // 5秒で終了
    //====================================================
    if (mLife >= mMaxLife)
    {
        Stop();
    }
}

void ShockwaveEffect::Render()
{
    if (mFlash) mFlash->render();
    if (mDustRing) mDustRing->render();
}

//============================================================
// 停止
//============================================================
void ShockwaveEffect::Stop()
{
    mStopped = true;

    if (mFlash)     mFlash->setEmit(false);
    if (mDustRing)  mDustRing->setEmit(false);
}