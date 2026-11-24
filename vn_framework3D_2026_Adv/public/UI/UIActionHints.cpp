#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"UIActionHints.h"

void UIActionHints::Initialize(vnScene* scene)
{
    if (!scene) return;

    const float sw = (float)vnMainFrame::screenWidth;
    const float sh = (float)vnMainFrame::screenHeight;

    // 右下の基準位置
    const float baseX = sw - 200.0f;
    const float baseY = sh - 170.0f;

    // ----------------------------
    // KBM（縦に並べる）
    // ----------------------------
    mKB_Attack.Create(scene, baseX - 100, baseY - 100, L"data/image/UI/icons_attack.png", L"data/image/UI/icons_key_leftButtoon.png");
    mKB_Avoid.Create(scene, baseX - 200, baseY + 20, L"data/image/UI/icons_avoid.png", L"data/image/UI/icons_key_LShift.png");
    mKB_Lock.Create(scene, baseX, baseY + - 200, L"data/image/UI/icons_lock.png", L"data/image/UI/icons_key_R.png");
    mKB_Jump.Create(scene, baseX, baseY + 60, L"data/image/UI/icons_jump.png", L"data/image/UI/icons_key_space.png");

    // ----------------------------
    // PAD（十字配置 + R3を下に）
    // ----------------------------
    const float padCX = baseX;
    const float padCY = baseY;

    // 十字：左□(攻撃) 右○(回避) 下×(ジャンプ)
    mPD_Attack.Create(scene, padCX - 120, padCY, padCX - 50, padCY, L"data/image/UI/icons_attack.png", L"data/image/UI/icons_pad_square.png");
    mPD_Avoid.Create(scene, padCX + 120, padCY,padCX + 50, padCY, L"data/image/UI/icons_avoid.png", L"data/image/UI/icons_pad_circle.png");
    mPD_Jump.Create(scene, padCX, padCY + 120, padCX, padCY + 50, L"data/image/UI/icons_jump.png", L"data/image/UI/icons_pad_cross.png");

    // ロックオンは下に（アイコン右上にボタン表示）
    mPD_Lock.Create(scene, padCX + 120, padCY - 200, L"data/image/UI/icons_lock.png", L"data/image/UI/icons_pad_R3.png");

    // 初期表示
    SetVisible(true);

}

void UIActionHints::Terminate(vnScene* scene)
{
    mKB_Attack.Destroy(scene);
    mKB_Avoid.Destroy(scene);
    mKB_Lock.Destroy(scene);
    mKB_Jump.Destroy(scene);

    mPD_Attack.Destroy(scene);
    mPD_Avoid.Destroy(scene);
    mPD_Jump.Destroy(scene);
    mPD_Lock.Destroy(scene);
}

void UIActionHints::SetVisible(bool v)
{
    mVisible = v;
    ApplyDeviceVisible(); // v と device に合わせてON/OFF
}

void UIActionHints::SetAvoidJustColor(bool on)
{
    if (on)
    {
        // 黄色
        mKB_Avoid.SetBaseColor(1.0f, 0.9f, 0.2f, 0.95f);
        mPD_Avoid.SetBaseColor(1.0f, 0.9f, 0.2f, 0.95f);
    }
    else
    {
        mKB_Avoid.ResetBaseColor();
        mPD_Avoid.ResetBaseColor();
    }
}

void UIActionHints::Update(float dt)
{
    if (!mVisible) return;

    InputDeviceTracker::Get().Update(dt);

    ApplyDeviceVisible();

    // 今どっちのデバイス表示か
    const ActiveDevice dev = InputDeviceTracker::Get().GetDevice();

    if (dev == ActiveDevice::KeyboardMouse)
    {
        const bool atkDown = false;            // 押下中は不明なのでfalse
        const bool atkTrg = vnMouse::trgL();   // 押した瞬間（リップル開始）

        const bool avdDown = vnKeyboard::on(DIK_LSHIFT);
        const bool avdTrg = vnKeyboard::trg(DIK_LSHIFT);

        const bool lockDown = vnKeyboard::on(DIK_R);
        const bool lockTrg = vnKeyboard::trg(DIK_R);

        const bool jmpDown = vnKeyboard::on(DIK_SPACE);
        const bool jmpTrg = vnKeyboard::trg(DIK_SPACE);

        // リップル対応：isDown + isTrigger + dt
        mKB_Attack.UpdatePress(atkDown, atkTrg, dt);
        mKB_Avoid.UpdatePress(avdDown, avdTrg, dt);
        mKB_Lock.UpdatePress(lockDown, lockTrg, dt);
        mKB_Jump.UpdatePress(jmpDown, jmpTrg, dt);
    }
    else
    {
        // PAD：PS配置（□攻撃/○回避/×ジャンプ/R3ロック）
        const bool atkDown  = Pad::on(PadButton::X, 0);   // □
        const bool atkTrg   = Pad::trg(PadButton::X, 0);  // □
                            
        const bool avdDown  = Pad::on(PadButton::B, 0);   // ○
        const bool avdTrg   = Pad::trg(PadButton::B, 0);  // 〇
                            
        const bool jmpDown  = Pad::on(PadButton::A, 0);   // ×
        const bool jmpTrg   = Pad::trg(PadButton::A, 0);  // ×

        const bool lockDown = Pad::on(PadButton::R3, 0);  // R3
        const bool lockTrg  = Pad::trg(PadButton::R3, 0); // R3

        mPD_Attack.UpdatePress(atkDown, atkTrg, dt);
        mPD_Avoid.UpdatePress(avdDown, avdTrg, dt);
        mPD_Jump.UpdatePress(jmpDown, jmpTrg, dt);
        mPD_Lock.UpdatePress(lockDown, lockTrg, dt);
    }

}

void UIActionHints::SetAttentionAvoid(bool on)
{
    mKB_Avoid.SetAttention(on, 0.35f, true);
    mPD_Avoid.SetAttention(on, 0.35f, true);
}

void UIActionHints::SetAttentionAttack(bool on)
{
    mKB_Attack.SetAttention(on, 0.35f, true);
    mPD_Attack.SetAttention(on, 0.35f, true);
}

void UIActionHints::SetHintAttention(Hint hint, bool on, float interval)
{
    // 表示自体がOFFなら何もしない
    if (!mVisible)
    {
        ClearAllHintAttention();
        return;
    }

    // デバイス切替で前のデバイス側に残る事故を防ぐため、
    // 両デバイスの同hintを一旦OFF
    switch (hint)
    {
    case Hint::Attack: mKB_Attack.SetAttention(false); mPD_Attack.SetAttention(false); break;
    case Hint::Avoid:  mKB_Avoid.SetAttention(false);  mPD_Avoid.SetAttention(false);  break;
    case Hint::Jump:   mKB_Jump.SetAttention(false);   mPD_Jump.SetAttention(false);   break;
    case Hint::Lock:   mKB_Lock.SetAttention(false);   mPD_Lock.SetAttention(false);   break;
    }

    if (!on) return;

    // 現在表示中のデバイス側だけON
    if (UIHintItem* item = GetCurrentHintItem(hint))
    {
        item->SetAttention(true, interval, true);
    }
}

void UIActionHints::ClearAllHintAttention()
{
    // 全部OFF（KB/PAD両方）
    mKB_Attack.SetAttention(false); mPD_Attack.SetAttention(false);
    mKB_Avoid.SetAttention(false);  mPD_Avoid.SetAttention(false);
    mKB_Jump.SetAttention(false);   mPD_Jump.SetAttention(false);
    mKB_Lock.SetAttention(false);   mPD_Lock.SetAttention(false);
}

void UIActionHints::ApplyDeviceVisible()
{
    const bool showKB = (mVisible && InputDeviceTracker::Get().GetDevice() == ActiveDevice::KeyboardMouse);
    const bool showPD = (mVisible && InputDeviceTracker::Get().GetDevice() == ActiveDevice::Gamepad);

    mKB_Attack.SetVisible(showKB);
    mKB_Avoid.SetVisible(showKB);
    mKB_Lock.SetVisible(showKB);
    mKB_Jump.SetVisible(showKB);

    mPD_Attack.SetVisible(showPD);
    mPD_Avoid.SetVisible(showPD);
    mPD_Jump.SetVisible(showPD);
    mPD_Lock.SetVisible(showPD);
}

UIHintItem* UIActionHints::GetCurrentHintItem(Hint hint)
{
    const ActiveDevice dev = InputDeviceTracker::Get().GetDevice();

    if (dev == ActiveDevice::KeyboardMouse)
    {
        switch (hint)
        {
        case Hint::Attack: return &mKB_Attack;
        case Hint::Avoid:  return &mKB_Avoid;
        case Hint::Jump:   return &mKB_Jump;
        case Hint::Lock:   return &mKB_Lock;
        }
    }
    else
    {
        switch (hint)
        {
        case Hint::Attack: return &mPD_Attack;
        case Hint::Avoid:  return &mPD_Avoid;
        case Hint::Jump:   return &mPD_Jump;
        case Hint::Lock:   return &mPD_Lock;
        }
    }

    return nullptr;
}
