#include "../framework.h"
#include "vn_environment.h"
#include "AtmosphericEscapeOptions.h"

namespace
{
    using Effect = AtmosphericEscapeOptions::Effect;
    using Quality = AtmosphericEscapeOptions::Quality;
    using Post = PostProcessManager;

    constexpr Post::eEffectMask kPostMasks[] =
    {
        Post::Bloom, Post::Fxaa, Post::Vignette, Post::ColorGrading,
        Post::ChromaticAberration, Post::LensFlare, Post::MotionBlur,
        Post::VolumetricFog, Post::SSSDiffusion, Post::SSGI
    };
    static_assert(sizeof(kPostMasks) / sizeof(kPostMasks[0]) == static_cast<int>(Effect::Count));

    constexpr int kFogSamples[] = {16, 32, 48};
    constexpr SSGIPass::Quality kSsgiQuality[] =
    {
        SSGIPass::Quality::Low, SSGIPass::Quality::Medium, SSGIPass::Quality::High
    };

    // 左から、近・中・遠カスケード それぞれ何フレームに一度更新するか
    constexpr unsigned int kCascadeIntervals[][3] =
    {
        {1, 3, 6}, {1, 2, 4}, {1, 1, 2}
    };
    constexpr unsigned int kSpotBudgets[] = {1, 2, 4};
    constexpr PointLightShadowManager::ShadowResolution kPointCaps[] =
    {
        PointLightShadowManager::ShadowResolution::Low,
        PointLightShadowManager::ShadowResolution::Medium,
        PointLightShadowManager::ShadowResolution::High
    };
}

AtmosphericEscapeOptions& AtmosphericEscapeOptions::Get()
{
    static AtmosphericEscapeOptions settings;
    return settings;
}

bool AtmosphericEscapeOptions::IsEffectEnabled(Effect effect) const
{
    return mEffects[static_cast<int>(effect)];
}

void AtmosphericEscapeOptions::SetEffectEnabled(Effect effect, bool enabled)
{
    mEffects[static_cast<int>(effect)] = enabled;
}

void AtmosphericEscapeOptions::BindMasterVoice(IXAudio2MasteringVoice* voice)
{
    mMasterVoice = voice;
    ApplyAudio();
}

void AtmosphericEscapeOptions::ApplyAudio() const
{
    if (mMasterVoice)
    {
        // 再生中のループや3D SEにも、再生を中断せず全体音量が反映される
        mMasterVoice->SetVolume((std::clamp)(masterVolume, 0, 100) / 100.0f);
    }
}

void AtmosphericEscapeOptions::ApplyPostEffects() const
{
    for (int i = 0; i < static_cast<int>(Effect::Count); ++i)
    {
        const auto mask = kPostMasks[i];

        // 変更のないON/OFFには触らず、Disableの履歴リセットを必要なときだけ行う
        if (Post::IsEnabled(mask) == mEffects[i]) continue;
        if (mEffects[i]) Post::Enable(mask);
        else Post::Disable(mask);
    }

    // PostProcessManagerに追加するGetterで、別項目変更時の履歴消去を防ぐ
    const auto ssgi = kSsgiQuality[static_cast<int>(ssgiQuality)];
    if (Post::GetSSGIQuality() != ssgi)
    {
        Post::SetSSGIQuality(ssgi); // 内部でSSGIの履歴をリセットする
    }

    const int fogSamples = kFogSamples[static_cast<int>(fogQuality)];
    if (Post::GetVolumetricFogSampleCount() != fogSamples)
    {
        Post::SetVolumetricFogSampleCount(fogSamples);
        Post::ResetVolumetricFogTemporalHistory();
    }
}

void AtmosphericEscapeOptions::ApplyShadows(ShadowMap* directionalMap) const
{
    if (directionalMap)
    {
        const auto& intervals = kCascadeIntervals[static_cast<int>(directionalShadow)];

        // 現在のエンジンで使っている3カスケード 差分だけ適用する
        for (unsigned int i = 0; i < 3; ++i)
        {
            if (directionalMap->GetCascadeUpdateInterval(i) != intervals[i])
            {
                directionalMap->SetCascadeUpdateInterval(i, intervals[i]);
            }
        }
    }

    // 次にライトを登録するフレームから反映される
    SpotLightShadowManager::SetShadowLightBudget(kSpotBudgets[static_cast<int>(spotShadow)]);
    PointLightShadowManager::SetResolutionCap(kPointCaps[static_cast<int>(pointShadow)]);
}

void AtmosphericEscapeOptions::ApplyCamera() const
{
    // ポーズ画面を表示中でもFOVの変化を背後の画面で確認できるようにする
    vnCamera::setFovY(XMConvertToRadians(static_cast<float>(fovYDegrees)));
}
