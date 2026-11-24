#pragma once

//============================================================
// Effectの描画経路
//============================================================
enum class EffectRenderPath
{
    //========================================================
    // 通常のFull Resolution
    //========================================================
    FullResolution,

    //========================================================
    // 1/2 Resolutionで描画してMain HDRへ加算する
    //========================================================
    HalfResolutionAdditive
};

class Effect
{
protected:
    bool mIsAlive = true;
    bool mStopped = false;
    float mOpacity = 1.0f;
    Character* mOwner;
public:
    Effect(Character* owner) : mOwner(owner) {};
    virtual ~Effect() = default;

    virtual void Update(float dt) = 0;
    virtual void Render() = 0;

    virtual void SetOpacity(float a) { mOpacity = std::clamp(a, 0.0f, 1.0f); }

    virtual void Stop() { mStopped = true; }

    bool IsAlive() const { return mIsAlive; }
    bool IsStopped() const { return mStopped; }
    Character* GetOwner() const { return mOwner; }

    //============================================================
    // 描画経路取得
    //============================================================
    EffectRenderPath GetRenderPath() const { return mRenderPath; }

    //============================================================
    // 描画経路設定
    //============================================================
    void SetRenderPath(EffectRenderPath renderPath) { mRenderPath = renderPath; }

private:
    EffectRenderPath mRenderPath = EffectRenderPath::FullResolution;
};