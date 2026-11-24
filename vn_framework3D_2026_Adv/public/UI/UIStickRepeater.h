#pragma once

//----------------------------------------------
// UI用：スティック入力を
// 「1回だけ移動」＋「押しっぱなしリピート」に変換する
//----------------------------------------------
class UIStickRepeater
{
public:
    void Reset();
    int UpdateHorizontal(float dt, float ax);
    int UpdateVertical(float dt, float ay);

    // 調整値
    float Dead = 0.25f;        // 入力開始しきい値
    float Release = 0.15f;     // ニュートラル復帰しきい値（Deadより小さく）
    float FirstDelay = 0.25f;  // 押しっぱ初回ディレイ
    float Repeat = 0.50f;      // リピート間隔

private:
    static int Sign(float v);

    int UpdateAxis(float dt, float a, int& holdDir, float& timer, bool& first);
private:
    int   mHoldDirX = 0, mHoldDirY = 0;
    float mTimerX = 0.0f, mTimerY = 0.0f;
    bool  mFirstX = true, mFirstY = true;
};