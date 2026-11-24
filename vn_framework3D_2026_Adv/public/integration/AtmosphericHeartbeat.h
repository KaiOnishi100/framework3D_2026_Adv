#pragma once

#include <algorithm>
#include <cmath>

// プレイヤーに聞こえる心音用 追跡者の聴覚やNoiseSystemから独立させる
class AtmosphericHeartbeat
{
public:
    enum class State
    {
        Quiet,
        Investigate,
        Chase,
        Search
    };

    // dtはゲームプレイの経過秒 EmitBeat(float volume)で心音を1拍だけ再生する
    float Update(float dt, State state)
    {
        if (!std::isfinite(dt) || dt <= 0.0f)
        {
            return 0.0f;
        }

        float targetBpm = 68.0f;
        float targetVolume = 0.0f;
        switch (state)
        {
        case State::Quiet:
            break;
        case State::Investigate:
            targetBpm = 80.0f;
            targetVolume = 0.75f;
            break;
        case State::Chase:
            targetBpm = 132.0f;
            targetVolume = 1.0f;
            break;
        case State::Search:
            targetBpm = 100.0f;
            targetVolume = 0.74f;
            break;
        }

        // 復帰直後などに長いdtが来ても一度に拍を進めすぎない
        const float elapsed = (std::min)(dt, 0.1f);
        const float rate = 1.0f - std::exp(-4.0f * elapsed);

        if (mVolume < 0.01f && targetVolume > 0.0f)
        {
            // 無音から鳴り始めるとき最初の拍を約0.2秒後に入れる
            mPhase = 0.73f;
        }

        // 状態が変わっても再生位置を巻き戻さず拍の間隔と音量だけを補間する
        mBpm += (targetBpm - mBpm) * rate;
        mVolume += (targetVolume - mVolume) * rate;
        if (mVolume < 0.01f)
        {
            return 0.0f;
        }

        mPhase += mBpm * elapsed / 60.0f;

        if (mPhase < 1.0f)
        {
            return 0.0f;
        }

        mPhase -= 1.0f;
        return mVolume;
    }

    void Reset()
    {
        mBpm = 68.0f;
        mVolume = 0.0f;
        mPhase = 0.0f;
    }

private:
    float mBpm = 68.0f;
    float mVolume = 0.0f;
    float mPhase = 0.0f;
};
