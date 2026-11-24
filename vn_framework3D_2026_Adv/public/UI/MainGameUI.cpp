#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"MainGameUI.h"

void MainGameUI::Initialize(vnScene* scene)
{
    // スプライト生成＆scene登録
    mHealth.Initialize(scene);
    mResult.Initialize(scene);
    mHints.Initialize(scene);
    mPause.Initialize(scene);

    mPause.Hide();
    // 最初はPlayingなので結果は隠す
    mResult.Hide();
    mHealth.SetVisible(true);
    mHints.SetVisible(true);

    const float sw = (float)vnMainFrame::screenWidth;
    const float sh = (float)vnMainFrame::screenHeight;

    // 暗幕
    mDimmer = new vnSprite(sw * 0.5f, sh * 0.5f, sw, sh, L"data/image/UI/white1x1.png");
    mDimmer->setRenderPriority(510); // ヒント(520)より下、ゲームより上に来る値にする
    scene->registerObject(mDimmer);

    // 初期は非表示
    SetDim(false);
}

void MainGameUI::Terminate(vnScene* scene)
{
    // deleteはscene側に任せる
    mResult.Terminate(scene);
    mHealth.Terminate(scene);
    mHints.Terminate(scene);
    mPause.Terminate(scene);

    if (mDimmer) scene->deleteObject(mDimmer);
    mDimmer = nullptr;
}

void MainGameUI::UpdatePlaying(const Player* player, const Boss* boss, float dt)
{
    // Playing中はHPバーを動かす
    mHealth.SetVisible(true);
    mHealth.Update(player, boss);
    mHints.Update(dt);
}

void MainGameUI::EnterResult(ResultType type)
{
    // 結果に入ったらHPを消す
    mHealth.SetVisible(false);
    mHints.SetVisible(false);

    // 背景とボタンを切り替え
    if (type == ResultType::GameOver)
        mResult.ShowGameOver();
    else
        mResult.ShowGameClear();
}

void MainGameUI::UpdateResult(float dt)
{
    // Result中はホバー更新だけ
    mResult.Update(dt);
}

UIResultOverlay::Action MainGameUI::HandleResultInput()
{
    return mResult.ConsumeAction();
}

void MainGameUI::HideAll()
{
    mPause.Hide();
    mHealth.SetVisible(false);
    mResult.Hide();
    mHints.SetVisible(false);
}

void MainGameUI::ClearTutorialVisuals()
{
    SetDodgeAttention(false);
    ClearTutorialHintAttention();
}

void MainGameUI::SetDim(bool on, float alpha)
{
    if (!mDimmer) return;

    mDimmer->setRenderEnable(on);
    mDimmer->setExecuteEnable(on);

    // 黒 + α
    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mDimmer->vtx[i].r = 0.0f;
        mDimmer->vtx[i].g = 0.0f;
        mDimmer->vtx[i].b = 0.0f;
        mDimmer->vtx[i].a = on ? alpha : 0.0f;
    }
}
