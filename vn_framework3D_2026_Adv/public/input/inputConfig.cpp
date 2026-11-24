#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "inputConfig.h"

//キー情報の設定
void InputConfig::SetupPlayer(InputHandler& handler)
{
    // -----------------------------
    // Keyboard
    // -----------------------------
    handler.BindKey(DIK_W, InputType::Hold,
		std::make_unique<MoveCommand>(Direction::Forward));
	
    handler.BindKey(DIK_S, InputType::Hold,
        std::make_unique<MoveCommand>(Direction::Backward));

    handler.BindKey(DIK_A, InputType::Hold,
        std::make_unique<MoveCommand>(Direction::Left));

    handler.BindKey(DIK_D, InputType::Hold,
        std::make_unique<MoveCommand>(Direction::Right));

    handler.BindMouse(vnMouse::LT, InputType::Trigger,
        std::make_unique<StartAttackCommand>());

    handler.BindMouse(vnMouse::LT, InputType::Release,
        std::make_unique<AttackReleaseCommand>());

    handler.BindKey(DIK_LSHIFT, InputType::Trigger,
        std::make_unique<AvoidCommand>());

    handler.BindKey(DIK_SPACE, InputType::Trigger,
        std::make_unique<JumpCommand>());

    handler.BindKey(DIK_R, InputType::Trigger,
        std::make_unique<LookOnCommand>());

    // -----------------------------
    // Gamepad
    // -----------------------------

    // 左スティック移動は毎フレーム読む
    handler.BindAlways(std::make_unique<MoveAxisCommand>(0, 0.20f, true));

    handler.BindPad(PadButton::X, InputType::Trigger, std::make_unique<StartAttackCommand>(), 0);   // □ = X
    handler.BindPad(PadButton::X, InputType::Release, std::make_unique<AttackReleaseCommand>(), 0); // □ = X
    handler.BindPad(PadButton::A, InputType::Trigger, std::make_unique<JumpCommand>(), 0);          // × = A
    handler.BindPad(PadButton::B, InputType::Trigger, std::make_unique<AvoidCommand>(), 0);         // ○ = B
    handler.BindPad(PadButton::R3, InputType::Trigger, std::make_unique<LookOnCommand>(), 0);
}
