#pragma once

class ChargeEffect : public Effect
{
public:
    ChargeEffect(Character* owner);
    ~ChargeEffect() override = default;

    void Update(float dt) override;
    void Render() override;

private:
    // 外側リング
    std::unique_ptr<Emitter> mOuterEmitter;

    // 内側リング
    std::unique_ptr<Emitter> mInnerEmitter;

    float mLifeTime = 0.0f;
    float mDuration = 2.0f;
};