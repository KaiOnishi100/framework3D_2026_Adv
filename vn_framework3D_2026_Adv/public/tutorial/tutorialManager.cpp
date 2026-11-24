#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "tutorialManager.h"

void TutorialManager::Update(float dt, Boss* boss, Player* player)
{
    if (mState == State::Disabled || mState == State::Done) return;
    if (!boss || !player) return;

    //--------------------------------------------------------
    // 点滅：停止中でも動かす（dtはSceneのdtを使う）
    //--------------------------------------------------------
    mBlinkT += dt * 7.0f; // 点滅速度
    mBlinkAlpha = 0.35f + 0.65f * (0.5f + 0.5f * sinf(mBlinkT)); // 0.35～1.0

    //--------------------------------------------------------
    // ステート進行
    //--------------------------------------------------------
    switch (mState)
    {
    case State::WaitingIntroEnd:
        // Intro終了をBossのフラグで受け取る
        if (boss->IsIntroFinished())
            mState = State::WaitingNearHit;
        break;

    case State::WaitingNearHit:
        // 確定ローリング中だけ判定
        if (boss->IsInGuaranteedRoll())
        {
            if (IsNearHit(boss, player))
            {
                // 「押すまで止めたい」ので長めの時間を入れる
                vnScene::StartHitStop(9999.0f);
                mState = State::PausedForDodge;
            }
        }
        break;

    case State::PausedForDodge:
        // 回避押したら解除
        if (IsDodgeTriggered())
        {
            vnScene::SetHitStop(false);
            mState = State::Done;
        }
        break;

    default:
        break;
    }
}

bool TutorialManager::IsNearHit(Boss* boss, Player* player) const
{
    //====================================================
     // 当たる直前判定
     //====================================================
    const float bossR = boss->GetRadius();
    const float playerR = player->GetRadius();

    const float margin = 5.0f; // 早めに止めたいなら大きく

    XMVECTOR b = *boss->getPosition();
    XMVECTOR p = *player->getPosition();

    XMVECTOR d = p - b;
    float dist = XMVectorGetX(XMVector3Length(d));

    return dist <= (bossR + playerR + margin);
}

bool TutorialManager::IsDodgeTriggered() const 
{
    // キーボード：左Shift
    bool kb = vnKeyboard::trg(DIK_LSHIFT);

    // パッド：B
    bool pad = (Pad::isValid(0) && Pad::trg(PadButton::B, 0));

    return kb || pad;
}