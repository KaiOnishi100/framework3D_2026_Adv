#pragma once

class vnScene;

//------------------------------------------------------------
// 共通リップル（リング拡大＋フェード）
// ・Trigger() で開始
// ・Update(dt) で寿命処理
//------------------------------------------------------------
class UIRipple
{
public:
    void Initialize(vnScene* scene, const WCHAR* ringPath, int renderPriority = 2250);
    void Terminate(vnScene* scene);

    // 画面自体が表示中か（親UIの表示/非表示）
    void SetVisibleAll(bool v);

    // 中心座標
    void SetCenter(float x, float y);

    // 基準サイズ（例：50なら 50*StartScale ～ 50*EndScale に広がる）
    void SetBaseSize(float s) { mBaseSize = s; }

    // パラメータ調整
    void SetParams(float dur, float maxA, float startScale, float endScale);

    void Trigger();      // リップル開始
    void Update(float dt);

private:
    void SetRender(vnSprite* s, bool on);
    void SetColorWhite(vnSprite* s, float a);
    void SetAlpha(vnSprite* s, float a);

private:
    vnSprite* mRing = nullptr;

    bool  mAllVisible = false;
    bool  mActive = false;
    float mTimer = 0.0f;

    float mCenterX = 0.0f;
    float mCenterY = 0.0f;

    float mEmitX = 0.0f;
    float mEmitY = 0.0f;

    float mBaseSize = 50.0f;

    float mDur = 0.28f;
    float mMaxA = 0.55f;
    float mStartScale = 1.0f;
    float mEndScale = 1.8f;
};