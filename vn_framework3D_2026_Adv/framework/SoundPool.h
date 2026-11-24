#pragma once

class SoundPool
{
public:
    // pathのwavを count 個ロードしてプールする
    SoundPool(const WCHAR* path, int count);
    // ワンショット再生（連打対応）
    void Play(float volume = 1.0f);
    // 全停止
    void StopAll();

    // SoundPool
    void ApplySpatialMix(const vnSound::SpatialMix& mix)
    {
        for (auto& voice : mVoices) voice->ApplySpatialMix(mix);
    }

private:
    std::vector<std::unique_ptr<vnSound>> mVoices;
    int mIndex = -1;
};