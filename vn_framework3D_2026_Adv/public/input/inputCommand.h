#pragma once
#include "IControllable.h"
#include "inputDeviceTracker.h"


enum class InputContext
{
    Game,  // プレイヤー操作など
    UI     // タイトル/結果画面など
};

enum class InputDevice
{
    Keyboard,
    Gamepad,
    Mouse,
    Always,   // 毎フレーム実行（スティックなど）
};

//==============================
// 入力種別
//==============================
enum class InputType
{
    Hold,     // 押している間
    Trigger,  // 押した瞬間
    Release   // 離した瞬間
};

//コマンドインターフェイス
class Command {
public:
    virtual ~Command() = default;
    virtual void Execute(IControllable* target) = 0;
};

struct InputBind
{
    InputDevice device = InputDevice::Keyboard;

    // keyboard用
    BYTE key = 0;
    InputType keyType = InputType::Hold;

    // mouse用
    vnMouse::eButton mouseButton;
    InputType mouseType;

    // gamepad用
    PadButton padButton = PadButton::A;
    InputType padType = InputType::Hold;
    int padId = 0;

    // 実行するコマンド
    std::unique_ptr<Command> command;
};


//==============================
// 移動系コマンド
//==============================
class MoveCommand : public Command
{
public:
    explicit MoveCommand(Direction dir) : mDir(dir) {}
    void Execute(IControllable* target) override;

private:
    Direction mDir;
};

class MoveAxisCommand : public Command
{
public:
    // どのコントローラーIDのスティックを見るか（基本0）
    MoveAxisCommand(int padId = 0, float dead = 0.20f, bool invertY = false)
        : mPadId(padId), mDead(dead), mInvertY(invertY) {}

    void Execute(IControllable* target) override;

private:
    int   mPadId = 0;
    float mDead = 0.20f;
    bool  mInvertY = false;
};

//==============================
// アクション系コマンド
//==============================
class JumpCommand : public Command {
public:
    void Execute(IControllable* target) override;
};
 
class AvoidCommand : public Command{
public:
    void Execute(IControllable* target) override;
};

class LookOnCommand : public Command
{
public:
    void Execute(IControllable* target) override;
};

class StartAttackCommand : public Command
{
public:
    void Execute(IControllable* target) override;
};

class AttackReleaseCommand : public Command
{
public:
    void Execute(IControllable* target) override;
};

//==============================
// キーバインド情報
//==============================
struct KeyBind
{
    BYTE key;
    InputType type;
    std::unique_ptr<Command> command;
};

class InputHandler
{
public:
    // キーボード
    void BindKey(BYTE key, InputType type, std::unique_ptr<Command> command);

    //マウス
    void BindMouse(vnMouse::eButton btn, InputType type, std::unique_ptr<Command> command);

    // ゲームパッド（DirectInputラッパPadのPadButtonを使う）
    void BindPad(PadButton button, InputType type, std::unique_ptr<Command> command, int padId = 0);

    // 毎フレーム実行（スティック等）
    void BindAlways(std::unique_ptr<Command> command);

    // 入力処理
    void HandleInput(IControllable* target);

private:
    std::vector<InputBind> mBinds;//キー情報と各コマンドの情報
};
