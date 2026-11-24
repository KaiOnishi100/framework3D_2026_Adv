#pragma once

#include "../../public/object/AtmosphericEscape_Object/Player/explorationPlayer.h"
#include "../../public/input/AtmosphericEscape_InputConfig/ExplorationInputHandler.h"
#include "../../public/input/AtmosphericEscape_InputConfig/AtmosphericEscapeSceneInputHandler.h"
#include "../world/AtmosphericEscapeWorld/save/atmosphericEscapeSaveData.h"
#include "../world/AtmosphericEscapeWorld/save/atmosphericEscapeSaveManager.h"
#include "../world/AtmosphericEscapeWorld/exploration/explorationLocationSystem.h"
#include "../world/AtmosphericEscapeWorld/exploration/ObjectiveManager.h"
#include "../world/AtmosphericEscapeWorld/content/atmosphericEscapeContentLoader.h"
#include "../world/AtmosphericEscapeWorld/content/documentContentLoader.h"
#include "../world/AtmosphericEscapeWorld/document/documentManager.h"
#include "../UI/AtmosphericEscapeWorld/UIDocumentViewer.h"
#include "../UI/AtmosphericEscapeWorld/UIExplorationMenu.h"
#include "../UI/AtmosphericEscapeWorld/UIExplorationControls.h"
#include "../UI//UIAtmosphericEscapeOptions.h"
#include "../../framework/AtmosphericEscapeOptions.h"
#include "../../framework/AEUIFont.h"
#include "../inventory/inventoryManager.h"
#include "../integration/AtmosphericHeartbeat.h"
#include "../integration/AtmosphericChaseVignette.h"
#include "../interaction/interactionSystem.h"
#include "../object/AtmosphericEscape_Object/Enemy/pursuerSystem.h"
#include "../noise/noiseSystem.h"

#include "../debug/runtimeLookDev/runtimeLookDevController.h"

//============================================================
// Atmospheric Escape
// Game Progress
//============================================================
enum class ExplorationPhase
{
    //========================================================
    // Mansion Intro
    //========================================================

    // Playerがまだ屋敷へ入っていない
    EnterMansion,

    // 屋敷へ入った直後
    // Entrance Doorを閉じて施錠する
    EntranceLocked,

    // Mapを開かせるTutorial
    TutorialOpenMap,

    // FlashlightをONにさせるTutorial
    TutorialFlashlight,

    // 電源室へ向かう
    ReachPowerTerminal,

    // Terminalを操作して電源復旧
    RestorePower,

    // Monster起動演出
    PursuerAwakening,

    // Monsterの声を聞かせる演出
    PursuerWakeVoice,

    // 最初のPuzzle開始
    FirstPuzzle,

    // 通常探索
    ExploreMansion,

    // 最終脱出
    EscapeMansion,

    Escaped,

    // Rose -> Book -> Clock -> Candle
    SecondPuzzle = 12,

    // West Bedroomが開いた後
    // BasementKeyを探す
    FindBasementKey = 13,

    // BasementKey取得後
    // 地下入口へ向かう
    ReachBasement = 14,

    // Main Building B1探索
    ExploreBasement = 15,

    // ヒューズボックスを調べて
    // 交換用ヒューズが必要だと判明した段階
    FindFuse = 16,

    // Fuse取得後、
    // 本館地下のヒューズボックスを復旧する段階
    RestoreBasementPower = 17,

    // 本館地下のヒューズボックスを復旧した後
    // 本館1Fの東側から中庭へ向かう
    ReachCourtyard = 18,

    // 中庭に到達した後。
    // 別館入口を開けるためのAnnexKeyを探す
    FindAnnexKey = 19,

    // AnnexKey取得後。
    // 別館の入口へ向かい建物内へ入る
    EnterAnnex = 20,

    // 別館1Fへ到達した後
    // 別館内の探索を開始する
    ExploreAnnex = 21,

    // 石の紋章を探す
    // 別館1FのArchiveに配置されたStoneEmblemを探す
    FindStoneEmblem = 22,

    // 別館地下へ向かう
    // StoneEmblemを取得した後
    // 地下の研究施設へ移動する
    ReachAnnexBasement = 23,

    // 封印装置を調べる
    // 地下研究室前のResearchSealに
    // StoneEmblemをはめ込む
    ReachResearchSeal = 24,

    // 制御端末を起動する
    // ResearchSealの解除後、
    // ControlRoomへ進んでTerminalを操作する
    ActivateControlTerminal = 25,

    // 別館資料室の認証パズル
    SolveAnnexArchivePuzzle = 26,

    // 観察室の排気弁と実験室の冷却装置を起動し
    // 安全制御盤から復旧操作を実行する
    RestoreAnnexSafety = 27,

    MaxValue = RestoreAnnexSafety
};

//============================================================
// Scene Flow State
//============================================================
enum class SceneFlowState
{
    Playing,
    EscapeComplete,
    PlayerDead,
    FadeToResult,
    Result,
    Pause,
    ExplorationMenu,
    DocumentViewer
};

//============================================================
// Scene Result Type
//============================================================
enum class SceneResultType
{
    None,
    GameClear,
    GameOver
};

//============================================================
// AtmosphericEscapeStartRequest
// 次にAtmosphericEscapeを生成するとき
// NewGame
// Continue
// のどちらで開始するかを一時的に保持する
//============================================================
class AtmosphericEscapeStartRequest
{
public:

    enum class Mode
    {
        NewGame,
        LoadGame
    };


public:

    static void RequestNewGame()
    {
        sMode = Mode::NewGame;
    }

    static void RequestLoadGame()
    {
        sMode = Mode::LoadGame;
    }


    //========================================================
    // 一度だけ取得する
    //========================================================
    static Mode Consume()
    {
        const Mode result = sMode;

        sMode = Mode::NewGame;

        return result;
    }


private:

    inline static Mode sMode = Mode::NewGame;
};

class SceneAtmosphericEscape : public vnScene
{
public:
    //初期化
    bool initialize();
    //終了
    void terminate();
    //処理
    void execute();
    //描画
    void render();

private:
    enum class LoadPhase
    {
        ShowLoading,
        Environment,
        BeginWorld,
        World,
        Player,
        Pursuer,
        Finish,
        Ready,
        Failed,
    };

    void UpdateLoading();
    void AdvanceLoading();
    bool InitializePlayerAndContent();
    bool InitializePursuer();
    bool FinishInitialization();
    void DrawLoadingUI() const;

    LoadPhase mLoadPhase = LoadPhase::ShowLoading;
    AtmosphericEscapeStartRequest::Mode mStartMode = AtmosphericEscapeStartRequest::Mode::NewGame;
    bool mEnvironmentInitAttempted = false;
    bool mFadeInitialized = false;

    //============================================================
    // Gameplay Update
    //============================================================
    void UpdateGameplay(float dt);

    void UpdateHeartbeat(float dt);

    //============================================================
    // ゲーム進行
    //============================================================
    void UpdateExplorationProgress(float dt);

    void UpdatePause(float dt);

    //============================================================
    // Escape Complete演出
    //============================================================
    void UpdateEscapeComplete(float dt);
    void SubmitEscapeGoalLight() const;
    void UpdateEscapeCompleteLighting(float dt);

    //============================================================
    // Result Update
    //============================================================
    void UpdateResult(float dt);

    void UpdatePlayerDeath(float dt);

    //============================================================
    // Gameplay UI
    // Objective
    // Interaction Prompt
    //============================================================
    void DrawGameplayUI();

    //============================================================
    // Escape Complete開始
    //============================================================
    void BeginEscapeComplete();

    void EnterPause();

    void ExitPause();

    //============================================================
    // Result開始
    //============================================================
    void EnterResult();

    //============================================================
    // Player Death
    //============================================================
    void BeginPlayerDeath();

    //============================================================
    // ExplorationPhaseからObjectiveを同期
    //============================================================
    void SyncObjectiveFromExplorationPhase();
    void SyncPursuerActivationFromExplorationPhase();

    //============================================================
    // Mansion Escape判定
    //============================================================
    bool IsPlayerInMansionEscapeArea() const;

    //============================================================
    // Document Viewer
    //============================================================
    void OpenDocumentViewer(const std::string& documentId);
    void CloseDocumentViewer();

private:

    //============================================================
    // Save機能
    //============================================================

    bool SaveGame();
    bool CanSaveGame() const;
    bool ApplySaveData(const AtmosphericEscapeSaveData& data);

    AtmosphericEscapeSaveData BuildSaveData() const;

private:

    //============================================================
    // Player
    //============================================================
    ExplorationPlayer* pPlayer = nullptr;

    //============================================================
    // Player Flashlight Model
    //============================================================
    ModelBase* mFlashlightModel = nullptr;

    //============================================================
    // UI表示用の暗い下地
    //============================================================
    vnSprite* mMenuBackdrop = nullptr;

    //============================================================
    // Player Input
    //============================================================
    ExplorationInputHandler mPlayerInput;

    //============================================================
    // Scene Input
    //============================================================
    AtmosphericEscapeSceneInputHandler mSceneInput;

    //========================================================
    // Interaction
    //========================================================
    InteractionSystem mInteractionSystem;

    //============================================================
    // Pursuer System
    //============================================================
    PursuerSystem mPursuerSystem;

    //============================================================
    // Noise System
    //============================================================
    NoiseSystem mNoiseSystem;

    //============================================================
    // 心音の拍と音量をフレーム間で保持する
    //============================================================
    AtmosphericHeartbeat mHeartbeat;

    //============================================================
    // 追跡中の心音に合わせて、画面の縁を赤くする
    //============================================================
    AtmosphericChaseVignette mChaseVignette;

    //============================================================
    // Inventory
    //============================================================
    InventoryManager mInventory;

    //========================================================
    // World
    //========================================================
    AtmosphericEscapeWorld mWorld;

    //============================================================
    // Location
    //============================================================
    ExplorationLocationSystem mLocationSystem;

    //============================================================
    // Objective
    //============================================================
    ObjectiveManager mObjectiveManager;

    //============================================================
    // Documents
    // Inventoryとは別管理
    // 一度取得するとDocument Menuから再閲覧できる
    //============================================================
    DocumentManager mDocumentManager;

    //========================================================
    // Runtime Debug / LookDev
    // F8  : Inspector
    // F9  : Page
    // F10 : Free Camera
    //========================================================
    RuntimeLookDevController mRuntimeDebug;

    //============================================================
    // Collision
    //============================================================
    std::vector<Collider*> mColliders;

    std::vector<INavigationDoor*> mAIDoors;

    //============================================================
    // Result UI
    //============================================================
    UIResultOverlay mResultUI;

    //============================================================
    // Result UI Input
    //============================================================
    UIInputHandler mUIInput;

    //============================================================
    // Pause UI
    //============================================================
    UIPauseOverlay mPauseUI;

    //============================================================
    // Pause UI Input
    //============================================================
    UIInputHandler mPauseInput;

    //============================================================
    // Exploration Menu
    //============================================================
    UIExplorationMenu mExplorationMenu;

    //============================================================
    // Exploration Menu Input
    //============================================================
    UIInputHandler mExplorationMenuInput;

    //============================================================
    // Option Menu UI
    //============================================================
    UIAtmosphericEscapeOptions mOptionsUI;

    //============================================================
    // Option Menu Input
    //============================================================
    UIInputHandler mOptionsInput;

    //============================================================
    // ゲームプレイ中の操作ガイド
    //============================================================
    UIExplorationControls mControlGuide;

    //============================================================
    // Document Viewer
    // World上でDocumentを初めて拾った瞬間に表示
    //============================================================
    UIDocumentViewer mDocumentViewer;

    int mUIPadId = 0;

    //フレーム依存をなくすためのタイマー(ゲーム時間計測用)
    std::chrono::steady_clock::time_point mPrevTime;

    //============================================================
    // 現在の探索進行状態
    //============================================================
    ExplorationPhase mExplorationPhase = ExplorationPhase::EnterMansion;

    SceneFlowState mSceneFlowState = SceneFlowState::Playing;

    //============================================================
    // Result Type
    //============================================================
    SceneResultType mResultType = SceneResultType::None;

    //============================================================
    // Player Death演出Timer
    //============================================================
    float mPlayerDeadTimer = 0.0f;

    //============================================================
    // Clear演出待機時間
    //============================================================
    float mEscapeCompleteTimer = 0.0f;

    //============================================================
    // Play Time
    //============================================================
    float mPlayTime = 0.0f;

    //============================================================
    // Mansion Intro Event Timer
    //============================================================

    // 全照明復旧後Monsterの気配まで少し間を空ける
    float mPursuerAwakeningTimer = 0.0f;

    // Monster Voice用
    float mPursuerWakeVoiceTimer = 0.0f;

    bool mMapOpenedOnce = false;
};

