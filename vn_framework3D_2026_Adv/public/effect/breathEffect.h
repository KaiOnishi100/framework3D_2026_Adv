#pragma once

class BreathEffect : public Effect
{
public:
    enum class Type
    {
        FireYellow,
        Ice
    };

    BreathEffect(Character* owner, Type type, float range, float angleRad,float lifeTime);
    ~BreathEffect() override = default;

    void Update(float dt) override;
    void Render() override;

    void Stop()override;

private:
    // 中心の強い発光
    std::unique_ptr<Emitter> mCoreEmitter;

    // 外側の広がり
    std::unique_ptr<Emitter> mOuterEmitter;



    Type  mType;
    float mDuration = 0.0f;
    float mLife = 0.0f;

    float mRange = 0.0f;
    float mAngle = 0.0f;
};