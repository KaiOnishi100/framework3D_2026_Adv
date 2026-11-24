#pragma once

class SlowSystem
{
public:
    static SlowSystem& Get() { static SlowSystem s; return s; }

    // ジャスト回避成立：スロー開始（解除は命中時に EndJustAvoidByHit で）
    void StartJustAvoidSlow(float scale);
    // ボスに命中したら呼ぶ
    void EndJustAvoidByHit();

    void End();

    bool  IsActive() const { return mActive; }
    bool  IsJustAvoidSlow() const { return mActive && mJustAvoid; }
    bool  IsAttackOnly() const { return mActive && mAttackOnly; }
    bool  IsWaitingHit() const { return mActive && mWaitingHit; }
    float GetScale() const { return mActive ? mScale : 1.0f; }
    float GetScaleForPlayer() const { return IsJustAvoidSlow() ? 1.0f : 1.0f; }  // 常に等倍
    float GetScaleForBoss()   const { return IsJustAvoidSlow() ? mScale : 1.0f; }

private:
    bool  mActive = false;
    bool  mJustAvoid = false;
    bool  mAttackOnly = false;
    bool  mWaitingHit = false;
    float mScale = 1.0f;
};