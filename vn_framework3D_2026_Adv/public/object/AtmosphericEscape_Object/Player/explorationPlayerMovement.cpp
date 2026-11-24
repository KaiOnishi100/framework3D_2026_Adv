#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "explorationPlayerMovement.h"

//============================================================
// Input
//============================================================
void PlayerMovement::AddMoveAxis( float x,float y)
{
    mMoveAxisX += x;
    mMoveAxisY += y;
}


//============================================================
// Update
//============================================================
void PlayerMovement::Update(ObjectBase& owner, float dt)
{
    dt = (std::min)(dt, 0.1f);

    //========================================================
    // 1Frame開始時の状態
    //========================================================
    mIsMoving = false;

    mCurrentSpeed = 0.0f;


    //========================================================
    // Horizontal Movement
    //========================================================
    UpdateHorizontalMovement(owner, dt);

    //========================================================
    // Gravity
    //========================================================
    UpdateGravity(owner, dt);

    //========================================================
    // Inputは1Frameのみ有効
    // 次Frameで入力がなければ自然に停止する
    //========================================================
    mMoveAxisX = 0.0f;
    mMoveAxisY = 0.0f;
}


//============================================================
// Horizontal Movement
//============================================================
void PlayerMovement::UpdateHorizontalMovement(ObjectBase& owner, float dt)
{
    //========================================================
    // Input Vector
    //========================================================
    XMVECTOR input =
        XMVectorSet(
            mMoveAxisX,
            0.0f,
            mMoveAxisY,
            0.0f
        );


    const float inputLengthSq =
        XMVectorGetX(XMVector3LengthSq(input));


    //========================================================
    // 入力なし
    //========================================================
    if (inputLengthSq < 0.0001f)
    {
        return;
    }


    //========================================================
    // Keyboard斜め入力
    // W + Dなどで速度が√2倍になるのを防ぐ
    //========================================================
    if (inputLengthSq > 1.0f)
    {
        input = XMVector3Normalize(input);
    }


    //========================================================
    // Camera Forward
    // Cameraが見ている方向を取得
    //========================================================
    XMVECTOR cameraForward = vnCamera::getForward();


    //========================================================
    // Player移動はXZ Plane上のみ
    // Cameraが上や下を向いても
    // Playerが空へ飛ばないようYを除去
    //========================================================
    cameraForward = XMVectorSetY(cameraForward, 0.0f);


    const float forwardLengthSq = XMVectorGetX(XMVector3LengthSq(cameraForward));


    //========================================================
    // Camera Forwardが取得できない場合のFallback
    //========================================================
    if (forwardLengthSq < 0.0001f)
    {
        cameraForward =
            XMVectorSet(
                0.0f,
                0.0f,
                1.0f,
                0.0f
            );
    }
    else
    {
        cameraForward = XMVector3Normalize(cameraForward);
    }


    //========================================================
    // Camera Right
    // Right = Up × Forward
    //========================================================
    const XMVECTOR up =
        XMVectorSet(
            0.0f,
            1.0f,
            0.0f,
            0.0f
        );


    XMVECTOR cameraRight = XMVector3Cross(up, cameraForward);

    cameraRight = XMVector3Normalize(cameraRight);


    //========================================================
    // Input Space
    //        ↓
    // World Space
    //========================================================
    XMVECTOR moveDirection =
        XMVectorScale(cameraRight, XMVectorGetX(input)) +
        XMVectorScale(cameraForward, XMVectorGetZ(input));

    moveDirection = XMVectorSetY(moveDirection, 0.0f);

    const float directionLengthSq = XMVectorGetX(XMVector3LengthSq(moveDirection));


    if (directionLengthSq < 0.0001f)
    {
        return;
    }

    moveDirection = XMVector3Normalize(moveDirection);

    //========================================================
    // Walk / Run Speed / Crouch
    //========================================================
    float speed = mSettings.WalkSpeed;

    if (mCrouching)
    {
        //========================================================
        // しゃがみ歩行
        //========================================================
        speed = mSettings.CrouchSpeed;
    }
    else if (mSprint)
    {
        //========================================================
        // 通常Sprint
        //========================================================
        speed = mSettings.RunSpeed;
    }

    //========================================================
    // Movement State
    // Animation Controllerがこの情報を見る
    //========================================================
    mIsMoving = true;

    mCurrentSpeed = speed;

    //============================================================
    // このFrameのHorizontal移動距離
    //============================================================
    const float horizontalMoveDistance = speed * dt;


    //============================================================
    // Step-Up判定
    // Step-Up可能: Yを先にStep上面まで持ち上げる
    // Step-Up不可:
    //      Positionは変更しない
    //      通常Collisionで壁として処理される
    //============================================================
    TryStepUp(owner, moveDirection, horizontalMoveDistance);

    //========================================================
    // Position更新
    //========================================================
    const XMVECTOR delta =
        XMVectorScale(
            moveDirection,
            speed * dt
        );

    owner.addPosition(&delta);

    //========================================================
    // Movement Directionへ向きを変更
    //========================================================
    UpdateFacingDirection(
        owner,
        moveDirection,
        dt
    );
}


//============================================================
// Facing Direction
//============================================================
void PlayerMovement::UpdateFacingDirection(ObjectBase& owner, const XMVECTOR& moveDirection, float dt)
{
    const float x = XMVectorGetX(moveDirection);
    const float z = XMVectorGetZ(moveDirection);


    //========================================================
    // +Z Forward基準でYaw計算
    //========================================================
    float desiredYaw = atan2f(x, z);


    //========================================================
    // ModelごとのForward差を吸収
    //========================================================
    desiredYaw += mSettings.ModelForwardYawOffset;


    //========================================================
    // Target Quaternion
    //========================================================
    XMVECTOR targetQuaternion =
        XMQuaternionRotationRollPitchYaw(
            0.0f,
            desiredYaw,
            0.0f
        );

    targetQuaternion = XMQuaternionNormalize(targetQuaternion);

    //========================================================
    // Current Quaternion
    //========================================================
    XMVECTOR currentQuaternion = *owner.getQuaternion();

    currentQuaternion = XMQuaternionNormalize(currentQuaternion);

    //========================================================
    // FrameRate Independent補間
    //========================================================
    const float blend = 1.0f - expf(-mSettings.RotationSharpness * dt);


    XMVECTOR result =
        XMQuaternionSlerp(
            currentQuaternion,
            targetQuaternion,
            blend
        );


    result = XMQuaternionNormalize(result);

    owner.setQuaternion(&result);
}

bool PlayerMovement::TryStepUp(ObjectBase& owner, const XMVECTOR& moveDirection, float horizontalMoveDistance)
{
    //========================================================
    // Colliderが無ければ判定不可
    //========================================================
    if (mStepColliders == nullptr)
    {
        return false;
    }


    //========================================================
    // 空中ではStep-Upしない
    // Jump / Fall中に壁へ接触しただけで
    // 勝手に上へ登るのを防ぐ
    //========================================================
    if (!mGrounded)
    {
        return false;
    }


    //========================================================
    // 上昇中もStep-Upしない
    //========================================================
    if (mVerticalSpeed > 0.01f)
    {
        return false;
    }


    //========================================================
    // Player Position
    // 現在のCapsule設定では
    // Object Position Y ≒ 足元Yとして使用できる
    //========================================================
    const XMVECTOR playerPosition = *owner.getPosition();


    const float playerX =
        XMVectorGetX(playerPosition);

    const float playerY =
        XMVectorGetY(playerPosition);

    const float playerZ =
        XMVectorGetZ(playerPosition);


    //========================================================
    // Horizontal Direction
    //========================================================
    XMVECTOR horizontalDirection = XMVectorSetY(moveDirection, 0.0f);

    const float directionLengthSq = XMVectorGetX(XMVector3LengthSq(horizontalDirection));

    if (directionLengthSq <= 0.000001f)
    {
        return false;
    }

    horizontalDirection = XMVector3Normalize(horizontalDirection);

    //========================================================
    // Capsule Radius
    // Player中心からRayを飛ばすため
    // Capsule表面までのRadius分も含める
    //========================================================
    constexpr float PlayerRadius = 0.35f;

    //========================================================
    // 足元Probe高さ
    //========================================================
    constexpr float LowProbeHeight = 0.08f;

    //========================================================
    // Forward Probe Distance
    //========================================================
    const float probeDistance =
        horizontalMoveDistance +
        PlayerRadius +
        mSettings.StepProbeExtraDistance;


    //========================================================
    // Low Ray
    // 段差の垂直面を探す
    //========================================================
    const XMVECTOR lowRayOrigin =
        XMVectorSet(
            playerX,
            playerY + LowProbeHeight,
            playerZ,
            1.0f
        );


    float nearestLowHit = FLT_MAX;

    const Collider* stepCollider = nullptr;

    //========================================================
    // 前方に低いObstacleがあるか調べる
    //========================================================
    for (const Collider* collider : *mStepColliders)
    {
        if (collider == nullptr)
        {
            continue;
        }


        //====================================================
        // Disabled Colliderは無視
        // 開いているDoor等
        //====================================================
        if (!collider->IsEnabled())
        {
            continue;
        }


        //====================================================
        // Player自身を無視
        //====================================================
        if (collider->GetOwner() == &owner)
        {
            continue;
        }

        float hitDistance = 0.0f;

        if (!collider->RayCast(
            lowRayOrigin,
            horizontalDirection,
            probeDistance,
            hitDistance
        ))
        {
            continue;
        }


        //====================================================
        // 一番手前にあるObstacleだけを見る
        //====================================================
        if (hitDistance < nearestLowHit)
        {
            nearestLowHit = hitDistance;
            stepCollider = collider;
        }
    }


    //========================================================
    // 前方にObstacleなし
    //========================================================
    if (stepCollider == nullptr)
    {
        return false;
    }


    //========================================================
    // High Ray
    //========================================================
    const float highProbeY =
        playerY + mSettings.MaxStepHeight + 0.05f;


    const XMVECTOR highRayOrigin =
        XMVectorSet(
            playerX,
            highProbeY,
            playerZ,
            1.0f
        );


    //========================================================
    // High Rayが何かにHitするか
    //========================================================
    for (const Collider* collider : *mStepColliders)
    {
        if (collider == nullptr || !collider->IsEnabled())
        {
            continue;
        }


        if (collider->GetOwner() == &owner)
        {
            continue;
        }

        float hitDistance = 0.0f;

        if (collider->RayCast(
            highRayOrigin,
            horizontalDirection,
            probeDistance,
            hitDistance
        ))
        {
            //================================================
            // LowでもHighでもHit
            // → 高さのあるWallなのでStep-Upしない
            //================================================
            return false;
        }
    }


    //========================================================
    // Step上面を探す
    // Low Rayで当たった垂直面より
    // 少し奥へ入った位置から下向きRayを飛ばす
    //========================================================
    const float landingForwardDistance =
        nearestLowHit + mSettings.StepLandingProbeInset;


    const XMVECTOR landingHorizontal =
        XMVectorScale(
            horizontalDirection,
            landingForwardDistance
        );


    //========================================================
    // MaxStepHeightより少し高い位置からRay Down
    //========================================================
    constexpr float DownProbeMargin = 0.10f;

    XMVECTOR downRayOrigin =
        XMVectorAdd(playerPosition, landingHorizontal);


    downRayOrigin =
        XMVectorSetY(downRayOrigin, playerY + mSettings.MaxStepHeight + DownProbeMargin);


    const XMVECTOR downDirection =
        XMVectorSet(
            0.0f,
            -1.0f,
            0.0f,
            0.0f
        );


    const float downProbeDistance =
        mSettings.MaxStepHeight + DownProbeMargin + 0.05f;


    float nearestDownHit = FLT_MAX;

    bool foundLanding = false;


    //========================================================
    // 最も高い位置から下へRayを飛ばしているので、
    // 最初にHitしたSurfaceが着地候補
    //========================================================
    for (const Collider* collider : *mStepColliders)
    {
        if (collider == nullptr || !collider->IsEnabled())
        {
            continue;
        }


        if (collider->GetOwner() == &owner)
        {
            continue;
        }

        float hitDistance = 0.0f;

        if (!collider->RayCast(
            downRayOrigin,
            downDirection,
            downProbeDistance,
            hitDistance
        ))
        {
            continue;
        }


        if (hitDistance < nearestDownHit)
        {
            nearestDownHit = hitDistance;
            foundLanding = true;
        }
    }


    if (!foundLanding)
    {
        return false;
    }


    //========================================================
    // Ray開始Y - Hit距離
    //      ↓
    // Step上面のWorld Y
    //========================================================
    const float landingY = XMVectorGetY(downRayOrigin) - nearestDownHit;

    //========================================================
    // Player足元からの段差高さ
    //========================================================
    const float stepHeight = landingY - playerY;

    //========================================================
    // 上方向の段差だけ対象
    // Floorそのものや下降段差ではStep-Upしない
    //========================================================
    constexpr float MinStepHeight = 0.02f;

    if (stepHeight < MinStepHeight)
    {
        return false;
    }


    //========================================================
    // 最大段差を超えている
    //========================================================
    if (stepHeight > mSettings.MaxStepHeight + 0.001f)
    {
        return false;
    }


    //========================================================
    // Step-Up成立
    //========================================================
    owner.addPosition(
        0.0f,
        stepHeight + 0.002f,
        0.0f
    );

    //========================================================
    // Step-Up直後に以前の下降速度を持ち越すと
    // 同Frameで強く下へ戻されるためReset
    //========================================================
    if (mVerticalSpeed < 0.0f)
    {
        mVerticalSpeed = 0.0f;
    }

    return true;
}


//============================================================
// Gravity
//============================================================
void PlayerMovement::UpdateGravity(ObjectBase& owner, float dt)
{
    //========================================================
    // 接地済みかつ落下中なら落下速度を一度0へ
    //========================================================
    if (mGrounded && mVerticalSpeed < 0.0f)
    {
        mVerticalSpeed = 0.0f;
    }


    //========================================================
    // Gravity
    //========================================================
    mVerticalSpeed += mSettings.Gravity * dt;


    //========================================================
    // Terminal Velocity
    //========================================================
    if (mVerticalSpeed < mSettings.MaxFallSpeed)
    {
        mVerticalSpeed = mSettings.MaxFallSpeed;
    }


    //========================================================
    // Vertical Movement
    //========================================================
    owner.addPosition(
        0.0f,
        mVerticalSpeed * dt,
        0.0f
    );
}