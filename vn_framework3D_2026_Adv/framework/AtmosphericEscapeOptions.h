#pragma once

struct IXAudio2MasteringVoice;
class ShadowMap;

// タイトルとゲームシーンで共有する 設定ゲームのセーブデータとは独立 
class AtmosphericEscapeOptions final
{
public:
    enum class Quality : int { Low, Medium, High };

    // PostProcessManagerのビット値とは独立した 画面で使う並び順
    enum class Effect : int
    {
        Bloom, Fxaa, Vignette, ColorGrading, ChromaticAberration,
        LensFlare, MotionBlur, VolumetricFog, SssDiffusion, Ssgi, Count
    };

    static AtmosphericEscapeOptions& Get();

    bool IsEffectEnabled(Effect effect) const;
    void SetEffectEnabled(Effect effect, bool enabled);

    int masterVolume = 100;                  // 0～100%
    Quality fogQuality = Quality::Medium;
    Quality ssgiQuality = Quality::Medium;
    Quality directionalShadow = Quality::Medium;
    Quality spotShadow = Quality::High;
    Quality pointShadow = Quality::High;

    int fovYDegrees = 55;                     // 縦FOV 45～80度
    int mouseSensitivity = 100;              // 元の感度に対する倍率（%）
    int padSensitivity = 100;
    bool invertLookY = false;

    // Mastering Voiceの所有権はvnXAudio2に残す 破棄の直前にnullptrを渡す
    void BindMasterVoice(IXAudio2MasteringVoice* voice);
    void ApplyAudio() const;

    // 描画デバイスおよび当該シーンのシャドウマップが準備できた後に呼ぶ
    void ApplyPostEffects() const;
    void ApplyShadows(ShadowMap* directionalMap) const;
    void ApplyCamera() const;

private:
    bool mEffects[static_cast<int>(Effect::Count)] =
    {
        true, true, true, true, true, false, false, true, false, true
    };
    IXAudio2MasteringVoice* mMasterVoice = nullptr;
};
