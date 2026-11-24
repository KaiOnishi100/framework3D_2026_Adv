#pragma once

class vnScene;

//------------------------------------------------------------
// 画面上に Player/Boss のHPバーを表示するクラス
// ・vnSprite の生成と配置
// ・HP比率に応じて Fill の幅を縮める
//
// 注意：vnSprite の delete は vnScene::deleteObject に任せる
//------------------------------------------------------------
class UIHealthBars
{
public:
    UIHealthBars() = default;
    ~UIHealthBars() = default;

    // vnSprite を作成し、sceneに登録する
    void Initialize(vnScene* scene);

    // sceneから削除（deleteもここで行われる）
    void Terminate(vnScene* scene);

    // HPを反映（縮める方式）
    void Update(const Player* player, const Boss* boss);

    // まとめて表示/非表示
    void SetVisible(bool visible);

private:
    // 内部：スプライト表示切替
    void SetRender(vnSprite* s, bool on);

private:
    // Player
    vnSprite* mHPBack_Player = nullptr;
    vnSprite* mHPFill_Player = nullptr;
    vnSprite* mHPFrame_Player = nullptr;

    // Boss
    vnSprite* mHPBack_Boss = nullptr;
    vnSprite* mHPFill_Boss = nullptr;
    vnSprite* mHPFrame_Boss = nullptr;

    bool mVisible = true;

    // バー定数
    const float PLAYER_BAR_W = 320.0f;
    const float PLAYER_BAR_H = 20.0f;
    const float BOSS_BAR_W = 520.0f;
    const float BOSS_BAR_H = 18.0f;

    const float UI_MARGIN_L = 24.0f;
    const float UI_MARGIN_T = 18.0f;
    const float UI_MARGIN_BOTTOM = 28.0f;
};