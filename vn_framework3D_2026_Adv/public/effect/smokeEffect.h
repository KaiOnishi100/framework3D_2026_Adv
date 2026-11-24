#pragma once

class SmokeEffect : public Effect
{
public:
    SmokeEffect(Character* owner, float durationSec = 1.5f);
    ~SmokeEffect() override = default;

    void Update(float dt) override;
    void Render() override;

private:
    std::unique_ptr<Emitter> mEmitter;

    float mDuration = 1.5f;
    float mLife = 0.0f;
};