#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "inputUIConfig.h"

void UIInputConfig::SetupTitle(UIInputHandler& h, int padId)
{
    h.BindDefaultStick(0.20f, false);

    h.BindKey(DIK_A, UIInputType::Trigger, std::make_unique<UIMoveH>(-1));
    h.BindKey(DIK_D, UIInputType::Trigger, std::make_unique<UIMoveH>(+1));
    h.BindKey(DIK_RETURN, UIInputType::Trigger, std::make_unique<UISubmit>());
    h.BindKey(DIK_ESCAPE, UIInputType::Trigger, std::make_unique<UICancel>());

    h.BindPad(PadButton::DPAD_LEFT, UIInputType::Trigger, std::make_unique<UIMoveH>(-1), padId);
    h.BindPad(PadButton::DPAD_RIGHT, UIInputType::Trigger, std::make_unique<UIMoveH>(+1), padId);

    h.BindPad(PadButton::A, UIInputType::Trigger, std::make_unique<UISubmit>(), padId);
    h.BindPad(PadButton::B, UIInputType::Trigger, std::make_unique<UICancel>(), padId);
}

void UIInputConfig::SetupResult(UIInputHandler& h, int padId)
{
    // Titleと同じ
    SetupTitle(h, padId);
}

void UIInputConfig::SetupPause(UIInputHandler& h, int padId)
{
    // Titleと同じ
    SetupTitle(h, padId);
}

void UIInputConfig::SetupMap(UIInputHandler& h, int padId)
{
    //========================================================
    // Analog Stick
    //========================================================
    h.BindDefaultStick(0.20f, false);

    //========================================================
    // Keyboard
    //
    // 左右 = Floor
    // 上下 = Area
    //========================================================
    h.BindKey(DIK_A, UIInputType::Trigger, std::make_unique<UIMoveH>(-1));
    h.BindKey(DIK_D, UIInputType::Trigger, std::make_unique<UIMoveH>(+1));
    h.BindKey(DIK_W, UIInputType::Trigger, std::make_unique<UIMoveV>(-1));
    h.BindKey(DIK_S, UIInputType::Trigger, std::make_unique<UIMoveV>(+1));

    //========================================================
    // Cancel
    //========================================================
    h.BindKey(DIK_M, UIInputType::Trigger, std::make_unique<UICancel>());

    //========================================================
    // Gamepad
    //========================================================
    h.BindPad(PadButton::DPAD_LEFT, UIInputType::Trigger, std::make_unique<UIMoveH>(-1), padId);
    h.BindPad(PadButton::DPAD_RIGHT, UIInputType::Trigger, std::make_unique<UIMoveH>(+1), padId);
    h.BindPad(PadButton::DPAD_UP, UIInputType::Trigger, std::make_unique<UIMoveV>(-1), padId);
    h.BindPad(PadButton::DPAD_DOWN, UIInputType::Trigger, std::make_unique<UIMoveV>(+1), padId);
    h.BindPad(PadButton::B, UIInputType::Trigger, std::make_unique<UICancel>(), padId);
}

void UIInputConfig::SetupInventory(UIInputHandler& h, int padId)
{
    //========================================================
    // Stick
    //========================================================
    h.BindDefaultStick(0.20f, false);

    //========================================================
    // Keyboard
    //========================================================
    h.BindKey(DIK_W, UIInputType::Trigger, std::make_unique<UIMoveV>(-1));
    h.BindKey(DIK_S, UIInputType::Trigger, std::make_unique<UIMoveV>(+1));
    h.BindKey(DIK_I, UIInputType::Trigger, std::make_unique<UICancel>());

    //========================================================
    // Gamepad
    //========================================================
    h.BindPad(PadButton::DPAD_UP, UIInputType::Trigger, std::make_unique<UIMoveV>(-1), padId);
    h.BindPad(PadButton::DPAD_DOWN, UIInputType::Trigger, std::make_unique<UIMoveV>(+1), padId);
    h.BindPad(PadButton::B, UIInputType::Trigger, std::make_unique<UICancel>(), padId);
}

void UIInputConfig::SetupExplorationMenu(UIInputHandler& h, int padId)
{
    h.BindDefaultStick(0.20f, false);
    //========================================================
    // Keyboard
    //========================================================
    h.BindKey(DIK_W, UIInputType::Trigger, std::make_unique<UIMoveV>(-1));
    h.BindKey(DIK_S, UIInputType::Trigger, std::make_unique<UIMoveV>(+1));
    h.BindKey(DIK_A, UIInputType::Trigger, std::make_unique<UIMoveH>(-1));
    h.BindKey(DIK_D, UIInputType::Trigger, std::make_unique<UIMoveH>(+1));
    h.BindKey(DIK_RETURN, UIInputType::Trigger, std::make_unique<UISubmit>());
    h.BindKey(DIK_ESCAPE, UIInputType::Trigger, std::make_unique<UICancel>());

    //========================================================
    // Gamepad
    //========================================================
    h.BindPad(PadButton::DPAD_UP, UIInputType::Trigger, std::make_unique<UIMoveV>(-1), padId);
    h.BindPad(PadButton::DPAD_DOWN, UIInputType::Trigger, std::make_unique<UIMoveV>(+1), padId);
    h.BindPad(PadButton::DPAD_LEFT, UIInputType::Trigger, std::make_unique<UIMoveH>(-1), padId);
    h.BindPad(PadButton::DPAD_RIGHT, UIInputType::Trigger, std::make_unique<UIMoveH>(+1), padId);
    h.BindPad(PadButton::A, UIInputType::Trigger, std::make_unique<UISubmit>(), padId);
    h.BindPad(PadButton::B, UIInputType::Trigger, std::make_unique<UICancel>(), padId);
}
