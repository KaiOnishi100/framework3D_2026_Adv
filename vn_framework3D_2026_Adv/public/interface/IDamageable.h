#pragma once

#include "../../framework/vn_environment.h"

//============================================================
// IDamageable
// Damageを与えるためのInterface
//============================================================
class IDamageable
{
public:

    virtual ~IDamageable() = default;

    //========================================================
    // Damage
    // true: 実際にDamageが入った
    // false: 無敵中 / 死亡済みなど
    //========================================================
    virtual bool ApplyDamage(float damage) = 0;


    //========================================================
    // Death
    //========================================================
    virtual bool IsDead() const = 0;
};