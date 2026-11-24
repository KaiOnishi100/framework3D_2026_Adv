#pragma once

//操作可能オブジェクトのインターフェイス
class IControllable
{
public:
    virtual ~IControllable() = default;

    //入力ゲート
    virtual bool IsControlLocked() const { return false; } // 移動/回避/視点など禁止
    virtual bool IsAttackOnly()   const { return false; }  // 攻撃だけ許可（=それ以外は禁止）

    virtual void AddMoveInput(Direction dir) = 0;
    virtual void AddMoveAxis(float x, float y) = 0;
    virtual void Jump() = 0;
    virtual void Avoid() = 0;
    virtual void ToggleLockOn() = 0;
    virtual void StartAttack() = 0;
    virtual void AttackRelease() = 0;
};