#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "UIStickRepeater.h"

void UIStickRepeater::Reset()
{
    mHoldDirX = 0; mHoldDirY = 0;
    mTimerX = 0.0f; mTimerY = 0.0f;
    mFirstX = true; mFirstY = true;
}

int UIStickRepeater::UpdateHorizontal(float dt, float ax)
{
    return UpdateAxis(dt, ax, mHoldDirX, mTimerX, mFirstX);
}

int UIStickRepeater::UpdateVertical(float dt, float ay)
{
    return UpdateAxis(dt, ay, mHoldDirY, mTimerY, mFirstY);
}

int UIStickRepeater::Sign(float v)
{
    return (v > 0) - (v < 0);
}

int UIStickRepeater::UpdateAxis(float dt, float a, int& holdDir, float& timer, bool& first)
{
    const float absA = fabsf(a);

    // ニュートラルに戻ったら解除
    if (absA < Release)
    {
        holdDir = 0;
        timer = 0.0f;
        first = true;
        return 0;
    }

    // しきい値未満なら何もしない（解除もされない）
    if (absA < Dead)
        return 0;

    const int dir = Sign(a);

    // 方向が変わったら初回扱いで即1回だけ出す
    if (dir != holdDir)
    {
        holdDir = dir;
        timer = 0.0f;
        first = false;   // 方向変化の次からはリピート扱い
        return dir;
    }

    // 押しっぱリピート
    timer += dt;
    const float interval = first ? FirstDelay : Repeat;
    if (timer >= interval)
    {
        timer -= interval;
        first = false;
        return holdDir;
    }
    return 0;

}
