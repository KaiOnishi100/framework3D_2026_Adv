#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "playerFlashlight.h"

//============================================================
// Update
//============================================================
void PlayerFlashlight::Update(ObjectBase& owner, bool crouching, const XMMATRIX* attachedBoneWorld)
{
    //========================================================
    // Player Root Position
    //========================================================
    const XMVECTOR ownerPosition = *owner.getPosition();


    //========================================================
    // Player Quaternion
    //========================================================
    XMVECTOR rotation = *owner.getQuaternion();

    rotation = XMQuaternionNormalize(rotation);


    //========================================================
    // Local Basis
    //========================================================
    const XMVECTOR localForward =
        XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    //========================================================
    // PlayerのQuaternionでWorld方向へ変換
    //========================================================
    XMVECTOR forward =
        XMVector3Rotate(
            localForward,
            rotation
        );

    forward = XMVectorSetY(forward, 0.0f);

    const float forwardLengthSq = XMVectorGetX(XMVector3LengthSq(forward));

    if (forwardLengthSq > 0.000001f)
    {
        forward = XMVector3Normalize(forward);
    }
    else
    {
        forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }

    //========================================================
    // World Up
    //========================================================
    const XMVECTOR up =
        XMVectorSet(
            0.0f,
            1.0f,
            0.0f,
            0.0f
        );

    //============================================================
    // Horizontal Right
    //============================================================
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));

    const float heightOffset =
        crouching ? mSettings.CrouchHeightOffset : mSettings.HeightOffset;

    //========================================================
    // Flashlight World Position
    //========================================================
    XMVECTOR flashlightPosition = ownerPosition;


    flashlightPosition += XMVectorScale(up, heightOffset);

    flashlightPosition += XMVectorScale(forward, mSettings.ForwardOffset);

    flashlightPosition += XMVectorScale(right, mSettings.SideOffset);

    //============================================================
    // Bone Attached Flashlight Position
    //============================================================
    if (attachedBoneWorld != nullptr && mVisualModel != nullptr)
    {
        //========================================================
        // モデルのBone Local Transform
        //========================================================
        const XMVECTOR localPosition =
            *mVisualModel->getPosition();

        const XMVECTOR localScale =
            *mVisualModel->getScale();

        const XMVECTOR localRotation =
            XMQuaternionNormalize(*mVisualModel->getQuaternion());

        //========================================================
        // Model Local Matrix
        // Scale * Rotation * Translation
        //========================================================
        const XMMATRIX modelLocal =
            XMMatrixScalingFromVector(localScale) *
            XMMatrixRotationQuaternion(localRotation) *
            XMMatrixTranslationFromVector(localPosition);

        //========================================================
        // Model World Matrix
        //========================================================
        const XMMATRIX modelWorld = modelLocal * (*attachedBoneWorld);

        //========================================================
        // 光源のLocal位置
        //========================================================
        const XMVECTOR localLightOrigin = XMVectorZero();

        //========================================================
        // Local -> World
        //========================================================
        flashlightPosition =
            XMVector3TransformCoord(
                localLightOrigin,
                modelWorld
            );
    }

    SubmitSpotLight(flashlightPosition, forward);
}

void PlayerFlashlight::SubmitSpotLight(const XMVECTOR& flashlightPosition, const XMVECTOR& forward) const
{
    if (!mEnabled)
    {
        return;
    }

    //========================================================
    // XMVECTOR
    //     ↓
    // XMFLOAT3
    // LocalLightManager APIへ渡す
    //========================================================
    XMFLOAT3 position;

    XMFLOAT3 direction;


    XMStoreFloat3(
        &position,
        flashlightPosition
    );


    XMStoreFloat3(
        &direction,
        forward
    );


    //========================================================
    // Flashlight Spot Light
    AtmosphericEscapeLightBudget::SpotLightRequest request;

    //============================================================
    // Flashlight Transform
    //============================================================
    request.Position = position;
    request.Direction = direction;

    //============================================================
    // Lighting
    //============================================================
    request.Range = mSettings.Range;
    request.Color = mSettings.Color;
    request.Intensity = mSettings.Intensity;
    request.InnerAngleDegrees = mSettings.InnerAngleDegrees;
    request.OuterAngleDegrees = mSettings.OuterAngleDegrees;

    //============================================================
    // Flashlightは常にSpot Shadow最優先
    //============================================================
    request.CastsShadow = true;
    request.ForceShadow = true;

    //============================================================
    // Flashlight Shadow品質
    //============================================================
    request.PCFRadius = 1.0f;
    request.MinimumBias = 0.0003f;
    request.SlopeBias = 0.0020f;

    request.ExcludedShadowCaster = mShadowExcludedCaster;

    AtmosphericEscapeLightBudget::SubmitSpotLight(request);

}
