#pragma once

class Emitter;

// 扉が開いた瞬間に、敷居のワールド座標から埃を1回放出する。
class DoorDustEffect final : public Effect
{
public:
    explicit DoorDustEffect(const XMVECTOR& thresholdWorldPos, float scale = 1.0f);
    ~DoorDustEffect() override;

    void Update(float dt) override;
    void Render() override;
    void Stop() override;

private:
    std::unique_ptr<Emitter> mEmitter;
    bool mEmitted = false;
};
