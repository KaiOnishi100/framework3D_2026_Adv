#pragma once

enum class PadButton
{
    A,  // 決定（PSなら×）
    B,  // キャンセル（PSなら○）
    X,  // PSなら□
    Y,  // PSなら△

    LB,
    RB,
    BACK,
    START,
    L3,
    R3,

    DPAD_UP,
    DPAD_DOWN,
    DPAD_LEFT,
    DPAD_RIGHT,
};

//--------------------------------------------
// DirectInputの生値 → -1～1 に変換
//--------------------------------------------
inline float DI_NormAxis(long v);

class Pad
{
public:
    // 接続確認（DirectInputで列挙できたか）
    static bool isValid(int id = 0);
    //========================================
    // ボタン：押下状態
    //========================================
    static bool on(PadButton b, int id = 0);
    //========================================
    // ボタン：押した瞬間
    //========================================
    static bool trg(PadButton b, int id = 0);
    //========================================
    // ボタン：離した瞬間
    //========================================
    static bool rel(PadButton b, int id = 0);
    //========================================
   // スティック：左（-1～1）
   //========================================
    static float leftX(int id = 0, float dead = 0.20f);
    static float leftY(int id = 0, float dead = 0.20f, bool invert = false);
    //========================================
   // スティック：右（-1～1）
   // 重要：DirectInputは「右スティック」が lZ / lRz / rglSlider に来たりして機種差がある
   //========================================
    static float rightX(int id = 0, float dead = 0.20f);
    static float rightY(int id = 0, float dead = 0.20f, bool invert = false);
private:
    static int MapButtonIndex(PadButton b, int /*id*/);
    static long ReadRightXRaw(const DIJOYSTATE& js, int /*id*/);
    static long ReadRightYRaw(const DIJOYSTATE& js, int /*id*/);
};