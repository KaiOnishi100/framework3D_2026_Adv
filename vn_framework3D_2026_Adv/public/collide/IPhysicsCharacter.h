#pragma once
//============================================================
// IPhysicsCharacter
// Collider / Physics側から見た
// 動くキャラクターの最小Interface
//============================================================
class IPhysicsCharacter
{
public:

    virtual ~IPhysicsCharacter() = default;


    //========================================================
    // Grounded
    // Mesh / Floorとの衝突解決時に
    // Collider側から接地状態を通知する
    //========================================================
    virtual void SetGrounded(bool grounded) = 0;


    virtual bool IsGrounded() const = 0;


    //========================================================
    // Vertical Velocity
    // 地面に着地した場合
    // 下向き速度を0へ戻すために使用する
    //========================================================
    virtual void SetVerticalSpeed(float speed) = 0;


    virtual float GetVerticalSpeed() const = 0;
};