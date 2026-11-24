#pragma once

#include <future>
#include <unordered_set>

#include "../AtmosphericEscapeWorld/forest/forestGuidanceLight.h"
#include "../AtmosphericEscapeWorld/facility/facilityGraybox.h"
#include "../AtmosphericEscapeWorld/puzzle/puzzleManager.h"
#include "../AtmosphericEscapeWorld/puzzle/annexSafetyPuzzle.h"

#include "../AtmosphericEscapeWorld/mansion/mansionGraybox.h"
#include "../AtmosphericEscapeWorld/mansion/mansionEntranceDoor.h"
#include "../AtmosphericEscapeWorld/mansion/mansionPowerTerminal.h"
#include "../AtmosphericEscapeWorld/mansion/mansionPowerLight.h"
#include "../AtmosphericEscapeWorld/mansion/mansionDoor.h"
#include "../AtmosphericEscapeWorld/mansion/mansionFuseBox.h"
#include "../AtmosphericEscapeWorld/mansion/mansionResearchSeal.h"
#include "../AtmosphericEscapeWorld/mansion/mansionControlTerminal.h"
#include "../AtmosphericEscapeWorld/mansion/mansionCandle.h"

#include "../../effect/AtomosphericEscape/ambientDustEffect.h"
#include "../../effect/AtomosphericEscape/doorDustEffect.h"

#include "../AtmosphericEscapeWorld/mansion/mansionArt.h"

#include "../AtmosphericEscapeWorld/item/worldItemPickup.h"
#include "../AtmosphericEscapeWorld/document/documentPickup.h"

#include "worldPlacement.h"

class vnScene;
class InteractionSystem;
class DocumentManager;

//============================================================
// AtmosphericEscapeWorld
// Atmospheric EscapeのWorld管理クラス
// ・地面
// ・施設
// ・Static Collider
// ・World Object
//============================================================
class AtmosphericEscapeWorld
{
public:

    AtmosphericEscapeWorld() = default;

    ~AtmosphericEscapeWorld() = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(vnScene* scene, InteractionSystem* interactionSystem);

    enum class InitializeResult
    {
        Loading,
        Ready,
        Failed,
    };

    // Beginで床やドアなどの基礎を作り、以降は毎フレームAdvanceで進める。
    bool BeginInitialize(vnScene* scene, InteractionSystem* interactionSystem);
    InitializeResult AdvanceInitialize(vnScene* scene, InteractionSystem* interactionSystem);

    size_t GetCompletedArtPlacements() const { return mNextArtPlacement; }
    size_t GetTotalArtPlacements() const { return mArtPlacementsToLoad.size(); }


    //========================================================
    // Terminate
    //========================================================
    void Terminate(vnScene* scene, InteractionSystem* interactionSystem);


    //========================================================
    // Update
    //========================================================
    void Update(float dt);

    void UpdateEffects(float dt);
    void RenderFullResolutionEffects();

    //========================================================
    // World ColliderをScene側CollisionManagerへ渡す
    // Colliderの所有権はWorld側が持つ
    // Sceneのvectorには非所有Pointerだけを追加する
    //========================================================
    void AppendColliders(std::vector<Collider*>& destination) const;

    void AppendAIDoors(std::vector<INavigationDoor*>& destination);

    bool IsInsideEscapeTrigger(const XMVECTOR& worldPosition) const;

    //============================================================
    // Puzzle State
    //============================================================
    bool IsPuzzleSolved(const std::string& puzzleId) const;

public:
    //============================================================
    // Facility内部判定
    //============================================================
    bool IsInsideFacility(const XMVECTOR& worldPosition) const
    {
        return mFacility.ContainsPosition(worldPosition);
    }

    //============================================================
    // Entrance Door状態
    //============================================================
    bool IsFacilityDoorOpen() const
    {
        return mFacility.IsEntranceDoorOpen();
    }

    //============================================================
    // Facility Inner Door
    //============================================================
    bool IsFacilityInnerDoorOpen() const
    {
        return mFacility.IsInnerDoorOpen();
    }


    bool IsFacilityInnerDoorLocked() const
    {
        return mFacility.IsInnerDoorLocked();
    }

    //============================================================
    // Facility Terminal
    //============================================================
    bool IsFacilityTerminalActivated() const
    {
        return mFacility.IsTerminalActivated();
    }

    //============================================================
    // Facility Power
    //============================================================
    void SetFacilityPowerEnabled(bool enabled)
    {
        mFacility.SetPowerEnabled(enabled);
    }

    bool IsFacilityPowerEnabled() const
    {
        return mFacility.IsPowerEnabled();
    }

    //============================================================
    // Access Key
    //============================================================
    bool IsFacilityAccessKeyCollected() const
    {
        return mFacility.IsAccessKeyCollected();
    }

    //============================================================
    // Exit Door Unlock
    //============================================================
    void UnlockFacilityExitDoor()
    {
        mFacility.SetExitDoorLocked(false);
    }


    //============================================================
    // Exit Door State
    //============================================================
    bool IsFacilityExitDoorOpen() const
    {
        return mFacility.IsExitDoorOpen();
    }

    void SetNoiseSystem(NoiseSystem* noiseSystem);

    //============================================================
    // Document Viewer Callback
    //============================================================
    void SetDocumentCollectedCallback(std::function<void(const std::string&)> callback);

    //============================================================
    // Inventory Manager
    // ContentLoaderによるItem Definition読み込み後に呼ぶ
    //============================================================
    void SetInventoryManager(InventoryManager* inventoryManager);

    void SetDocumentManager(DocumentManager* documentManager);

public:

    //============================================================
    // Mansion Entrance Sequence
    //============================================================
    bool BeginMansionEntranceCloseAndLock()
    {
        if (!mMansionEntranceDoorInitialized)
        {
            return false;
        }

        return mMansionEntranceDoor.BeginCloseAndLock();
    }

    void RestoreMansionEntranceFromProgress(bool enteredMansion, bool escapeUnlocked)
    {
        if (!mMansionEntranceDoorInitialized)
        {
            return;
        }

        mMansionEntranceDoor.RestoreFromProgress(enteredMansion, escapeUnlocked);
    }

    bool IsMansionEntranceDoorClosed() const
    {
        return mMansionEntranceDoorInitialized && mMansionEntranceDoor.IsClosed();
    }

    bool IsMansionEntranceDoorLocked() const
    {
        return mMansionEntranceDoorInitialized && mMansionEntranceDoor.IsLocked();
    }

    void UnlockMansionEntranceDoor()
    {
        if (mMansionEntranceDoorInitialized)
        {
            mMansionEntranceDoor.UnlockForEscape();
        }
    }

    void SetMansionInteractionEnabled(
        bool powerTerminalEnabled,
        bool firstPuzzleEnabled,
        bool secondPuzzleEnabled
    );

    //============================================================
    // Mansion Power Terminal
    //============================================================
    bool IsMansionPowerTerminalActivated() const
    {
        return mMansionPowerTerminalInitialized && mMansionPowerTerminal.IsActivated();
    }

    //============================================================
    // Mansion Power
    //============================================================
    void SetMansionPowerEnabled(bool enabled);

    bool IsMansionPowerEnabled() const
    {
        return mMansionPowerEnabled;
    }

    bool BeginMansionPowerRestore();

    bool IsMansionPowerRestoreFinished() const
    {
        return mMansionPowerRestoreSequenceFinished;
    }

    //============================================================
    // Mansion Fuse Box Inspection
    // Scene側から配電盤の調査状態を確認する
    // World側で初期化されていない場合はfalse
    //============================================================
    bool IsMansionFuseBoxInspected() const
    {
        return mMansionFuseBoxInitialized && mMansionFuseBox.IsInspected();
    }

    //============================================================
    // Basement Fuse Box
    //============================================================
    bool IsMansionFuseBoxActivated() const
    {
        return mMansionFuseBoxInitialized && mMansionFuseBox.IsActivated();
    }

    //============================================================
    // Annex Control Terminal
    //============================================================
    bool IsAnnexControlTerminalActivated() const
    {
        return mAnnexControlTerminalInitialized && mAnnexControlTerminal.IsActivated();
    }

    //============================================================
    // Annex Research Seal
    // true:
    //   StoneEmblemがResearchSealに取り付けられ、
    //   地下制御室の扉が解錠可能になっている
    // false: 未初期化、または封印装置が未起動
    //============================================================
    bool IsAnnexResearchSealActivated() const
    {
        return mAnnexResearchSealInitialized && mAnnexResearchSeal.IsActivated();
    }

public:

    //============================================================
    // Persistent World Objects
    // World以下に存在するSave対象Objectを収集する
    //============================================================
    void AppendPersistentObjects(std::vector<IPersistentWorldObject*>& destination);
    void AppendPersistentObjects(std::vector<const IPersistentWorldObject*>& destination) const;

    //============================================================
    // Restore Persistent World Objects
    //============================================================
    void RestorePersistentObjects(const std::vector<WorldObjectSaveState>& states);

    void RebuildMansionPersistentDerivedState();

private:

    //========================================================
    // Debug / Graybox生成
    //========================================================
    bool CreateGrayboxGround(vnScene* scene);

    bool CreateGrayboxForest(vnScene* scene);

    //========================================================
    // MansionDoor ID Search
    // CSVのPlacement ID == Persistent IDを使用して検索する
    //========================================================
    MansionDoor* FindMansionDoorById(const std::string& persistentId);

    //============================================================
    // Document取得通知
    //============================================================
    std::function<void(const std::string&)> mDocumentCollectedCallback;

private:

    //============================================================
    // Escape Trigger
    //============================================================
    XMFLOAT3 mEscapeTriggerPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };

    XMFLOAT3 mEscapeTriggerScale =
    {
        1.0f,
        1.0f,
        1.0f
    };

    bool mEscapeTriggerValid = false;

    bool mTestFacilityEnabled = false;// 以前ゲームシステム確認用に使用していたFacility
    //============================================================
    // World Placement
    // CSVから読み込んだ配置情報
    //============================================================
    enum class InitializeStage
    {
        Idle,
        StaticBlocks,
        WaitForVnm,
        MansionArt,
        Finish,
        Ready,
        Failed,
    };

    bool PrepareInitialize(vnScene* scene, InteractionSystem* interactionSystem);
    bool FinishInitialize(vnScene* scene, InteractionSystem* interactionSystem);
    void StartVnmPreload();
    void StartDdsPreload(size_t chunkEnd);
    bool CreateArtChunk(vnScene* scene, size_t chunkEnd);
    void ReleaseInitializeCaches();

    InitializeStage mInitializeStage = InitializeStage::Idle;

    // 配置表をClearするまでは、各要素へのポインターが有効。
    std::vector<const WorldObjectPlacement*> mBlocksToLoad;
    std::vector<const WorldObjectPlacement*> mArtPlacementsToLoad;
    size_t mNextBlockPlacement = 0;
    size_t mNextArtPlacement = 0;

    // 既にGPUへ登録したモデルのDDSを重複して読まない。
    std::unordered_set<std::wstring> mPreloadedArtKeys;
    std::unordered_multimap<std::string, MansionPowerLight*> mLightsByExcludedArtId;

    // Workerに渡すのはファイルパスのみ。GPU処理はAdvance側で行う。
    std::future<bool> mPreloadWork;
    bool mOwnsInitializeCaches = false;

    WorldPlacementTable mPlacementTable;

    //========================================================
    // Ground
    //========================================================
    ModelBase* mGround = nullptr;

    std::unique_ptr<BoxCollider> mGroundCollider;

    //========================================================
    // 森の誘導灯
    //========================================================
    ForestGuidanceLight mForestGuidanceLight;

    //========================================================
    // Forest Graybox Objects
    //========================================================
    std::vector<ModelBase*> mForestObjects;

    //============================================================
    // Facility
    //============================================================
    FacilityGraybox mFacility;

    PuzzleManager mPuzzleManager;

    std::vector<std::unique_ptr<BoxCollider>> mForestColliders;

    //============================================================
    // Mansion
    //============================================================
    MansionGraybox mMansion;

    //============================================================
    // Mansion Entrance Door
    //============================================================
    MansionEntranceDoor mMansionEntranceDoor;

    bool mMansionEntranceDoorInitialized = false;

    //============================================================
    // Mansion Power Terminal
    //============================================================
    MansionPowerTerminal mMansionPowerTerminal;

    bool mMansionPowerTerminalInitialized = false;

    bool mMansionPowerEnabled = false;

    //============================================================
    // Mansion Power Lights
    //============================================================
    std::vector<std::unique_ptr<MansionPowerLight>> mMansionPowerLights;

    //============================================================
    // Power Restore Sequence
    //============================================================

    // シーケンス再生中
    bool mMansionPowerRestoreSequenceActive = false;

    // 全照明点灯完了
    bool mMansionPowerRestoreSequenceFinished = false;

    // 次に点けるLight
    std::size_t mMansionPowerRestoreNextLight = 0;

    // Light点灯間隔Timer
    float mMansionPowerRestoreTimer = 0.0f;

    // 1灯ずつ点く間隔
    float mMansionPowerRestoreInterval = 0.20f;

    //============================================================
    // Mansion Generic Doors
    //============================================================
    std::vector<std::unique_ptr<MansionDoor>> mMansionDoors;

    //============================================================
    // Mansion First Puzzle
    // Library:
    // Moon -> Sun -> Bird
    // 解決するとMain1F_NorthAccessDoorをUnlockする
    //============================================================
    std::unique_ptr<SequencePuzzle> mMansionFirstPuzzle;

    std::unique_ptr<SequencePuzzle> mMansionSecondPuzzle;

    //============================================================
    // World Item Pickups
    // 書斎のNote 鍵 Fuseなどをまとめて管理
    //============================================================
    std::vector<std::unique_ptr<WorldItemPickup>> mWorldItemPickups;

    //============================================================
    // World Document Pickups
    // DataIdはDocument ID
    //============================================================
    std::vector<std::unique_ptr<DocumentPickup>> mDocumentPickups;

    //============================================================
    // Main Building B1 Fuse Box
    //============================================================
    MansionFuseBox mMansionFuseBox;

    bool mMansionFuseBoxInitialized = false;

    //============================================================
    // Annex B1 Research Seal
    //============================================================
    MansionResearchSeal mAnnexResearchSeal;

    // 正常にInitialize済みか
    bool mAnnexResearchSealInitialized = false;

    // ResearchSealから解除されるDoor
    MansionDoor* mAnnexControlRoomDoor = nullptr;

    //============================================================
    // Annex B1 Control Terminal
    //============================================================
    MansionControlTerminal mAnnexControlTerminal;

    bool mAnnexControlTerminalInitialized = false;

    //============================================================
    // Annex Archive Security Puzzle
    //============================================================
    std::unique_ptr<SequencePuzzle> mAnnexArchiveSequencePuzzle;

    //============================================================
    // Annex B1 Safety Puzzle
    //============================================================
    std::unique_ptr<AnnexSafetyPuzzle> mAnnexSafetyPuzzle;

    //============================================================
    // Safety Puzzle Reward Door
    //============================================================
    MansionDoor* mAnnexCentralResearchDoor = nullptr;

    //============================================================
    // Mansion Art
    // Grayboxとは独立した屋敷のVisual Model管理
    //============================================================
    MansionArt mMansionArt;

    //============================================================
    // 点灯中のろうそく 炎の所有権はEffectManagerが持つ
    //============================================================
    std::vector<MansionCandle> mMansionCandles;

    EffectManager mEnvironmentEffects;

    NoiseSystem* mNoiseSystem = nullptr;
};

