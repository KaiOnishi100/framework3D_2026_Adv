#pragma once

class vnScene;
class vnSprite;

//============================================================
// UIExplorationControls
//
// Atmospheric Escape専用の操作ガイド
//
// ・画面右下に表示
// ・キーボードとゲームパッドの表示を自動で切替
// ・移動などの基本操作は表示しない
// ・ゲームプレイ以外の画面では非表示
//============================================================
class UIExplorationControls
{
public:
    void Initialize(vnScene* scene);
    void Terminate(vnScene* scene);

    void SetVisible(bool visible);

    // 毎フレームのUI文字描画
    void Draw() const;

private:
    vnSprite* mBackground = nullptr;

    float mLeft = 0.0f;
    float mTop = 0.0f;

    bool mVisible = false;
};