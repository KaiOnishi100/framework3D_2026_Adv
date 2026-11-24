#pragma once

#include"../../collide/allCollider.h"

class BossStage : public Object
{
private:
    XMVECTOR mCenter;
    float    mRadius;
    bool     mActivated; //ボスエリア侵入フラグ

public:
    BossStage(const WCHAR* folder, const WCHAR* file,float radius);
        
    XMVECTOR GetCenter() { return mCenter; }

    bool CheckPlayerInside(const XMVECTOR& playerPos);
    bool ClampPosition(XMVECTOR& pos);

    bool IsActivated() const { return mActivated; }

    const MeshCollider* GetStageMesh() const { return dynamic_cast<const MeshCollider*>(mCollider); }//メッシュコライダーに変換
};