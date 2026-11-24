#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "inputCommand.h"

void JumpCommand::Execute(IControllable* target)
{
    if (!target) return;
    if (SlowSystem::Get().IsAttackOnly())    return;
    target->Jump();
}

void MoveCommand::Execute(IControllable* target)
{
    if (!target) return;
    if (SlowSystem::Get().IsJustAvoidSlow()) return;
    if (SlowSystem::Get().IsAttackOnly())    return;
    target->AddMoveInput(mDir);
}

void MoveAxisCommand::Execute(IControllable* target)
{
    if (!target)return;
    if (SlowSystem::Get().IsJustAvoidSlow()) return;
    if (SlowSystem::Get().IsAttackOnly())    return;
    // パッドが無ければ何もしない
    if (!Pad::isValid(mPadId)) return;

    float x = Pad::leftX(mPadId, mDead);
    float y = Pad::leftY(mPadId, mDead, mInvertY);

    // 入力がなければ何もしない
    if (x == 0.0f && y == 0.0f) return;

    // IControllable に軸入力を渡す
    target->AddMoveAxis(x, y);
}

void AvoidCommand::Execute(IControllable* target)
{
    if (!target) return;
    if (SlowSystem::Get().IsJustAvoidSlow()) return;
    if (SlowSystem::Get().IsAttackOnly())    return;    
    target->Avoid();
}

void LookOnCommand::Execute(IControllable* target)
{
    if (!target) return;
    target->ToggleLockOn();
}

void StartAttackCommand::Execute(IControllable* target)
{
    if (!target) return;
    target->StartAttack();
}

void AttackReleaseCommand::Execute(IControllable* target)
{
    if (!target) return;
    target->AttackRelease();
}


void InputHandler::BindKey(BYTE key, InputType type, std::unique_ptr<Command> command)
{
    //std::move１つのオブジェクトを他方にレンジを指定して入れることが出来る
    InputBind b;
    b.device = InputDevice::Keyboard;
    b.key = key;
    b.keyType = type;
    b.command = std::move(command);
    mBinds.push_back(std::move(b));
}

void InputHandler::BindMouse(vnMouse::eButton btn, InputType type, std::unique_ptr<Command> command)
{
    InputBind b;
    b.device = InputDevice::Mouse;
    b.mouseButton = btn;
    b.mouseType = type;
    b.command = std::move(command);
    mBinds.push_back(std::move(b));
}

void InputHandler::BindPad(PadButton button, InputType type, std::unique_ptr<Command> command, int padId)
{
    InputBind b;
    b.device = InputDevice::Gamepad;
    b.padButton = button;
    b.padType = type;
    b.padId = padId;
    b.command = std::move(command);
    mBinds.push_back(std::move(b));
}

void InputHandler::BindAlways(std::unique_ptr<Command> command)
{
    InputBind b;
    b.device = InputDevice::Always;
    b.command = std::move(command);
    mBinds.push_back(std::move(b));
}

void InputHandler::HandleInput(IControllable* target)
{
    for (auto& bind : mBinds)
    {
        bool execute = false;
        bool isFromKeyboard = false;
        bool isFromGamepad = false;

        switch (bind.device)
        {
        case InputDevice::Keyboard:
        {
            switch (bind.keyType)
            {
            case InputType::Hold:    execute = vnKeyboard::on(bind.key);  break;
            case InputType::Trigger: execute = vnKeyboard::trg(bind.key); break;
            case InputType::Release: execute = vnKeyboard::rel(bind.key); break;
            }
            // 実行された＝キーボード操作があった
            if (execute) isFromKeyboard = true;
            break;
        }
        case InputDevice::Mouse:
        {
            switch (bind.mouseType)
            {
            case InputType::Hold:    execute = vnMouse::on(bind.mouseButton); break;
            case InputType::Trigger: execute = vnMouse::trg(bind.mouseButton); break;
            case InputType::Release: execute = vnMouse::rel(bind.mouseButton); break;
            }

            if (execute) isFromKeyboard = true;
            break;
        }
        case InputDevice::Gamepad:
        {
            // パッドが無ければスキップ（抜き差し対策）
            if (!Pad::isValid(bind.padId)) break;

            switch (bind.padType)
            {
            case InputType::Hold:    execute = Pad::on(bind.padButton, bind.padId);  break;
            case InputType::Trigger: execute = Pad::trg(bind.padButton, bind.padId); break;
            case InputType::Release: execute = Pad::rel(bind.padButton, bind.padId); break;
            }
            // 実行された＝パッド操作があった
            if (execute) isFromGamepad = true;
            break;
        }

        case InputDevice::Always:
        {
            // 毎フレーム実行
            execute = true;
            break;
        }
        }

        // Always（スティック）の場合：入力がある時だけGamepad扱い
        if (bind.device == InputDevice::Always)
        {
            // 左スティックが倒れてたらNotify
            // ※ dead は MoveAxisCommand 側の値に合わせる
            if (Pad::isValid(0))
            {
                float x = Pad::leftX(0, 0.20f);
                float y = Pad::leftY(0, 0.20f, true);
                if (x != 0.0f || y != 0.0f)
                {
                    isFromGamepad = true;
                }
            }
        }

        // ここで「最後に入力があったデバイス」を更新
        if (isFromKeyboard)
            InputDeviceTracker::Get().NotifyKeyboard();
        else if (isFromGamepad)
            InputDeviceTracker::Get().NotifyGamepad();


        if (execute && bind.command)
        {
            bind.command->Execute(target);
        }
    }
}

