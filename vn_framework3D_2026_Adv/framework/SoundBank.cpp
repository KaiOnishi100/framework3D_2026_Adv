#include "../framework.h"
#include "vn_environment.h"

#include "SoundBank.h"

SoundBank* SoundBank::sInstance = nullptr;

namespace
{
    struct AtmosphericSfxEntry
    {
        SoundID id;
        const WCHAR* path;
        int voiceCount;
    };

    // 足音など前の音が鳴り終わる前に次の音が来るSEだけ複数用意する
    constexpr AtmosphericSfxEntry kAtmosphericSfx[] =
    {
        { SoundID::AE_Footstep,     L"data/mySound/AtmosphericEscape/Footstep.wav",     3 },
        { SoundID::AE_Flashlight,   L"data/mySound/AtmosphericEscape/Flashlight.wav",   2 },
        { SoundID::AE_DoorOpen,     L"data/mySound/AtmosphericEscape/DoorOpen.wav",     2 },
        { SoundID::AE_DoorClose,    L"data/mySound/AtmosphericEscape/DoorClose.wav",    2 },
        { SoundID::AE_Pickup,       L"data/mySound/AtmosphericEscape/Pickup.wav",       2 },
        { SoundID::AE_PowerRestore, L"data/mySound/AtmosphericEscape/PowerRestore.wav", 1 },
        { SoundID::AE_PursuerWake,  L"data/mySound/AtmosphericEscape/PursuerWake.wav",  1 },
        { SoundID::AE_Interact,      L"data/mySound/AtmosphericEscape/Interact.wav",       2 },
        { SoundID::AE_PursuerBreath, L"data/mySound/AtmosphericEscape/PursuerBreath.wav",  1 },
        { SoundID::AE_PursuerAttack, L"data/mySound/AtmosphericEscape/PursuerAttack.wav",  1 },
        { SoundID::AE_PursuerStep,   L"data/mySound/AtmosphericEscape/PursuerStep.wav",    2 },
        { SoundID::AE_PursuerStep2,  L"data/mySound/AtmosphericEscape/PursuerStep_2.wav",  1 },
        { SoundID::AE_PursuerStep3,  L"data/mySound/AtmosphericEscape/PursuerStep_3.wav",  1 },
        { SoundID::AE_Footstep2,     L"data/mySound/AtmosphericEscape/Footstep_2.wav",     1 },
        { SoundID::AE_Footstep3,     L"data/mySound/AtmosphericEscape/Footstep_3.wav",     1 },
        { SoundID::AE_FootstepWood,    L"data/mySound/AtmosphericEscape/Footstep_Wood.wav", 3 },
        { SoundID::AE_FootstepConcrete,L"data/mySound/AtmosphericEscape/Footstep_Concrete.wav", 3 },
        { SoundID::AE_PlayerRun,     L"data/mySound/AtmosphericEscape/PlayerRun.wav",      2 },
        { SoundID::AE_PlayerRun2,    L"data/mySound/AtmosphericEscape/PlayerRun_2.wav",    1 },
        { SoundID::AE_PlayerRun3,    L"data/mySound/AtmosphericEscape/PlayerRun_3.wav",    1 },
        { SoundID::AE_PlayerHit,     L"data/mySound/AtmosphericEscape/PlayerHit.wav",      2 },
        { SoundID::AE_PlayerDeath,   L"data/mySound/AtmosphericEscape/PlayerDeath.wav",    1 },
        { SoundID::AE_DocumentPage,  L"data/mySound/AtmosphericEscape/DocumentPage.wav",  2 },
        { SoundID::AE_FusePickup,    L"data/mySound/AtmosphericEscape/FusePickup.wav",    2 },
        { SoundID::AE_FuseSet,       L"data/mySound/AtmosphericEscape/FuseSet.wav",       2 },
        { SoundID::AE_PuzzleInteract,L"data/mySound/AtmosphericEscape/PuzzleInteract.wav",2 },
        { SoundID::AE_PuzzleError,   L"data/mySound/AtmosphericEscape/PuzzleError.wav",   2 },
        { SoundID::AE_PursuerNotice, L"data/mySound/AtmosphericEscape/PursuerNotice.wav", 1 },
        { SoundID::AE_Heartbeat,     L"data/mySound/AtmosphericEscape/Heartbeat.wav",     1 },
        { SoundID::AE_EntranceDoorOpen,L"data/mySound/AtmosphericEscape/EntranceDoorOpen.wav", 1 },
        { SoundID::AE_EntranceDoorSlam,L"data/mySound/AtmosphericEscape/EntranceDoorSlam.wav", 1 },
        { SoundID::AE_StoneEmblemPickup,        L"data/mySound/AtmosphericEscape/StoneEmblemPickup.wav",        1 },
        { SoundID::AE_FuseBoxInspect,           L"data/mySound/AtmosphericEscape/FuseBoxInspect.wav",           2 },
        { SoundID::AE_ResearchSealInspect,      L"data/mySound/AtmosphericEscape/ResearchSealInspect.wav",      2 },
        { SoundID::AE_ResearchSealSet,          L"data/mySound/AtmosphericEscape/ResearchSealSet.wav",          1 },
        { SoundID::AE_ControlTerminalActivate,  L"data/mySound/AtmosphericEscape/ControlTerminalActivate.wav", 1 },
        { SoundID::AE_PuzzleSolved,             L"data/mySound/AtmosphericEscape/PuzzleSolved.wav",             1 },
        { SoundID::AE_SafetyVent,               L"data/mySound/AtmosphericEscape/SafetyVent.wav",               2 },
        { SoundID::AE_SafetyCooling,            L"data/mySound/AtmosphericEscape/SafetyCooling.wav",            2 },
        { SoundID::AE_SafetyController,         L"data/mySound/AtmosphericEscape/SafetyController.wav",         1 },
        { SoundID::AE_UIMove,                   L"data/mySound/SE_UIMove.wav", 2 },
        { SoundID::AE_UISubmit,                 L"data/mySound/SE_Click.wav", 2 },
        { SoundID::AE_BgmClear,                 L"data/mySound/AtmosphericEscape/BGM_Clear.wav", 1 },
    };
}

void SoundBank::Init()
{
    //念のため初期化
     StopAllBgm();
     mSfxPools.clear();
     mLoopSounds.clear();
     mBgmMain.reset();
     mBgmClear.reset();
     mBgmGameOver.reset();

     // ---- ボス系 ----
     mSfxPools[SoundID::Boss_BeamCharge] = std::make_unique<SoundPool>(L"data/mySound/SE_BeamCharge.wav", 4);
     mLoopSounds[SoundID::Boss_BeamLoop] = std::make_unique<vnSound>(L"data/mySound/SE_Beam.wav");

     mSfxPools[SoundID::Boss_RollCharge] = std::make_unique<SoundPool>(L"data/mySound/SE_Rolling.wav", 4);
     mLoopSounds[SoundID::Boss_RollLoop] = std::make_unique<vnSound>(L"data/mySound/SE_Rolling.wav");

     mSfxPools[SoundID::Boss_PressLand] = std::make_unique<SoundPool>(L"data/mySound/SE_Press.wav", 4);
     mLoopSounds[SoundID::Boss_BreathLoop] = std::make_unique<vnSound>(L"data/mySound/SE_Breath.wav");
     mSfxPools[SoundID::Boss_HitWall] = std::make_unique<SoundPool>(L"data/mySound/SE_WallHit.wav", 4);
     mSfxPools[SoundID::Hit] = std::make_unique<SoundPool>(L"data/mySound/SE_Hit.wav", 6);          //命中は連打されやすいので多め


     // ---- プレイヤー/共通 ----
     mSfxPools[SoundID::Player_AttackRelese] = std::make_unique<SoundPool>(L"data/mySound/SE_AttackRelese.wav", 4);
     mSfxPools[SoundID::Player_AttackRollCharge] = std::make_unique<SoundPool>(L"data/mySound/SE_Charge.wav", 4);
     mSfxPools[SoundID::Player_ChargeComplete] = std::make_unique<SoundPool>(L"data/mySound/SE_ChargeComplete.wav", 4);
     mSfxPools[SoundID::Player_JustAvoidAttack] = std::make_unique<SoundPool>(L"data/mySound/SE_justAvoidAttack.wav", 4);
     mSfxPools[SoundID::Player_AttackHit] = std::make_unique<SoundPool>(L"data/mySound/SE_PlayerAttackHit.wav", 4);

     mSfxPools[SoundID::Player_Jump] = std::make_unique<SoundPool>(L"data/mySound/SE_jump.wav", 4);
     mSfxPools[SoundID::Player_Avoid] = std::make_unique<SoundPool>(L"data/mySound/SE_Avoid.wav", 4);

     mSfxPools[SoundID::Character_Dead] = std::make_unique<SoundPool>(L"data/mySound/SE_CharacterDead.wav", 6);


     // ---- BGM ----
     mBgmMain = std::make_unique<vnSound>(L"data/mySound/BGM_MainLoop.wav");
     mBgmClear = std::make_unique<vnSound>(L"data/mySound/BGM_GameClear.wav");
     mBgmGameOver = std::make_unique<vnSound>(L"data/mySound/BGM_GameOver.wav");
     mLoopSounds[SoundID::SE_SlowMotion] = std::make_unique<vnSound>(L"data/mySound/BGM_slowMotion.wav");

     mSfxPools[SoundID::UI_Move] = std::make_unique<SoundPool>(L"data/mySound/SE_UIMove.wav", 6);
     mSfxPools[SoundID::UI_Submit] = std::make_unique<SoundPool>(L"data/mySound/SE_Click.wav", 6);


     // BGMは音量を少し下げる
     mBgmMain->setVolume(0.5f);
     mBgmClear->setVolume(0.6f);
     mBgmGameOver->setVolume(0.6f);
    
}
void SoundBank::InitAtmosphericEscape()
{
    ShutdownAtmosphericEscape();
    mSfxEnabled = true;

    for (const auto& entry : kAtmosphericSfx)
    {
        mSfxPools.emplace(
            entry.id,
            std::make_unique<SoundPool>(entry.path, entry.voiceCount)
        );
    }

    const auto loadLoop = [this](SoundID id, const WCHAR* path)
        {
            auto track = std::make_unique<vnSound>(path);
            if (track->IsLoaded())
            {
                mLoopSounds.emplace(id, std::move(track));
            }
        };

    loadLoop(SoundID::AE_BgmTitle, L"data/mySound/AtmosphericEscape/BGM_Title.wav");
    loadLoop(SoundID::AE_BgmAmbience, L"data/mySound/AtmosphericEscape/Ambience.wav");
    loadLoop(SoundID::AE_BgmFinalChase, L"data/mySound/AtmosphericEscape/BGM_FinalChase.wav");
    loadLoop(SoundID::AE_BgmGameOver, L"data/mySound/AtmosphericEscape/BGM_GameOver.wav");
}
void SoundBank::ShutdownAtmosphericEscape()
{
    // このシーンで登録したループ音とSEを止めて解放する
    StopLoop(SoundID::AE_BgmAmbience);
    mLoopSounds.erase(SoundID::AE_BgmAmbience);
    StopLoop(SoundID::AE_BgmFinalChase);
    mLoopSounds.erase(SoundID::AE_BgmFinalChase);
    StopLoop(SoundID::AE_BgmGameOver);
    mLoopSounds.erase(SoundID::AE_BgmGameOver);
    StopLoop(SoundID::AE_BgmTitle);
    mLoopSounds.erase(SoundID::AE_BgmTitle);

    for (const auto& entry : kAtmosphericSfx)
    {
        const auto it = mSfxPools.find(entry.id);
        if (it != mSfxPools.end())
        {
            it->second->StopAll();
            mSfxPools.erase(it);
        }
    }
}

SoundBank::SoundBank()
{
	sInstance = this;
}

SoundBank* SoundBank::getInstance()
{
	if (!sInstance)
		sInstance = new SoundBank();
	return sInstance;
}
