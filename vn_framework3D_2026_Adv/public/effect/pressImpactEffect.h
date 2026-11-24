#pragma once

class PressImpactEffect : public Effect
{
public:
    PressImpactEffect(Character* owner, float durationSec = 1.2f);
    void Update(float dt) override;
    void Render() override;

private:
    std::unique_ptr<Emitter> mRing;   // 輪っか土煙
    std::unique_ptr<Emitter> mChunks; // 土塊

    float mDuration = 1.2f;
    float mLife = 0.0f;
    float mEmitTime = 0.08f;  // 最初だけ放出
};