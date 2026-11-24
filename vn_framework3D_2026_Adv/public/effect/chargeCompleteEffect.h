#pragma once

class ChargeCompleteEffect : public Effect
{
public:
    explicit ChargeCompleteEffect(Character* owner);
    ~ChargeCompleteEffect() override = default;

    void Update(float dt) override;
    void Render() override;

private:
    std::unique_ptr<Emitter> mFlash; // 中心フラッシュ
    std::unique_ptr<Emitter> mRing;  // リング（衝撃波）

    float mTime = 0.0f;

    // それぞれの寿命（秒）
    const float mFlashDuration = 0.15f;
    const float mRingDuration = 0.50f;
    const float mTotalDuration = 0.60f; // 全体が消える時間
};