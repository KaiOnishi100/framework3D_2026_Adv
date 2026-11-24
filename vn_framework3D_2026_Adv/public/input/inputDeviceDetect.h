#pragma once

struct InputDeviceDetectState
{
    int  prevMouseX = 0;
    int  prevMouseY = 0;
    bool mouseInit = false;
};

// 1フレームに1回呼ぶ
inline void UpdateActiveInputDevice(float dt, InputDeviceDetectState& st, int padId = 0)
{
    auto& tracker = InputDeviceTracker::Get();
    tracker.Update(dt);

    // -------------------------
    // マウス移動で KeyboardMouse 扱い
    // -------------------------
    const int mx = vnMouse::getX();
    const int my = vnMouse::getY();

    if (!st.mouseInit)
    {
        st.prevMouseX = mx;
        st.prevMouseY = my;
        st.mouseInit = true;
    }
    else
    {
        const int dx = mx - st.prevMouseX;
        const int dy = my - st.prevMouseY;

        // 1px以上
        if ((dx * dx + dy * dy) >= 1)
        {
            tracker.NotifyKeyboard();
        }

        st.prevMouseX = mx;
        st.prevMouseY = my;
    }

    // -------------------------
    // パッド入力があれば Gamepad 扱い
    // -------------------------
    if (Pad::isValid(padId))
    {
        // スティック（少し倒れたら）
        const float dead = 0.20f;
        const float lx = Pad::leftX(padId, dead);
        const float ly = Pad::leftY(padId, dead, true);
        if (lx != 0.0f || ly != 0.0f)
        {
            tracker.NotifyGamepad();
            return;
        }

        // ボタン
        const PadButton watchButtons[] =
        {
            PadButton::A, PadButton::B, PadButton::X, PadButton::Y,
            PadButton::LB, PadButton::RB,
            PadButton::START, PadButton::BACK,
        };

        for (PadButton b : watchButtons)
        {
            if (Pad::trg(b, padId) || Pad::on(b, padId) || Pad::rel(b, padId))
            {
                tracker.NotifyGamepad();
                return;
            }
        }
    }
}