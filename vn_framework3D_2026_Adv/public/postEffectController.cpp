#include "../framework.h"
#include "../framework/vn_environment.h"

#include "postEffectController.h"
#include "../framework/postProcessManager.h"

//============================================================
// 初期化
//============================================================
void PostEffectController::Initialize()
{
	//============================================================
	// 通常時の値
	//============================================================
	mBaseValue.brightness = 1.04f;
	mBaseValue.contrast = 1.18f;
	mBaseValue.saturation = 1.15f;

	mBaseValue.vignetteStart = 0.38f;
	mBaseValue.vignetteEnd = 0.80f;
	mBaseValue.vignetteStrength = 0.35f;

	mBaseValue.chromaticStrength = 0.002f;

	//============================================================
	// 現在値も通常値から開始
	//============================================================
	mCurrentValue = mBaseValue;
	mPeakValue = mBaseValue;

	mTimer = 0.0f;
	mDuration = 0.0f;

	//============================================================
	// 初期値をPostProcessManagerへ反映
	//============================================================
	ApplyValue(mCurrentValue);
}

//============================================================
// 更新
//============================================================
void PostEffectController::Update(float dt)
{
	//============================================================
	// 演出中でない場合は通常値を維持
	//============================================================
	if (mTimer <= 0.0f || mDuration <= 0.0f)
	{
		mCurrentValue = mBaseValue;
		ApplyValue(mCurrentValue);
		return;
	}

	//============================================================
	// タイマーを減らす
	//============================================================
	mTimer -= dt;

	if (mTimer < 0.0f)
	{
		mTimer = 0.0f;
	}

	//============================================================
	// ratio
	// 1.0 : 演出開始直後
	// 0.0 : 通常状態
	//============================================================
	float ratio = 0.0f;

	if (mDuration > 0.0f)
	{
		ratio = mTimer / mDuration;
	}

	ratio = Clamp01(ratio);

	//============================================================
	// 戻り方を少し滑らかにする
	// ratioそのままだと直線的に戻る
	// ratio * ratio にすると、最初は強く残って
	// 後半で自然に通常値へ戻る
	//============================================================
	float t = ratio * ratio;

	//============================================================
	// Base値とPeak値を補間
	// t = 1.0 に近いほどPeak
	// t = 0.0 に近いほどBase
	//============================================================
	mCurrentValue = LerpValue(
		mBaseValue,
		mPeakValue,
		t
	);

	ApplyValue(mCurrentValue);
}

//============================================================
// 被弾演出
//============================================================
void PostEffectController::PlayDamageEffect()
{
	PostEffectValue value = mBaseValue;

	//============================================================
	// 被弾時は、
	// ・彩度を少し落とす
	// ・コントラストを上げる
	// ・Vignetteを強くする
	// ・色収差を強くする
	//============================================================
	value.brightness = 0.98f;
	value.contrast = 1.35f;
	value.saturation = 0.85f;

	value.vignetteStart = 0.25f;
	value.vignetteEnd = 0.78f;
	value.vignetteStrength = 0.85f;

	value.chromaticStrength = 0.015f;

	StartImpulse(value, 0.35f);
}

//============================================================
// パリィ成功演出
//============================================================
void PostEffectController::PlayParryEffect()
{
	PostEffectValue value = mBaseValue;

	//============================================================
	// パリィ成功時は、
	// 一瞬だけ画面を派手にする
	//============================================================
	value.brightness = 1.12f;
	value.contrast = 1.55f;
	value.saturation = 1.35f;

	value.vignetteStart = 0.32f;
	value.vignetteEnd = 0.82f;
	value.vignetteStrength = 0.55f;

	value.chromaticStrength = 0.018f;

	StartImpulse(value, 0.22f);
}

//============================================================
// ダッシュ演出
//============================================================
void PostEffectController::PlayDashEffect()
{
	PostEffectValue value = mBaseValue;

	//============================================================
	// ダッシュ時は軽めにする
	// 常時出ても邪魔にならない程度
	//============================================================
	value.brightness = 1.05f;
	value.contrast = 1.22f;
	value.saturation = 1.18f;

	value.vignetteStart = 0.35f;
	value.vignetteEnd = 0.82f;
	value.vignetteStrength = 0.42f;

	value.chromaticStrength = 0.007f;

	StartImpulse(value, 0.25f);
}

//============================================================
// 通常値変更
//============================================================
void PostEffectController::SetBaseValue(const PostEffectValue& value)
{
	mBaseValue = value;

	//============================================================
	// 演出中でなければ即反映
	//============================================================
	if (mTimer <= 0.0f)
	{
		mCurrentValue = mBaseValue;
		ApplyValue(mCurrentValue);
	}
}

//============================================================
// 一時演出開始
//============================================================
void PostEffectController::StartImpulse(
	const PostEffectValue& peakValue,
	float duration
)
{
	mPeakValue = peakValue;
	mDuration = duration;
	mTimer = duration;

	//============================================================
	// 演出開始直後は最大値を即反映
	//============================================================
	mCurrentValue = mPeakValue;
	ApplyValue(mCurrentValue);
}

//============================================================
// PostProcessManagerへ反映
//============================================================
void PostEffectController::ApplyValue(
	const PostEffectValue& value
)
{
	//============================================================
	// ColorGrading
	//============================================================
	PostProcessManager::SetBrightness(value.brightness);
	PostProcessManager::SetContrast(value.contrast);
	PostProcessManager::SetSaturation(value.saturation);

	//============================================================
	// Vignette
	//============================================================
	PostProcessManager::SetVignette(
		value.vignetteStart,
		value.vignetteEnd,
		value.vignetteStrength
	);

	//============================================================
	// Chromatic Aberration
	//============================================================
	PostProcessManager::SetChromaticStrength(
		value.chromaticStrength
	);
}

//============================================================
// 0.0 ～ 1.0 に制限
//============================================================
float PostEffectController::Clamp01(float value)
{
	if (value < 0.0f)
	{
		return 0.0f;
	}

	if (value > 1.0f)
	{
		return 1.0f;
	}

	return value;
}

//============================================================
// 線形補間
//============================================================
float PostEffectController::Lerp(
	float a,
	float b,
	float t
)
{
	return a + (b - a) * t;
}

//============================================================
// PostEffectValue補間
//============================================================
PostEffectValue PostEffectController::LerpValue(
	const PostEffectValue& a,
	const PostEffectValue& b,
	float t
)
{
	PostEffectValue result;

	result.brightness =
		Lerp(a.brightness, b.brightness, t);

	result.contrast =
		Lerp(a.contrast, b.contrast, t);

	result.saturation =
		Lerp(a.saturation, b.saturation, t);

	result.vignetteStart =
		Lerp(a.vignetteStart, b.vignetteStart, t);

	result.vignetteEnd =
		Lerp(a.vignetteEnd, b.vignetteEnd, t);

	result.vignetteStrength =
		Lerp(a.vignetteStrength, b.vignetteStrength, t);

	result.chromaticStrength =
		Lerp(a.chromaticStrength, b.chromaticStrength, t);

	return result;
}