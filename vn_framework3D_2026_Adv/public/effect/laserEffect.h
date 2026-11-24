#pragma once

class LaserBeamEffect : public Effect
{
public:
    LaserBeamEffect(Character* owner, float durationSec, const LaserQuad::Desc& quadDesc)
        : Effect(owner), mDuration(durationSec)
    {
        mQuad = std::make_unique<LaserQuad>(quadDesc);
    }

    void SetSegment(const XMVECTOR& s, const XMVECTOR& e)
    {
        mStart = s; mEnd = e;
    }

    void Update(float dt) override
    {
        if (!mOwner || !mQuad) return;

        mLife += dt;
        if (mLife >= mDuration)
        {
            mIsAlive = false;
            return;
        }

        // 毎フレーム線分更新 → quad更新
        mQuad->SetSegment(mStart, mEnd);
        mQuad->execute(dt);
    }

    void Render() override
    {
        if (mQuad) mQuad->render();
    }

private:
    std::unique_ptr<LaserQuad> mQuad;
    float mDuration = 0.0f;
    float mLife = 0.0f;
    XMVECTOR mStart = XMVectorZero();
    XMVECTOR mEnd = XMVectorSet(0, 0, 1, 0);
};