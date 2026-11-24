#include "../framework.h"
#include "vn_environment.h"

SoundPool::SoundPool(const WCHAR* path, int count)
{
    if (count <= 0)
    {
        return;
    }

    mVoices.reserve(count);

    for (int i = 0; i < count; ++i)
    {
        auto voice = std::make_unique<vnSound>(path);

        // 同じWAVのロードに失敗したら残りのボイス生成を打ち切る
        // すでに生成できたボイスがあれば引き続き使える
        if (!voice->IsLoaded())
        {
            break;
        }

        mVoices.emplace_back(std::move(voice));
    }
}

void SoundPool::Play(float volume)
{
    if (mVoices.empty()) return;

    // ラウンドロビンで次の音を鳴らす
    mIndex = (mIndex + 1) % (int)mVoices.size();

    // 音量調整して再生
    mVoices[mIndex]->setVolume(volume);
    mVoices[mIndex]->play(true);
}

void SoundPool::StopAll()
{
    for (auto& v : mVoices) v->stop();
}
