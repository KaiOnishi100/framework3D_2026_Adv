#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "explorationInputHandler.h"
#include "IExplorationControllable.h"

void ExplorationInputHandler::HandleInput(IExplorationControllable* target, int padId)
{
    if (target == nullptr)
    {
        return;
    }


    //============================================================
    // 操作ロック中
    // Sprintだけは解除しておく
    // ※Sprint=trueが残るのを防ぐ
    //============================================================
    if (target->IsControlLocked())
    {
        target->SetSprint(false);
        return;
    }


    //============================================================
    // Movement Axis
    //============================================================
    float moveX = 0.0f;
    float moveY = 0.0f;

    bool keyboardMove = false;
    bool gamepadMove = false;


    //============================================================
    // Keyboard
    //============================================================

    if (vnKeyboard::on(DIK_W))
    {
        moveY += 1.0f;
        keyboardMove = true;
    }

    if (vnKeyboard::on(DIK_S))
    {
        moveY -= 1.0f;
        keyboardMove = true;
    }

    if (vnKeyboard::on(DIK_A))
    {
        moveX -= 1.0f;
        keyboardMove = true;
    }

    if (vnKeyboard::on(DIK_D))
    {
        moveX += 1.0f;
        keyboardMove = true;
    }


    //============================================================
    // GamePad Left Stick
    //============================================================
    if (Pad::isValid(padId))
    {
        const float padX = Pad::leftX(padId, 0.20f);
        const float padY = Pad::leftY(padId, 0.20f, true);


        if (padX != 0.0f || padY != 0.0f)
        {
            moveX += padX;
            moveY += padY;

            gamepadMove = true;
        }
    }


    //============================================================
    // Playerへ移動入力を渡す
    //============================================================
    if (moveX != 0.0f || moveY != 0.0f)
    {
        target->AddMoveAxis(moveX, moveY);
    }


    //============================================================
    // Active Input Device更新
    //============================================================
    if (keyboardMove)
    {
        InputDeviceTracker::Get().NotifyKeyboard();
    }
    else if (gamepadMove)
    {
        InputDeviceTracker::Get().NotifyGamepad();
    }


    //============================================================
    // Sprint
    // Keyboard : Shift
    // GamePad  : L3
    //============================================================
    bool sprint = false;


    if (vnKeyboard::on(DIK_LSHIFT) || vnKeyboard::on(DIK_RSHIFT))
    {
        sprint = true;
    }


    if (Pad::isValid(padId) && Pad::on(PadButton::RB, padId))
    {
        sprint = true;
    }


    target->SetSprint(sprint);


    //============================================================
    // Interact
    // Keyboard : E
    // GamePad  : A
    //============================================================
    const bool keyboardInteract = vnKeyboard::trg(DIK_E);

    const bool padInteract = Pad::isValid(padId) && Pad::trg(PadButton::A, padId);

    if (keyboardInteract || padInteract)
    {
        if (padInteract)
        {
            InputDeviceTracker::Get().NotifyGamepad();
        }
        else
        {
            InputDeviceTracker::Get().NotifyKeyboard();
        }

        target->Interact();
    }

    //============================================================
    // Crouch Toggle
    // Keyboard : C
    // GamePad  : B
    //============================================================
    const bool keyboardCrouchToggle = vnKeyboard::trg(DIK_C);

    const bool padCrouchToggle = Pad::isValid(padId) && Pad::trg(PadButton::B, padId);

    if (keyboardCrouchToggle || padCrouchToggle)
    {
        if (padCrouchToggle)
        {
            InputDeviceTracker::Get().NotifyGamepad();
        }
        else
        {
            InputDeviceTracker::Get().NotifyKeyboard();
        }

        target->SetCrouching(!target->IsCrouching());
    }


    const bool keyboardtoggleViewModed = vnKeyboard::trg(DIK_V);

    const bool padtoggleViewModed = Pad::isValid(padId) && Pad::trg(PadButton::R3, padId);

    if (keyboardtoggleViewModed || padtoggleViewModed)
    {
        if (padtoggleViewModed)
        {
            InputDeviceTracker::Get().NotifyGamepad();
        }
        else
        {
            InputDeviceTracker::Get().NotifyKeyboard();
        }

        target->ToggleViewMode();
    }


    //============================================================
    // Flashlight
    // Keyboard : F
    // GamePad  : X
    //============================================================
    bool flashlightTriggered = vnKeyboard::trg(DIK_F);


    if (Pad::isValid(padId) && Pad::trg(PadButton::X, padId))
    {
        flashlightTriggered = true;
    }


    if (flashlightTriggered)
    {
        target->ToggleFlashlight();
    }
}
