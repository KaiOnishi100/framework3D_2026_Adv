#pragma once

class HitImpactEffect : public Effect
{
public:
    // sizeScaleで強さ調整
    HitImpactEffect(Character* owner,
        float sizeScale,
        const XMVECTOR& colorMin,
        const XMVECTOR& colorMax);

    void Update(float dt) override;
    void Render() override;

private:
    //========================================================
    // 2層構成
    //========================================================
    std::unique_ptr<Emitter> mBurst; // 球状に弾ける本体
    std::unique_ptr<Emitter> mRing;  // 一瞬の衝撃波リング

    //========================================================
    // 発生位置固定
    //========================================================
    XMVECTOR mSpawnPos = XMVectorZero();

    //========================================================
    // 初回1回だけ Emit するためのフラグ
    //========================================================
    bool mEmitted = false;

    //========================================================
    // エフェクト全体の寿命目安
    //========================================================
    float mLife = 0.0f;
    float mDuration = 0.35f;
};