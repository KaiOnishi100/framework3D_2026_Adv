#pragma once

// 屋敷の主電源に連動する PointLight
// シャンデリアや照明器具の外観は、既存の MansionArt の配置で扱う
class MansionPowerLight
{
public:
    MansionPowerLight(const std::string& lightId, const XMFLOAT3& position);

    // 電源が入っている間、毎フレーム既存のライトバケットへ送る
    void Update();

    void SetPowered(bool powered)
    {
        mPowered = powered;
    }

    bool IsPowered() const
    {
        return mPowered;
    }

    // World の初期化時に、MansionArt が生成したモデルを受け取る
    void SetExcludedShadowCaster(const ObjectBase* caster)
    {
        mExcludedShadowCaster = caster;
    }

private:
    // CSV の Position が実際の光源位置になる
    XMFLOAT3 mPosition;
    XMFLOAT3 mColor{ 1.0f, 0.78f, 0.52f };

    float mRange = 15.5f;
    float mIntensity = 45.0f;
    bool mPowered = false;

    // モデルは MansionArt が所有する
    const ObjectBase* mExcludedShadowCaster = nullptr;

    // Point Shadow Cache がフレームをまたいで同じ照明を識別する
    std::uint32_t mStableLightId = 0u;
};
