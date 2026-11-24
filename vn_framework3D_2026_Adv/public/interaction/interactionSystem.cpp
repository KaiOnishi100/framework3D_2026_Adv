#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "interactionSystem.h"
#include "IInteractable.h"

//============================================================
// Register
//============================================================
void InteractionSystem::Register(IInteractable* interactable)
{
    if (interactable == nullptr)
    {
        return;
    }

    //========================================================
    // 二重登録防止
    //========================================================
    const auto iterator =
        std::find(
            mInteractables.begin(),
            mInteractables.end(),
            interactable
        );


    if (iterator != mInteractables.end())
    {
        return;
    }

    mInteractables.push_back(interactable);
}


//============================================================
// Unregister
//============================================================
void InteractionSystem::Unregister(IInteractable* interactable)
{
    mInteractables.erase(std::remove(mInteractables.begin(), mInteractables.end(), interactable), mInteractables.end());
}


//============================================================
// Find Best Target
//============================================================
IInteractable* InteractionSystem::FindBestTarget(
    const XMVECTOR& position,
    const XMVECTOR& forward,
    float maxDistance,
    float minFacingDot,
    const ExplorationPlayer* player
) const
{
    IInteractable* bestTarget = nullptr;

    float bestScore = FLT_MAX;


    //========================================================
    // Player ForwardはXZだけ使用
    //========================================================
    XMVECTOR flatForward = XMVectorSetY(forward, 0.0f);


    if (XMVectorGetX(XMVector3LengthSq(flatForward)) < 0.0001f)
    {
        return nullptr;
    }


    flatForward = XMVector3Normalize(flatForward);


    //========================================================
    // 全Interaction対象から最適なものを探す
    //========================================================
    for (IInteractable* interactable : mInteractables)
    {
        if (interactable == nullptr)
        {
            continue;
        }

        //========================================================
        // 距離や角度で候補を比較する前にこのプレイヤーが操作できるか確認する
        //========================================================
        const bool canInteract = player != nullptr
            ? interactable->CanInteractFrom(*player) : interactable->CanInteract();

        if (!canInteract)
        {
            continue;
        }

        //====================================================
        // Player → Target
        //====================================================
        XMVECTOR toTarget = interactable->GetInteractionPosition() - position;

        // 正なら足元より上 負なら足元より下に対象がある
        const float deltaY = XMVectorGetY(toTarget);

        constexpr float kMaxBelowFeet = 0.25f;
        constexpr float kMaxAboveFeet = 1.65f;

        if (deltaY < -kMaxBelowFeet || deltaY > kMaxAboveFeet)
        {
            continue;
        }

        //====================================================
        // 高さ差はInteraction方向判定では無視
        //====================================================
        XMVECTOR flatToTarget = XMVectorSetY(toTarget, 0.0f);

        const float distance = XMVectorGetX(XMVector3Length(flatToTarget));


        //====================================================
        // 距離外
        //====================================================
        if (distance > maxDistance)
        {
            continue;
        }


        if (distance < 0.0001f)
        {
            bestTarget = interactable;
            bestScore = 0.0f;

            continue;
        }

        const XMVECTOR direction = XMVector3Normalize(flatToTarget);

        //====================================================
        // PlayerがTargetを向いているか
        // dot:
        // 1.0  = 完全に正面
        // 0.0  = 真横
        // -1.0 = 真後ろ
        //====================================================
        const float facingDot = XMVectorGetX(XMVector3Dot(flatForward, direction));


        if (facingDot < minFacingDot)
        {
            continue;
        }

        if (distance < bestScore)
        {
            bestScore = distance;

            bestTarget = interactable;
        }
    }

    return bestTarget;
}