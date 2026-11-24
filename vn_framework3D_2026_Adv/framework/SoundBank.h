#pragma once

enum class SoundID
{
    Boss_BeamCharge,        // ボス攻撃：ビームチャージ
    Boss_BeamLoop,          // ボス攻撃：ビーム発射中（ループ）
    Boss_RollCharge,        // ローリング攻撃：チャージ
    Boss_RollLoop,          // ローリング攻撃中：轟音（ループ）
    Boss_PressLand,         // プレス攻撃：着地
    Boss_BreathLoop,        // ブレス発射中（ループ）
    Boss_HitWall,           // ローリング攻撃時壁にぶつかった音
    Hit,                    // ボスの攻撃命中

    Player_AttackRelese,    // プレイヤー攻撃ボタンリリース
    Player_AttackRollCharge,// プレイヤー：アタックローリング溜め
    Player_ChargeComplete,  // 攻撃の最大タメ
    Player_JustAvoidAttack, // プレイヤージャスト回避からの攻撃
    Player_AttackHit,       // プレイヤーの攻撃命中

    Player_Jump,            // ジャンプ
    Player_Avoid,           // 回避発動

    Character_Dead,         //死亡時

    BGM_Main,               // メインBGM（ループ）
    BGM_Clear,              // クリアBGM
    BGM_GameOver,           // ゲームオーバーBGM
    SE_SlowMotion,         // スローモーション

    UI_Move,     // 選択移動
    UI_Submit,   // 決定


    AE_BgmTitle,       // タイトル画面用
    AE_BgmFinalChase,  // 制御端末起動後の最終脱出
    AE_BgmAmbience,
    AE_BgmClear,       // クリア画面で1回再生
    AE_BgmGameOver,    // ゲームオーバー画面でループ
    AE_Footstep,
    AE_Flashlight,
    AE_DoorOpen,
    AE_DoorClose,
    AE_Pickup,
    AE_PowerRestore,
    AE_PursuerWake,

    AE_Interact,
    AE_PursuerBreath,
    AE_PursuerAttack,
    AE_PursuerStep,
    AE_PursuerStep2,
    AE_PursuerStep3,
    AE_Footstep2,
    AE_Footstep3,
    AE_FootstepWood,       // 木の床を踏んだ音
    AE_FootstepConcrete,   // コンクリートの床を踏んだ音
    AE_PlayerRun,
    AE_PlayerRun2,
    AE_PlayerRun3,
    AE_PlayerHit,
    AE_PlayerDeath,
    AE_DocumentPage,
    AE_FusePickup,
    AE_FuseSet,
    AE_PuzzleInteract,
    AE_PuzzleError,
    AE_PursuerNotice,
    AE_Heartbeat,
    AE_EntranceDoorOpen,
    AE_EntranceDoorSlam,

    // アイテムと装置
    AE_StoneEmblemPickup,       // 石の紋章を拾う
    AE_FuseBoxInspect,          // ヒューズがない状態で箱を調べる
    AE_ResearchSealInspect,     // 石の紋章がない状態でくぼみを調べる
    AE_ResearchSealSet,         // 石の紋章をはめ込む
    AE_ControlTerminalActivate,// 別館の制御端末を起動する

    // パズル
    AE_PuzzleSolved,            // 順番パズルを解く
    AE_SafetyVent,              // 排気弁を開く
    AE_SafetyCooling,           // 冷却装置を起動する
    AE_SafetyController,        // 安全装置の復旧に成功する

    AE_UIMove,       // タイトルなどのメニュー選択音
    AE_UISubmit,     // メニュー決定音
};

class SoundBank
{
public:
    // 起動時/シーン初期化時に一度だけ呼ぶ（wavロードが走る）
    void Init();
   
    void InitAtmosphericEscape();
    void ShutdownAtmosphericEscape();

    void SetSfxEnabled(bool enabled) { mSfxEnabled = enabled; }
    // ---- SE（短音） ----
    void PlaySfx(SoundID id, float volume = 1.0f)
    {
        if (!mSfxEnabled) return;
        auto it = mSfxPools.find(id);
        if (it == mSfxPools.end()) return;
        it->second->Play(volume);
    }

    // ---- ループ音（発射中など） ----
    void StartLoop(SoundID id, float volume = 1.0f)
    {
        if (!mSfxEnabled) return;
        auto it = mLoopSounds.find(id);
        if (it == mLoopSounds.end()) return;

        it->second->setVolume(volume);
        it->second->playLoop(true);
    }

    void StopLoop(SoundID id)
    {
        auto it = mLoopSounds.find(id);
        if (it == mLoopSounds.end()) return;
        it->second->stop();
    }

    // ---- BGM ----
    void PlayBgmMain()
    {
        StopAllBgm();
        mBgmMain->playLoop(true);
    }

    void PlayBgmClear()
    {
        StopAllBgm();
        mBgmClear->play(false); // クリアBGMはループしないなら play()
    }

    void PlayBgmGameOver()
    {
        StopAllBgm();
        mBgmGameOver->play(false);
    }

    void StopAllBgm()
    {
        if (mBgmMain) mBgmMain->stop();
        if (mBgmClear) mBgmClear->stop();
        if (mBgmGameOver) mBgmGameOver->stop();
    }

    void SetSfxSpatialMix(SoundID id, const vnSound::SpatialMix& mix)
    {
        const auto it = mSfxPools.find(id);

        if (it != mSfxPools.end())
        {
            it->second->ApplySpatialMix(mix);
        }
    }

    SoundBank();

    static SoundBank* getInstance();
private:
    static SoundBank* sInstance;
    bool mSfxEnabled = true;

    std::unordered_map<SoundID, std::unique_ptr<SoundPool>> mSfxPools;     // 短いSE（重ね鳴らし）
    std::unordered_map<SoundID, std::unique_ptr<vnSound>>   mLoopSounds;   // ループ音（連続発射など）

    std::unique_ptr<vnSound> mBgmMain;
    std::unique_ptr<vnSound> mBgmClear;
    std::unique_ptr<vnSound> mBgmGameOver;
};