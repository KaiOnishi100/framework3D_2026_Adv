#pragma once

class BoxCollider : public Collider
{
private:
    //========================================================
    // AABBの半サイズ
    //========================================================
    XMVECTOR halfSize;

    //========================================================
    // Ownerからの相対オフセット
    //========================================================
    XMVECTOR offset;

    //========================================================
    // 静的Colliderの中心座標
    // vnModelを持たないMansionBlockに使用する
    //========================================================
    XMFLOAT3 mFixedCenter = { 0.0f, 0.0f, 0.0f };

    //========================================================
    // true  : mFixedCenterを使用
    // false : Ownerの座標を使用
    //========================================================
    bool mUseFixedCenter = false;

    // Y軸回転角度（ラジアン）
    float mYawRadians = 0.0f;

    // ドアがプレイヤーに接触したときの通知
    // 通常の壁・床では未設定のまま使用する
    std::function<void(const Collider*)> mContactCallback;

public:

    //========================================================
    // 動的Collider
    // Ownerの移動に追従する
    //========================================================
    BoxCollider(ObjectBase* owner, const XMVECTOR& hs, XMVECTOR ownerOffset = XMVectorZero()) : Collider(owner),
        halfSize(hs), offset(ownerOffset), mUseFixedCenter(false) { }

    //========================================================
    // 静的Collider
    // Ownerなしでワールド中心座標を保持する
    //========================================================
    BoxCollider(const XMFLOAT3& worldCenter, const XMVECTOR& hs) : Collider(nullptr),
        halfSize(hs), offset(XMVectorZero()), mFixedCenter(worldCenter), mUseFixedCenter(true) { }

    //========================================================
    // Collider中心座標
    //========================================================
    XMVECTOR GetCenter() const
    {
        //====================================================
        // 静的Colliderの場合
        // Ownerがnullptrでも原点ではなく
        // CSVから取得した中心座標を返す
        //====================================================
        if (mUseFixedCenter)
        {
            return XMLoadFloat3(&mFixedCenter);
        }

        //====================================================
        // 動的Colliderの場合
        //====================================================
        if (mOwner != nullptr)
        {
            return *mOwner->getPosition() + offset;
        }

        return XMVectorZero();
    }

    const XMVECTOR& GetHalfSize() const
    {
        return halfSize;
    }

    //------------------------------------------------------------
    // Yaw角度設定
    //------------------------------------------------------------
    void SetYawRadians(float yaw)
    {
        mYawRadians = yaw;
    }

    float GetYawRadians() const
    {
        return mYawRadians;
    }

    //------------------------------------------------------------
    // World座標 → Boxのローカル座標
    // 回転したBoxを回転していないAABBとして計算できる
    //------------------------------------------------------------
    XMVECTOR ToLocalPoint(const XMVECTOR& worldPoint) const
    {
        const XMVECTOR relative =
            worldPoint - GetCenter();

        return XMVector3TransformNormal(
            relative,
            XMMatrixRotationY(-mYawRadians)
        );
    }

    //------------------------------------------------------------
    // World方向 → Boxローカル方向
    //------------------------------------------------------------
    XMVECTOR ToLocalDirection(const XMVECTOR& worldDirection) const
    {
        return XMVector3TransformNormal(
            worldDirection,
            XMMatrixRotationY(-mYawRadians)
        );
    }

    //------------------------------------------------------------
    // Boxローカル方向 → World方向
    //------------------------------------------------------------
    XMVECTOR ToWorldDirection(const XMVECTOR& localDirection) const
    {
        return XMVector3TransformNormal(
            localDirection,
            XMMatrixRotationY(mYawRadians)
        );
    }


    //============================================================
    // BoxCollider - OwnerからのWorld座標Offset変更
    // モデルの描画位置とColliderの中心位置を
    // 一致させたくない場合に使用する
    //============================================================
    void SetOffset(const XMVECTOR& worldOffset)
    {
        offset = XMVectorSet(
            XMVectorGetX(worldOffset),
            XMVectorGetY(worldOffset),
            XMVectorGetZ(worldOffset),
            0.0f
        );
    }


    //------------------------------------------------------------
    // 接触通知を登録
    //------------------------------------------------------------
    void SetContactCallback( std::function<void(const Collider*)> callback)
    {
        mContactCallback = std::move(callback);
    }

    //------------------------------------------------------------
    // CollisionManagerから呼び出す
    //------------------------------------------------------------
    void NotifyContact(const Collider* other) const
    {
        if (mContactCallback)
        {
            mContactCallback(other);
        }
    }

    //============================================================
    // Ray vs AABB
    //============================================================
    bool RayCast(
        const XMVECTOR& start,
        const XMVECTOR& dir,
        float maxDistance,
        float& outDistance
    ) const override;

    // 基本の判定
    bool CollisionCheck(const Collider* other) const override;
    bool CollisionWithSphere(const SphereCollider* sphere) const override;
    bool CollisionWithBox(const BoxCollider* box) const override;
    bool CollisionWithMesh(const MeshCollider* mesh) const override;
    bool CollisionWithCapsule(const CapsuleCollider* capsule) const override;

    //押し出し処理
    void Resolve(const Collider* other)const override;
};