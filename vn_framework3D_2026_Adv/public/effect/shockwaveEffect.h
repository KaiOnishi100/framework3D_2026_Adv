#pragma once

class ShockwaveEffect : public Effect
{
public:
    //========================================================
    // radiusMax     : 最大半径
    // expandSpeed   : 広がる速度
    // scale         : 全体スケール
    //========================================================
    ShockwaveEffect(
        Character* owner,
        float groundY,
        float radiusMax = 45.0f,
        float expandSpeed = 55.0f,
        float scale = 1.0f
    );

    ~ShockwaveEffect() override = default;

    void Update(float dt) override;
    void Render() override;
    void Stop() override;

private:
    //========================================================
    // 発生位置（着地時点で固定）
    //========================================================
    XMVECTOR mSpawnPos = XMVectorZero();

    //========================================================
    // 進行パラメータ
    //========================================================
    float mGroundY = 0.0f;
    float mRadius = 0.0f;
    float mRadiusMax = 45.0f;
    float mExpandSpeed = 55.0f;
    float mLife = 0.0f;
    float mMaxLife = 1.2f;
    float mScale = 1.0f;

    //========================================================
    // 着地フラッシュ
    //========================================================
    std::unique_ptr<Emitter> mFlash;

    //========================================================
    // 外周ダストリング
    //========================================================
    std::unique_ptr<Emitter> mDustRing;

    //========================================================
    // 1回だけ出す制御
    //========================================================
    bool mFlashEmitted = false;
};