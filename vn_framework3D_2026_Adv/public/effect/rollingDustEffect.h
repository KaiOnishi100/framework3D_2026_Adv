#pragma once

class RollingDustEffect : public Effect
{
public:
	enum class Mode
	{
		Spin,      // その場高速回転
		Rolling    // 移動しながら
	};

	RollingDustEffect(Character* owner, float durationSec, Mode mode);
	~RollingDustEffect() override = default;

	void Update(float dt) override;
	void Render() override;

	void Stop()override;
	void SetOpacity(float a) override;

private:
	std::unique_ptr<Emitter> mEmitter;
	float mDuration = 0.0f;
	float mLife = 0.0f;

	float mYOffset = 0.2f;        // 地面からちょい上
	float mBackwardBias = 0.7f;   // 後ろへ流す強さ
	Mode mMode;
};