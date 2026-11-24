#include "../framework.h"
#include "../framework/vn_environment.h"

#include "slowSystem.h"

void SlowSystem::StartJustAvoidSlow(float scale)
{
    if (mActive && mJustAvoid) return;
    mActive = true;
    mJustAvoid = true;
    mScale = std::clamp(scale, 0.05f, 1.0f);

    mAttackOnly = true;     // © ƒXƒ[’†‚ÍUŒ‚ˆÈŠO—}§
    mWaitingHit = true;     // © –½’†‰ðœ‘Ò‚¿
    if (auto* snd = SoundBank::getInstance())
    {
        snd->StartLoop(SoundID::SE_SlowMotion, 1.5f);
    }
    vnScene::StartHitStop(0.05f);
}

void SlowSystem::EndJustAvoidByHit()
{
    if (!(mActive && mJustAvoid && mWaitingHit)) return;
    if (auto* snd = SoundBank::getInstance())
    {
        snd->StopLoop(SoundID::SE_SlowMotion);
    }
    End();
}

void SlowSystem::End()
{
    mActive = false;
    mJustAvoid = false;
    mScale = 1.0f;

    mAttackOnly = false;
    mWaitingHit = false;
}
