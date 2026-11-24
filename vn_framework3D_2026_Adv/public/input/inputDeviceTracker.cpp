#include "../../framework.h"
#include "../../framework/vn_environment.h"


#include "inputDeviceTracker.h"

InputDeviceTracker& InputDeviceTracker::Get()
{
	static InputDeviceTracker inst;
	return inst;
}

void InputDeviceTracker::Update(float dt)
{
    if (mHoldTimer > 0.0f) mHoldTimer -= dt;

    // 抜き差しの変化だけ見たい場合
    const bool padConnected = Pad::isValid(0);

    // パッドが抜かれたら、表示をキーマウへ戻す
    if (!padConnected && mWasPadConnected)
    {
        if (mDevice == ActiveDevice::Gamepad)
            mDevice = ActiveDevice::KeyboardMouse;
    }

    // パッドが今ささった場合：
    // まだキーマウ入力が一度も無いならパッド優先へ
    if (padConnected && !mWasPadConnected)
    {
        if (!mEverKeyboardMouseInput)
        {
            mDevice = ActiveDevice::Gamepad;
            mHoldTimer = mHoldSeconds;
        }
    }

    mWasPadConnected = padConnected;
}

void InputDeviceTracker::NotifyKeyboard()
{
	// Hold中でもKBが来たら切替
    mEverKeyboardMouseInput = true;

    mDevice = ActiveDevice::KeyboardMouse;
    mHoldTimer = mHoldSeconds;
}

void InputDeviceTracker::NotifyGamepad()
{
	mDevice = ActiveDevice::Gamepad;
	mHoldTimer = mHoldSeconds;
}

void InputDeviceTracker::SyncInitialDeviceFromConnection()
{
    if (Pad::isValid(0))
        mDevice = ActiveDevice::Gamepad;
    else
        mDevice = ActiveDevice::KeyboardMouse;

    mHoldTimer = mHoldSeconds;
}
