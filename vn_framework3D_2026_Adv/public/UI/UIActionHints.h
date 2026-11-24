#pragma once

class UIActionHints
{
public:
    // どのヒントを注目させるか
    enum class Hint
    {
        Attack,
        Avoid,
        Jump,
        Lock,
    };

    void Initialize(vnScene* scene);
    void Terminate(vnScene* scene);
    void SetVisible(bool v);
    void SetAvoidJustColor(bool on);

    // dtは DeviceTracker のUpdate用（チラつき軽減）
    void Update(float dt);

    //============================================================
    // 汎用：ヒント注目をON/OFF
    //============================================================
    void SetHintAttention(Hint hint, bool on, float interval = 0.35f);
    void SetAttentionAvoid(bool on);
    void SetAttentionAttack(bool on);
    //============================================================
    // まとめてOFF
    //============================================================
    void ClearAllHintAttention();
private:
    void ApplyDeviceVisible();

    UIHintItem* GetCurrentHintItem(Hint hint);
private:
    bool mVisible = true;

    // KBM
    UIHintItem mKB_Attack, mKB_Avoid, mKB_Lock, mKB_Jump;

    // PAD
    UIHintItem mPD_Attack, mPD_Avoid, mPD_Jump, mPD_Lock;
};