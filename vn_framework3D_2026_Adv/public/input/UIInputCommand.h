#pragma once
#include "IUIControllable.h"
#include "../../public/UI/UIStickRepeater.h"

//--------------------------------------------
// UI用コマンド
//--------------------------------------------
class UICommand
{
public:
    virtual ~UICommand() = default;
    virtual void Execute(IUIControllable* ui, float dt, int padId) = 0;
};

// 単発移動（Dキー / D-Pad用）
class UIMoveH : public UICommand
{
public:
    explicit UIMoveH(int dir) : mDir(dir) {}
    void Execute(IUIControllable* ui, float, int) override
    {
        if (ui) ui->MoveUIHorizontal(mDir);
    }
private:
    int mDir = 0;
};

class UIMoveV : public UICommand
{
public:
    explicit UIMoveV(int dir) : mDir(dir) {}
    void Execute(IUIControllable* ui, float, int) override
    {
        if (ui) ui->MoveUIVertical(mDir);
    }
private:
    int mDir = 0;
};

// Always：左スティック→移動（リピート付き）
class UIStickMoveCommand : public UICommand
{
public:
    UIStickMoveCommand(UIStickRepeater* rep, float dead = 0.20f, bool invertY = false)
        : mRep(rep), mDead(dead), mInvertY(invertY) {}

    void Execute(IUIControllable* ui, float dt, int padId) override;

private:
    UIStickRepeater* mRep = nullptr; // handlerのmStickを渡す
    float mDead = 0.20f;
    bool  mInvertY = false;
};

// 決定/キャンセル
class UISubmit : public UICommand
{
public:
    void Execute(IUIControllable* ui, float, int) override
    {
        if (ui) ui->Submit();
    }
};

class UICancel : public UICommand
{
public:
    void Execute(IUIControllable* ui, float, int) override
    {
        if (ui) ui->Cancel();
    }
};

//--------------------------------------------
// UI入力バインド
//--------------------------------------------
enum class UIInputType
{
    Trigger,
    Hold,
    Release
};

struct UIBind
{
    // keyboard
    bool useKey = false;
    BYTE key = 0;
    UIInputType keyType = UIInputType::Trigger;

    // pad
    bool usePad = false;
    PadButton padButton = PadButton::A;
    UIInputType padType = UIInputType::Trigger;
    int padId = 0;

    std::unique_ptr<UICommand> cmd;
};

//--------------------------------------------
// UI入力ハンドラ
//--------------------------------------------
class UIInputHandler
{
public:
    void BindKey(BYTE key, UIInputType type, std::unique_ptr<UICommand> cmd);
    void BindPad(PadButton btn, UIInputType type, std::unique_ptr<UICommand> cmd, int padId = 0);

    // Always（毎フレーム）
    void BindAlways(std::unique_ptr<UICommand> cmd);

    // Scene/UIからはこれ1本
    void HandleInput(IUIControllable* ui, float dt);

    void Reset() { mStick.Reset(); }

    // 便利：スティック用Always登録
    void BindDefaultStick(float dead = 0.20f, bool invertY = false)
    {
        BindAlways(std::make_unique<UIStickMoveCommand>(&mStick, dead, invertY));
    }

private:
    void HandleButtons(IUIControllable* ui, float dt);

private:
    std::vector<UIBind> mBinds;
    std::vector<std::unique_ptr<UICommand>> mAlways;
    UIStickRepeater mStick;
};