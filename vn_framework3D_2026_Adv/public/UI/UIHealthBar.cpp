#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "UIHealthBar.h"

void UIHealthBars::SetRender(vnSprite* s, bool on)
{
    if (!s) return;
    s->setRenderEnable(on);
    s->setExecuteEnable(on);
}

void UIHealthBars::Initialize(vnScene* scene)
{
    if (!scene) return;

    const float screenW = (float)vnMainFrame::screenWidth;
    const float screenH = (float)vnMainFrame::screenHeight;

    // 位置計算（左上・下部中央）
    const float playerLeft = UI_MARGIN_L;
    const float playerTop = UI_MARGIN_T;

    const float bossLeft = (screenW - BOSS_BAR_W) * 0.5f;
    const float bossTop = screenH - UI_MARGIN_BOTTOM - BOSS_BAR_H;

    //--------------------------------------------------------
    // Player
    //--------------------------------------------------------
    mHPBack_Player = new vnSprite(
        playerLeft + PLAYER_BAR_W * 0.5f,
        playerTop + PLAYER_BAR_H * 0.5f,
        PLAYER_BAR_W, PLAYER_BAR_H,
        L""
    );

    mHPFill_Player = new vnSprite(
        playerLeft + PLAYER_BAR_W * 0.5f,
        playerTop + PLAYER_BAR_H * 0.5f,
        PLAYER_BAR_W, PLAYER_BAR_H,
        L"data/image/UI/Gauge.png"
    );

    mHPFrame_Player = new vnSprite(
        playerLeft + PLAYER_BAR_W * 0.5f,
        playerTop + PLAYER_BAR_H * 0.5f,
        PLAYER_BAR_W, PLAYER_BAR_H,
        L"data/image/UI/Frame.png"
    );

    mHPBack_Player->setRenderPriority(300);
    mHPFill_Player->setRenderPriority(301);
    mHPFrame_Player->setRenderPriority(302);

    // 背景色（暗い板）
    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mHPBack_Player->vtx[i].r = 0.05f;
        mHPBack_Player->vtx[i].g = 0.05f;
        mHPBack_Player->vtx[i].b = 0.05f;
        mHPBack_Player->vtx[i].a = 0.80f;
    }

    scene->registerObject(mHPBack_Player);
    scene->registerObject(mHPFill_Player);
    scene->registerObject(mHPFrame_Player);

    //--------------------------------------------------------
    // Boss
    //--------------------------------------------------------
    mHPBack_Boss = new vnSprite(
        bossLeft + BOSS_BAR_W * 0.5f,
        bossTop + BOSS_BAR_H * 0.5f,
        BOSS_BAR_W, BOSS_BAR_H,
        L""
    );

    mHPFill_Boss = new vnSprite(
        bossLeft + BOSS_BAR_W * 0.5f,
        bossTop + BOSS_BAR_H * 0.5f,
        BOSS_BAR_W, BOSS_BAR_H,
        L"data/image/UI/GaugeBoss.png"
    );

    mHPFrame_Boss = new vnSprite(
        bossLeft + BOSS_BAR_W * 0.5f,
        bossTop + BOSS_BAR_H * 0.5f,
        BOSS_BAR_W, BOSS_BAR_H,
        L"data/image/UI/Frame.png"
    );

    mHPBack_Boss->setRenderPriority(300);
    mHPFill_Boss->setRenderPriority(301);
    mHPFrame_Boss->setRenderPriority(302);

    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        mHPBack_Boss->vtx[i].r = 0.05f;
        mHPBack_Boss->vtx[i].g = 0.05f;
        mHPBack_Boss->vtx[i].b = 0.05f;
        mHPBack_Boss->vtx[i].a = 0.80f;
    }

    scene->registerObject(mHPBack_Boss);
    scene->registerObject(mHPFill_Boss);
    scene->registerObject(mHPFrame_Boss);

    mVisible = true;
    SetVisible(true);
}

void UIHealthBars::Terminate(vnScene* scene)
{
    if (!scene) return;

    // delete は vnScene::deleteObject がやる
    if (mHPBack_Player)  scene->deleteObject(mHPBack_Player);
    if (mHPFill_Player)  scene->deleteObject(mHPFill_Player);
    if (mHPFrame_Player) scene->deleteObject(mHPFrame_Player);

    if (mHPBack_Boss)  scene->deleteObject(mHPBack_Boss);
    if (mHPFill_Boss)  scene->deleteObject(mHPFill_Boss);
    if (mHPFrame_Boss) scene->deleteObject(mHPFrame_Boss);

    mHPBack_Player = mHPFill_Player = mHPFrame_Player = nullptr;
    mHPBack_Boss = mHPFill_Boss = mHPFrame_Boss = nullptr;
}

void UIHealthBars::SetVisible(bool visible)
{
    mVisible = visible;

    SetRender(mHPBack_Player, visible);
    SetRender(mHPFill_Player, visible);
    SetRender(mHPFrame_Player, visible);

    SetRender(mHPBack_Boss, visible);
    SetRender(mHPFill_Boss, visible);
    SetRender(mHPFrame_Boss, visible);
}

void UIHealthBars::Update(const Player* player, const Boss* boss)
{
    if (!mVisible) return;
    if (!player || !boss) return;

    // Player Fill
    {
        float hp = player->GetHP();
        float mx = player->GetMaxHP();
        float ratio = (mx > 0.0001f) ? (hp / mx) : 0.0f;
        ratio = std::clamp(ratio, 0.0f, 1.0f);

        const float left = UI_MARGIN_L;
        const float w = PLAYER_BAR_W * ratio;

        mHPFill_Player->sizeX = w;
        mHPFill_Player->sizeY = PLAYER_BAR_H;
        mHPFill_Player->posX = left + w * 0.5f;
        mHPFill_Player->posY = UI_MARGIN_T + PLAYER_BAR_H * 0.5f;
    }

    // Boss Fill
    {
        float hp = boss->GetHP();
        float mx = boss->GetMaxHP();
        float ratio = (mx > 0.0001f) ? (hp / mx) : 0.0f;
        ratio = std::clamp(ratio, 0.0f, 1.0f);

        const float screenW = (float)vnMainFrame::screenWidth;
        const float screenH = (float)vnMainFrame::screenHeight;

        const float left = (screenW - BOSS_BAR_W) * 0.5f;
        const float top = screenH - UI_MARGIN_BOTTOM - BOSS_BAR_H;

        const float w = BOSS_BAR_W * ratio;

        mHPFill_Boss->sizeX = w;
        mHPFill_Boss->sizeY = BOSS_BAR_H;
        mHPFill_Boss->posX = left + w * 0.5f;
        mHPFill_Boss->posY = top + BOSS_BAR_H * 0.5f;
    }
}