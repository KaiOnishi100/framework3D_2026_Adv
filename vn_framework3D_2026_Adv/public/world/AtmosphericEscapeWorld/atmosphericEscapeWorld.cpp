#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "../../interaction/interactionSystem.h"

#include "atmosphericEscapeWorld.h"


//============================================================
// Initialize
//============================================================
// 同期初期化を使う既存の呼び出しにも同じ処理を提供する
bool AtmosphericEscapeWorld::Initialize(vnScene* scene, InteractionSystem* interactionSystem)
{
    if (!BeginInitialize(scene, interactionSystem))
    {
        return false;
    }

    InitializeResult result = InitializeResult::Loading;
    while (result == InitializeResult::Loading)
    {
        if (mPreloadWork.valid())
        {
            mPreloadWork.wait();
        }
        result = AdvanceInitialize(scene, interactionSystem);
    }
    return result == InitializeResult::Ready;
}

bool AtmosphericEscapeWorld::BeginInitialize(vnScene* scene, InteractionSystem* interactionSystem)
{
    if (mInitializeStage != InitializeStage::Idle)
    {
        return false;
    }

    // 準備段階ではCSVと、ライト・ドア・パズルなどの初期化を行う。
    if (!PrepareInitialize(scene, interactionSystem))
    {
        mInitializeStage = InitializeStage::Failed;
        return false;
    }

    mInitializeStage = InitializeStage::StaticBlocks;
    return true;
}

AtmosphericEscapeWorld::InitializeResult AtmosphericEscapeWorld::AdvanceInitialize(
    vnScene* scene, InteractionSystem* interactionSystem)
{
    constexpr size_t kBlocksPerFrame = 32;
    constexpr size_t kArtPerFrame = 8;
    constexpr auto kPollDuration = std::chrono::milliseconds(0);

    switch (mInitializeStage)
    {
    case InitializeStage::StaticBlocks:
    {
        const size_t end = (std::min)(mNextBlockPlacement + kBlocksPerFrame, mBlocksToLoad.size());
        for (; mNextBlockPlacement < end; ++mNextBlockPlacement)
        {
            const auto* placement = mBlocksToLoad[mNextBlockPlacement];
            if (placement != nullptr && !mMansion.AddStaticBlock(
                scene, placement->Id, placement->Position, placement->Scale))
            {
                OutputDebugStringA("[WorldLoad] Failed to create MansionBlock.\n");
                mInitializeStage = InitializeStage::Failed;
                return InitializeResult::Failed;
            }
        }
        if (mNextBlockPlacement == mBlocksToLoad.size())
        {
            mBlocksToLoad.clear();
            StartVnmPreload();
            mInitializeStage = InitializeStage::WaitForVnm;
        }
        return InitializeResult::Loading;
    }

    case InitializeStage::WaitForVnm:
        // future.get()は完了を確認してから呼ぶ 待っている間も描画する
        if (mPreloadWork.wait_for(kPollDuration) != std::future_status::ready)
        {
            return InitializeResult::Loading;
        }
        if (!mPreloadWork.get())
        {
            OutputDebugStringA("[WorldLoad] Some VNM files could not be preloaded.\n");
        }
        mInitializeStage = InitializeStage::MansionArt;
        return InitializeResult::Loading;

    case InitializeStage::MansionArt:
    {
        if (mNextArtPlacement == mArtPlacementsToLoad.size())
        {
            ReleaseInitializeCaches();
            mLightsByExcludedArtId.clear();
            mInitializeStage = InitializeStage::Finish;
            return InitializeResult::Loading;
        }

        const size_t chunkEnd = (std::min)(mNextArtPlacement + kArtPerFrame, mArtPlacementsToLoad.size());
        if (mPreloadWork.valid())
        {
            if (mPreloadWork.wait_for(kPollDuration) != std::future_status::ready)
            {
                return InitializeResult::Loading;
            }
            if (!mPreloadWork.get())
            {
                // DDSが一部読めなかった場合も既存のモデル生成時の読み込みを試す
                OutputDebugStringA("[WorldLoad] Some DDS files could not be preloaded.\n");
            }
        }
        else
        {
            StartDdsPreload(chunkEnd);
            if (mPreloadWork.valid())
            {
                return InitializeResult::Loading;
            }
        }

        // 生成とGPU転送は同じフレームに収めCommandListの処理をまたがない
        const bool created = CreateArtChunk(scene, chunkEnd);
        ModelBase::ReleaseDdsPreloadCache();
        if (!created)
        {
            ReleaseInitializeCaches();
            mInitializeStage = InitializeStage::Failed;
            return InitializeResult::Failed;
        }

        // GPU転送が完了した件数だけをロード画面に表示する。
        mNextArtPlacement = chunkEnd;
        return InitializeResult::Loading;
    }

    case InitializeStage::Finish:
        mInitializeStage = FinishInitialize(scene, interactionSystem)
            ? InitializeStage::Ready : InitializeStage::Failed;
        return mInitializeStage == InitializeStage::Ready
            ? InitializeResult::Ready : InitializeResult::Failed;

    case InitializeStage::Ready:
        return InitializeResult::Ready;

    case InitializeStage::Idle:
    case InitializeStage::Failed:
        return InitializeResult::Failed;
    }

    return InitializeResult::Failed;
}

void AtmosphericEscapeWorld::StartVnmPreload()
{
    std::vector<std::wstring> paths;
    paths.reserve(mArtPlacementsToLoad.size());
    for (const auto* placement : mArtPlacementsToLoad)
    {
        if (placement != nullptr && !placement->DataId.empty())
        {
            paths.push_back(std::filesystem::path(placement->DataId).wstring());
        }
    }

    mOwnsInitializeCaches = true;
    mPreloadWork = std::async(std::launch::async, [paths = std::move(paths)]()
        {
            return ModelBase::PreloadVnmFilesParallel(paths, 4);
        });
}

void AtmosphericEscapeWorld::StartDdsPreload(size_t chunkEnd)
{
    std::vector<std::wstring> paths;
    paths.reserve(chunkEnd - mNextArtPlacement);
    for (size_t i = mNextArtPlacement; i < chunkEnd; ++i)
    {
        const auto* placement = mArtPlacementsToLoad[i];
        if (placement == nullptr || placement->DataId.empty())
        {
            continue;
        }

        const std::filesystem::path modelPath(placement->DataId);
        if (!std::filesystem::is_regular_file(modelPath))
        {
            continue;
        }

        // パスを正規化して別表記で登録されたモデルも同じものと判定する
        std::error_code error;
        const auto absolutePath = std::filesystem::absolute(modelPath, error);
        auto cacheKey = (error ? modelPath : absolutePath).lexically_normal().wstring();
        std::transform(cacheKey.begin(), cacheKey.end(), cacheKey.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

        if (mPreloadedArtKeys.insert(cacheKey).second)
        {
            paths.push_back(modelPath.wstring());
        }
    }

    if (!paths.empty())
    {
        mPreloadWork = std::async(std::launch::async, [paths = std::move(paths)]()
            {
                return ModelBase::PreloadDdsFromVnmFilesParallel(paths, 4);
            });
    }
}

bool AtmosphericEscapeWorld::CreateArtChunk(vnScene* scene, size_t chunkEnd)
{
    if (!ModelBase::BeginTextureUploadBatch())
    {
        OutputDebugStringA("[WorldLoad] Texture batch begin failed.\n");
        return false;
    }

    // モデルの生成が途中で失敗しても開始した転送バッチを閉じる
    struct BatchGuard
    {
        bool Active = true;
        ~BatchGuard()
        {
            if (Active)
            {
                ModelBase::EndTextureUploadBatch();
            }
        }
    } batchGuard;

    for (size_t i = mNextArtPlacement; i < chunkEnd; ++i)
    {
        const WorldObjectPlacement* placement = mArtPlacementsToLoad[i];

        //====================================================
        // Safety Check
        //====================================================
        if (placement == nullptr)
        {
            continue;
        }

        //====================================================
        // DataIdにはVNMのファイルパスを保存する
        //====================================================
        if (placement->DataId.empty())
        {
            OutputDebugStringA("[MansionArt] DataId is empty.\n");

            return false;
        }

        //====================================================
        // VNMパスをFolderとFileに分解する
        //====================================================
        const std::filesystem::path modelPath(placement->DataId);

        //====================================================
        // モデルファイルが存在するか確認
        // パスの誤入力を早期に発見するための処理
        //====================================================
        if (!std::filesystem::is_regular_file(modelPath))
        {
            const std::string message =
                "[MansionArt] Model file not found : " +
                placement->DataId + "\n";

            OutputDebugStringA(message.c_str());

            return false;
        }

        //====================================================
        // MansionArtPlacementを生成する
        //====================================================
        MansionArtPlacement art = {};

        //====================================================
        // ID
        //====================================================
        art.Id = placement->Id;

        //====================================================
        // Model Path
        // vnModelのコンストラクタがFolder+Fileを
        // 使用するので、Folder末尾へ区切り文字を付ける
        //====================================================
        art.Folder = modelPath.parent_path().wstring();
        art.Folder += L"/";
        art.File = modelPath.filename().wstring();

        //====================================================
        // Position
        //====================================================
        art.Position = placement->Position;

        //====================================================
        // Rotation
        //====================================================
        art.Rotation = XMFLOAT3(
            XMConvertToRadians(placement->Rotation.x),
            XMConvertToRadians(placement->Rotation.y),
            XMConvertToRadians(placement->Rotation.z)
        );

        //====================================================
        // Scale
        //====================================================
        art.Scale = placement->Scale;

        //====================================================
        // Static Shadow
        //====================================================
        art.CastShadow = true;

        //============================================================
        // 床モデルのShadowCaster設定
        //============================================================
        const std::wstring modelFileName =
            modelPath.filename().wstring();

        const bool isFloor =
            modelFileName == L"entrance_floor.vnm" ||
            modelFileName == L"corridor_floor_7x16.vnm" ||
            modelFileName == L"basement_floor_10x575.vnm";

        art.CastShadow = !isFloor;

        //====================================================
        // Model生成・Scene登録
        //====================================================
        ModelBase* createdModel = nullptr;
        if (!mMansionArt.AddStaticModel(scene, art, &createdModel))
        {
            const std::string message =
                "[MansionArt] Failed to create : " +
                placement->Id + "\n";

            OutputDebugStringA(message.c_str());

            return false;
        }

        // CSVのDataIdと実際に生成したMansionArtのIDを照合する
        const auto [first, last] = mLightsByExcludedArtId.equal_range(art.Id);

        for (auto entry = first; entry != last; ++entry)
        {
            entry->second->SetExcludedShadowCaster(createdModel);
        }

        mLightsByExcludedArtId.erase(first, last);

    }

    const bool uploaded = ModelBase::EndTextureUploadBatch();
    batchGuard.Active = false;
    if (!uploaded)
    {
        OutputDebugStringA("[WorldLoad] Texture batch upload failed.\n");
    }
    return uploaded;
}

void AtmosphericEscapeWorld::ReleaseInitializeCaches()
{
    if (mPreloadWork.valid())
    {
        // 終了・タイトル復帰時は、Workerが共有キャッシュへ書き終わるまで待つ
        // validなfutureを破棄する際の待機は通常のロードフレームでは発生しない
        mPreloadWork = {};
    }

    if (mOwnsInitializeCaches)
    {
        ModelBase::ReleaseDdsPreloadCache();
        ModelBase::ReleaseVnmPreloadCache();
        mOwnsInitializeCaches = false;
    }
    mPreloadedArtKeys.clear();
}

bool AtmosphericEscapeWorld::PrepareInitialize(vnScene* scene, InteractionSystem* interactionSystem)
{
    if (scene == nullptr)
    {
        return false;
    }

    //============================================================
    // World配置データ
    // Excel / Text Editorから編集可能
    //============================================================
    const std::vector<std::filesystem::path> worldPlacementFiles =
    {
        L"data/csv/world/AtmosphericEscapeWorld.csv",
        L"data/csv/world/AtmosphericEscape/MainBuilding.csv",
        L"data/csv/world/AtmosphericEscape/MainBuilding2F.csv",
        L"data/csv/world/AtmosphericEscape/MainBuildingBasement.csv",
        L"data/csv/world/AtmosphericEscape/MainBuildingRoof.csv",
        L"data/csv/world/AtmosphericEscape/Courtyard.csv",
        L"data/csv/world/AtmosphericEscape/Annex.csv",
        L"data/csv/world/AtmosphericEscape/AnnexBasement.csv",

        L"data/csv/world/AtmosphericEscape/art/EntranceHallArt.csv",
        L"data/csv/world/AtmosphericEscape/art/CentralHallArt.csv",
        L"data/csv/world/AtmosphericEscape/art/WestCorridorArt.csv",
        L"data/csv/world/AtmosphericEscape/art/EastCorridorArt.csv",
        L"data/csv/world/AtmosphericEscape/art/LibraryArt.csv",
        L"data/csv/world/AtmosphericEscape/art/StudyArt.csv",
        L"data/csv/world/AtmosphericEscape/art/PowerRoomArt.csv",
        L"data/csv/world/AtmosphericEscape/art/GrandStairHallArt.csv",
        L"data/csv/world/AtmosphericEscape/art/WestBedroomArt.csv",
        L"data/csv/world/AtmosphericEscape/art/WestDressingRoomArt.csv",
        L"data/csv/world/AtmosphericEscape/art/EastBedroomArt.csv",
        L"data/csv/world/AtmosphericEscape/art/EastPrivateRoomArt.csv",
        L"data/csv/world/AtmosphericEscape/art/BasementArt.csv",
        L"data/csv/world/AtmosphericEscape/art/MansionFrontArt.csv",
        L"data/csv/world/AtmosphericEscape/art/CourtyardArt.csv",

        L"data/csv/world/AtmosphericEscape/art/AnnexArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexEntranceHallArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexOfficeArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexStorageArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexMainCorridorArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexArchiveArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexLaboratoryArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexServiceAreaArt.csv",

        L"data/csv/world/AtmosphericEscape/art/AnnexB1EntryArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexB1MainCorridorArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexB1ObservationArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexB1ExperimentalArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexB1CentralResearchArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexB1ControlRoomArt.csv",
        L"data/csv/world/AtmosphericEscape/art/AnnexB1StairwellArt.csv"

    };

    if (!mPlacementTable.LoadFiles(worldPlacementFiles))
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"Failed to load world placement files.\n"
        );

        return false;
    }



    //============================================================
    // Mansion Graybox Initialize
    //============================================================
    if (!mMansion.Initialize(scene))
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"Mansion initialization failed.\n"
        );

        return false;
    }


    //============================================================
    // Mansion Static Blocks
    // Mansion CSVに
    // Prefab = MansionBlock
    // と書かれている全Objectを生成する
    //============================================================
    mBlocksToLoad = mPlacementTable.FindByPrefab("MansionBlock");

    //============================================================
    // Mansion Entrance Door
    //============================================================
    const WorldObjectPlacement* entranceDoorPlacement =
        mPlacementTable.Find("Main1F_EntranceDoor");

    if (entranceDoorPlacement == nullptr)
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"Main1F_EntranceDoor placement not found.\n"
        );

        return false;
    }

    // 通常扉と玄関扉で、同じエフェクト登録処理を使う。
    const auto spawnDoorDust = [this](const XMFLOAT3& thresholdPosition)
        {
            const XMVECTOR worldPosition = XMLoadFloat3(&thresholdPosition);
            mEnvironmentEffects.AddEffect(std::make_unique<DoorDustEffect>(worldPosition, 1.0f));
        };


    if (!mMansionEntranceDoor.Initialize(
        scene,
        interactionSystem,
        entranceDoorPlacement->Id,
        entranceDoorPlacement->Position,
        entranceDoorPlacement->Scale
    ))
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"Mansion Entrance Door initialization failed.\n"
        );

        return false;
    }

    mMansionEntranceDoorInitialized = true;

    mMansionEntranceDoor.SetOpenedCallback(spawnDoorDust);

    //============================================================
    // Mansion Power Terminal
    //============================================================
    const WorldObjectPlacement* powerTerminalPlacement =
        mPlacementTable.Find("Main1F_PowerTerminal");


    if (powerTerminalPlacement == nullptr)
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"Main1F_PowerTerminal placement not found.\n"
        );

        return false;
    }


    //============================================================
    // Initialize
    //============================================================
    if (!mMansionPowerTerminal.Initialize(
        scene,
        interactionSystem,
        powerTerminalPlacement->Id,
        powerTerminalPlacement->Position,
        powerTerminalPlacement->Scale,
        powerTerminalPlacement->Rotation
    ))
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"Mansion Power Terminal initialization failed.\n"
        );

        return false;
    }

    mMansionPowerTerminalInitialized = true;

    //============================================================
    // Mansionはゲーム開始時停電中
    //============================================================
    mMansionPowerEnabled = false;

    //============================================================
    // Main B1 Fuse Box
    //============================================================
    const WorldObjectPlacement* fuseBoxPlacement =
        mPlacementTable.Find("MainB1_FuseBox");


    if (fuseBoxPlacement == nullptr)
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"MainB1_FuseBox placement not found.\n"
        );

        return false;
    }


    if (!mMansionFuseBox.Initialize(
        scene,
        interactionSystem,
        fuseBoxPlacement->Id,
        fuseBoxPlacement->Position,
        fuseBoxPlacement->Scale,
        fuseBoxPlacement->Rotation
    ))
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"MansionFuseBox initialization failed.\n"
        );

        return false;
    }

    mMansionFuseBoxInitialized = true;

    //============================================================
    // Mansion Power Lights
    //============================================================
    auto powerLightPlacements =
        mPlacementTable.FindByPrefab("MansionPowerLight");


    //============================================================
    // ID順へSort
    //============================================================
    std::sort(powerLightPlacements.begin(), powerLightPlacements.end(), [](const WorldObjectPlacement* a, const WorldObjectPlacement* b)
        {
            if (a == nullptr)
            {
                return false;
            }

            if (b == nullptr)
            {
                return true;
            }

            return a->Id < b->Id;
        }
    );

    // キーはMansionArt行の配置ID 同じ器具を複数のライトに指定できる
    for (const WorldObjectPlacement* placement : powerLightPlacements)
    {
        auto light = std::make_unique<MansionPowerLight>(placement->Id, placement->Position);

        if (!placement->DataId.empty())
        {
            mLightsByExcludedArtId.emplace(placement->DataId, light.get());
        }

        mMansionPowerLights.push_back(std::move(light));
    }

    //============================================================
    // Mansion Generic Doors
    //============================================================
    const auto mansionDoorPlacements =
        mPlacementTable.FindByPrefab("MansionDoor");


    for (const WorldObjectPlacement* placement : mansionDoorPlacements)
    {
        if (placement == nullptr)
        {
            continue;
        }

        auto door = std::make_unique<MansionDoor>();

        if (!door->Initialize(
            scene,
            interactionSystem,
            placement->Id,
            placement->Position,
            placement->Scale,
            placement->Rotation
        ))
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"MansionDoor initialization failed.\n"
            );

            return false;
        }

        door->SetRequiredItemId(placement->DataId);

        door->SetOpenedCallback(spawnDoorDust);

        mMansionDoors.push_back(std::move(door));
    }

    //============================================================
    // Courtyard Door
    //============================================================
    MansionDoor* courtyardDoor =
        FindMansionDoorById("Main1F_CourtyardDoor");

    if (courtyardDoor == nullptr)
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"Main1F_CourtyardDoor not found.\n"
        );

        return false;
    }

    //============================================================
    // Game開始時はLock
    //============================================================
    courtyardDoor->SetLocked(true);

    //============================================================
    // Annex B1 Research Seal
    // AnnexBasement.csv:
    // AnnexB1_ResearchSeal,MansionResearchSeal,...
    // をRuntime Objectへ変換する
    //============================================================
    const WorldObjectPlacement* researchSealPlacement =
        mPlacementTable.Find("AnnexB1_ResearchSeal");


    if (researchSealPlacement == nullptr)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"AnnexB1_ResearchSeal placement not found.\n"
        );

#endif

        return false;
    }


    //============================================================
    // Research Seal Initialize
    // ・Scene
    // ・InteractionSystem
    // ・Persistent ID
    // ・Position
    // ・Scale
    //============================================================
    if (!mAnnexResearchSeal.Initialize(
        scene,
        interactionSystem,
        researchSealPlacement->Id,
        researchSealPlacement->Position,
        researchSealPlacement->Scale,
        researchSealPlacement->Rotation
    ))
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"MansionResearchSeal initialization failed.\n"
        );

#endif

        return false;
    }

    mAnnexResearchSealInitialized = true;


    //============================================================
    // Annex B1 Control Room Door
    //============================================================
    mAnnexControlRoomDoor =
        FindMansionDoorById("AnnexB1_ControlRoomDoor");

    //============================================================
    // Placement / Door生成失敗は初期化エラーとして扱う
    //============================================================
    if (mAnnexControlRoomDoor == nullptr)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"AnnexB1_ControlRoomDoor not found.\n"
        );

#endif

        return false;
    }

    //============================================================
    // Research Sealが有効になるまではLock
    //============================================================
    mAnnexControlRoomDoor->SetLocked(true);

    //============================================================
    // Annex B1 Control Terminal
    //============================================================
    const WorldObjectPlacement* controlTerminalPlacement =
        mPlacementTable.Find("AnnexB1_ControlTerminal");


    if (controlTerminalPlacement == nullptr)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"AnnexB1_ControlTerminal placement not found.\n"
        );

#endif
        return false;
    }


    //============================================================
    // Runtime Object初期化
    //============================================================
    if (!mAnnexControlTerminal.Initialize(
        scene,
        interactionSystem,
        controlTerminalPlacement->Id,
        controlTerminalPlacement->Position,
        controlTerminalPlacement->Scale,
        controlTerminalPlacement->Rotation
    ))
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"MansionControlTerminal initialization failed.\n"
        );

#endif
        return false;
    }

    mAnnexControlTerminalInitialized = true;

    //============================================================
    // Annex B1 Safety Puzzle
    // 観察室: 排気弁
    // 実験室: 冷却装置 / 復旧制御盤
    // Reward: AnnexB1_CentralResearchDoor
    //============================================================
    {
        //========================================================
        // CSVから3つの操作パネルを取得
        //========================================================
        const WorldObjectPlacement* vent =
            mPlacementTable.Find("AnnexB1_SafetyVent");

        const WorldObjectPlacement* cooling =
            mPlacementTable.Find("AnnexB1_SafetyCooling");

        const WorldObjectPlacement* controller =
            mPlacementTable.Find("AnnexB1_SafetyController");

        if (vent == nullptr || cooling == nullptr || controller == nullptr)
        {
#if defined(_DEBUG)
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"Annex Safety Puzzle placement missing.\n"
            );
#endif
            return false;
        }

        //========================================================
        // Reward Door取得
        // この扉は実験室と中央研究区画の間に存在する
        //========================================================
        mAnnexCentralResearchDoor = FindMansionDoorById("AnnexB1_CentralResearchDoor");

        if (mAnnexCentralResearchDoor == nullptr)
        {
#if defined(_DEBUG)
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"Annex Central Research Door not found.\n"
            );
#endif
            return false;
        }

        //========================================================
        // 新規ゲームでは扉を施錠しておく
        //========================================================
        mAnnexCentralResearchDoor->SetLocked(true);

        //========================================================
        // 配置順とPanelTypeの値を対応させる
        // 0: Vent
        // 1: Cooling
        // 2: Controller
        //========================================================
        const std::array<XMFLOAT3, 3> positions =
        {
            vent->Position,
            cooling->Position,
            controller->Position
        };

        const std::array<XMFLOAT3, 3> scales =
        {
            vent->Scale,
            cooling->Scale,
            controller->Scale
        };

        const std::array<XMFLOAT3, 3> rotations =
        {
            vent->Rotation,
            cooling->Rotation,
            controller->Rotation
        };


        //========================================================
        // Puzzle生成
        // このIDをObjective・Save・Worldで共通使用する
        //========================================================
        mAnnexSafetyPuzzle = std::make_unique<AnnexSafetyPuzzle>();

        if (!mAnnexSafetyPuzzle->Initialize(
            scene,
            interactionSystem,
            "AnnexSafetyRecoveryPuzzle",
            positions,
            scales,
            rotations
        ))
        {
            mAnnexSafetyPuzzle.reset();
            return false;
        }

        //========================================================
        // Puzzle解決時に扉を解錠
        // 通常操作・Save Load復元の両方に対応する
        //========================================================
        mAnnexSafetyPuzzle->SetSolvedCallback([rewardDoor = mAnnexCentralResearchDoor](bool restored)
            {
                (void)restored;

                if (rewardDoor != nullptr)
                {
                    rewardDoor->SetLocked(false);
                }
            }
        );

        //========================================================
        // PuzzleManagerへ登録
        //========================================================
        if (!mPuzzleManager.Register(mAnnexSafetyPuzzle.get()))
        {
            mAnnexSafetyPuzzle->Terminate(scene, interactionSystem);
            mAnnexSafetyPuzzle.reset();
            return false;
        }
    }

    //============================================================
    // Mansion First Puzzle
    //============================================================
    {
        //========================================================
        // Puzzle Switch Placements
        //========================================================
        const WorldObjectPlacement* moon = mPlacementTable.Find("MansionPuzzle_Moon");
        const WorldObjectPlacement* sun = mPlacementTable.Find("MansionPuzzle_Sun");
        const WorldObjectPlacement* bird = mPlacementTable.Find("MansionPuzzle_Bird");

        if (moon == nullptr || sun == nullptr || bird == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"Mansion first puzzle placement not found.\n"
            );

            return false;
        }

        //========================================================
        // Reward Door
        //========================================================
        MansionDoor* rewardDoor = FindMansionDoorById("Main1F_NorthAccessDoor");


        if (rewardDoor == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"First puzzle reward door not found.\n"
            );

            return false;
        }

        //========================================================
        // Puzzle未解決状態ではDoorをLock
        //========================================================
        rewardDoor->SetLocked(true);

        //========================================================
        // Switch Positions
        //========================================================
        const std::vector<XMFLOAT3> switchPositions =
        {
            moon->Position,
            sun->Position,
            bird->Position
        };

        const std::vector<XMFLOAT3> switchRotations =
        {
            moon->Rotation,
            sun->Rotation,
            bird->Rotation
        };


        const std::vector<std::wstring> switchTexts =
        {
            L"Press Moon",
            L"Press Sun",
            L"Press Bird"
        };

        const std::vector<std::wstring> modelFiles =
        {
            L"puzzle_moon_01.vnm",
            L"puzzle_sun_01.vnm",
            L"puzzle_bird_01.vnm"
        };

        //============================================================
        // 月 → 太陽 → 鳥
        //============================================================
        const std::vector<std::uint32_t> correctOrder =
        {
            0, 1, 2
        };

        //========================================================
        // Puzzle生成
        //========================================================
        mMansionFirstPuzzle = std::make_unique<SequencePuzzle>();

        if (!mMansionFirstPuzzle->Initialize(
            scene,
            interactionSystem,
            "MansionFirstSequencePuzzle",
            switchPositions,
            switchRotations,
            switchTexts,
            L"data/model/mansion/dynamic/puzzle/",
            modelFiles,
            correctOrder
        ))
        {
            mMansionFirstPuzzle.reset();
            return false;
        }

        //========================================================
        // Solved Reward
        // restored == false: Gameplayで解いた
        // restored == true: Save LoadからSolved状態を復元
        // どちらでも最終状態としてDoorはUnlockする
        //========================================================
        mMansionFirstPuzzle->SetSolvedCallback([rewardDoor](bool restored)
            {
                (void)restored;

                if (rewardDoor != nullptr)
                {
                    rewardDoor->SetLocked(false);
                }
            }
        );

        //========================================================
        // PuzzleManagerへ登録
        //========================================================
        if (!mPuzzleManager.Register(mMansionFirstPuzzle.get()))
        {
            return false;
        }
    }

    //============================================================
    // Mansion Second Puzzle
    // 2F West Dressing Room
    // Correct: Rose -> Book -> Clock
    // Reward: Main2F_WestBedroomDoor Unlock
    //============================================================
    {
        //========================================================
        // Puzzle Switch Placements
        //========================================================
        const WorldObjectPlacement* rose =
            mPlacementTable.Find("MansionPuzzle2F_Rose");
        const WorldObjectPlacement* book =
            mPlacementTable.Find("MansionPuzzle2F_Book");
        const WorldObjectPlacement* clock =
            mPlacementTable.Find("MansionPuzzle2F_Clock");
        const WorldObjectPlacement* candle =
            mPlacementTable.Find("MansionPuzzle2F_Candle");

        //========================================================
        // CSVの配置漏れを検出
        //========================================================
        if (rose == nullptr || book == nullptr || clock == nullptr || candle == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"Mansion second puzzle placement not found.\n"
            );

            return false;
        }


        //========================================================
        // Puzzle Reward Door
        // West BedroomにはBasementKeyがある
        // Puzzleを解くまではBedroomへ入れなくする
        //========================================================
        MansionDoor* rewardDoor =
            FindMansionDoorById("Main2F_WestBedroomDoor");


        if (rewardDoor == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"Second puzzle reward door not found.\n"
            );

            return false;
        }


        //========================================================
        // Game開始時はWest BedroomをLock
        //========================================================
        rewardDoor->SetLocked(true);


        //========================================================
        // Switch Position
        //========================================================
        const std::vector<XMFLOAT3> switchPositions =
        {
            rose->Position,
            book->Position,
            clock->Position,
            candle->Position
        };

        const std::vector<XMFLOAT3> switchRotations =
        {
            rose->Rotation,
            book->Rotation,
            clock->Rotation,
            candle->Rotation
        };

        const std::vector<std::wstring> switchTexts =
        {
            L"Press Rose",
            L"Press Book",
            L"Press Clock",
            L"Press Candle"
        };

        const std::vector<std::wstring> modelFiles =
        {
            L"puzzle_rose_01.vnm",     // Index 0
            L"puzzle_book_01.vnm",     // Index 1
            L"puzzle_clock_01.vnm",    // Index 2
            L"puzzle_candle_01.vnm"    // Index 3：不正解
        };

        const std::vector<std::uint32_t> correctOrder =
        {
            0, 1, 2, 3
        };

        //========================================================
        // Puzzle生成
        //========================================================
        mMansionSecondPuzzle = std::make_unique<SequencePuzzle>();

        if (!mMansionSecondPuzzle->Initialize(
            scene,
            interactionSystem,
            "MansionSecondSequencePuzzle",
            switchPositions,
            switchRotations,
            switchTexts,
            L"data/model/mansion/dynamic/puzzle/",
            modelFiles,
            correctOrder
        ))
        {
            mMansionSecondPuzzle.reset();

            return false;
        }


        //========================================================
        // Puzzle Solved
        // Gameplay中に解いた場合も
        // Save LoadによるRestoreの場合も
        // 最終状態としてWest BedroomをUnlockする
        //========================================================
        mMansionSecondPuzzle->SetSolvedCallback([rewardDoor](bool restored)
            {
                (void)restored;

                if (rewardDoor != nullptr)
                {
                    rewardDoor->SetLocked(false);
                }
            }
        );


        //========================================================
        // PuzzleManagerへ登録
        //========================================================
        if (!mPuzzleManager.Register(mMansionSecondPuzzle.get()))
        {
            return false;
        }
    }

    //============================================================
    // Annex Archive Security Puzzle
    //============================================================
    {
        //========================================================
        // 各部屋に配置したスイッチを取得
        //========================================================
        const WorldObjectPlacement* office =
            mPlacementTable.Find("Annex1F_Security_Office");

        const WorldObjectPlacement* storage =
            mPlacementTable.Find("Annex1F_Security_Storage");

        const WorldObjectPlacement* laboratory =
            mPlacementTable.Find("Annex1F_Security_Laboratory");

        //========================================================
        // 配置情報が存在するか確認
        //========================================================
        if (office == nullptr || storage == nullptr || laboratory == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"Annex Archive Puzzle placement missing.\n"
            );

            return false;
        }

        //========================================================
        // 報酬Door
        //========================================================
        MansionDoor* archiveDoor =
            FindMansionDoorById("Annex1F_ArchiveDoor");

        if (archiveDoor == nullptr)
        {
            return false;
        }

        //========================================================
        // 初期状態では資料室を施錠
        //========================================================
        archiveDoor->SetLocked(true);

        const std::vector<XMFLOAT3> positions =
        {
            office->Position,
            storage->Position,
            laboratory->Position
        };

        const std::vector<XMFLOAT3> rotations =
        {
            office->Rotation,
            storage->Rotation,
            laboratory->Rotation
        };

        const std::vector<std::wstring> modelFiles =
        {
            L"annex_security_office_01.vnm",
            L"annex_security_storage_01.vnm",
            L"annex_security_laboratory_01.vnm"
        };

        //============================================================
        // 管理者認証 → 予備電源 → 研究施設認証
        //============================================================
        const std::vector<std::uint32_t> correctOrder =
        {
            0, 1, 2
        };

        //========================================================
        // 操作テキスト
        //========================================================
        const std::vector<std::wstring> texts =
        {
            L"管理者認証を実行する",
            L"予備電源を接続する",
            L"研究施設の認証を実行する"
        };

        //========================================================
        // Puzzle生成
        //========================================================
        mAnnexArchiveSequencePuzzle = std::make_unique<SequencePuzzle>();

        if (!mAnnexArchiveSequencePuzzle->Initialize(
            scene,
            interactionSystem,
            "AnnexArchiveSequencePuzzle",
            positions,
            rotations,
            texts,
            L"data/model/mansion/dynamic/annexPuzzle/",
            modelFiles,
            correctOrder
        ))
        {
            mAnnexArchiveSequencePuzzle.reset();
            return false;
        }

        //========================================================
        // Puzzle解決時の処理
        //========================================================
        mAnnexArchiveSequencePuzzle->SetSolvedCallback([archiveDoor](bool restored)
            {
                (void)restored;

                if (archiveDoor != nullptr)
                {
                    archiveDoor->SetLocked(false);
                }
            }
        );

        //========================================================
        // PuzzleManagerへ登録
        //========================================================
        if (!mPuzzleManager.Register(mAnnexArchiveSequencePuzzle.get()))
        {
            return false;
        }
    }

    //============================================================
    // World Item Pickups
    //============================================================
    const auto itemPlacements =
        mPlacementTable.FindByPrefab("WorldItemPickup");


    for (const WorldObjectPlacement* placement : itemPlacements)
    {
        if (placement == nullptr)
        {
            continue;
        }


        //========================================================
        // DataId必須
        // DataId == Inventory Item ID
        //========================================================
        if (placement->DataId.empty())
        {
#if defined(_DEBUG)

            std::string message =
                "[AtmosphericEscapeWorld] "
                "WorldItemPickup DataId is empty : " +
                placement->Id +
                "\n";


            OutputDebugStringA(message.c_str());

#endif
            return false;
        }

        auto pickup = std::make_unique<WorldItemPickup>();

        if (!pickup->Initialize(
            scene,
            interactionSystem,
            placement->Id,
            placement->DataId,
            placement->Position,
            placement->Scale
        ))
        {
            return false;
        }

        mWorldItemPickups.push_back(std::move(pickup));
    }

    //============================================================
    // Document Pickups
    //============================================================
    const auto documentPlacements =
        mPlacementTable.FindByPrefab("DocumentPickup");


    for (const WorldObjectPlacement* placement : documentPlacements)
    {
        if (placement == nullptr)
        {
            continue;
        }


        //========================================================
        // Document ID必須
        //========================================================
        if (placement->DataId.empty())
        {
#if defined(_DEBUG)

            const std::string message =
                "[AtmosphericEscapeWorld] "
                "DocumentPickup DataId is empty : " +
                placement->Id +
                "\n";


            OutputDebugStringA(message.c_str());

#endif

            return false;
        }

        //========================================================
        // Runtime Document Pickup生成
        //========================================================
        auto pickup = std::make_unique<DocumentPickup>();


        if (!pickup->Initialize(
            scene,
            interactionSystem,
            placement->Id,
            placement->DataId,
            placement->Position,
            placement->Scale)
            )
        {
            return false;
        }

        mDocumentPickups.push_back(std::move(pickup));
    }

    // 配置ファイルの読み込み完了後に、処理対象の行を保持する。
    mArtPlacementsToLoad = mPlacementTable.FindByPrefab("MansionArt");
    return true;
}

bool AtmosphericEscapeWorld::FinishInitialize(
    vnScene* scene, InteractionSystem* interactionSystem)
{
    ////============================================================
    //// MansionHideGraybox - CSVによるVisual切り替え
    ////============================================================
    //{
    //    const auto hidePlacements = mPlacementTable.FindByPrefab("MansionHideGraybox");

    //    for (const WorldObjectPlacement* placement : hidePlacements)
    //    {
    //        if (placement == nullptr)
    //        {
    //            continue;
    //        }

    //        //====================================================
    //        // DataIdには対象のGraybox IDが必要
    //        //====================================================
    //        if (placement->DataId.empty())
    //        {
    //            OutputDebugStringA("[MansionHideGraybox] Target ID is empty.\n");
    //            return false;
    //        }

    //        //====================================================
    //        // 対応するGrayboxの描画とShadowをOFFにする
    //        //====================================================
    //        if (!mMansion.SetBlockVisualEnabled(
    //            placement->DataId,
    //            false))
    //        {
    //            const std::string message =
    //                "[MansionHideGraybox] Target not found: " +
    //                placement->DataId + "\n";
    //
    //            OutputDebugStringA(message.c_str());
    //            return false;
    //        }
    //    }
    //}


    if (mTestFacilityEnabled)
    {
        //========================================================
        // Ground
        //========================================================
        if (!CreateGrayboxGround(scene))
        {
            return false;
        }


        //========================================================
        // Forest
        //========================================================
        if (!CreateGrayboxForest(scene))
        {
            return false;
        }

        //============================================================
        // Forest Guidance Light
        //============================================================
        const WorldObjectPlacement* guidancePlacement =
            mPlacementTable.Find("GuidanceLight");


        if (guidancePlacement == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"GuidanceLight placement not found.\n"
            );

            return false;
        }

        const bool guidanceLightInitialized =
            mForestGuidanceLight.Initialize(
                scene,
                guidancePlacement->Position
            );

        if (!guidanceLightInitialized)
        {
            return false;
        }

        //============================================================
        // Facility
        //============================================================
        const WorldObjectPlacement* facilityPlacement =
            mPlacementTable.Find("FacilityMain");

        if (facilityPlacement == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"FacilityMain placement not found.\n"
            );

            return false;
        }

        const WorldObjectPlacement* terminalPlacement =
            mPlacementTable.Find("FacilityTerminal");


        if (terminalPlacement == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"FacilityTerminal placement not found.\n"
            );

            return false;
        }

        //============================================================
        // Inner Door Placement
        //============================================================
        const WorldObjectPlacement* innerDoorPlacement =
            mPlacementTable.Find("FacilityInnerDoor");


        if (innerDoorPlacement == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"FacilityInnerDoor placement not found.\n"
            );

            return false;
        }

        const bool facilityInitialized =
            mFacility.Initialize(
                scene,
                interactionSystem,
                facilityPlacement->Position,
                terminalPlacement->Id,
                terminalPlacement->Position,
                terminalPlacement->Scale,
                innerDoorPlacement->Position
            );


        if (!facilityInitialized)
        {
            return false;
        }

        //============================================================
        // Facility Power Lights
        //============================================================
        const auto facilityLights =
            mPlacementTable.FindByPrefab("FacilityLight");


        for (const WorldObjectPlacement* placement : facilityLights)
        {
            if (placement == nullptr)
            {
                continue;
            }

            const bool lightCreated =
                mFacility.AddPowerLight(
                    scene,
                    placement->Position,
                    placement->Scale
                );

            if (!lightCreated)
            {
                OutputDebugStringW(
                    L"[AtmosphericEscapeWorld] "
                    L"Facility Power Light initialization failed.\n"
                );

                return false;
            }
        }

        //============================================================
        // CSV Facility Blocks
        // Prefab == FacilityBlock のObjectを
        // すべてFacilityへ追加する
        //============================================================
        const auto facilityBlocks =
            mPlacementTable.FindByPrefab("FacilityBlock");


        for (const WorldObjectPlacement* placement : facilityBlocks)
        {
            if (placement == nullptr)
            {
                continue;
            }

            const bool created =
                mFacility.AddStaticBlock(
                    scene,
                    placement->Position,
                    placement->Scale
                );

            if (!created)
            {
                OutputDebugStringW(
                    L"[AtmosphericEscapeWorld] "
                    L"Failed to create FacilityBlock.\n"
                );

                return false;
            }
        }

        //============================================================
        // Facility Interactive Doors
        //============================================================
        const auto doorPlacements =
            mPlacementTable.FindByPrefab("FacilityDoor");


        for (const WorldObjectPlacement* placement : doorPlacements)
        {
            if (placement == nullptr)
            {
                continue;
            }

            if (!mFacility.AddInteractiveDoor(
                scene,
                interactionSystem,
                placement->Id,
                placement->Position
            ))
            {
                OutputDebugStringW(
                    L"[AtmosphericEscapeWorld] "
                    L"Facility Door initialization failed.\n"
                );


                return false;
            }
        }

        //============================================================
        // Facility Access Key
        //============================================================
        const WorldObjectPlacement* keyPlacement =
            mPlacementTable.Find("FacilityAccessKey");


        if (keyPlacement == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"FacilityAccessKey placement not found.\n"
            );

            return false;
        }


        if (!mFacility.InitializeAccessKey(
            scene,
            interactionSystem,
            keyPlacement->Id,
            keyPlacement->Position,
            keyPlacement->Scale
        ))
        {
            return false;
        }

        //============================================================
        // Facility Exit Door
        //============================================================
        const WorldObjectPlacement* exitDoorPlacement =
            mPlacementTable.Find("FacilityExitDoor");


        if (exitDoorPlacement == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"FacilityExitDoor placement not found.\n"
            );

            return false;
        }


        if (!mFacility.InitializeExitDoor(
            scene,
            interactionSystem,
            "FacilityExitDoor",
            exitDoorPlacement->Position
        ))
        {
            return false;
        }

        const WorldObjectPlacement* moon =
            mPlacementTable.Find("PuzzleSwitchMoon");
        const WorldObjectPlacement* sun =
            mPlacementTable.Find("PuzzleSwitchSun");
        const WorldObjectPlacement* bird =
            mPlacementTable.Find("PuzzleSwitchBird");

        if (moon == nullptr || sun == nullptr || bird == nullptr)
        {
            return false;
        }

        const std::array<XMFLOAT3, 3>switchPositions =
        {
            moon->Position,
            sun->Position,
            bird->Position
        };

        //============================================================
        // Sequence Puzzle
        //============================================================
        if (!mFacility.InitializeSequencePuzzle(
            scene,
            interactionSystem,
            "PuzzleSequence01",
            switchPositions,
            "PuzzleRewardDoor"
        ))
        {
            return false;
        }


        //============================================================
        // Escape Trigger
        //============================================================
        const WorldObjectPlacement* escapeTriggerPlacement =
            mPlacementTable.Find("EscapeTrigger");


        if (escapeTriggerPlacement == nullptr)
        {
            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"EscapeTrigger placement not found.\n"
            );

            return false;
        }

        SequencePuzzle* sequencePuzzle = mFacility.GetSequencePuzzle();

        if (sequencePuzzle == nullptr)
        {
            return false;
        }

        if (!mPuzzleManager.Register(sequencePuzzle))
        {
            return false;
        }

        //============================================================
        // CSV Transform保存
        //============================================================
        mEscapeTriggerPosition = escapeTriggerPlacement->Position;
        mEscapeTriggerScale = escapeTriggerPlacement->Scale;
        mEscapeTriggerValid = true;

    }

    // 配置テーブルから、各ろうそくの芯を取得する
    for (const WorldObjectPlacement* placement : mPlacementTable.FindByPrefab("MansionCandle"))
    {
        if (placement == nullptr) continue;

        auto& candle = mMansionCandles.emplace_back(placement->Id, placement->Position, placement->Scale.x);

        mEnvironmentEffects.AddEffect(candle.CreateFlame());
    }

    return true;
}

//============================================================
// Terminate
//============================================================
void AtmosphericEscapeWorld::Terminate(vnScene* scene, InteractionSystem* interactionSystem)
{
    if (scene == nullptr)
    {
        return;
    }

    ReleaseInitializeCaches();
    mBlocksToLoad.clear();
    mArtPlacementsToLoad.clear();
    mLightsByExcludedArtId.clear();
    mNextBlockPlacement = 0;
    mNextArtPlacement = 0;
    mInitializeStage = InitializeStage::Idle;

    (void)interactionSystem;

    mForestColliders.clear();

    mGroundCollider.reset();

    mPuzzleManager.Clear();
    //========================================================
    // Forest Object
    //========================================================
    for (ModelBase* object : mForestObjects)
    {
        if (object != nullptr)
        {
            scene->deleteObject(object);
        }
    }

    mMansionArt.Terminate(scene);
    mFacility.Terminate(scene, interactionSystem);
    mForestGuidanceLight.Terminate(scene);
    mForestObjects.clear();

    //============================================================
    // Mansion Entrance Door
    //============================================================
    if (mMansionEntranceDoorInitialized)
    {
        mMansionEntranceDoor.Terminate(scene, interactionSystem);
        mMansionEntranceDoorInitialized = false;
    }

    //============================================================
    // Mansion Power Terminal
    //============================================================
    if (mMansionPowerTerminalInitialized)
    {
        mMansionPowerTerminal.Terminate(scene, interactionSystem);
        mMansionPowerTerminalInitialized = false;
    }

    mMansion.Terminate(scene);

    mMansionPowerLights.clear();

    mMansionPowerRestoreSequenceActive = false;
    mMansionPowerRestoreSequenceFinished = false;
    mMansionPowerRestoreNextLight = 0;
    mMansionPowerRestoreTimer = 0.0f;

    //============================================================
    // Mansion Fuse Box
    //============================================================
    if (mMansionFuseBoxInitialized)
    {
        mMansionFuseBox.Terminate(scene, interactionSystem);
        mMansionFuseBoxInitialized = false;
    }

    //============================================================
    // Mansion Doors
    //============================================================
    for (auto& door : mMansionDoors)
    {
        if (door != nullptr)
        {
            door->Terminate(scene, interactionSystem);
        }
    }

    mMansionDoors.clear();

    //============================================================
    // Mansion First Puzzle
    //============================================================
    if (mMansionFirstPuzzle != nullptr)
    {
        mMansionFirstPuzzle->Terminate(scene, interactionSystem);
        mMansionFirstPuzzle.reset();
    }

    //============================================================
    // Mansion Second Puzzle
    //============================================================
    if (mMansionSecondPuzzle != nullptr)
    {
        mMansionSecondPuzzle->Terminate(scene, interactionSystem);
        mMansionSecondPuzzle.reset();
    }

    //============================================================
    // Annex Archive Puzzle終了
    //============================================================
    if (mAnnexArchiveSequencePuzzle != nullptr)
    {
        mAnnexArchiveSequencePuzzle->Terminate(scene, interactionSystem);
        mAnnexArchiveSequencePuzzle.reset();
    }

    //============================================================
    // Annex B1 Research Seal
    //============================================================
    if (mAnnexResearchSealInitialized)
    {
        mAnnexResearchSeal.Terminate(scene, interactionSystem);
        mAnnexResearchSealInitialized = false;
    }

    mAnnexControlRoomDoor = nullptr;

    //============================================================
    // Annex B1 Control Terminal
    //============================================================
    if (mAnnexControlTerminalInitialized)
    {
        mAnnexControlTerminal.Terminate(scene, interactionSystem);
        mAnnexControlTerminalInitialized = false;
    }

    //============================================================
    // Annex B1 Safety Puzzle
    //============================================================
    if (mAnnexSafetyPuzzle != nullptr)
    {
        mAnnexSafetyPuzzle->Terminate(scene, interactionSystem);
        mAnnexSafetyPuzzle.reset();
    }

    mAnnexCentralResearchDoor = nullptr;

    //============================================================
    // World Item Pickups
    //============================================================
    for (auto& pickup : mWorldItemPickups)
    {
        if (pickup != nullptr)
        {
            pickup->Terminate(scene, interactionSystem);
        }
    }

    mWorldItemPickups.clear();

    //============================================================
    // Document Pickups
    //============================================================
    for (auto& pickup : mDocumentPickups)
    {
        if (pickup != nullptr)
        {
            pickup->Terminate(scene, interactionSystem);
        }
    }

    mDocumentPickups.clear();

    //========================================================
    // Ground
    //========================================================
    if (mGround != nullptr)
    {
        scene->deleteObject(mGround);
        mGround = nullptr;
    }

    mPlacementTable.Clear();

    mEnvironmentEffects.Clear();
}

//============================================================
// Update
//============================================================
void AtmosphericEscapeWorld::Update(float dt)
{
    //============================================================
    // Mansion Power Restore Sequence
    //============================================================
    if (mMansionPowerRestoreSequenceActive)
    {
        mMansionPowerRestoreTimer += dt;

        while (mMansionPowerRestoreTimer >=
            mMansionPowerRestoreInterval &&
            mMansionPowerRestoreNextLight <
            mMansionPowerLights.size())
        {
            mMansionPowerRestoreTimer -= mMansionPowerRestoreInterval;

            MansionPowerLight* light = mMansionPowerLights[mMansionPowerRestoreNextLight].get();

            if (light != nullptr)
            {
                light->SetPowered(true);
            }

            ++mMansionPowerRestoreNextLight;
        }


        //========================================================
        // 全Light点灯
        //========================================================
        if (mMansionPowerRestoreNextLight >= mMansionPowerLights.size())
        {
            mMansionPowerRestoreSequenceActive = false;
            mMansionPowerRestoreSequenceFinished = true;

#if defined(_DEBUG)

            OutputDebugStringW(
                L"[AtmosphericEscapeWorld] "
                L"Mansion power restore sequence finished.\n"
            );

#endif
        }
    }

    //============================================================
    // Powered Light Update
    // 点灯済みLightだけPoint Lightを登録する
    //============================================================
    for (const auto& light : mMansionPowerLights)
    {
        if (light != nullptr)
        {
            light->Update();
        }
    }

    //============================================================
    // ろうそくは屋敷の電源から独立して点灯する
    //============================================================
    for (auto& candle : mMansionCandles)
    {
        candle.UpdateLight(dt);
    }

    //============================================================
    // Mansion
    //============================================================
    if (mMansionEntranceDoorInitialized)
    {
        mMansionEntranceDoor.Update(dt);
    }

    //============================================================
    // Mansion Generic Doors
    //============================================================
    for (auto& door : mMansionDoors)
    {
        if (door != nullptr)
        {
            door->Update(dt);
        }
    }

    if (mTestFacilityEnabled)
    {
        mForestGuidanceLight.Update();

        mFacility.Update(dt);
    }

    //============================================================
    // Fuse Box -> Courtyard Door
    //============================================================
    if (mMansionFuseBoxInitialized && mMansionFuseBox.IsActivated())
    {
        MansionDoor* courtyardDoor =
            FindMansionDoorById("Main1F_CourtyardDoor");

        if (courtyardDoor != nullptr)
        {
            if (courtyardDoor->IsLocked())
            {
                courtyardDoor->SetLocked(false);
#if defined(_DEBUG)

                OutputDebugStringW(
                    L"[AtmosphericEscapeWorld] "
                    L"Courtyard Door unlocked by Fuse Box.\n"
                );
#endif
            }
        }
    }

    //============================================================
    // Annex Safety Puzzle -> Central Research Door
    // 安全装置の復旧状態を扉へ同期する
    // Save/Loadや状態変化後にも整合性を保つ
    //============================================================
    if (mAnnexCentralResearchDoor != nullptr)
    {
        const bool safetySolved =
            mAnnexSafetyPuzzle != nullptr && mAnnexSafetyPuzzle->IsSolved();

        //========================================================
        // 既存の進行済みセーブへの配慮
        // ※ResearchSealがすでに起動している場合は
        // ※新しく追加したパズルが未登録だったとしても
        // ※中央研究区画の扉を通行可能にする
        //========================================================
        const bool researchSealActivated =
            mAnnexResearchSealInitialized && mAnnexResearchSeal.IsActivated();

        const bool terminalActivated =
            mAnnexControlTerminalInitialized && mAnnexControlTerminal.IsActivated();

        const bool accessGranted =
            safetySolved || researchSealActivated || terminalActivated;

        mAnnexCentralResearchDoor->SetLocked(!accessGranted);
    }

    //============================================================
    // Research Seal -> Control Room Door
    // Sealを使用したフレームですぐDoorを解除する
    //============================================================
    if (mAnnexControlRoomDoor != nullptr)
    {
        mAnnexControlRoomDoor->SetLocked(!mAnnexResearchSeal.IsActivated());
    }
}

void AtmosphericEscapeWorld::UpdateEffects(float dt)
{
    mEnvironmentEffects.Update(dt);
}

void AtmosphericEscapeWorld::RenderFullResolutionEffects()
{
    mEnvironmentEffects.RenderFullResolution();
}

void AtmosphericEscapeWorld::AppendColliders(std::vector<Collider*>& destination) const
{

    if (mTestFacilityEnabled)
    {
        //========================================================
        // Ground
        //========================================================
        if (mGroundCollider != nullptr)
        {
            destination.push_back(mGroundCollider.get());
        }


        //========================================================
        // Forest
        //========================================================
        for (const auto& collider : mForestColliders)
        {
            if (collider != nullptr)
            {
                destination.push_back(collider.get());
            }
        }

        //============================================================
        // Facility
        //============================================================
        mFacility.AppendColliders(destination);
    }

    //============================================================
    // Mansion
    //============================================================
    mMansion.AppendColliders(destination);

    //============================================================
    // Mansion Entrance Door
    //============================================================
    if (mMansionEntranceDoorInitialized)
    {
        if (mMansionEntranceDoorInitialized)
        {
            mMansionEntranceDoor.AppendColliders(destination);
        }
    }

    //============================================================
    // Mansion Power Terminal
    //============================================================
    if (mMansionPowerTerminalInitialized)
    {
        Collider* terminalCollider = mMansionPowerTerminal.GetCollider();

        if (terminalCollider != nullptr)
        {
            destination.push_back(terminalCollider);
        }
    }

    //============================================================
    // Mansion Doors
    //============================================================
    for (const auto& door : mMansionDoors)
    {
        if (door == nullptr)
        {
            continue;
        }

        Collider* collider = door->GetCollider();

        if (collider != nullptr)
        {
            destination.push_back(collider);
        }
    }

    //============================================================
    // Mansion Fuse Box
    //============================================================
    if (mMansionFuseBoxInitialized)
    {
        Collider* collider = mMansionFuseBox.GetCollider();

        if (collider != nullptr)
        {
            destination.push_back(collider);
        }
    }
}

void AtmosphericEscapeWorld::AppendAIDoors(std::vector<INavigationDoor*>& destination)
{
    if (mTestFacilityEnabled)
    {
        mFacility.AppendAIDoors(destination);
    }

    //========================================================
    // Mansion
    //========================================================
    for (auto& door : mMansionDoors)
    {
        if (door != nullptr)
        {
            destination.push_back(door.get());
        }
    }
}

bool AtmosphericEscapeWorld::IsInsideEscapeTrigger(const XMVECTOR& worldPosition) const
{
    if (!mEscapeTriggerValid)
    {
        return false;
    }

    //========================================================
    // World Position
    //========================================================
    const float x = XMVectorGetX(worldPosition);
    const float y = XMVectorGetY(worldPosition);
    const float z = XMVectorGetZ(worldPosition);


    //========================================================
    // Half Extents
    //========================================================
    const float halfX =
        mEscapeTriggerScale.x * 0.5f;
    const float halfY =
        mEscapeTriggerScale.y * 0.5f;
    const float halfZ =
        mEscapeTriggerScale.z * 0.5f;

    //========================================================
    // AABB
    //========================================================
    const float minX =
        mEscapeTriggerPosition.x - halfX;
    const float maxX =
        mEscapeTriggerPosition.x + halfX;
    const float minY =
        mEscapeTriggerPosition.y - halfY;
    const float maxY =
        mEscapeTriggerPosition.y + halfY;
    const float minZ =
        mEscapeTriggerPosition.z - halfZ;
    const float maxZ =
        mEscapeTriggerPosition.z + halfZ;


    //========================================================
    // Inside
    //========================================================
    return
        x >= minX &&
        x <= maxX &&
        y >= minY &&
        y <= maxY &&
        z >= minZ &&
        z <= maxZ;
}

bool AtmosphericEscapeWorld::IsPuzzleSolved(const std::string& puzzleId) const
{
    return mPuzzleManager.IsSolved(puzzleId);
}

void AtmosphericEscapeWorld::SetNoiseSystem(NoiseSystem* noiseSystem)
{
    mNoiseSystem = noiseSystem;
    mFacility.SetNoiseSystem(noiseSystem);
    if (mMansionPowerTerminalInitialized) mMansionPowerTerminal.SetNoiseSystem(noiseSystem);
    for (auto& door : mMansionDoors)
    {
        if (door != nullptr)
        {
            door->SetNoiseSystem(noiseSystem);
        }
    }
}

void AtmosphericEscapeWorld::SetDocumentCollectedCallback(std::function<void(const std::string&)> callback)
{
    //============================================================
    // World側でも保持
    //============================================================
    mDocumentCollectedCallback = std::move(callback);

    //============================================================
    // 既に生成済みのDocument Pickupへ配布
    //============================================================
    for (auto& pickup : mDocumentPickups)
    {
        if (pickup == nullptr)
        {
            continue;
        }

        pickup->SetDocumentCollectedCallback(mDocumentCollectedCallback);
    }
}

void AtmosphericEscapeWorld::SetInventoryManager(InventoryManager* inventoryManager)
{
    for (auto& pickup : mWorldItemPickups)
    {
        if (pickup != nullptr)
        {
            pickup->SetInventoryManager(inventoryManager);
        }
    }

    //============================================================
    // Mansion Doors
    // BasementKey / AnnexKeyなど
    // Inventory ItemでUnlockするDoorにも渡す
    //============================================================
    for (auto& door : mMansionDoors)
    {
        if (door != nullptr)
        {
            door->SetInventoryManager(inventoryManager);
        }
    }

    //============================================================
    // Mansion Fuse Box
    //============================================================
    if (mMansionFuseBoxInitialized)
    {
        mMansionFuseBox.SetInventoryManager(inventoryManager);
    }

    //============================================================
    // Annex Research Seal
    //============================================================
    if (mAnnexResearchSealInitialized)
    {
        mAnnexResearchSeal.SetInventoryManager(inventoryManager);
    }
}

void AtmosphericEscapeWorld::SetDocumentManager(DocumentManager* documentManager)
{
    //============================================================
    // 全DocumentPickupへ同じManagerを渡す
    //============================================================
    for (auto& pickup : mDocumentPickups)
    {
        if (pickup == nullptr)
        {
            continue;
        }
        pickup->SetDocumentManager(documentManager);
    }
}

void AtmosphericEscapeWorld::SetMansionInteractionEnabled(
    bool powerTerminalEnabled,
    bool firstPuzzleEnabled,
    bool secondPuzzleEnabled
)
{
    mMansionPowerTerminal.SetInteractionEnabled(powerTerminalEnabled);

    if (mMansionFirstPuzzle)
    {
        mMansionFirstPuzzle->SetInteractionEnabled(firstPuzzleEnabled);
    }

    if (mMansionSecondPuzzle)
    {
        mMansionSecondPuzzle->SetInteractionEnabled(secondPuzzleEnabled);
    }
}

void AtmosphericEscapeWorld::SetMansionPowerEnabled(bool enabled)
{
    if (mMansionPowerEnabled == enabled)
    {
        return;
    }

    mMansionPowerEnabled = enabled;

#if defined(_DEBUG)

    OutputDebugStringW(
        enabled
        ? L"[AtmosphericEscapeWorld] Mansion Power ON.\n"
        : L"[AtmosphericEscapeWorld] Mansion Power OFF.\n"
    );

#endif
    //========================================================
    // Load / Immediate RestoreなのでSequence停止
    //========================================================
    mMansionPowerRestoreSequenceActive = false;

    mMansionPowerRestoreSequenceFinished = enabled;

    mMansionPowerRestoreTimer = 0.0f;


    if (enabled)
    {
        mMansionPowerRestoreNextLight = mMansionPowerLights.size();
    }
    else
    {
        mMansionPowerRestoreNextLight = 0;
    }


    //========================================================
    // 全Lightへ即時反映
    //========================================================
    for (const auto& light :
        mMansionPowerLights)
    {
        if (light != nullptr)
        {
            light->SetPowered(enabled);
        }
    }
}

bool AtmosphericEscapeWorld::BeginMansionPowerRestore()
{
    //========================================================
    // すでに電源ONなら再生しない
    //========================================================
    if (mMansionPowerEnabled)
    {
        return false;
    }

    mMansionPowerEnabled = true;

    //========================================================
    // Sequence初期化
    //========================================================
    mMansionPowerRestoreSequenceActive = true;
    mMansionPowerRestoreSequenceFinished = false;
    mMansionPowerRestoreNextLight = 0;
    mMansionPowerRestoreTimer = 0.0f;


    //========================================================
    // 念のため全照明をOFF状態から開始
    //========================================================
    for (const auto& light : mMansionPowerLights)
    {
        if (light != nullptr)
        {
            light->SetPowered(false);
        }
    }

#if defined(_DEBUG)

    OutputDebugStringW(
        L"[AtmosphericEscapeWorld] "
        L"Mansion power restore sequence started.\n"
    );

#endif

    return true;
}

//============================================================
// Persistent Objects
//============================================================
void AtmosphericEscapeWorld::AppendPersistentObjects(std::vector<IPersistentWorldObject*>& destination)
{
    //========================================================
    // Mansion
    //========================================================
    if (mMansionPowerTerminalInitialized)
    {
        destination.push_back(&mMansionPowerTerminal);
    }

    if (mTestFacilityEnabled)
    {
        //========================================================
        // Facility以下のPersistent Objectを追加
        //========================================================
        mFacility.AppendPersistentObjects(destination);
    }

    for (auto& door : mMansionDoors)
    {
        if (door != nullptr)
        {
            destination.push_back(door.get());
        }
    }
    //============================================================
    // Mansion First Puzzle
    //============================================================
    if (mMansionFirstPuzzle != nullptr)
    {
        destination.push_back(mMansionFirstPuzzle.get());
    }

    //============================================================
    // Mansion Second Puzzle
    //============================================================
    if (mMansionSecondPuzzle != nullptr)
    {
        destination.push_back(mMansionSecondPuzzle.get());
    }

    //============================================================
    // World Item Pickups
    //============================================================
    for (auto& pickup : mWorldItemPickups)
    {
        if (pickup != nullptr)
        {
            destination.push_back(pickup.get());
        }
    }

    //============================================================
    // Mansion Fuse Box
    //============================================================
    if (mMansionFuseBoxInitialized)
    {
        destination.push_back(&mMansionFuseBox);
    }

    //============================================================
    // Annex B1 Research Seal
    // StoneEmblem挿入済み状態をSave対象にする
    //============================================================
    if (mAnnexResearchSealInitialized)
    {
        destination.push_back(&mAnnexResearchSeal);
    }

    //============================================================
    // Annex B1 Control Terminal
    //============================================================
    if (mAnnexControlTerminalInitialized)
    {
        destination.push_back(&mAnnexControlTerminal);
    }

    //============================================================
    // Document Pickups
    //============================================================
    for (auto& pickup : mDocumentPickups)
    {
        if (pickup != nullptr)
        {
            destination.push_back(pickup.get());
        }
    }

    //============================================================
    // Annex Archive Puzzle
    //============================================================
    if (mAnnexArchiveSequencePuzzle != nullptr)
    {
        destination.push_back(mAnnexArchiveSequencePuzzle.get());
    }

    //============================================================
    // Annex B1 Safety Puzzle
    //============================================================
    if (mAnnexSafetyPuzzle != nullptr)
    {
        destination.push_back(mAnnexSafetyPuzzle.get());
    }
}


//============================================================
// Const Version
//============================================================
void AtmosphericEscapeWorld::AppendPersistentObjects(std::vector<const IPersistentWorldObject*>& destination) const
{
    if (mMansionPowerTerminalInitialized)
    {
        destination.push_back(&mMansionPowerTerminal);
    }

    if (mTestFacilityEnabled)
    {
        mFacility.AppendPersistentObjects(destination);
    }

    for (const auto& door : mMansionDoors)
    {
        if (door != nullptr)
        {
            destination.push_back(door.get());
        }
    }

    //============================================================
    // Mansion First Puzzle
    //============================================================
    if (mMansionFirstPuzzle != nullptr)
    {
        destination.push_back(mMansionFirstPuzzle.get());
    }

    //============================================================
    // Mansion Second Puzzle
    //============================================================
    if (mMansionSecondPuzzle != nullptr)
    {
        destination.push_back(mMansionSecondPuzzle.get());
    }

    //============================================================
    // World Item Pickups
    //============================================================
    for (const auto& pickup : mWorldItemPickups)
    {
        if (pickup != nullptr)
        {
            destination.push_back(pickup.get());
        }
    }

    //============================================================
    // Mansion Fuse Box
    //============================================================
    if (mMansionFuseBoxInitialized)
    {
        destination.push_back(&mMansionFuseBox);
    }

    //============================================================
    // Annex B1 Research Seal
    // StoneEmblem挿入済み状態をSave対象にする
    //============================================================
    if (mAnnexResearchSealInitialized)
    {
        destination.push_back(&mAnnexResearchSeal);
    }

    //============================================================
    // Annex B1 Control Terminal
    //============================================================
    if (mAnnexControlTerminalInitialized)
    {
        destination.push_back(&mAnnexControlTerminal);
    }

    //============================================================
    // Document Pickups
    //============================================================
    for (const auto& pickup : mDocumentPickups)
    {
        if (pickup != nullptr)
        {
            destination.push_back(pickup.get());
        }
    }

    //============================================================
    // Annex Archive Puzzle
    //============================================================
    if (mAnnexArchiveSequencePuzzle != nullptr)
    {
        destination.push_back(mAnnexArchiveSequencePuzzle.get());
    }
}

void AtmosphericEscapeWorld::RestorePersistentObjects(const std::vector<WorldObjectSaveState>& states)
{
    //============================================================
    // Runtime Object収集
    //============================================================
    std::vector<IPersistentWorldObject*> runtimeObjects;

    AppendPersistentObjects(runtimeObjects);

    //============================================================
    // ID Map
    //============================================================
    std::unordered_map<std::string, IPersistentWorldObject*> objectMap;


    for (IPersistentWorldObject* object : runtimeObjects)
    {
        if (object == nullptr)
        {
            continue;
        }


        objectMap.emplace(
            object->GetPersistentId(),
            object
        );
    }


    //============================================================
    // 全Object自身の状態を復元
    //============================================================
    for (const WorldObjectSaveState& state : states)
    {
        const auto found = objectMap.find(state.Id);

        if (found == objectMap.end())
        {
            continue;
        }

        found->second->RestoreSaveState(state);
    }

    //============================================================
    // Mansion Derived State
    //
    // Terminal Activated
    //      ↓
    // Mansion Power
    //============================================================
    if (mMansionPowerTerminalInitialized)
    {
        SetMansionPowerEnabled(mMansionPowerTerminal.IsActivated());
    }

    if (mTestFacilityEnabled)
    {
        //============================================================
        // 全ObjectのRestore終了後に
        // Object間の依存状態を再構築する
        //============================================================
        mFacility.RebuildPersistentDerivedState();
    }

    //============================================================
    // Mansion
    //============================================================
    RebuildMansionPersistentDerivedState();

#if defined(_DEBUG)

    OutputDebugStringW(
        L"[AtmosphericEscapeWorld] "
        L"Persistent objects restored.\n"
    );

#endif
}

void AtmosphericEscapeWorld::RebuildMansionPersistentDerivedState()
{
    //============================================================
    // First Puzzle
    //      ↓
    // North Access Door
    //============================================================
    {
        MansionDoor* rewardDoor =
            FindMansionDoorById("Main1F_NorthAccessDoor");


        if (rewardDoor != nullptr)
        {
            const bool solved = mMansionFirstPuzzle != nullptr && mMansionFirstPuzzle->IsSolved();

            //====================================================
            // Puzzle未解決: Locked
            // Puzzle解決済み: Unlocked
            //====================================================
            rewardDoor->SetLocked(!solved);
        }
    }

    //============================================================
    // Second Puzzle
    //      ↓
    // West Bedroom Door
    //============================================================
    {
        MansionDoor* rewardDoor =
            FindMansionDoorById("Main2F_WestBedroomDoor");

        if (rewardDoor != nullptr)
        {
            //====================================================
            // Puzzleの復元結果からDoor状態を決める
            //====================================================
            const bool solved = mMansionSecondPuzzle != nullptr && mMansionSecondPuzzle->IsSolved();
            rewardDoor->SetLocked(!solved);
        }
    }

    //============================================================
    // Basement Fuse Box
    //      ↓
    // Courtyard Door
    //============================================================
    {
        MansionDoor* courtyardDoor =
            FindMansionDoorById("Main1F_CourtyardDoor");


        if (courtyardDoor != nullptr)
        {
            const bool fuseBoxActivated = mMansionFuseBoxInitialized && mMansionFuseBox.IsActivated();

            //====================================================
            // Fuse Box OFF: Locked
            // Fuse Box ON: Unlocked
            //====================================================
            courtyardDoor->SetLocked(!fuseBoxActivated);
        }
    }

    //============================================================
    // Control Room Door
    //============================================================
    if (mAnnexControlRoomDoor != nullptr)
    {
        mAnnexControlRoomDoor->SetLocked(!mAnnexResearchSeal.IsActivated());
    }

    //============================================================
    // Annex Archive Puzzle
    //============================================================
    {
        MansionDoor* archiveDoor =
            FindMansionDoorById("Annex1F_ArchiveDoor");

        if (archiveDoor != nullptr)
        {
            const bool solved = mAnnexArchiveSequencePuzzle != nullptr && mAnnexArchiveSequencePuzzle->IsSolved();

            archiveDoor->SetLocked(!solved);
        }
    }

    //============================================================
    // Annex B1 Safety Puzzle
    //      ↓
    // Central Research Door
    // 復元済みPuzzleのStateから施錠状態を再構築する
    //============================================================
    if (mAnnexCentralResearchDoor != nullptr)
    {
        const bool safetySolved =
            mAnnexSafetyPuzzle != nullptr && mAnnexSafetyPuzzle->IsSolved();

        const bool researchSealActivated =
            mAnnexResearchSealInitialized && mAnnexResearchSeal.IsActivated();

        const bool terminalActivated =
            mAnnexControlTerminalInitialized && mAnnexControlTerminal.IsActivated();

        const bool accessGranted =
            safetySolved || researchSealActivated || terminalActivated;

        mAnnexCentralResearchDoor->SetLocked(!accessGranted);
    }
}

//============================================================
// Ground
//============================================================
bool AtmosphericEscapeWorld::CreateGrayboxGround(vnScene* scene)
{
    if (scene == nullptr)
    {
        return false;
    }


    //============================================================
    // CSVからGround配置を取得
    //============================================================
    const WorldObjectPlacement* placement = mPlacementTable.Find("Ground");


    if (placement == nullptr)
    {
        OutputDebugStringW(
            L"[AtmosphericEscapeWorld] "
            L"Ground placement not found.\n"
        );

        return false;
    }


    //============================================================
    // Ground Model
    //============================================================
    mGround = new ModelBase(L"data/model/primitive/", L"cube.vnm");


    if (mGround == nullptr)
    {
        return false;
    }


    //============================================================
    // Position
    // CSV:
    // PosX / PosY / PosZ
    //============================================================
    mGround->setPosition(
        placement->Position.x,
        placement->Position.y,
        placement->Position.z
    );


    //============================================================
    // Scale
    //============================================================
    mGround->setScale(
        placement->Scale.x,
        placement->Scale.y,
        placement->Scale.z
    );


    //============================================================
    // Rotation
    //============================================================
    mGround->setRotation(
        placement->Rotation.x,
        placement->Rotation.y,
        placement->Rotation.z
    );


    //============================================================
    // Rendering
    //============================================================
    mGround->SetPBREnabled(true);
    mGround->setDiffuse(0.12f, 0.14f, 0.11f, 1.0f);
    mGround->SetMetallic(0.0f);
    mGround->SetRoughness(0.9f);
    mGround->setCastShadow(false);
    mGround->setReceiveShadow(true);

    //============================================================
    // Collider
    //============================================================
    const XMFLOAT3 halfExtents =
    {
        placement->Scale.x * 0.5f,
        placement->Scale.y * 0.5f,
        placement->Scale.z * 0.5f
    };


    mGroundCollider =
        std::make_unique<BoxCollider>(
            mGround,
            XMVectorSet(halfExtents.x, halfExtents.y, halfExtents.z, 0.0f)
        );

    //============================================================
    // Scene登録
    //============================================================
    scene->registerObject(mGround);

    return true;
}


//============================================================
// Forest Graybox
//============================================================
bool AtmosphericEscapeWorld::CreateGrayboxForest(vnScene* scene)
{
    if (scene == nullptr)
    {
        return false;
    }


    //============================================================
    // CSVからPrefab == "Tree" をすべて取得
    //============================================================
    const std::vector<const WorldObjectPlacement*> trees = mPlacementTable.FindByPrefab("Tree");


    //============================================================
    // Tree生成
    //============================================================
    for (const WorldObjectPlacement* placement : trees)
    {
        if (placement == nullptr)
        {
            continue;
        }


        //========================================================
        // Model
        //========================================================
        ModelBase* tree = new ModelBase(L"data/model/primitive/", L"cube.vnm");


        if (tree == nullptr)
        {
            return false;
        }


        //========================================================
        // Position
        //========================================================
        tree->setPosition(
            placement->Position.x,
            placement->Position.y,
            placement->Position.z
        );


        //========================================================
        // Rotation
        //========================================================
        tree->setRotation(
            placement->Rotation.x,
            placement->Rotation.y,
            placement->Rotation.z
        );


        //========================================================
        // Scale
        //========================================================
        tree->setScale(
            placement->Scale.x,
            placement->Scale.y,
            placement->Scale.z
        );

        //========================================================
        // Material
        //========================================================
        tree->SetPBREnabled(true);
        tree->setDiffuse(0.10f, 0.055f, 0.025f, 1.0f);
        tree->SetMetallic(0.0f);
        tree->SetRoughness(0.95f);
        tree->setCastShadow(true);
        tree->setReceiveShadow(true);


        //========================================================
        // Collider
        //========================================================
        const XMFLOAT3 halfExtents =
        {
            placement->Scale.x * 0.5f,
            placement->Scale.y * 0.5f,
            placement->Scale.z * 0.5f
        };


        auto collider =
            std::make_unique<BoxCollider>(
                tree,
                XMVectorSet(halfExtents.x, halfExtents.y, halfExtents.z, 0.0f)
            );


        //========================================================
        // Sceneへ登録
        //========================================================
        scene->registerObject(tree);

        //========================================================
        // World側で所有
        //========================================================
        mForestObjects.push_back(tree);

        mForestColliders.push_back(std::move(collider));
    }

    return true;
}

MansionDoor* AtmosphericEscapeWorld::FindMansionDoorById(const std::string& persistentId)
{
    for (auto& door : mMansionDoors)
    {
        if (door == nullptr)
        {
            continue;
        }

        //====================================================
        // MansionDoorはIPersistentWorldObjectなので
        // 固有IDを取得できる
        //====================================================
        if (door->GetPersistentId() == persistentId)
        {
            return door.get();
        }
    }

    return nullptr;
}
