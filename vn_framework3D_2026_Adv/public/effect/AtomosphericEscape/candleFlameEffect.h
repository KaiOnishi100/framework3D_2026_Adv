#pragma once
#pragma once

class CandleFlameEffect final : public Effect
{
public:
    explicit CandleFlameEffect(std::unique_ptr<Emitter> emitter)
        : Effect(nullptr), mEmitter(std::move(emitter))
    {
        mIsAlive = (mEmitter != nullptr);

        // まずはフル解像度で炎を描画し、芯の位置と輪郭を確認する
        SetRenderPath(EffectRenderPath::FullResolution);
    }

    void Update(float dt) override
    {
        // エミッターの更新は、この Effect を経由して毎フレーム1回行う
        if (IsAlive()) mEmitter->execute(dt);
    }

    void Render() override
    {
        if (IsAlive())
            mEmitter->render();
    }

    void Stop() override
    {
        Effect::Stop();

        // 今回の試作では停止時に即消灯し次のManager更新で破棄する
        mIsAlive = false;
    }

private:
    std::unique_ptr<Emitter> mEmitter;
};