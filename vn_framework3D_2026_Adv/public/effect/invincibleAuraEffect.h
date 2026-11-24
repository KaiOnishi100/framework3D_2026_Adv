#pragma once

class InvincibleAuraEffect : public Effect
{
public:
	InvincibleAuraEffect(Character* owner, float durationSec);
	void Update(float dt) override;
	void Render() override;
private:
	std::unique_ptr<Emitter> mEmitter;
	float mDuration = 0.0f;
	float mLife = 0.0f;
};