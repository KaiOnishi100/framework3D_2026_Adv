#include "../framework.h"
#include "vn_environment.h"

bool Pad::isValid(int id)
{
	return DirectInput::useJoystick(id);
}

bool Pad::on(PadButton b, int id)
{
    int idx = MapButtonIndex(b, id);
    if (idx < 0) return false;

    const DIJOYSTATE* js = DirectInput::getCurrentJoystick(id);
    if (!js) return false;

    return (js->rgbButtons[idx] & 0x80) != 0;
}

bool Pad::trg(PadButton b, int id)
{
    int idx = MapButtonIndex(b, id);
    if (idx < 0) return false;

    const DIJOYSTATE* now = DirectInput::getCurrentJoystick(id);
    const DIJOYSTATE* old = DirectInput::getOldJoystick(id);
    if (!now || !old) return false;

    bool n = (now->rgbButtons[idx] & 0x80) != 0;
    bool o = (old->rgbButtons[idx] & 0x80) != 0;
    return (n && !o);
}

bool Pad::rel(PadButton b, int id)
{
    int idx = MapButtonIndex(b, id);
    if (idx < 0) return false;

    const DIJOYSTATE* now = DirectInput::getCurrentJoystick(id);
    const DIJOYSTATE* old = DirectInput::getOldJoystick(id);
    if (!now || !old) return false;

    bool n = (now->rgbButtons[idx] & 0x80) != 0;
    bool o = (old->rgbButtons[idx] & 0x80) != 0;
    return (!n && o);
}

float Pad::leftX(int id, float dead)
{
    const DIJOYSTATE* js = DirectInput::getCurrentJoystick(id);
    if (!js) return 0.0f;

    float x = DI_NormAxis(js->lX);
    if (std::fabs(x) < dead) x = 0.0f;
    return x;
}

float Pad::leftY(int id, float dead, bool invert)
{
    const DIJOYSTATE* js = DirectInput::getCurrentJoystick(id);
    if (!js) return 0.0f;

    float y = DI_NormAxis(js->lY);
    if (invert) y = -y;
    if (std::fabs(y) < dead) y = 0.0f;
    return y;
}

float Pad::rightX(int id, float dead)
{
    const DIJOYSTATE* js = DirectInput::getCurrentJoystick(id);
    if (!js) return 0.0f;

    long raw = ReadRightXRaw(*js, id);
    float x = DI_NormAxis(raw);
    if (std::fabs(x) < dead) x = 0.0f;
    return x;
}

float Pad::rightY(int id, float dead, bool invert)
{
    const DIJOYSTATE* js = DirectInput::getCurrentJoystick(id);
    if (!js) return 0.0f;

    long raw = ReadRightYRaw(*js, id);
    float y = DI_NormAxis(raw);
    if (invert) y = -y;
    if (std::fabs(y) < dead) y = 0.0f;
    return y;
}

int Pad::MapButtonIndex(PadButton b, int)
{
    switch (b)
    {
    case PadButton::X: return 0; // □
    case PadButton::A: return 1; // 決定（×）
    case PadButton::B: return 2; // キャンセル（○）a
    case PadButton::Y: return 3; // △

        // Shoulder 等は機種差が大きい
    case PadButton::LB: return 4;
    case PadButton::RB: return 5;

    case PadButton::BACK:  return 8;  // SHARE/SELECT など
    case PadButton::START: return 9;  // OPTIONS/START など

    case PadButton::L3: return 10;
    case PadButton::R3: return 11;

        // DPADはボタンではなく POV で来ることが多いので -1 にして別処理が安全
    case PadButton::DPAD_UP:
    case PadButton::DPAD_DOWN:
    case PadButton::DPAD_LEFT:
    case PadButton::DPAD_RIGHT:
        return -1;
    default:
        return -1;
    }
}

long Pad::ReadRightXRaw(const DIJOYSTATE& js, int)
{
    return js.lZ;
}

long Pad::ReadRightYRaw(const DIJOYSTATE& js, int)
{
    return js.lRz;
}

inline float DI_NormAxis(long v)
{
    const float inv = DirectInput::getJoystickAxisRangeI(); // 1/1000
    float f = (float)v * inv;

    // 念のためクランプ
    if (f > 1.0f) f = 1.0f;
    if (f < -1.0f) f = -1.0f;
    return f;
}
