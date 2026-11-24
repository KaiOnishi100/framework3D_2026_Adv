#pragma once

// UI表示に使う「今の入力デバイス」
enum class ActiveDevice
{
    KeyboardMouse,
    Gamepad
};

class InputDeviceTracker
{
public:
    static InputDeviceTracker& Get();
    // dtは「ちょっとの間は切り替えない」用（チラつき防止）
    void Update(float dt);
    ActiveDevice GetDevice() const { return mDevice; }

    // キーボード入力が発生した
    void NotifyKeyboard();
    // パッド入力が発生した
    void NotifyGamepad();
    void SetHoldSeconds(float sec) { mHoldSeconds = sec; }
    void SyncInitialDeviceFromConnection();
    void NotifyMouseMove() { NotifyKeyboard(); }
private:
    ActiveDevice mDevice = ActiveDevice::KeyboardMouse;
    float mHoldSeconds = 0.25f;
    float mHoldTimer = 0.0f;

    bool  mWasPadConnected = false;
    bool  mEverKeyboardMouseInput = false;
};