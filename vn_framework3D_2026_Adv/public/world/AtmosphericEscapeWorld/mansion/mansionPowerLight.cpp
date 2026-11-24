#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "mansionPowerLight.h"

MansionPowerLight::MansionPowerLight(const std::string& lightId, const XMFLOAT3& position)
    : mPosition(position)
    , mStableLightId(static_cast<std::uint32_t>(std::hash<std::string>{}(lightId)))
{
    // 既存の配置 ID をそのまま Point Shadow Cache の識別に使う
    if (mStableLightId == PointLightShadowManager::InvalidStableLightID)
    {
        mStableLightId = 1u;
    }
}

void MansionPowerLight::Update()
{
    if (!mPowered)
    {
        return;
    }

    AtmosphericEscapeLightBudget::PointLightRequest request{};
    request.Position = mPosition;
    request.Range = mRange;
    request.Color = mColor;
    request.Intensity = mIntensity;

    request.LimitToRoomBounds = true;
    request.CastsShadow = true;
    request.UpdateMode = PointLightShadowManager::UpdateMode::EveryFrame;
    request.Resolution = PointLightShadowManager::ShadowResolution::Medium;
    request.StableLightId = mStableLightId;

    request.ExcludedShadowCaster = mExcludedShadowCaster;

    AtmosphericEscapeLightBudget::SubmitPointLight(request);
}
