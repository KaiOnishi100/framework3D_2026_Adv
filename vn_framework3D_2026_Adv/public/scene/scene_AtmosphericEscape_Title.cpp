#include "../../framework.h"
#include "../../framework/vn_environment.h"

bool SceneAtmosphericEscapeTitle::initialize()
{
    //============================================================
    // Time Initialize
    //============================================================
    mPrevTime = std::chrono::steady_clock::now();

    //============================================================
    // Sound
    //============================================================
    SoundBank* sound = SoundBank::getInstance();

    sound->InitAtmosphericEscape();

    // タイトル専用の不気味な環境BGMをループ再生する
    sound->StartLoop(SoundID::AE_BgmTitle, 0.40f);

    //============================================================
    // UI Initialize
    //============================================================
    mTitleUI.Initialize(this);
    UIInputConfig::SetupExplorationMenu(mTitleInput, 0);
    mTitleInput.BindKey(DIK_UP, UIInputType::Trigger, std::make_unique<UIMoveV>(-1));
    mTitleInput.BindKey(DIK_DOWN, UIInputType::Trigger, std::make_unique<UIMoveV>(+1));
    mTitleInput.Reset();

    mOptionsUI.Initialize(this);
    UIInputConfig::SetupExplorationMenu(mOptionsInput, 0);
    mOptionsInput.Reset();

    //============================================================
    // Continue Availability
    // セーブデータが正しく読み込めた場合のみ
    // CONTINUEボタンを使用可能にする
    //============================================================
    AtmosphericEscapeSaveData saveData{};

    const bool hasSave =
        AtmosphericEscapeSaveManager::Load(saveData);

    mTitleUI.SetContinueEnabled(hasSave);

    //============================================================
    // Fade In
    //============================================================
    FadeManager::Get().Initialize(this);
    FadeManager::Get().StartFadeIn(0.65f);

    PostProcessManager::AllDisable();

    return true;
}

void SceneAtmosphericEscapeTitle::terminate()
{
    // シーン所有のスプライトを破棄する
    FadeManager::Get().Terminate(this);
    mTitleUI.Terminate(this);
    mOptionsUI.Terminate(this);

    // タイトルBGMを止めタイトルでロードした音を解放する
    SoundBank* sound = SoundBank::getInstance();

    sound->StopLoop(SoundID::AE_BgmTitle);
    sound->ShutdownAtmosphericEscape();
}

void SceneAtmosphericEscapeTitle::execute()
{
    //============================================================
     // Delta Time
     //============================================================
    const auto now = std::chrono::steady_clock::now();

    float dt = std::chrono::duration<float>(now - mPrevTime).count();

    mPrevTime = now;
    dt = (std::clamp)(dt, 0.0f, 0.10f);

    //============================================================
    // UI Input
    // マウスで動作確認する
    // ゲームパッド入力は既存のUIInputHandlerへ接続可能
    //============================================================
    if (mOptionsUI.IsVisible())
    {
        mOptionsInput.HandleInput(&mOptionsUI, dt);
        mOptionsUI.Update(0);

        mOptionsUI.ConsumeChanges();

        if (mOptionsUI.WantsClose())
        {
            mOptionsUI.Hide();
            mOptionsInput.Reset();
            mTitleInput.Reset();
            mTitleUI.SetMenuVisible(true);
        }
        else
        {
            mOptionsUI.Draw();
        }
    }
    else if (!FadeManager::Get().IsFading())
    {

        mTitleInput.HandleInput(&mTitleUI, dt);
        mTitleUI.Update(dt);

        //========================================================
        // Action
        //========================================================
        const TitleUI::Action action = mTitleUI.ConsumeAction();

        switch (action)
        {
        case TitleUI::Action::Start:
        {
            // フェード完了後 新規ゲームで開始する
            FadeManager::Get().StartFadeOut(
                0.65f,
                [this]()
                {
                    AtmosphericEscapeStartRequest::RequestNewGame();
                    switchScene(eSceneTable::AtmosphericEscape, true);
                }
            );
            break;
        }

        case TitleUI::Action::Continue:
        {
            // セーブロード開始要求を設定してから
            // AtmosphericEscapeシーンへ遷移する
            FadeManager::Get().StartFadeOut(0.65f, [this]()
                {
                    AtmosphericEscapeStartRequest::RequestLoadGame();
                    switchScene(eSceneTable::AtmosphericEscape, true);
                }
            );

            break;
        }

        case TitleUI::Action::Exit:
        {
            FadeManager::Get().StartFadeOut(0.45f, []()
                {
                    PostQuitMessage(0);
                }
            );

            break;
        }

        case TitleUI::Action::Options:
        {
            mTitleUI.SetMenuVisible(false);
            mTitleInput.Reset();
            mOptionsInput.Reset();

            mOptionsUI.Show();
            mOptionsUI.Draw(); // 開いたフレームから背景と項目名を一緒に表示する
            break;
        }

        default:
            break;
        }
    }

    //============================================================
    // Scene Update
    //============================================================
    vnScene::execute();

    // フェード完了時のシーン切り替えが発生し得るため
    // FadeManagerの更新は最後に行う
    FadeManager::Get().Update(dt);
}

void SceneAtmosphericEscapeTitle::render()
{
    vnScene::render();
}
