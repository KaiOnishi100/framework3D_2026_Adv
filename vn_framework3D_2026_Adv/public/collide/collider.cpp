#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include"collider.h"
#include "IPhysicsCharacter.h"

Character* Collider::GetOwnerCharacter() const
{

    if (mOwner == nullptr)
    {
        return nullptr;
    }

    return dynamic_cast<Character*>(mOwner);
}

IPhysicsCharacter* Collider::GetOwnerPhysicsCharacter() const
{
    if (mOwner == nullptr)
    {
        return nullptr;
    }


    return dynamic_cast<IPhysicsCharacter*>(mOwner);
}

IDamageable* Collider::GetOwnerDamageable() const
{
    if (mOwner == nullptr)
    {
        return nullptr;
    }

    return dynamic_cast<IDamageable*>(mOwner);
}
