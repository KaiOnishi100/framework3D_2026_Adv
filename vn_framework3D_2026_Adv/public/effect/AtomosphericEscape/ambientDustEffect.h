#pragma once

class Emitter;

// 屋敷の一角に置く常設の浮遊塵worldPos は塵が漂う範囲の中心
class AmbientDustEffect final : public Effect
{
public:
    explicit AmbientDustEffect(const XMVECTOR& worldPos, float scale = 1.0f);
    ~AmbientDustEffect() override;

    void Update(float dt) override;
    void Render() override;
    void Stop() override;

private:
    std::unique_ptr<Emitter> mEmitter;
};
