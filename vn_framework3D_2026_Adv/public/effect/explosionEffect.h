#pragma once

class ExplosionEffect : public Effect
{
public:
    // scale：爆発の大きさ係数（プレイヤー 1.0 / ボス 3.0 など）
    // lifetimeScale：煙の残り時間調整
    ExplosionEffect(Character* owner, float scale = 1.0f, float lifetimeScale = 1.0f);
    ~ExplosionEffect() override = default;

    void Update(float dt) override;
    void Render() override;

private:
    // 爆発が起きた位置（死亡した瞬間の座標を固定したいので保持）
    XMVECTOR mSpawnPos = XMVectorZero();

    // 2段構成
    std::unique_ptr<Emitter> mFlash;   // 白っぽい閃光（広がる）
    std::unique_ptr<Emitter> mCore;    // オレンジの核（とにかく目立つ）
    std::unique_ptr<Emitter> mSparks;  // 火花（輪郭と勢い）
    std::unique_ptr<Emitter> mSmoke;   // 煙（余韻）

    // 生成直後に1回だけEmitするため
    bool mEmitted = false;

    // 演出用スケール
    float mScale = 1.0f;
    float mLifeScale = 1.0f;
};