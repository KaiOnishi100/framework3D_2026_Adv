#pragma once

class vnScene;
//------------------------------------------------------------
// UI用：選択中ボタンの近くに「×(決定)」を表示し、
// Submitされた瞬間にリップル（リング拡大＋フェード）を出す部品
//------------------------------------------------------------
class UIPadSubmitHint
{
public:
    void Initialize(vnScene* scene, const WCHAR* crossPath, const WCHAR* ringPath);
    void Terminate(vnScene* scene);

    void SetVisibleAll(bool v);     // 親UIが表示中か
    void SetShowIcon(bool show);    // ×アイコンだけ出す/出さない

    void SetTargetPos(float x, float y);
    void SetIconTargetRect(const UIRect& r, float offsetX, float offsetY);
    void SetRippleTargetRect(const UIRect& r, float offsetX = 0, float offsetY = 0);
    void SetRippleToCenterOfRect(const UIRect& r) { SetRippleTargetRect(r, 0.0f, 0.0f); }

    void TriggerRipple();
    void Update(float dt);

private:
    void SetRender(vnSprite* s, bool on);

private:
    vnSprite* mIcon = nullptr; // ×

    bool  mAllVisible = false;
    bool  mShowIcon = false;

    float mSize = 50.0f;

    float mIconX = 0.0f;
    float mIconY = 0.0f;

    float mRippleX = 0.0f;
    float mRippleY = 0.0f;

    UIRipple mRipple; // 共通部品
};