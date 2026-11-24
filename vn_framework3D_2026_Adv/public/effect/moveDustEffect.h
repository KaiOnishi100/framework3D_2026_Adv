#pragma once

class MoveDustEffect : public Effect
{
public:
    MoveDustEffect(Character* owner, float scale, float durationSec = 0.35f);
    void Update(float dt) override;

    void Render() override;

private:
    std::unique_ptr<Emitter> mEmitter;
    float mDuration = 0.35f;
    float mLife = 0.0f;
};