#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "atmosphericEscapeSceneInputHandler.h"


//============================================================
// Pause Trigger
//============================================================
bool AtmosphericEscapeSceneInputHandler::IsPauseTriggered(int padId) const
{
    //========================================================
    // Keyboard
    //========================================================
    if (vnKeyboard::trg(DIK_TAB))
    {
        //====================================================
        // ※使用Deviceも更新しておく
        //====================================================
        InputDeviceTracker::Get().NotifyKeyboard();
        return true;
    }


    //========================================================
    // GamePad
    //========================================================
    if (Pad::isValid(padId) && Pad::trg(PadButton::START, padId))
    {
        InputDeviceTracker::Get().NotifyGamepad();
        return true;
    }


    return false;
}

bool AtmosphericEscapeSceneInputHandler::IsDebugLoadTriggered(int padId) const
{
    (void)padId;

#if defined(_DEBUG)
    //============================================================
    // Debug Load
    // F6など未使用キーを使用
    //============================================================
    if (vnKeyboard::trg(DIK_F6))
    {
        return true;
    }

#endif

    return false;
}

bool AtmosphericEscapeSceneInputHandler::IsMapTriggered(int padId) const
{
    //========================================================
    // Keyboard
    //========================================================
    if (vnKeyboard::trg(DIK_M))
    {
        //====================================================
        // ※使用Deviceも更新しておく
        //====================================================
        InputDeviceTracker::Get().NotifyKeyboard();
        return true;
    }


    //========================================================
    // GamePad
    //========================================================
    if (Pad::isValid(padId) && Pad::trg(PadButton::L3, padId))
    {
        InputDeviceTracker::Get().NotifyGamepad();
        return true;
    }


    return false;
}

bool AtmosphericEscapeSceneInputHandler::IsInventoryTriggered(int padId) const
{
    //========================================================
    // Keyboard
    //========================================================
    if (vnKeyboard::trg(DIK_I))
    {
        //====================================================
        // 使用Deviceも更新しておく
        //====================================================
        InputDeviceTracker::Get().NotifyKeyboard();
        return true;
    }


    //========================================================
    // GamePad
    //========================================================
    if (Pad::isValid(padId) && Pad::trg(PadButton::BACK, padId))
    {
        InputDeviceTracker::Get().NotifyGamepad();
        return true;
    }


    return false;
}
