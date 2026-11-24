#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "playerInteraction.h"
#include "explorationPlayer.h"

#include "../../../interaction/IInteractable.h"
#include "../../../interaction/interactionSystem.h"

//============================================================
// Update
//============================================================
void PlayerInteraction::Update(ExplorationPlayer& player)
{
    //========================================================
    // InteractionSystemが無ければ何もしない
    //========================================================
    if (mSystem == nullptr)
    {
        mCurrentTarget = nullptr;
        mInteractRequested = false;

        return;
    }


    //========================================================
    // Player Position
    //========================================================
    const XMVECTOR position = *player.getPosition();


    //========================================================
    // Player Forward
    //========================================================
    const XMVECTOR localForward =
        XMVectorSet(
            0.0f,
            0.0f,
            1.0f,
            0.0f
        );


    const XMVECTOR rotation = *player.getQuaternion();

    XMVECTOR forward = XMVector3Rotate(localForward, rotation);

    if (player.IsFirstPerson())
    {
        //========================================================
        // 一人称では、立ち止まって視点だけ動かしたときも
        // 画面が向いている水平方向で対象を探す
        //========================================================
        const XMVECTOR viewForward = vnCamera::getForward();
        const XMVECTOR horizontal = XMVectorSetY(viewForward, 0.0f);

        //========================================================
        // ほぼ真上・真下を向いたときのゼロベクトル正規化を避ける
        //========================================================
        if (XMVectorGetX(XMVector3LengthSq(horizontal)) > 0.000001f)
        {
            forward = XMVector3Normalize(horizontal);
        }
    }

    //========================================================
    // 現在操作できるTargetを探す
    //========================================================
    mCurrentTarget =
        mSystem->FindBestTarget(
            position,
            forward,
            mMaxDistance,
            mMinFacingDot,
            &player
        );


    //========================================================
    // Interact入力が無ければTarget確認だけで終了
    //========================================================
    if (!mInteractRequested)
    {
        return;
    }


    //========================================================
    // Requestは1Frameで消費
    //========================================================
    mInteractRequested = false;


    //========================================================
    // Target無し
    //========================================================
    if (mCurrentTarget == nullptr)
    {
        return;
    }


    //========================================================
    // 実行
    //========================================================
    if (mCurrentTarget->CanInteract() && mCurrentTarget != nullptr && mCurrentTarget->CanInteractFrom(player))
    {
        mCurrentTarget->Interact(player);
    }
}