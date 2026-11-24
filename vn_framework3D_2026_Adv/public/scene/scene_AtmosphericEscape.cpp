#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include <exception>

// 初期化に入ったフレームでは開始要求を保存して描画ループへ返す
bool SceneAtmosphericEscape::initialize()
{
    mStartMode = AtmosphericEscapeStartRequest::Consume();
    mLoadPhase = LoadPhase::ShowLoading;
    mEnvironmentInitAttempted = false;
    mFadeInitialized = false;

    PostProcessManager::AllDisable();
    SetGameplayCursorHidden(true);
    vnMainFrame::SetLoadingOnly(true);
    return true;
}

void SceneAtmosphericEscape::UpdateLoading()
{
    if (mLoadPhase != LoadPhase::Failed)
    {
        try
        {
            AdvanceLoading();
        }
        catch (const std::exception& error)
        {
            OutputDebugStringA(error.what());
            OutputDebugStringA("\n");
            mLoadPhase = LoadPhase::Failed;
        }
    }

    if (mLoadPhase == LoadPhase::Failed)
    {
        // ポストエフェクトを設定した後の失敗でもエラー表示を続ける
        PostProcessManager::AllDisable();

        if (vnKeyboard::trg(DIK_ESCAPE))
        {
            switchScene(eSceneTable::AtmosphericEscapeTitle);
            return;
        }
    }

    if (mLoadPhase != LoadPhase::Ready)
    {
        DrawLoadingUI();
    }
}

void SceneAtmosphericEscape::AdvanceLoading()
{
    using Result = AtmosphericEscapeWorld::InitializeResult;

    bool succeeded = true;
    switch (mLoadPhase)
    {
    case LoadPhase::ShowLoading:
        // このフレームの最後にMainFrameが文字を描いてPresentする
        // 重い初期化は、次のフレームから1段階ずつ進める
        mLoadPhase = LoadPhase::Environment;
        break;

    case LoadPhase::Environment:
        mEnvironmentInitAttempted = true;
        succeeded = EnvironmentMapManager::Initialize();
        if (succeeded)
        {
            EnvironmentMapManager::SetEnvironmentExposureEV(-0.5f);
        }
        mLoadPhase = LoadPhase::BeginWorld;
        break;

    case LoadPhase::BeginWorld:
        succeeded = mWorld.BeginInitialize(this, &mInteractionSystem);
        mLoadPhase = LoadPhase::World;
        break;

    case LoadPhase::World:
    {
        const Result result = mWorld.AdvanceInitialize(this, &mInteractionSystem);
        if (result == Result::Loading)
        {
            return;
        }
        succeeded = result == Result::Ready;
        mLoadPhase = LoadPhase::Player;
        break;
    }

    case LoadPhase::Player:
        succeeded = InitializePlayerAndContent();
        mLoadPhase = LoadPhase::Pursuer;
        break;

    case LoadPhase::Pursuer:
        succeeded = InitializePursuer();
        mLoadPhase = LoadPhase::Finish;
        break;

    case LoadPhase::Finish:
        if (FinishInitialization())
        {
            // ロード時間を、ゲーム側のdtに含めない。
            mPrevTime = std::chrono::steady_clock::now();
            mLoadPhase = LoadPhase::Ready;
            vnMainFrame::SetLoadingOnly(false);
            return;
        }
        succeeded = false;
        break;

    case LoadPhase::Ready:
    case LoadPhase::Failed:
        return;
    }

    if (!succeeded)
    {
        OutputDebugStringW(L"[SceneAtmosphericEscape] Gameplay initialization failed.\n");
        mLoadPhase = LoadPhase::Failed;
    }
}

void SceneAtmosphericEscape::DrawLoadingUI() const
{
    const float width = static_cast<float>(vnMainFrame::screenWidth);
    const float height = static_cast<float>(vnMainFrame::screenHeight);
    const float x = width * 0.70f;
    const float y = height * 0.82f;

    AEUIFont::Scope font(AEUIFont::Style::Label);
    vnFont::setColor(0xffe8c98f);

    if (mLoadPhase == LoadPhase::Failed)
    {
        vnFont::print(x, y, L"読み込みに失敗しました");
        vnFont::print(x, y + 32.0f, L"[Esc] タイトルへ");
        return;
    }

    // 実時間で点を切り替える CPUプリロードの待機中もフレームごとに更新する
    using namespace std::chrono;
    const auto ticks = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    const wchar_t* labels[] = { L"LOADING", L"LOADING.", L"LOADING..", L"LOADING..." };
    vnFont::print(x, y, L"%s", labels[(ticks / 250) % 4]);

    const unsigned int total = static_cast<unsigned int>(mWorld.GetTotalArtPlacements());
    if (total != 0)
    {
        const unsigned int completed = static_cast<unsigned int>(mWorld.GetCompletedArtPlacements());
        font.Use(AEUIFont::Style::Hint);
        vnFont::print(x, y + 32.0f, L"読み込み中  %u / %u", completed, total);
    }
}

bool SceneAtmosphericEscape::InitializePlayerAndContent()
{
    //============================================================
    // Atmospheric Escape Gameplay Content
    // ・Rooms
    // ・Objectives
    // ・Inventory Item Definitions
    // をCSVから一括読み込みする
    //============================================================
    if (!AtmosphericEscapeContentLoader::LoadAll(
        mLocationSystem,
        mObjectiveManager,
        mInventory
    ))
    {
        OutputDebugStringW(
            L"[SceneAtmosphericEscape] "
            L"Gameplay content initialization failed.\n"
        );

        return false;
    }

    if (!DocumentContentLoader::Load(L"data/csv/world/gameplay/Documents.csv", L"data/text/AtmosphericEscape/documents/", mDocumentManager))
    {
        OutputDebugStringW(L"[SceneAtmosphericEscape] Document metadata load failed.\n");
    }

    mWorld.SetInventoryManager(&mInventory);
    mWorld.SetDocumentManager(&mDocumentManager);

    //============================================================
    // Document Viewer
    //============================================================
    if (!mDocumentViewer.Initialize(this))
    {
        return false;
    }

    mWorld.SetDocumentCollectedCallback([this](const std::string& documentId)
        {
            OpenDocumentViewer(documentId);
        }
    );

    pPlayer = new ExplorationPlayer(L"data/model/player/", L"player.vnm", L"player.bone");
    pPlayer->setPosition(0.0f, 2.0f, -26.0f);
    pPlayer->setScale(1.0f, 1.0f, 1.0f);
    pPlayer->SetInteractionSystem(&mInteractionSystem);

    if (pPlayer->GetModel() != nullptr)
    {
        pPlayer->GetModel()->SetPBREnabled(true);
        pPlayer->GetModel()->setCastShadow(true);
        pPlayer->GetModel()->setReceiveShadow(true);
        pPlayer->GetModel()->SetShadowMobility(ShadowMobility::Dynamic);
    }

    PlayerLocomotionAnimationSet locomotionAnimations;
    locomotionAnimations.IdleFile = L"data/model/player/motion/idle.mot";
    locomotionAnimations.WalkFile = L"data/model/player/motion/walk.mot";
    locomotionAnimations.RunFile = L"data/model/player/motion/run.mot";
    locomotionAnimations.CrouchIdleFile = L"data/model/player/motion/CrouchIdle.mot";
    locomotionAnimations.CrouchWalkFile = L"data/model/player/motion/sneakingWalk.mot";
    const bool animationInitialized = pPlayer->InitializeLocomotionAnimations(locomotionAnimations);
    if (!animationInitialized)
    {
        // RegisterObject前なので失敗時はここで解放する
        delete pPlayer;
        pPlayer = nullptr;
        return false;
    }
    PlayerActionAnimationSet actionAnimations;
    actionAnimations.PickupDownFile = L"data/model/player/motion/pickup_down.mot";
    actionAnimations.PickupMidFile = L"data/model/player/motion/pickup_mid.mot";
    actionAnimations.HitReactionFile = L"data/model/player/motion/hitReaction.mot";
    actionAnimations.DeathFile = L"data/model/player/motion/death.mot";

    //============================================================
    // Initialize
    //============================================================
    const bool actionInitialized = pPlayer->InitializeActionAnimations(actionAnimations);
    if (!actionInitialized)
    {
        delete pPlayer;
        pPlayer = nullptr;
        return false;
    }

    pPlayer->RegisterObject(this);
    //pPlayer->DebugPrintBoneNames();

    if (pPlayer != nullptr && pPlayer->GetModel() != nullptr)
    {
        pPlayer->SetFlashlightShadowExcludedCaster(pPlayer->GetModel());
    }

    mFlashlightModel = new ModelBase(L"data/model/player/flashLight/", L"FlashLight.vnm");

    if (mFlashlightModel != nullptr)
    {
        mFlashlightModel->useQuaternion(true);
        mFlashlightModel->setScale(0.15f, 0.15f, 0.15f);
        mFlashlightModel->SetPBREnabled(true);
        mFlashlightModel->setCastShadow(false);
        mFlashlightModel->setReceiveShadow(true);
        registerObject(mFlashlightModel);
        const bool attached =
            pPlayer->AttachFlashlightVisualModel(mFlashlightModel, L"smartrig:Spine2", XMFLOAT3(0.0010f, 0.0f, 0.0008f));

        if (!attached)
        {
            OutputDebugStringW(L"[Flashlight] Failed to attach to chest bone.\n");
        }
    }

    const float width = static_cast<float>(vnMainFrame::screenWidth);
    const float height = static_cast<float>(vnMainFrame::screenHeight);

    mMenuBackdrop = new vnSprite(
        width * 0.5f, height * 0.5f,
        width, height,
        L"data/image/UI/white1x1.png"
    );

    mMenuBackdrop->setRenderPriority(2800);

    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        // 白いテクスチャを黒く着色し、背景の光を抑える
        auto& vertex = mMenuBackdrop->vtx[i];
        vertex.r = 0.0f;
        vertex.g = 0.0f;
        vertex.b = 0.0f;
        vertex.a = 0.90f;
    }

    registerObject(mMenuBackdrop);
    mMenuBackdrop->setRenderEnable(false);

    return true;
}

bool SceneAtmosphericEscape::InitializePursuer()
{
    const bool pursuerInitialized = mPursuerSystem.Initialize(this, mColliders, &mNoiseSystem, mAIDoors);
    if (!pursuerInitialized) return false;

    //============================================================
    // Pursuer Animation Files
    // モデルの読み込みはPursuerMonsterが担当
    // モーションのファイルパスはSceneが設定する
    //============================================================
    PursuerAnimationSet pursuerAnimations;
    //============================================================
    // Locomotion
    //============================================================
    pursuerAnimations.IdleFile = L"data/model/enemy/motion/idle.mot";
    pursuerAnimations.WalkFile = L"data/model/enemy/motion/walk.mot";
    pursuerAnimations.RunFile = L"data/model/enemy/motion/run.mot";
    //============================================================
    // Action
    //============================================================
    pursuerAnimations.AttackFile = L"data/model/enemy/motion/attack.mot";

    //============================================================
    // Animation初期化
    //============================================================
    const bool pursuerAnimationInitialized =
        mPursuerSystem.InitializeAnimations(pursuerAnimations);

    if (!pursuerAnimationInitialized)
    {
        OutputDebugStringW(
            L"[SceneAtmosphericEscape] "
            L"Pursuer animation initialization failed.\n"
        );

        return false;
    }
    return true;
}

bool SceneAtmosphericEscape::FinishInitialization()
{
    //============================================================
    // World Door収集
    //============================================================
    mWorld.AppendAIDoors(mAIDoors);

    //============================================================
    // Noise System登録
    //============================================================
    mWorld.SetNoiseSystem(&mNoiseSystem);
    pPlayer->SetNoiseSystem(&mNoiseSystem);

    //============================================================
    // Collider Registration
    //============================================================
    mColliders.push_back(pPlayer->GetCollider());
    mWorld.AppendColliders(mColliders);
    mPursuerSystem.AppendColliders(mColliders);
    pPlayer->SetStepColliders(&mColliders);

    //============================================================
    // Result UI
    //============================================================
    mResultUI.Initialize(this);
    mResultUI.Hide();
    UIInputConfig::SetupResult(mUIInput, mUIPadId);
    mUIInput.Reset();
    //============================================================
    // Pause UI
    //============================================================
    mPauseUI.Initialize(this);
    mPauseUI.Hide();
    UIInputConfig::SetupPause(mPauseInput, mUIPadId);

    if (!mExplorationMenu.Initialize(this, &mInventory, &mLocationSystem, &mObjectiveManager, &mDocumentManager)) return false;
    UIInputConfig::SetupExplorationMenu(mExplorationMenuInput, 0);
    mExplorationMenuInput.Reset();

    mOptionsUI.Initialize(this);
    UIInputConfig::SetupExplorationMenu(mOptionsInput, mUIPadId);


    //============================================================
    // 操作ガイドを生成する
    // 表示はPlaying状態のときに切り替える
    //============================================================
    mControlGuide.Initialize(this);
    mChaseVignette.Initialize(this);

    //========================================================
    // 使用するPostProcessを有効化
    //========================================================
    PostProcessManager::Enable(PostProcessManager::Bloom);
    PostProcessManager::Enable(PostProcessManager::ColorGrading);
    PostProcessManager::Enable(PostProcessManager::Vignette);
    PostProcessManager::Enable(PostProcessManager::ChromaticAberration);
    PostProcessManager::Enable(PostProcessManager::VolumetricFog);
    PostProcessManager::Enable(PostProcessManager::SSGI);
    PostProcessManager::Enable(PostProcessManager::Fxaa);

    //============================================================
    // Color Grading
    //============================================================
    PostProcessManager::SetBrightness(1.02f);
    PostProcessManager::SetContrast(1.18f);
    PostProcessManager::SetSaturation(0.86f);

    //============================================================
    // Vignette
    // 周辺減光は強めのまま少し緩和する
    //============================================================
    PostProcessManager::SetVignette(0.35f, 0.88f, 0.48f);

    //============================================================
    // Chromatic Aberration
    //============================================================
    PostProcessManager::SetChromaticStrength(0.0025f);

    //============================================================
    // Bloom
    //============================================================
    PostProcessManager::SetBloomIntensity(0.90f);

    // 霧の密度を少し上げ廊下の奥や光の通り道に厚みを出す
    PostProcessManager::SetVolumetricFogDensity(0.092f);
    PostProcessManager::SetVolumetricFogMaxDistance(30.0f);
    PostProcessManager::SetVolumetricFogColor(0.23f, 0.28f, 0.35f);

    // 懐中電灯の光筋を見せる 天井灯の周りは光が膨らみすぎない値から試す
    PostProcessManager::SetVolumetricFogAnisotropy(0.65f);
    PostProcessManager::SetVolumetricFogSpotLightStrength(0.550f);
    PostProcessManager::SetVolumetricFogPointLightStrength(0.35f);

    // 光の当たっていない場所にも霧が重なるので環境光の寄与を少し抑える
    PostProcessManager::SetVolumetricFogAmbientStrength(0.10f);
    PostProcessManager::SetVolumetricFogShadowStrength(0.75f);
    //============================================================
    // SSGI
    //============================================================
    PostProcessManager::SetSSGIIntensity(1.00f);


    //============================================================
    // Runtime Debug / LookDev
    //============================================================
    mRuntimeDebug.Initialize();


    //============================================================
    // Scene共通ShadowMapを取得
    //============================================================
    if (!EnableShadowMap(pPlayer)) return false;

    const auto& options = AtmosphericEscapeOptions::Get();
    options.ApplyPostEffects();
    options.ApplyShadows(GetShadowMap());
    options.ApplyCamera();

    //============================================================
    // Scene Flow
    //============================================================
    mSceneFlowState = SceneFlowState::Playing;
    //============================================================
    // Result
    //============================================================
    mResultType = SceneResultType::None;
    //============================================================
    // Clear / Death Timer
    //============================================================
    mEscapeCompleteTimer = 0.0f;
    mPlayerDeadTimer = 0.0f;
    mPlayTime = 0.0f;

    //============================================================
    // Scene Start Request
    //============================================================
    const AtmosphericEscapeStartRequest::Mode startMode = mStartMode;


#if defined(_DEBUG)

    if (startMode == AtmosphericEscapeStartRequest::Mode::LoadGame)
    {
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"StartMode = LoadGame\n"
        );
    }
    else
    {
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"StartMode = NewGame\n"
        );
    }

#endif


    //============================================================
    // Load Game
    //============================================================
    if (startMode == AtmosphericEscapeStartRequest::Mode::LoadGame)
    {
        AtmosphericEscapeSaveData saveData;


        //========================================================
        // File Load
        //========================================================
        if (!AtmosphericEscapeSaveManager::Load(saveData))
        {
#if defined(_DEBUG)

            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Load failed. Starting new game.\n"
            );

#endif
        }
        else
        {
#if defined(_DEBUG)

            wchar_t buffer[256];

            swprintf_s(
                buffer,
                L"[AtmosphericEscape] "
                L"Loaded PlayerPos=(%.2f, %.2f, %.2f) "
                L"HP=%.1f\n",
                saveData.PlayerPosition.x,
                saveData.PlayerPosition.y,
                saveData.PlayerPosition.z,
                saveData.PlayerHealth
            );
            OutputDebugStringW(buffer);

#endif
            //====================================================
            // SaveDataをRuntimeへ適用
            //====================================================
            if (!ApplySaveData(saveData))
            {
#if defined(_DEBUG)

                OutputDebugStringW(
                    L"[AtmosphericEscape] "
                    L"ApplySaveData failed.\n"
                );
#endif
            }
        }
    }

    // ロード直後の進行に合わせ最初の描画・AI更新から表示状態と追跡状態を揃える
    SyncPursuerActivationFromExplorationPhase();


    SoundBank* sound = SoundBank::getInstance();
    sound->InitAtmosphericEscape();

    // 復元済みの進行度に合わせて曲を選ぶ
    if (mExplorationPhase == ExplorationPhase::EscapeMansion)
    {
        sound->StartLoop(SoundID::AE_BgmFinalChase, 0.60f);
    }
    else
    {
        sound->StartLoop(SoundID::AE_BgmAmbience, 0.50f);
    }
    // すべてのリソースとセーブ適用が完了してからフェードインする
    FadeManager::Get().Initialize(this);
    mFadeInitialized = true;
    FadeManager::Get().StartFadeIn(0.50f);
    return true;
}


//終了関数
void SceneAtmosphericEscape::terminate()
{
    PostProcessManager::AllDisable();

    mColliders.clear();

    mWorld.Terminate(this, &mInteractionSystem);

    mPursuerSystem.Terminate(this);

    if (pPlayer != nullptr)
    {
        deleteObject(pPlayer);
        pPlayer = nullptr;
    }

    if (mFlashlightModel != nullptr)
    {
        deleteObject(mFlashlightModel);
        mFlashlightModel = nullptr;
    }

    if (mMenuBackdrop != nullptr)
    {
        deleteObject(mMenuBackdrop);
        mMenuBackdrop = nullptr;
    }

    mResultUI.Terminate(this);
    mPauseUI.Terminate(this);
    mDocumentViewer.Terminate(this);
    mControlGuide.Terminate(this);
    mChaseVignette.Terminate(this);
    mOptionsUI.Terminate(this);
    SetGameplayCursorHidden(false);
    if (mEnvironmentInitAttempted)
    {
        EnvironmentMapManager::Terminate();
    }
    if (mFadeInitialized)
    {
        FadeManager::Get().Terminate(this);
    }
    SoundBank::getInstance()->ShutdownAtmosphericEscape();
    // エンジンに共有されるローカルライトの上限をシーンを離れる際に戻す
    SpotLightShadowManager::SetShadowLightBudget(SpotLightShadowManager::MaxShadowLights);
    PointLightShadowManager::SetResolutionCap(PointLightShadowManager::ShadowResolution::High);

}

//処理関数
void SceneAtmosphericEscape::execute()
{
    if (mLoadPhase != LoadPhase::Ready)
    {
        UpdateLoading();

        if (mLoadPhase != LoadPhase::Ready)
        {
            return;
        }
    }

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> delta = now - mPrevTime;
    mPrevTime = now;
    float dt = delta.count();
    dt = (std::min)(dt, 0.1f);

    FadeManager::Get().Update(dt);

    if (mOptionsUI.IsVisible())
    {
        mChaseVignette.Apply(false);

        // メニューを開いている間はゲーム側・ポーズ画面側へ入力を渡さない
        mOptionsInput.HandleInput(&mOptionsUI, dt);
        mOptionsUI.Update(mUIPadId);

        const auto changes = mOptionsUI.ConsumeChanges();
        const auto& options = AtmosphericEscapeOptions::Get();
        if (changes & UIAtmosphericEscapeOptions::Post) options.ApplyPostEffects();
        if (changes & UIAtmosphericEscapeOptions::Shadows) options.ApplyShadows(GetShadowMap());
        if (changes & UIAtmosphericEscapeOptions::Camera) options.ApplyCamera();

        if (mOptionsUI.WantsClose())
        {
            mOptionsUI.Hide();
            mOptionsInput.Reset();
            mPauseInput.Reset();
            mPauseUI.Show();
        }
        else
        {
            mOptionsUI.Draw();
        }

        vnScene::execute();
        return;
    }


    //============================================================
    // Pause Input
    //============================================================
    const bool pauseTriggered = mSceneInput.IsPauseTriggered(0);

    if (pauseTriggered)
    {
        //========================================================
        // Playing → Pause
        //========================================================
        if (mSceneFlowState == SceneFlowState::Playing)
        {
            EnterPause();
        }

        //========================================================
        // Pause → Playing
        //========================================================
        else if (mSceneFlowState == SceneFlowState::Pause)
        {
            ExitPause();
        }
    }

#if defined(_DEBUG)

    if (mSceneInput.IsDebugLoadTriggered(0))
    {
        //============================================================
        // 次のAtmosphericEscape SceneをLoad開始にする
        //============================================================
        AtmosphericEscapeStartRequest::RequestLoadGame();

        //============================================================
        // 同じSceneを強制再生成
        //============================================================
        switchScene(eSceneTable::AtmosphericEscape, true);

        return;
    }

#endif

    if (mSceneFlowState == SceneFlowState::Playing)
    {
        if (mSceneInput.IsMapTriggered(0))
        {

            //========================================================
            // Tutorial判定
            //========================================================
            if (mExplorationPhase == ExplorationPhase::TutorialOpenMap)
            {
                mMapOpenedOnce = true;
            }
            //====================================================
            // Gameplay停止
            //====================================================
            pPlayer->SetControlLocked(true);
            mExplorationMenuInput.Reset();
            mExplorationMenu.Show(ExplorationMenuPage::Map);
            mSceneFlowState = SceneFlowState::ExplorationMenu;

            return;
        }
    }

    //============================================================
    // Inventory Open
    //============================================================
    if (mSceneFlowState == SceneFlowState::Playing)
    {
        if (mSceneInput.IsInventoryTriggered(0))
        {
            //====================================================
            // Player停止
            //====================================================
            pPlayer->SetControlLocked(true);

            //====================================================
            // UI Input初期化
            //====================================================
            mExplorationMenuInput.Reset();

            //====================================================
            // Show
            //====================================================
            mExplorationMenu.Show(ExplorationMenuPage::Inventory);
            //====================================================
            // Scene State
            //====================================================
            mSceneFlowState = SceneFlowState::ExplorationMenu;

            return;
        }
    }

    //============================================================
    // Scene Flow
    //============================================================
    switch (mSceneFlowState)
    {
        //========================================================
        // 通常Gameplay
        //========================================================
    case SceneFlowState::Playing:
    {
        UpdateGameplay(dt);
        break;
    }

    case SceneFlowState::Pause:
    {
        UpdatePause(dt);
        break;
    }

    //========================================================
    // Escape成功後の余韻
    //========================================================
    case SceneFlowState::EscapeComplete:
    {
        UpdateEscapeComplete(dt);
        break;
    }

    //============================================================
    // Player死亡演出
    //============================================================
    case SceneFlowState::PlayerDead:
    {
        UpdatePlayerDeath(dt);
        break;
    }

    //========================================================
    // FadeOut中
    // FadeManagerだけ更新する
    // Gameplayは停止
    //========================================================
    case SceneFlowState::FadeToResult:
    {
        if (mResultType == SceneResultType::GameClear)
        {
            UpdateEscapeCompleteLighting(dt);
        }

        break;
    }

    //========================================================
    // Result
    //========================================================
    case SceneFlowState::Result:
    {
        UpdateResult(dt);

        break;
    }

    case SceneFlowState::ExplorationMenu:
    {
        //========================================================
        // 通常UI入力
        //========================================================
        mExplorationMenuInput.HandleInput(&mExplorationMenu, dt);

        //========================================================
        // Tab切替
        // MapではA/DをFloor変更に使うため
        // TabはQ/Eに分離する
        //========================================================
        if (vnKeyboard::trg(DIK_Q))
        {
            mExplorationMenu.MoveTab(-1);
        }

        if (vnKeyboard::trg(DIK_E))
        {
            mExplorationMenu.MoveTab(+1);
        }

        //========================================================
        // Gamepad
        //========================================================
        if (Pad::trg(PadButton::LB, 0))
        {
            mExplorationMenu.MoveTab(-1);
        }

        if (Pad::trg(PadButton::RB, 0))
        {
            mExplorationMenu.MoveTab(+1);
        }


        //========================================================
        // Mouse + Page Update
        //========================================================
        mExplorationMenu.Update(dt);

        if (mExplorationPhase == ExplorationPhase::TutorialOpenMap && mExplorationMenu.GetCurrentPage() == ExplorationMenuPage::Map)
        {
            mMapOpenedOnce = true;
        }

        //========================================================
        // Font
        //========================================================
        mExplorationMenu.Draw();

        //========================================================
        // Close
        //========================================================
        const auto action = mExplorationMenu.ConsumeAction();


        if (action == UIExplorationMenu::Action::Close)
        {
            mExplorationMenu.Hide();
            mExplorationMenuInput.Reset();
            pPlayer->SetControlLocked(false);
            mSceneFlowState = SceneFlowState::Playing;
        }

        break;
    }

    case SceneFlowState::DocumentViewer:
    {
        mExplorationMenuInput.HandleInput(&mDocumentViewer, dt);
        mDocumentViewer.Update(dt);
        mDocumentViewer.Draw();
        const UIDocumentViewer::Action action = mDocumentViewer.ConsumeAction();

        if (action == UIDocumentViewer::Action::Close)
        {
            CloseDocumentViewer();
        }

        break;
    }

    }

    //============================================================
    // 操作ガイド表示
    // ポーズ・マップ・インベントリ・資料閲覧・結果画面では
    // 画面右下の操作ガイドを隠す
    //============================================================
    const bool showControls =
        mSceneFlowState == SceneFlowState::Playing && !mRuntimeDebug.BlocksPlayerControl();

    mControlGuide.SetVisible(showControls);

    // このフレームのUI描画として登録する
    if (showControls)
    {
        mControlGuide.Draw();
    }

    mChaseVignette.Apply(
        mSceneFlowState == SceneFlowState::Playing &&
        !mOptionsUI.IsVisible() &&
        !mRuntimeDebug.BlocksPlayerControl()
    );


    //============================================================
    // Engine共通Update
    //============================================================
    vnScene::execute();

#if defined(_DEBUG)

    //============================================================
    // Flashlight Attachment Debug
    // 最初の1回だけ胸Boneとライトモデルの
    // World Positionを比較する
    //============================================================
    static bool printedFlashlightDebug = false;

    if (!printedFlashlightDebug && pPlayer != nullptr && mFlashlightModel != nullptr)
    {
        ObjectBase* chestBone = pPlayer->GetBone(5);

        if (chestBone != nullptr)
        {
            //====================================================
            // Chest Bone World Position
            //====================================================
            const XMMATRIX& boneWorld = *chestBone->getWorld();

            //====================================================
            // Flashlight Model World Position
            //====================================================
            const XMMATRIX& lightWorld = *mFlashlightModel->getWorld();

            //====================================================
            // 実際にSpine2を親としているか
            //====================================================
            const bool parentCorrect =
                mFlashlightModel->getParent() == chestBone;

            wchar_t buffer[512] = {};

            swprintf_s(
                buffer,
                L"[Flashlight Debug]\n"
                L"ParentCorrect=%d\n"
                L"BoneWorld=(%.3f, %.3f, %.3f)\n"
                L"LightWorld=(%.3f, %.3f, %.3f)\n",

                parentCorrect ? 1 : 0,

                XMVectorGetX(boneWorld.r[3]),
                XMVectorGetY(boneWorld.r[3]),
                XMVectorGetZ(boneWorld.r[3]),

                XMVectorGetX(lightWorld.r[3]),
                XMVectorGetY(lightWorld.r[3]),
                XMVectorGetZ(lightWorld.r[3])
            );

            OutputDebugStringW(buffer);

            printedFlashlightDebug = true;
        }
    }

#endif
}

//描画関数
void SceneAtmosphericEscape::render()
{
    const bool showCursor =
        mOptionsUI.IsVisible() ||
        mSceneFlowState == SceneFlowState::Pause ||
        mSceneFlowState == SceneFlowState::ExplorationMenu ||
        mSceneFlowState == SceneFlowState::DocumentViewer ||
        mSceneFlowState == SceneFlowState::Result;

    SetGameplayCursorHidden(!showCursor);

    const bool showMenuBackdrop =
        mOptionsUI.IsVisible() ||
        mSceneFlowState == SceneFlowState::Pause ||
        mSceneFlowState == SceneFlowState::ExplorationMenu ||
        mSceneFlowState == SceneFlowState::DocumentViewer;

    if (mMenuBackdrop != nullptr)
    {
        mMenuBackdrop->setRenderEnable(showMenuBackdrop);
    }

    EnvironmentMapManager::GenerateBRDFLUT();
    EnvironmentMapManager::GeneratePrefilteredEnvironment();
    EnvironmentMapManager::GenerateIrradianceEnvironment();
    vnScene::render();
    EnvironmentMapManager::RenderSkybox();
    Direct3D::setMainRenderTarget();
    mWorld.RenderFullResolutionEffects();
    Direct3D::TransitionMainDepthToSRV();
    HalfResolutionEffectPass::Begin();
    HalfResolutionEffectPass::EndAndComposite();
}

void SceneAtmosphericEscape::UpdateGameplay(float dt)
{
    //============================================================
    // Play Time
    //============================================================
    mPlayTime += dt;


    //============================================================
    // Light Frame Begin
    //============================================================
    LocalLightManager::BeginFrame();
    SpotLightShadowManager::BeginFrame();
    PointLightShadowManager::BeginFrame();
    AtmosphericEscapeLightBudget::BeginFrame();

    //============================================================
    // Noise Frame Begin
    //============================================================
    mNoiseSystem.BeginFrame();

#if defined(_DEBUG)
    //============================================================
    // Debug
    //============================================================
    mRuntimeDebug.Update(dt);
#endif

    //============================================================
    // Player Control
    // Debug Inspector / FreeCamera使用時だけ停止
    //============================================================
    pPlayer->SetControlLocked(mRuntimeDebug.BlocksPlayerControl());

    // 端末を操作済みで照明の復旧演出も終わったかを確認する
    const bool powerRestored =
        mWorld.IsMansionPowerTerminalActivated() && mWorld.IsMansionPowerRestoreFinished();

    // プレイヤーの操作を処理する前に現在の進行で使える装置を決める
    mWorld.SetMansionInteractionEnabled(
        mExplorationPhase == ExplorationPhase::ReachPowerTerminal,
        powerRestored && mExplorationPhase == ExplorationPhase::FirstPuzzle,
        powerRestored && mExplorationPhase == ExplorationPhase::SecondPuzzle
    );

    //============================================================
    // Input
    //============================================================
    mPlayerInput.HandleInput(pPlayer, 0);

    const bool useGameplayCamera = !mRuntimeDebug.OwnsCamera();

    pPlayer->PrepareGameplayCamera(dt, useGameplayCamera);

    //============================================================
    // Player
    //============================================================
    pPlayer->Update(dt);

    if (pPlayer->ConsumePickupCommitted())
    {
        if (auto* shadowMap = GetShadowMap())
        {
            shadowMap->InvalidateStaticShadowCache();
        }
    }

    //============================================================
    // Location
    // Playerの現在位置から
    // Area / Floor / Roomを更新する
    //============================================================
    mLocationSystem.Update(*pPlayer->getPosition());

    //============================================================
    // Pursuer
    //============================================================
    mPursuerSystem.Update(dt, *pPlayer);

    UpdateHeartbeat(dt);

    //============================================================
    // World
    //============================================================
    mWorld.Update(dt);

    //============================================================
    // Game Progress
    //============================================================
    const ExplorationPhase previousPhase = mExplorationPhase;
    UpdateExplorationProgress(dt);
    if (previousPhase != ExplorationPhase::EscapeMansion &&
        mExplorationPhase == ExplorationPhase::EscapeMansion &&
        mSceneFlowState == SceneFlowState::Playing)
    {
        auto* sound = SoundBank::getInstance();
        sound->StopLoop(SoundID::AE_BgmAmbience);
        sound->StartLoop(SoundID::AE_BgmFinalChase, 0.60f);
    }


    //============================================================
    // Objective同期
    //============================================================
    SyncObjectiveFromExplorationPhase();

    //============================================================
    // Pursuer Active
    // Phase変更に合わせてMonster AI状態も同期する
    //============================================================
    SyncPursuerActivationFromExplorationPhase();

    //============================================================
    // Collision
    //============================================================
    pPlayer->SetGrounded(false);

    CollisionManager::Process(
        mColliders,
        nullptr,
        nullptr
    );

    //============================================================
    // Pursuer Physics Sync
    //============================================================
    mPursuerSystem.SyncAfterCollision();

    // 衝突解決後の視点を確定し懐中電灯をライトバケットへ登録する
    pPlayer->UpdateViewAndFlashlight(dt, useGameplayCamera, mColliders);

    mPursuerSystem.UpdateAudio(dt, *pPlayer);

    //============================================================
    // Local Light / Shadow Budget Resolve
    // Player / WorldがすべてLight Candidateを登録した後
    // 現在RoomとPlayer位置からShadow対象を決定する
    //============================================================
    XMFLOAT3 playerPosition;
    XMStoreFloat3(&playerPosition, *pPlayer->getPosition());
    SubmitEscapeGoalLight();
    AtmosphericEscapeLightBudget::ResolveAndRegister(playerPosition, mLocationSystem);

    //============================================================
    // Local Light Upload
    //============================================================
    LocalLightManager::Upload();

    //============================================================
    // Player Death Check
    //============================================================
    if (pPlayer->IsDead())
    {
        BeginPlayerDeath();
        return;
    }

    ////============================================================
    //// Camera
    ////============================================================
    //if (!mRuntimeDebug.OwnsCamera())
    //{
    //    vnCamera::UpdateFollowAngles(dt);
    //    vnCamera::FollowTargetWithColliders(
    //        *pPlayer->getPosition(),
    //        dt,
    //        mColliders
    //    );
    //}

    mWorld.UpdateEffects(dt);

    //============================================================
    // Gameplay UI
    //============================================================
    DrawGameplayUI();

#if defined(_DEBUG)

    //============================================================
    // 120回ごとにCPU / GPUメモリ使用量を確認する実際のD3D12Device取得関数を渡す
    //============================================================
    mRuntimeDebug.DebugLogMemoryUsage(Direct3D::getDevice());

#endif
}

void SceneAtmosphericEscape::UpdateHeartbeat(float dt)
{
    //============================================================
    // 追跡者の攻撃でこのフレームに死亡した場合も
    // ここで止めて新しい拍が死亡音に重なるのを防ぐ
    //============================================================

    if (pPlayer == nullptr || pPlayer->IsDead())
    {
        mHeartbeat.Reset();
        mChaseVignette.Reset();
        return;
    }

    using PursuerState = PursuerMonster::State;
    using HeartbeatState = AtmosphericHeartbeat::State;

    HeartbeatState heartbeatState = HeartbeatState::Quiet;

    switch (mPursuerSystem.GetState())
    {
    case PursuerState::Investigate:
        heartbeatState = HeartbeatState::Investigate;
        break;

    case PursuerState::Chase:
    case PursuerState::Attack:
        // 追跡から攻撃へ移っても速い心音を維持する
        heartbeatState = HeartbeatState::Chase;
        break;

    case PursuerState::Search:
        heartbeatState = HeartbeatState::Search;
        break;

    default:
        // 巡回・待機などでは鳴っていた心音を徐々に弱める
        break;
    }

    const float beatVolume = mHeartbeat.Update(dt, heartbeatState);

    const bool chasing = heartbeatState == HeartbeatState::Chase;
    mChaseVignette.Update(dt, chasing, beatVolume > 0.0f);

    if (beatVolume > 0.0f)
    {
        SoundBank::getInstance()->PlaySfx(SoundID::AE_Heartbeat, beatVolume);
    }
}

void SceneAtmosphericEscape::UpdateExplorationProgress(float dt)
{
    //============================================================
    // Annex Progress
    // 別館探索中にControlTerminalが既に起動していた場合は
    // 現在の細かなObjectiveに関係なく最終脱出へ進める
    //============================================================
    const bool isAnnexProgress =
        mExplorationPhase == ExplorationPhase::ExploreAnnex ||
        mExplorationPhase == ExplorationPhase::SolveAnnexArchivePuzzle ||
        mExplorationPhase == ExplorationPhase::FindStoneEmblem ||
        mExplorationPhase == ExplorationPhase::ReachAnnexBasement ||
        mExplorationPhase == ExplorationPhase::RestoreAnnexSafety ||
        mExplorationPhase == ExplorationPhase::ReachResearchSeal ||
        mExplorationPhase == ExplorationPhase::ActivateControlTerminal;

    if (isAnnexProgress && mWorld.IsAnnexControlTerminalActivated())
    {
        //========================================================
        // 最終脱出へ移行
        //========================================================
        mExplorationPhase = ExplorationPhase::EscapeMansion;

        //========================================================
        // 開始時に施錠された正面玄関を解錠する
        // 既存のEscapeMansion側でも再確認される
        //========================================================
        mWorld.UnlockMansionEntranceDoor();

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Annex Control Terminal activated. "
            L"Phase -> EscapeMansion\n"
        );
#endif
    }

    switch (mExplorationPhase)
    {
        //============================================================
        // Mansionへ入る
        //============================================================
    case ExplorationPhase::EnterMansion:
    {
        //========================================================
        // LocationSystemはPlayer位置から
        // 現在Roomを既に更新している
        //========================================================
        const RoomDefinition* currentRoom = mLocationSystem.GetCurrentRoom();

        if (currentRoom == nullptr)
        {
            break;
        }

        //========================================================
        // Playerが玄関ホールへ入った
        //========================================================
        if (currentRoom->Id == "Main1F_EntranceHall")
        {
            //====================================================
            // 背後の玄関Doorを閉じ始める
            //====================================================
            if (mWorld.BeginMansionEntranceCloseAndLock())
            {
                mExplorationPhase = ExplorationPhase::EntranceLocked;

#if defined(_DEBUG)

                OutputDebugStringW(
                    L"[AtmosphericEscape] "
                    L"Phase -> EntranceLocked\n"
                );
#endif
            }
        }

        break;
    }

    //============================================================
    // Entrance Door Closing
    //============================================================
    case ExplorationPhase::EntranceLocked:
    {
        //========================================================
        // Doorが完全に閉じてLockまで完了するのを待つ
        //========================================================
        if (mWorld.IsMansionEntranceDoorClosed() && mWorld.IsMansionEntranceDoorLocked())
        {
            mExplorationPhase = ExplorationPhase::TutorialOpenMap;


#if defined(_DEBUG)

            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Phase -> TutorialOpenMap\n"
            );

#endif
        }


        break;
    }


    //============================================================
    // Map Tutorial
    //============================================================
    case ExplorationPhase::TutorialOpenMap:
    {
        if (mMapOpenedOnce)
        {
            mExplorationPhase = ExplorationPhase::TutorialFlashlight;
        }

        break;
    }

    //============================================================
    // Flashlight Tutorial
    //============================================================
    case ExplorationPhase::TutorialFlashlight:
    {
        if (pPlayer->IsFlashlightEnabled())
        {
            mExplorationPhase = ExplorationPhase::ReachPowerTerminal;
        }

        break;
    }

    //============================================================
    // Power Terminal
    //============================================================
    case ExplorationPhase::ReachPowerTerminal:
    {
        //========================================================
        // PlayerがTerminalを操作したか
        //========================================================
        if (mWorld.IsMansionPowerTerminalActivated())
        {
            mExplorationPhase = ExplorationPhase::RestorePower;

#if defined(_DEBUG)

            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Phase -> RestorePower\n"
            );

#endif
        }
        break;
    }

    //============================================================
    // Mansion Power Restore
    //============================================================
    case ExplorationPhase::RestorePower:
    {
        //========================================================
        // 順番に照明を復旧させるSequence開始
        //========================================================
        mWorld.BeginMansionPowerRestore();

        //========================================================
        // Awakening待機Timer
        //========================================================
        mPursuerAwakeningTimer = 0.0f;

        //========================================================
        // 次のPhase
        //========================================================
        mExplorationPhase = ExplorationPhase::PursuerAwakening;

#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Phase -> PursuerAwakening\n"
        );

#endif


        break;
    }

    case ExplorationPhase::PursuerAwakening:
    {
        //========================================================
        // まだ電源復旧Sequence中
        //========================================================
        if (!mWorld.IsMansionPowerRestoreFinished())
        {
            break;
        }

        //========================================================
        // 全照明が点いてから少し待つ
        //========================================================
        mPursuerAwakeningTimer += dt;

        constexpr float AwakeningDelay = 1.0f;

        if (mPursuerAwakeningTimer < AwakeningDelay)
        {
            break;
        }

        //========================================================
        // Monster Voice Phase
        //========================================================
        mPursuerWakeVoiceTimer = 0.0f;

        mExplorationPhase = ExplorationPhase::PursuerWakeVoice;

        SoundBank::getInstance()->PlaySfx(SoundID::AE_PursuerWake, 0.92f);
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Phase -> PursuerWakeVoice "
            L"(sound placeholder)\n"
        );

#endif


        break;
    }

    case ExplorationPhase::PursuerWakeVoice:
    {
        mPursuerWakeVoiceTimer += dt;

        //========================================================
        // Sound実装までの仮演出時間
        //========================================================
        constexpr float WakeVoicePlaceholderDuration = 1.25f;

        if (mPursuerWakeVoiceTimer < WakeVoicePlaceholderDuration)
        {
            break;
        }

        //========================================================
        // 通常探索開始
        //========================================================
        mExplorationPhase = ExplorationPhase::FirstPuzzle;

#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Phase -> FirstPuzzle\n"
        );

#endif

        break;
    }

    case ExplorationPhase::FirstPuzzle:
    {
        //========================================================
        // Library Puzzle解決待ち
        //========================================================
        if (!mWorld.IsPuzzleSolved("MansionFirstSequencePuzzle"))
        {
            break;
        }

        mExplorationPhase = ExplorationPhase::SecondPuzzle;

#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"First Puzzle solved. "
            L"Phase -> SecondPuzzle\n"
        );

#endif
        break;
    }

    case ExplorationPhase::SecondPuzzle:
    {
        //========================================================
        // Second Puzzle解決待ち
        //========================================================
        if (!mWorld.IsPuzzleSolved("MansionSecondSequencePuzzle"))
        {
            break;
        }

        //========================================================
        // Puzzle解決後はBasementKey探索へ
        //========================================================
        mExplorationPhase = ExplorationPhase::FindBasementKey;

#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Second Puzzle solved. "
            L"Phase -> FindBasementKey\n"
        );

#endif

        break;
    }

    //============================================================
    // Find Basement Key
    //============================================================
    case ExplorationPhase::FindBasementKey:
    {
        //========================================================
        // BasementKeyをまだ所持していない
        // 西寝室の探索を継続する
        //========================================================
        if (!mInventory.HasItem("BasementKey", 1))
        {
            break;
        }

        //========================================================
        // BasementKey取得済み
        //========================================================
        mExplorationPhase = ExplorationPhase::ReachBasement;

#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"BasementKey collected. "
            L"Phase -> ReachBasement\n"
        );

#endif

        break;
    }

    //============================================================
    // Reach Basement
    //============================================================
    case ExplorationPhase::ReachBasement:
    {
        //========================================================
        // 現在地を取得
        //========================================================
        const ExplorationLocation& location = mLocationSystem.GetCurrentLocation();

        //========================================================
        // 本館であることを確認
        // Annexにも地下があるため
        // FloorだけでなくAreaも確認する
        //========================================================
        if (location.Area != ExplorationArea::MainBuilding)
        {
            break;
        }

        //========================================================
        // 地下1階に到達したか確認
        // 階段途中など、まだB1のRoomに入っていない場合は
        // ReachBasementのままにする
        //========================================================
        if (location.Floor != ExplorationFloor::Basement1)
        {
            break;
        }

        //========================================================
        // 本館地下1階に到達
        //========================================================
        mExplorationPhase = ExplorationPhase::ExploreBasement;

#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Player reached Main Building B1. "
            L"Phase -> ExploreBasement\n"
        );

#endif

        break;
    }

    case ExplorationPhase::ExploreBasement:
    {
        //========================================================
        // 配電盤が既に起動済み
        //========================================================
        if (mWorld.IsMansionFuseBoxActivated())
        {
            mExplorationPhase = ExplorationPhase::ExploreMansion;
            break;
        }

        //========================================================
        // Fuseを先に拾った場合
        //========================================================
        if (mInventory.HasItem("Fuse", 1))
        {
            mExplorationPhase = ExplorationPhase::RestoreBasementPower;

#if defined(_DEBUG)
            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Fuse already collected. "
                L"Phase -> RestoreBasementPower\n"
            );
#endif
            break;
        }

        //========================================================
        // FuseBoxを調査済み
        //========================================================
        if (mWorld.IsMansionFuseBoxInspected())
        {
            mExplorationPhase = ExplorationPhase::FindFuse;

#if defined(_DEBUG)
            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Fuse Box inspected. "
                L"Phase -> FindFuse\n"
            );
#endif
        }

        break;
    }

    //============================================================
    // Find Fuse
    // 地下倉庫にある交換用ヒューズを探す
    //============================================================
    case ExplorationPhase::FindFuse:
    {
        //========================================================
        // 配電盤が起動済みなら探索完了
        //========================================================
        if (mWorld.IsMansionFuseBoxActivated())
        {
            mExplorationPhase = ExplorationPhase::ExploreMansion;
            break;
        }

        //========================================================
        // Fuseをまだ取得していない
        //========================================================
        if (!mInventory.HasItem("Fuse", 1))
        {
            break;
        }

        //========================================================
        // Fuse取得完了
        //========================================================
        mExplorationPhase = ExplorationPhase::RestoreBasementPower;

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Fuse collected. "
            L"Phase -> RestoreBasementPower\n"
        );
#endif

        break;
    }

    //============================================================
    // Restore Basement Power
    // Fuseを配電盤へ取り付ける段階
    //============================================================
    case ExplorationPhase::RestoreBasementPower:
    {
        //========================================================
        // FuseBoxがまだ起動していない
        //========================================================
        if (!mWorld.IsMansionFuseBoxActivated())
        {
            break;
        }

        //========================================================
        // FuseBox起動完了
        //========================================================
        mExplorationPhase = ExplorationPhase::ExploreMansion;

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Fuse Box activated. "
            L"Phase -> ExploreMansion\n"
        );
#endif

        break;
    }

    //============================================================
    // Reach Courtyard
    //============================================================
    case ExplorationPhase::ReachCourtyard:
    {
        const ExplorationLocation& location = mLocationSystem.GetCurrentLocation();

        //========================================================
        // 既に別館に到達している
        //========================================================
        if (location.Area == ExplorationArea::Annex)
        {
            mExplorationPhase = ExplorationPhase::ExploreAnnex;
            break;
        }

        //========================================================
        // AnnexKeyを先に取得している
        //========================================================
        if (mInventory.HasItem("AnnexKey", 1))
        {
            mExplorationPhase = ExplorationPhase::EnterAnnex;
            break;
        }

        //========================================================
        // 中庭に到達したか
        // LocationSystemがRooms.csvを元に判定する
        //========================================================
        if (location.Area != ExplorationArea::Courtyard)
        {
            break;
        }

        //========================================================
        // 中庭に到達
        //========================================================
        mExplorationPhase = ExplorationPhase::FindAnnexKey;

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Courtyard reached. "
            L"Phase -> FindAnnexKey\n"
        );
#endif

        break;
    }

    //============================================================
    // Find Annex Key
    // 中庭に置かれたAnnexKeyの取得を待つ
    //============================================================
    case ExplorationPhase::FindAnnexKey:
    {
        //========================================================
        // 既に別館に入っていれば探索開始
        //========================================================
        const ExplorationLocation& location = mLocationSystem.GetCurrentLocation();

        if (location.Area == ExplorationArea::Annex)
        {
            mExplorationPhase = ExplorationPhase::ExploreAnnex;
            break;
        }

        //========================================================
        // AnnexKey取得確認
        // WorldItemPickupがInventoryに追加した状態を検出
        //========================================================
        if (!mInventory.HasItem("AnnexKey", 1))
        {
            break;
        }

        //========================================================
        // Key取得済み
        //========================================================
        mExplorationPhase = ExplorationPhase::EnterAnnex;

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"AnnexKey collected. "
            L"Phase -> EnterAnnex\n"
        );
#endif

        break;
    }

    //============================================================
    // Enter Annex
    //============================================================
    case ExplorationPhase::EnterAnnex:
    {
        const ExplorationLocation& location = mLocationSystem.GetCurrentLocation();

        //========================================================
        // 別館1階に到達したか確認
        //========================================================
        if (location.Area != ExplorationArea::Annex || location.Floor != ExplorationFloor::Floor1)
        {
            break;
        }

        //========================================================
        // 別館探索開始
        //========================================================
        mExplorationPhase = ExplorationPhase::ExploreAnnex;

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Annex entered. "
            L"Phase -> ExploreAnnex\n"
        );
#endif

        break;
    }

    //============================================================
    // Find Stone Emblem
    //============================================================
    case ExplorationPhase::FindStoneEmblem:
    {
        //========================================================
        // ResearchSealが既に起動済み
        //========================================================
        if (mWorld.IsAnnexResearchSealActivated())
        {
            mExplorationPhase =
                ExplorationPhase::ActivateControlTerminal;

            break;
        }

        //========================================================
        // StoneEmblem未取得
        //========================================================
        if (!mInventory.HasItem("StoneEmblem", 1))
        {
            break;
        }

        //========================================================
        // StoneEmblem取得完了
        //========================================================
        mExplorationPhase = ExplorationPhase::ReachAnnexBasement;

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"StoneEmblem collected. "
            L"Phase -> ReachAnnexBasement\n"
        );
#endif

        break;
    }

    //============================================================
    // Reach Annex Basement
    // StoneEmblem取得後 別館の地下研究施設へ向かう
    //============================================================
    case ExplorationPhase::ReachAnnexBasement:
    {
        //========================================================
        // ResearchSeal起動済み
        //========================================================
        if (mWorld.IsAnnexResearchSealActivated())
        {
            mExplorationPhase = ExplorationPhase::ActivateControlTerminal;

            break;
        }

        //========================================================
        // StoneEmblemを持っていない場合
        // 封印装置も未起動なので紋章の探索状態へ戻す
        //========================================================
        if (!mInventory.HasItem("StoneEmblem", 1))
        {
            mExplorationPhase =
                ExplorationPhase::FindStoneEmblem;

            break;
        }

        //========================================================
        // 現在地を取得
        //========================================================
        const ExplorationLocation& location =
            mLocationSystem.GetCurrentLocation();

        //========================================================
        // 別館地下1階に到達したか確認
        //========================================================
        if (location.Area != ExplorationArea::Annex || location.Floor != ExplorationFloor::Basement1)
        {
            break;
        }

        //============================================================
        // 別館B1へ到達
        // 追加した安全装置パズルの解決状態を確認する
        //============================================================
        if (mWorld.IsPuzzleSolved("AnnexSafetyRecoveryPuzzle"))
        {
            //========================================================
            // すでに安全装置が復旧している
            // Save/Loadなどで解決済みの場合は
            // 再度Puzzleを要求しない
            //========================================================
            mExplorationPhase = ExplorationPhase::ReachResearchSeal;
        }
        else
        {
            //========================================================
            // 安全装置が未復旧
            // 観察室・実験室の探索を促す
            //========================================================
            mExplorationPhase = ExplorationPhase::RestoreAnnexSafety;
        }

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Annex B1 reached. "
            L"Safety puzzle state checked.\n"
        );
#endif

        break;
    }

    //============================================================
    // Reach Research Seal
    //============================================================
    case ExplorationPhase::ReachResearchSeal:
    {
        //========================================================
        // ResearchSealが起動したか確認
        //========================================================
        if (mWorld.IsAnnexResearchSealActivated())
        {
            if (!mWorld.IsPuzzleSolved("AnnexSafetyRecoveryPuzzle"))
            {
                mExplorationPhase =
                    ExplorationPhase::RestoreAnnexSafety;

                break;
            }

            mExplorationPhase = ExplorationPhase::ActivateControlTerminal;

#if defined(_DEBUG)
            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Research Seal activated. "
                L"Phase -> ActivateControlTerminal\n"
            );
#endif

            break;
        }

        //========================================================
        // 未起動なのにStoneEmblemを持っていない
        //========================================================
        if (!mInventory.HasItem("StoneEmblem", 1))
        {
            mExplorationPhase = ExplorationPhase::FindStoneEmblem;
        }

        break;
    }

    //============================================================
    // Restore Annex Safety
    // Annex B1の中間Puzzle
    //============================================================
    case ExplorationPhase::RestoreAnnexSafety:
    {
        //========================================================
        // ResearchSeal起動済み
        // セーブデータや先行操作による状態の差を吸収する
        //========================================================
        if (mWorld.IsAnnexResearchSealActivated())
        {
            mExplorationPhase = ExplorationPhase::ActivateControlTerminal;

            break;
        }

        //========================================================
        // Safety Puzzle未解決
        //========================================================
        if (!mWorld.IsPuzzleSolved("AnnexSafetyRecoveryPuzzle"))
        {
            break;
        }

        //========================================================
        // 安全装置の復旧完了
        //========================================================
        mExplorationPhase = ExplorationPhase::ReachResearchSeal;

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Annex Safety Puzzle solved. "
            L"Phase -> ReachResearchSeal\n"
        );
#endif

        break;
    }

    //============================================================
    // Activate Control Terminal
    //============================================================
    case ExplorationPhase::ActivateControlTerminal:
    {
        //========================================================
        // Terminal起動待ち
        //========================================================
        break;
    }

    case ExplorationPhase::ExploreMansion:
    {
        //========================================================
        // Annex B1 Control Terminal
        // 起動済みならゲーム最終段階へ進む
        //========================================================
        if (mWorld.IsAnnexControlTerminalActivated())
        {
            mExplorationPhase = ExplorationPhase::EscapeMansion;

            //====================================================
            // 屋敷の正面玄関を解錠する
            //====================================================
            mWorld.UnlockMansionEntranceDoor();

#if defined(_DEBUG)
            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Annex Terminal activated. "
                L"Phase -> EscapeMansion\n"
            );
#endif
            break;
        }

        //========================================================
        // ExploreMansionから中庭探索へ移行
        //========================================================
        if (mExplorationPhase == ExplorationPhase::ExploreMansion && mWorld.IsMansionFuseBoxActivated())
        {
            const ExplorationLocation& location = mLocationSystem.GetCurrentLocation();

            // 既に別館へ到達している
            if (location.Area == ExplorationArea::Annex)
            {
                mExplorationPhase = ExplorationPhase::ExploreAnnex;
            }
            // AnnexKeyを既に取得している
            else if (mInventory.HasItem("AnnexKey", 1))
            {
                mExplorationPhase = ExplorationPhase::EnterAnnex;
            }
            // 既に中庭にいる
            else if (location.Area == ExplorationArea::Courtyard)
            {
                mExplorationPhase = ExplorationPhase::FindAnnexKey;
            }
            // 通常は中庭へ向かう段階から開始
            else
            {
                mExplorationPhase = ExplorationPhase::ReachCourtyard;
            }

#if defined(_DEBUG)
            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"FuseBox restored. "
                L"Courtyard progression started.\n"
            );
#endif
        }

        break;
    }

    case ExplorationPhase::ExploreAnnex:
    {
        //========================================================
           // 最優先：ResearchSealが既に起動している
           //========================================================
        if (mWorld.IsAnnexResearchSealActivated())
        {
            mExplorationPhase =
                ExplorationPhase::ActivateControlTerminal;

            break;
        }

        //========================================================
        // StoneEmblemを取得済み
        //========================================================
        if (mInventory.HasItem("StoneEmblem", 1))
        {
            mExplorationPhase =
                ExplorationPhase::ReachAnnexBasement;

            break;
        }

        //========================================================
        // 資料室の認証パズルが未解決
        //========================================================
        if (!mWorld.IsPuzzleSolved("AnnexArchiveSequencePuzzle"))
        {
            mExplorationPhase =
                ExplorationPhase::SolveAnnexArchivePuzzle;

            break;
        }

        //========================================================
        // 認証パズル解決済み
        // 資料室に入ってStoneEmblemを探す
        //========================================================
        mExplorationPhase = ExplorationPhase::FindStoneEmblem;

        break;
    }

    //============================================================
    // Solve Annex Archive Puzzle
    //============================================================
    case ExplorationPhase::SolveAnnexArchivePuzzle:
    {
        //========================================================
        // すでに研究封印装置を解除している
        //========================================================
        if (mWorld.IsAnnexResearchSealActivated())
        {
            mExplorationPhase =
                ExplorationPhase::ActivateControlTerminal;

            break;
        }

        //========================================================
        // StoneEmblemを先に取得している
        //========================================================
        if (mInventory.HasItem("StoneEmblem", 1))
        {
            mExplorationPhase = ExplorationPhase::ReachAnnexBasement;

            break;
        }

        //========================================================
        // 認証パズル未解決
        //========================================================
        if (!mWorld.IsPuzzleSolved("AnnexArchiveSequencePuzzle"))
        {
            break;
        }

        //========================================================
        // 認証パズル解決
        //========================================================
        mExplorationPhase = ExplorationPhase::FindStoneEmblem;

#if defined(_DEBUG)
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Annex Archive Puzzle solved. "
            L"Phase -> FindStoneEmblem\n"
        );
#endif

        break;
    }

    //============================================================
    // Final Escape
    // Control Terminal起動後
    // Playerは屋敷玄関へ戻って脱出する
    //============================================================
    case ExplorationPhase::EscapeMansion:
    {
        //========================================================
        // Save / Load対策
        // EscapeMansion状態でLoadした場合でも
        // Mansion Entranceが確実にUnlockされるようにする
        //========================================================
        if (mWorld.IsMansionEntranceDoorLocked())
        {
            mWorld.UnlockMansionEntranceDoor();

#if defined(_DEBUG)

            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Mansion entrance unlocked for escape.\n"
            );
#endif
        }

        //========================================================
        // Playerが玄関外まで脱出したか
        //========================================================
        if (!IsPlayerInMansionEscapeArea())
        {
            break;
        }


#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Player reached mansion escape area.\n"
        );

#endif


        //========================================================
        // Game Clear開始
        //========================================================
        BeginEscapeComplete();

        break;
    }

    case ExplorationPhase::Escaped:
    {
        //========================================================
        // GameClear後なので進行処理なし
        //========================================================
        break;
    }

    default:
        break;
    }
}

void SceneAtmosphericEscape::UpdatePause(float dt)
{
    //========================================================
    // UI Inputのみ
    //========================================================
    mPauseInput.HandleInput(&mPauseUI, dt);
    //========================================================
    // Pause UI
    //========================================================
    mPauseUI.Update(dt);

    //========================================================
    // Action
    //========================================================
    const UIPauseOverlay::Action action = mPauseUI.ConsumeAction();

    switch (action)
    {
    case UIPauseOverlay::Action::Resume:
    {
        ExitPause();
        break;
    }


    case UIPauseOverlay::Action::Save:
    {
        //====================================================
        // Scene側でももう一度検証する
        //====================================================
        if (SaveGame())
        {
#if defined(_DEBUG)

            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Manual save completed.\n"
            );

#endif
        }

        break;
    }


    case UIPauseOverlay::Action::ToTitle:
    {
        if (!FadeManager::Get().IsFading())
        {
            FadeManager::Get().StartFadeOut(0.50f, [this]()
                {
                    switchScene(eSceneTable::AtmosphericEscapeTitle);
                }
            );
        }

        break;
    }


    case UIPauseOverlay::Action::Exit:
    {
        PostQuitMessage(0);

        break;
    }

    case UIPauseOverlay::Action::Options:
    {
        if (!FadeManager::Get().IsFading())
        {
            mPauseUI.Hide();
            mPauseInput.Reset();
            mOptionsInput.Reset();
            mOptionsUI.Show();
            mOptionsUI.Draw(); // 開いたフレームから文字も表示する
        }
        break;
    }


    case UIPauseOverlay::Action::None:
    default:
    {
        break;
    }
    }
}

void SceneAtmosphericEscape::DrawGameplayUI()
{
    //============================================================
    // Interaction Prompt
    //============================================================
    if (!mRuntimeDebug.BlocksPlayerControl())
    {
        IInteractable* interactionTarget = pPlayer->GetInteractionTarget();


        if (interactionTarget != nullptr && interactionTarget->CanInteract())
        {
            const float promptX =
                static_cast<float>(vnMainFrame::screenWidth) * 0.5f - 80.0f;

            const float promptY =
                static_cast<float>(vnMainFrame::screenHeight) - 120.0f;

            AEUIFont::Scope font(AEUIFont::Style::Label);
            vnFont::setColor(0xffffffff);

            const bool useGamepad = InputDeviceTracker::Get().GetDevice() == ActiveDevice::Gamepad;

            vnFont::print(
                promptX,
                promptY,
                useGamepad ? L"[A] %s" : L"[E] %s",
                interactionTarget->GetInteractionText()
            );
        }
    }


    //============================================================
    // Objective
    //============================================================
    const ExplorationObjective* objective = mObjectiveManager.GetCurrentObjective();

    if (objective != nullptr)
    {
        AEUIFont::Scope font(AEUIFont::Style::Label);
        vnFont::setColor(0xffffffff);

        //========================================================
        // Main Objective
        //========================================================
        vnFont::print(
            30.0f,
            40.0f,
            L"Objective: %s",
            objective->Text.c_str()
        );

        //========================================================
        // Hint
        // 空なら表示しない
        //========================================================
        if (!objective->Hint.empty())
        {
            font.Use(AEUIFont::Style::Hint);
            vnFont::setColor(0xffd8d8d8);

            vnFont::print(
                30.0f,
                70.0f,
                L"Hint: %s",
                objective->Hint.c_str()
            );
        }
    }

#if defined(_DEBUG)

    //============================================================
    // Current Room Debug
    //============================================================
    const RoomDefinition* currentRoom = mLocationSystem.GetCurrentRoom();


    if (currentRoom != nullptr)
    {
        vnFont::setColor(0xffa0d8ff);

        vnFont::print(
            30.0f,
            110.0f,
            L"Room: %s",
            currentRoom->DisplayName.c_str()
        );
    }

#endif

#if defined(_DEBUG)

    //============================================================
    // Inventory Debug
    //============================================================
    const auto inventoryEntries = mInventory.GetEntries();

    float inventoryDebugY = 150.0f;

    vnFont::setColor(0xff90ff90);

    vnFont::print(
        30.0f,
        inventoryDebugY,
        L"Inventory:"
    );

    inventoryDebugY += 24.0f;


    for (const InventoryEntry& entry : inventoryEntries)
    {
        const InventoryItemDefinition* definition = mInventory.FindDefinition(entry.ItemId);


        if (definition == nullptr)
        {
            continue;
        }

        vnFont::print(
            50.0f,
            inventoryDebugY,
            L"%s x%u",
            definition->Name.c_str(),
            entry.Count
        );

        inventoryDebugY += 22.0f;
    }

#endif

#if defined(_DEBUG)

    {
        const auto& lightStats = AtmosphericEscapeLightBudget::GetDebugStats();

        //============================================================
        // Light Budget Debug
        // Submit: Scene内で今Frame有効なLight候補数
        // Active: 実際にLocalLightManagerへ送った数
        // Shadow: Shadow Mapを持っている数
        //============================================================
        vnFont::setColor(0xffffd080);

        vnFont::print(
            30.0f,
            280.0f,
            L"Light Point: Submit=%u Active=%u Shadow=%u",
            lightStats.SubmittedPointLights,
            lightStats.ActivePointLights,
            lightStats.PointShadowLights
        );

        vnFont::print(
            30.0f,
            305.0f,
            L"Light Spot : Submit=%u Active=%u Shadow=%u",
            lightStats.SubmittedSpotLights,
            lightStats.ActiveSpotLights,
            lightStats.SpotShadowLights
        );
    }

#endif
}

void SceneAtmosphericEscape::BeginEscapeComplete()
{
    //============================================================
    // 二重実行防止
    //============================================================
    if (mSceneFlowState != SceneFlowState::Playing)
    {
        return;
    }

    SoundBank::getInstance()->StopLoop(SoundID::AE_BgmFinalChase);

    //============================================================
    // Result GameClear
    //============================================================
    mResultType = SceneResultType::GameClear;

    //============================================================
    // Game Progress
    //============================================================
    mExplorationPhase = ExplorationPhase::Escaped;


    //============================================================
    // Player停止
    //============================================================
    pPlayer->SetControlLocked(true);

    //============================================================
    // Scene Flow
    //============================================================
    mSceneFlowState = SceneFlowState::EscapeComplete;

    //============================================================
    // Clear演出時間
    //============================================================
    mEscapeCompleteTimer = 1.25f;


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[AtmosphericEscape] "
        L"Escape Complete.\n"
    );

#endif
}

void SceneAtmosphericEscape::UpdateEscapeComplete(float dt)
{
    UpdateEscapeCompleteLighting(dt);

    mEscapeCompleteTimer -= dt;
    if (mEscapeCompleteTimer > 0.0f || FadeManager::Get().IsFading())
    {
        return;
    }

    mSceneFlowState = SceneFlowState::FadeToResult;

    // 景色が白く覆われたところでクリア画面とBGMに切り替える
    FadeManager::Get().StartFadeOut(0.65f, [this]()
        {
            EnterResult();
            FadeManager::Get().StartFadeIn(0.35f, FadeColor::White);
        },
        FadeColor::White
    );
}

void SceneAtmosphericEscape::SubmitEscapeGoalLight() const
{
    // ControlTerminal起動後の逃走中とクリア演出中に点灯する
    // ゴール到達フレームで進行フェーズがEscapedになっても
    // EscapeComplete の状態を見てライトの登録を継続する
    const bool finalEscape = mExplorationPhase == ExplorationPhase::EscapeMansion;
    const bool clearPresentation =
        mSceneFlowState == SceneFlowState::EscapeComplete ||
        (mSceneFlowState == SceneFlowState::FadeToResult && mResultType == SceneResultType::GameClear);

    if (!finalEscape && !clearPresentation)
    {
        return;
    }

    AtmosphericEscapeLightBudget::SpotLightRequest request{};

    // ゴール判定の少し先正面通路の中心線上に配置する
    request.Position = XMFLOAT3(0.0f, 5.5f, -31.0f);

    // 玄関側の +Z へ向けながら光の軸をプレイヤーの高さまで下げる
    XMStoreFloat3(
        &request.Direction,
        XMVector3Normalize(XMVectorSet(0.0f, -0.60f, 1.0f, 0.0f))
    );

    request.Color = XMFLOAT3(1.0f, 0.94f, 0.84f);
    request.Intensity = 220.0f;
    request.Range = 40.0f;

    request.InnerAngleDegrees = 16.0f;
    request.OuterAngleDegrees = 36.0f;

    request.CastsShadow = true;
    request.ForceShadow = true;

    AtmosphericEscapeLightBudget::SubmitSpotLight(request);
}

void SceneAtmosphericEscape::UpdateEscapeCompleteLighting(float dt)
{
    LocalLightManager::BeginFrame();
    SpotLightShadowManager::BeginFrame();
    PointLightShadowManager::BeginFrame();
    AtmosphericEscapeLightBudget::BeginFrame();

    // ワールドのライト候補とクリア地点のスポットライトを登録する
    mWorld.Update(dt);
    SubmitEscapeGoalLight();

    // 候補をこのフレームのライトとして確定してからGPUへ送る
    XMFLOAT3 playerPosition;
    XMStoreFloat3(&playerPosition, *pPlayer->getPosition());

    AtmosphericEscapeLightBudget::ResolveAndRegister(playerPosition, mLocationSystem);
    LocalLightManager::Upload();
}

void SceneAtmosphericEscape::EnterPause()
{
    if (mSceneFlowState != SceneFlowState::Playing)
    {
        return;
    }

    //========================================================
    // Gameplayが止まる前にSave可否を取得
    //========================================================
    const bool canSave = CanSaveGame();

    //========================================================
    // Pause UI
    //========================================================
    mPauseUI.SetSaveEnabled(canSave);
    mPauseUI.Show();
    mPauseInput.Reset();

    //========================================================
    // Player Input停止
    //========================================================
    if (pPlayer != nullptr)
    {
        pPlayer->SetControlLocked(true);
    }

    //========================================================
    // Scene State
    //========================================================
    mSceneFlowState = SceneFlowState::Pause;
}

void SceneAtmosphericEscape::ExitPause()
{
    if (mSceneFlowState != SceneFlowState::Pause)
    {
        return;
    }

    mPauseUI.Hide();
    mPauseInput.Reset();

    //========================================================
    // Gameplayへ戻る
    //========================================================
    mSceneFlowState = SceneFlowState::Playing;

    //========================================================
    // Player Control
    //========================================================
    if (pPlayer != nullptr)
    {
        pPlayer->SetControlLocked(mRuntimeDebug.BlocksPlayerControl());
    }
}

void SceneAtmosphericEscape::EnterResult()
{
    //============================================================
    // Scene Flow
    //============================================================
    mSceneFlowState = SceneFlowState::Result;


    //============================================================
    // Player停止
    //============================================================
    if (pPlayer != nullptr)
    {
        pPlayer->SetControlLocked(true);
    }

    auto* sound = SoundBank::getInstance();
    sound->StopLoop(SoundID::AE_BgmAmbience);

    //============================================================
    // UI Input
    // Gameplay中の入力状態が残らないようにリセット
    //============================================================
    mUIInput.Reset();


    //============================================================
    // Result Type
    //============================================================
    switch (mResultType)
    {
    case SceneResultType::GameClear:
    {
        LocalLightManager::BeginFrame();
        SpotLightShadowManager::BeginFrame();
        PointLightShadowManager::BeginFrame();
        LocalLightManager::Upload();

        PostProcessManager::AllDisable();

        mResultUI.ShowGameClear();
        sound->PlaySfx(SoundID::AE_BgmClear, 0.58f);
        break;
    }


    case SceneResultType::GameOver:
    {
        mResultUI.ShowGameOver();
        sound->StartLoop(SoundID::AE_BgmGameOver, 0.48f);
        break;
    }


    case SceneResultType::None:
    default:
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"EnterResult: ResultType is None.\n"
        );

#endif

        break;
    }
    }


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[AtmosphericEscape] "
        L"Enter Result.\n"
    );

#endif

}

void SceneAtmosphericEscape::UpdateResult(float dt)
{
    //============================================================
    // UI Input
    //============================================================
    mUIInput.HandleInput(&mResultUI, dt);

    //============================================================
    // Result UI
    //============================================================
    mResultUI.Update(dt);

    //============================================================
    // Action
    //============================================================
    const UIResultOverlay::Action action = mResultUI.ConsumeAction();


    switch (action)
    {
    case UIResultOverlay::Action::Restart:
    {
        //========================================================
        // Fade中の二重入力防止
        //========================================================
        if (FadeManager::Get().IsFading())
        {
            break;
        }


        //========================================================
        // Restart
        //========================================================
        FadeManager::Get().StartFadeOut(0.50f, [this]()
            {
                switchScene(eSceneTable::AtmosphericEscape, true);
            }
        );

        break;
    }


    case UIResultOverlay::Action::ToTitle:
    {
        if (FadeManager::Get().IsFading())
        {
            break;
        }


        FadeManager::Get().StartFadeOut(0.50f, [this]()
            {
                switchScene(eSceneTable::AtmosphericEscapeTitle);
            }
        );

        break;
    }


    case UIResultOverlay::Action::Exit:
    {
        PostQuitMessage(0);

        break;
    }


    case UIResultOverlay::Action::None:
    default:
    {
        break;
    }
    }
}

void SceneAtmosphericEscape::BeginPlayerDeath()
{
    //============================================================
    // 二重実行防止
    //============================================================
    if (mSceneFlowState != SceneFlowState::Playing)
    {
        return;
    }

    SoundBank::getInstance()->StopLoop(SoundID::AE_BgmFinalChase);

    //============================================================
    // ResultはGameOver
    //============================================================
    mResultType = SceneResultType::GameOver;

    //============================================================
    // Player停止
    //============================================================
    if (pPlayer != nullptr)
    {
        pPlayer->SetControlLocked(true);
    }


    //============================================================
    // Death State
    //============================================================
    mSceneFlowState = SceneFlowState::PlayerDead;

    mPlayerDeadTimer = 1.50f;


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[AtmosphericEscape] "
        L"Player Dead.\n"
    );

#endif
}

void SceneAtmosphericEscape::SyncObjectiveFromExplorationPhase()
{
    switch (mExplorationPhase)
    {
        //========================================================
        // Mansionへ入る
        //========================================================
    case ExplorationPhase::EnterMansion:
    {
        mObjectiveManager.SetCurrentObjective("EnterMansion");

        break;
    }

    //========================================================
    // Entrance Doorが閉じる演出
    //========================================================
    case ExplorationPhase::EntranceLocked:
    {
        mObjectiveManager.ClearCurrentObjective();
        break;
    }

    //========================================================
    // Map Tutorial
    //========================================================
    case ExplorationPhase::TutorialOpenMap:
    {
        mObjectiveManager.SetCurrentObjective("TutorialOpenMap");
        break;
    }

    //========================================================
    // Flashlight Tutorial
    //========================================================
    case ExplorationPhase::TutorialFlashlight:
    {
        mObjectiveManager.SetCurrentObjective("TutorialFlashlight");
        break;
    }

    //========================================================
    // Power Roomへ向かう
    //========================================================
    case ExplorationPhase::ReachPowerTerminal:
    {
        mObjectiveManager.SetCurrentObjective("ReachPowerTerminal");

        break;
    }

    //========================================================
    // Terminal操作
    //========================================================
    case ExplorationPhase::RestorePower:
    {
        mObjectiveManager.SetCurrentObjective("RestorePower");

        break;
    }

    //========================================================
    // Monster Awakening
    // 演出中なのでObjectiveを変更しない
    //========================================================
    case ExplorationPhase::PursuerAwakening:
    {
        break;
    }

    //========================================================
    // Monster Voice
    //========================================================
    case ExplorationPhase::PursuerWakeVoice:
    {
        mObjectiveManager.SetCurrentObjective("InvestigateNoise");

        break;
    }

    //========================================================
    // First Puzzle
    //========================================================
    case ExplorationPhase::FirstPuzzle:
    {
        mObjectiveManager.SetCurrentObjective("SolveFirstPuzzle");

        break;
    }

    //========================================================
    // Second Puzzle
    //========================================================
    case ExplorationPhase::SecondPuzzle:
    {
        mObjectiveManager.SetCurrentObjective("SecondPuzzle");

        break;
    }


    //========================================================
    // Basement Key探索
    //========================================================
    case ExplorationPhase::FindBasementKey:
    {
        mObjectiveManager.SetCurrentObjective("FindBasementKey");

        break;
    }

    //============================================================
    // Reach Basement
    //============================================================
    case ExplorationPhase::ReachBasement:
    {
        mObjectiveManager.SetCurrentObjective("ReachBasement");
        break;
    }

    //============================================================
    // Explore Basement
    //============================================================
    case ExplorationPhase::ExploreBasement:
    {
        mObjectiveManager.SetCurrentObjective("ExploreBasement");

        break;
    }

    //============================================================
    // Find Fuse
    // 地下のヒューズボックスを調査した後
    //============================================================
    case ExplorationPhase::FindFuse:
    {
        mObjectiveManager.SetCurrentObjective("FindFuse");
        break;
    }

    //============================================================
    // Restore Basement Power
    // 交換用ヒューズを取得した後
    //============================================================
    case ExplorationPhase::RestoreBasementPower:
    {
        mObjectiveManager.SetCurrentObjective("RestoreBasementPower");
        break;
    }

    //============================================================
    // Reach Courtyard
    //============================================================
    case ExplorationPhase::ReachCourtyard:
    {
        mObjectiveManager.SetCurrentObjective("ReachCourtyard");
        break;
    }

    //============================================================
    // Find Annex Key
    //============================================================
    case ExplorationPhase::FindAnnexKey:
    {
        mObjectiveManager.SetCurrentObjective("FindAnnexKey");
        break;
    }

    //============================================================
    // Enter Annex
    //============================================================
    case ExplorationPhase::EnterAnnex:
    {
        mObjectiveManager.SetCurrentObjective("EnterAnnex");
        break;
    }

    //============================================================
    // Explore Annex
    //============================================================
    case ExplorationPhase::ExploreAnnex:
    {
        mObjectiveManager.SetCurrentObjective("ExploreAnnex");
        break;
    }

    case ExplorationPhase::SolveAnnexArchivePuzzle:
    {
        mObjectiveManager.SetCurrentObjective("SolveAnnexArchivePuzzle");
        break;
    }

    //============================================================
    // Find Stone Emblem
    //============================================================
    case ExplorationPhase::FindStoneEmblem:
    {
        mObjectiveManager.SetCurrentObjective("FindStoneEmblem");
        break;
    }

    //============================================================
    // Reach Annex Basement
    //============================================================
    case ExplorationPhase::ReachAnnexBasement:
    {
        mObjectiveManager.SetCurrentObjective("ReachAnnexBasement");
        break;
    }

    //============================================================
    // Annex B1 Safety Puzzle Objective
    //============================================================
    case ExplorationPhase::RestoreAnnexSafety:
    {
        mObjectiveManager.SetCurrentObjective("RestoreAnnexSafety");
        break;
    }

    //============================================================
    // Reach Research Seal
    //============================================================
    case ExplorationPhase::ReachResearchSeal:
    {
        mObjectiveManager.SetCurrentObjective("ReachResearchSeal");
        break;
    }

    //============================================================
    // Activate Control Terminal
    //============================================================
    case ExplorationPhase::ActivateControlTerminal:
    {
        mObjectiveManager.SetCurrentObjective("ActivateControlTerminal");
        break;
    }

    //========================================================
    // Mansion通常探索
    //========================================================
    case ExplorationPhase::ExploreMansion:
    {
        mObjectiveManager.SetCurrentObjective("ExploreMansion");
        break;
    }

    //========================================================
    // Final Escape
    //========================================================
    case ExplorationPhase::EscapeMansion:
    {
        mObjectiveManager.SetCurrentObjective("EscapeMansion");
        break;
    }

    //========================================================
    // Game Clear
    //========================================================
    case ExplorationPhase::Escaped:
    {
        mObjectiveManager.ClearCurrentObjective();
        break;
    }

    default:
        break;
    }
}

void SceneAtmosphericEscape::SyncPursuerActivationFromExplorationPhase()
{
    bool shouldBeActive = false;


    switch (mExplorationPhase)
    {
        //========================================================
        // Monster通常活動開始
        //========================================================
    case ExplorationPhase::FirstPuzzle:

    case ExplorationPhase::SecondPuzzle:

    case ExplorationPhase::FindBasementKey:

    case ExplorationPhase::ReachBasement:

    case ExplorationPhase::ExploreBasement:

    case ExplorationPhase::FindFuse:

    case ExplorationPhase::RestoreBasementPower:

    case ExplorationPhase::ReachCourtyard:

    case ExplorationPhase::FindAnnexKey:

    case ExplorationPhase::EnterAnnex:

    case ExplorationPhase::ExploreAnnex:

    case ExplorationPhase::FindStoneEmblem:

    case ExplorationPhase::ReachAnnexBasement:

    case ExplorationPhase::RestoreAnnexSafety:

    case ExplorationPhase::ReachResearchSeal:

    case ExplorationPhase::SolveAnnexArchivePuzzle:

    case ExplorationPhase::ActivateControlTerminal:

    case ExplorationPhase::ExploreMansion:

    case ExplorationPhase::EscapeMansion:
    {
        shouldBeActive = true;

        break;
    }


    //========================================================
    // Intro / Awakening中
    //========================================================
    case ExplorationPhase::EnterMansion:

    case ExplorationPhase::EntranceLocked:

    case ExplorationPhase::TutorialOpenMap:

    case ExplorationPhase::TutorialFlashlight:

    case ExplorationPhase::ReachPowerTerminal:

    case ExplorationPhase::RestorePower:

    case ExplorationPhase::PursuerAwakening:

    case ExplorationPhase::PursuerWakeVoice:

    case ExplorationPhase::Escaped:

    default:
    {
        shouldBeActive = false;

        break;
    }
    }

    mPursuerSystem.SetActive(shouldBeActive);

    if (mExplorationPhase == ExplorationPhase::EscapeMansion && pPlayer != nullptr)
    {
        mPursuerSystem.BeginFinalChase(*pPlayer);
    }

}

bool SceneAtmosphericEscape::IsPlayerInMansionEscapeArea() const
{
    //========================================================
    // Safety
    //========================================================
    if (pPlayer == nullptr)
    {
        return false;
    }

    //========================================================
    // Player Position
    //========================================================
    XMFLOAT3 playerPosition;

    XMStoreFloat3(&playerPosition, *pPlayer->getPosition());

    //========================================================
    // Escape Area
    //========================================================
    constexpr float EscapeMinX = -5.0f;
    constexpr float EscapeMaxX = 5.0f;
    constexpr float EscapeMaxZ = -24.0f;

    //========================================================
    // X判定
    //========================================================
    const bool insideX = playerPosition.x >= EscapeMinX && playerPosition.x <= EscapeMaxX;

    //========================================================
    // Z判定
    //========================================================
    const bool outsideMansion = playerPosition.z <= EscapeMaxZ;

    return insideX && outsideMansion;
}

void SceneAtmosphericEscape::OpenDocumentViewer(const std::string& documentId)
{
    //============================================================
    // Playing中のみ開く
    //============================================================
    if (mSceneFlowState != SceneFlowState::Playing)
    {
        return;
    }

    //============================================================
    // Definition取得
    //============================================================
    const DocumentDefinition* document = mDocumentManager.FindDefinition(documentId);

    if (document == nullptr)
    {
        return;
    }

    //============================================================
    // Player停止
    //============================================================
    if (pPlayer != nullptr)
    {
        pPlayer->SetControlLocked(true);
    }

    //============================================================
    // UI Inputを初期化
    //============================================================
    mExplorationMenuInput.Reset();

    //============================================================
    // Viewer
    //============================================================
    mDocumentViewer.Show(document);
    SoundBank::getInstance()->PlaySfx(SoundID::AE_DocumentPage, 0.68f);
    mSceneFlowState = SceneFlowState::DocumentViewer;
}

void SceneAtmosphericEscape::CloseDocumentViewer()
{
    mDocumentViewer.Hide();

    mExplorationMenuInput.Reset();

    //============================================================
    // Player Control復帰
    //============================================================
    if (pPlayer != nullptr)
    {
        pPlayer->SetControlLocked(false);
    }

    //============================================================
    // Gameplayへ戻る
    //============================================================
    mSceneFlowState = SceneFlowState::Playing;
}

void SceneAtmosphericEscape::UpdatePlayerDeath(float dt)
{
    //============================================================
    // Light Frame Begin
    //============================================================
    LocalLightManager::BeginFrame();
    SpotLightShadowManager::BeginFrame();
    PointLightShadowManager::BeginFrame();
    AtmosphericEscapeLightBudget::BeginFrame();

    //============================================================
    // World
    //============================================================
    mWorld.Update(dt);

    // LightBudgetのBeginFrame後に死亡演出と懐中電灯を更新する
    pPlayer->UpdateDeathPresentation(dt, !mRuntimeDebug.OwnsCamera(), mColliders);

    XMFLOAT3 playerPosition;
    XMStoreFloat3(&playerPosition, *pPlayer->getPosition());

    // カメラと懐中電灯がこのフレームの姿勢になってからライトを確定する
    AtmosphericEscapeLightBudget::ResolveAndRegister(playerPosition, mLocationSystem);

    //============================================================
    // Light Upload
    //============================================================
    LocalLightManager::Upload();


    //============================================================
    // Death表示
    //============================================================
    vnFont::setColor(0xffffffff);


    vnFont::print(
        30.0f,
        40.0f,
        L"YOU DIED"
    );


    //============================================================
    // Death Timer
    //============================================================
    mPlayerDeadTimer -= dt;

    if (mPlayerDeadTimer > 0.0f)
    {
        return;
    }

    //============================================================
    // すでにFade中なら何もしない
    //============================================================
    if (FadeManager::Get().IsFading())
    {
        return;
    }

    //============================================================
    // Resultへ
    //============================================================
    mSceneFlowState = SceneFlowState::FadeToResult;

    FadeManager::Get().StartFadeOut(0.75f, [this]()
        {
            //================================================
            // 完全に暗くなった瞬間にResultを表示
            //================================================
            EnterResult();

            //================================================
            // 黒画面からResultへFade In
            //================================================
            FadeManager::Get().StartFadeIn(0.50f);
        }
    );
}

bool SceneAtmosphericEscape::SaveGame()
{
    //========================================================
    // Save禁止状態
    //========================================================
    if (!CanSaveGame())
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Save rejected.\n"
        );

#endif

        return false;
    }


    //========================================================
    // Snapshot作成
    //========================================================
    const AtmosphericEscapeSaveData data = BuildSaveData();


    //========================================================
    // File Save
    //========================================================
    const bool saved = AtmosphericEscapeSaveManager::Save(data);


#if defined(_DEBUG)

    if (saved)
    {
        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Game saved.\n"
        );
    }

#endif


    return saved;
}

bool SceneAtmosphericEscape::CanSaveGame() const
{
    //========================================================
    // Playerなし
    //========================================================
    if (pPlayer == nullptr)
    {
        return false;
    }


    //========================================================
    // 死亡済み
    //========================================================
    if (pPlayer->IsDead())
    {
        return false;
    }


    //========================================================
    // Fade中
    //========================================================
    if (FadeManager::Get().IsFading())
    {
        return false;
    }


    //========================================================
    // 通常Gameplay / Pause中のみ許可
    // Pause Stateを追加する場合はPauseも許可する
    //========================================================
    if (mSceneFlowState != SceneFlowState::Playing && mSceneFlowState != SceneFlowState::Pause)
    {
        return false;
    }


    //========================================================
    // Pursuer
    // Chase / Attack中はSave不可
    //========================================================
    const PursuerMonster::State pursuerState = mPursuerSystem.GetState();


    if (pursuerState == PursuerMonster::State::Chase || pursuerState == PursuerMonster::State::Attack)
    {
        return false;
    }

    return true;
}

bool SceneAtmosphericEscape::ApplySaveData(const AtmosphericEscapeSaveData& data)
{
    //============================================================
    // Safety
    //============================================================
    if (pPlayer == nullptr)
    {
        return false;
    }


    //============================================================
    // Player
    //============================================================
    pPlayer->RestoreFromSave(
        data.PlayerPosition,
        data.PlayerHealth
    );


    //============================================================
    // Exploration Phase Validation
    //============================================================
    const std::uint32_t maxPhase = static_cast<std::uint32_t>(ExplorationPhase::MaxValue);


    if (data.ExplorationPhase > maxPhase)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[AtmosphericEscape] "
            L"Invalid ExplorationPhase in save.\n"
        );

#endif

        return false;
    }


    //============================================================
    // Exploration Phase
    //============================================================
    mExplorationPhase = static_cast<ExplorationPhase>(data.ExplorationPhase);


    //============================================================
    // Play Time
    //============================================================
    mPlayTime = (std::max)(data.PlayTime, 0.0f);

    //============================================================
    // World Persistent State
    // Door / Terminal / Key / Puzzleなどを
    // Persistent IDで復元する
    //============================================================
    mWorld.RestorePersistentObjects(data.WorldObjects);

    //============================================================
    // 玄関ドアの進行状況復元
    // 玄関は通常ドアと異なり 専用イベントで施錠される
    // 復元済みのExplorationPhaseから状態を決定する
    //============================================================

    // 屋敷に入る前のフェーズだけ玄関を開けておく
    const bool enteredMansion = mExplorationPhase != ExplorationPhase::EnterMansion;

    // 最終脱出中または脱出済みなら玄関の鍵を解除
    const bool escapeUnlocked =
        mExplorationPhase == ExplorationPhase::EscapeMansion || mExplorationPhase == ExplorationPhase::Escaped;

    // モデル、コライダー、施錠状態をまとめて復元
    mWorld.RestoreMansionEntranceFromProgress(enteredMansion, escapeUnlocked);

    //============================================================
    // Inventory
    //============================================================
    mInventory.RestoreEntries(data.Inventory);


    //============================================================
    // V2 Save Migration
    // V2にはInventoryが存在しない
    // 旧Saveで既にAccessKeyを取得済みなら
    // Persistent World側の取得状態からInventoryへ補完する
    //============================================================
    if (data.Version == 2)
    {
        if (mWorld.IsFacilityAccessKeyCollected())
        {
            mInventory.AddItem("FacilityAccessKey");
        }
    }


    //============================================================
    // Map Discovery
    //============================================================
    mLocationSystem.RestoreDiscoveredRooms(data.DiscoveredRooms);

    //============================================================
    // Current Objective
    //============================================================
    bool objectiveRestored = false;


    if (!data.CurrentObjectiveId.empty())
    {
        objectiveRestored = mObjectiveManager.SetCurrentObjective(data.CurrentObjectiveId);
    }


    //============================================================
    // V2または削除されたObjective IDなどの場合
    // ExplorationPhaseから現在Objectiveを再構築する
    //============================================================
    if (!objectiveRestored)
    {
        SyncObjectiveFromExplorationPhase();
    }


    //============================================================
    // Gameplay State
    //============================================================
    mSceneFlowState = SceneFlowState::Playing;


#if defined(_DEBUG)

    wchar_t buffer[256];

    swprintf_s(
        buffer,
        L"[AtmosphericEscape] "
        L"Save data applied. "
        L"PlayerPos=(%.2f, %.2f, %.2f) "
        L"HP=%.1f\n",
        data.PlayerPosition.x,
        data.PlayerPosition.y,
        data.PlayerPosition.z,
        data.PlayerHealth
    );

    OutputDebugStringW(buffer);

#endif


    return true;
}

AtmosphericEscapeSaveData SceneAtmosphericEscape::BuildSaveData() const
{
    AtmosphericEscapeSaveData data;

    //============================================================
    // Player
    //============================================================
    if (pPlayer != nullptr)
    {
        XMStoreFloat3(&data.PlayerPosition, *pPlayer->getPosition());
        data.PlayerHealth = pPlayer->GetHealth();
    }


    //============================================================
    // Progress
    //============================================================
    data.ExplorationPhase = static_cast<std::uint32_t>(mExplorationPhase);

    data.PlayTime = mPlayTime;

    //============================================================
    // World Persistent Objects
    //============================================================
    std::vector<const IPersistentWorldObject*> persistentObjects;

    mWorld.AppendPersistentObjects(persistentObjects);

    //============================================================
    // Current Objective
    //============================================================
    data.CurrentObjectiveId = mObjectiveManager.GetCurrentObjectiveId();

    //============================================================
    // Inventory
    //============================================================
    data.Inventory = mInventory.GetEntries();

    //============================================================
    // Map Discovery
    // LocationSystem側はunordered_setなので
    // Save側ではvectorへ変換する
    //============================================================
    {
        const auto& discoveredRooms = mLocationSystem.GetDiscoveredRooms();
        data.DiscoveredRooms.assign(discoveredRooms.begin(), discoveredRooms.end());

        //========================================================
        // Save Binaryを毎回安定させるためSort
        // unordered_setは順番が保証されないため
        //========================================================
        std::sort(data.DiscoveredRooms.begin(), data.DiscoveredRooms.end());
    }

    //============================================================
    // Runtime Object
    //      ↓
    // Save Record
    //============================================================
    for (const IPersistentWorldObject* object : persistentObjects)
    {
        if (object == nullptr)
        {
            continue;
        }


        const WorldObjectSaveState state = object->CaptureSaveState();


        //========================================================
        // IDなしのObjectはSaveしない
        //========================================================
        if (state.Id.empty())
        {
#if defined(_DEBUG)

            OutputDebugStringW(
                L"[AtmosphericEscape] "
                L"Persistent Object has empty ID.\n"
            );

#endif

            continue;
        }

        data.WorldObjects.push_back(state);
    }


#if defined(_DEBUG)

    wchar_t buffer[256];

    swprintf_s(
        buffer,
        L"[AtmosphericEscape] "
        L"Captured %zu persistent objects.\n",
        data.WorldObjects.size()
    );

    OutputDebugStringW(buffer);

#endif

    return data;
}
