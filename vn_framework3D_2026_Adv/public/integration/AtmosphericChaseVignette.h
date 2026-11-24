#pragma once

class vnScene;
class vnSprite;

// AtmosphericHeartbeatの拍で画面の縁に赤いビネットを描く
class AtmosphericChaseVignette
{
public:
    void Initialize(vnScene* scene);
    void Terminate(vnScene* scene);

    // 呼び出し元から追跡の判定と拍の有無を受け取る
    void Update(float dt, bool chasing, bool emittedBeat);
    void Reset();

    // そのフレームの最終的なシーン状態から描画の可否を決める
    void Apply(bool gameplayVisible);

private:
    vnSprite* mSprite = nullptr;
    float mPresence = 0.0f;        // 追跡開始・終了時のフェード
    float mTimeSinceBeat = 1.0f;   // 直近の心音を再生してからの経過秒
    float mPulse = 0.0f;           // 心音の強弱
    float mOpacity = 0.0f;         // テクスチャ全体に掛けるアルファ
};
