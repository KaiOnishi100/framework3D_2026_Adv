#include "../../framework.h"
#include "../../framework/vn_environment.h"
#include "UIInputCommand.h"

//------------------------------
// Bind
//------------------------------
void UIInputHandler::BindKey(BYTE key, UIInputType type, std::unique_ptr<UICommand> cmd)
{
    UIBind b;
    b.useKey = true;
    b.key = key;
    b.keyType = type;
    b.cmd = std::move(cmd);
    mBinds.push_back(std::move(b));
}

void UIInputHandler::BindPad(PadButton btn, UIInputType type, std::unique_ptr<UICommand> cmd, int padId)
{
    UIBind b;
    b.usePad = true;
    b.padButton = btn;
    b.padType = type;
    b.padId = padId;
    b.cmd = std::move(cmd);
    mBinds.push_back(std::move(b));
}

void UIInputHandler::BindAlways(std::unique_ptr<UICommand> cmd)
{
    mAlways.push_back(std::move(cmd));
}

//------------------------------
// Main
//------------------------------
void UIInputHandler::HandleInput(IUIControllable* ui, float dt)
{
    if (!ui) return;

    // Always
    for (auto& c : mAlways)
    {
        if (c) c->Execute(ui, dt, 0);
    }

    // Bind（キー/ボタン）
    HandleButtons(ui, dt);
}

void UIInputHandler::HandleButtons(IUIControllable* ui, float dt)
{
    for (auto& b : mBinds)
    {
        bool exec = false;
        bool isFromKeyboard = false;
        bool isFromGamepad = false;
        // keyboard
        if (b.useKey)
        {
            switch (b.keyType)
            {
            case UIInputType::Hold:    exec = vnKeyboard::on(b.key);  break;
            case UIInputType::Trigger: exec = vnKeyboard::trg(b.key); break;
            case UIInputType::Release: exec = vnKeyboard::rel(b.key); break;
            }
            if (exec)isFromKeyboard = true;
        }

        // pad
        if (!exec && b.usePad)
        {
            if (!Pad::isValid(b.padId)) continue;

            switch (b.padType)
            {
            case UIInputType::Hold:    exec = Pad::on(b.padButton, b.padId);  break;
            case UIInputType::Trigger: exec = Pad::trg(b.padButton, b.padId); break;
            case UIInputType::Release: exec = Pad::rel(b.padButton, b.padId); break;
            }
            if (exec)isFromGamepad = true;
        }

        if (isFromKeyboard)
            InputDeviceTracker::Get().NotifyKeyboard();
        else if (isFromGamepad)
            InputDeviceTracker::Get().NotifyGamepad();

        if (exec && b.cmd)
        {
            // padId は bind に入ってるものを渡す
            b.cmd->Execute(ui, dt, b.padId);
        }
    }
}

//------------------------------
// Commands
//------------------------------
void UIStickMoveCommand::Execute(IUIControllable* ui, float dt, int padId)
{
    if (!ui) return;
    if (!mRep) return;
    if (!Pad::isValid(padId)) return;

    // 左スティック（Padの仕様に合わせる）
    float ax = Pad::leftX(padId, mDead);
    float ay = Pad::leftY(padId, mDead, mInvertY);

    // リピートで -1/+1
    int mx = mRep->UpdateHorizontal(dt, ax);
    int my = mRep->UpdateVertical(dt, ay);

    if (mx != 0) ui->MoveUIHorizontal(mx);
    if (my != 0) ui->MoveUIVertical(my);
}