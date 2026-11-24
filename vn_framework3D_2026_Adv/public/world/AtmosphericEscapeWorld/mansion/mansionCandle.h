#pragma once

class Effect;
// CSV の位置はろうそく本体の原点ではなく、芯の先端のワールド座標
class MansionCandle
{
public:
    MansionCandle(const std::string& placementId, const XMFLOAT3& wickPosition, float scale);

    // World初期化時に1回呼び生成した炎をEffectManagerへ渡す
    std::unique_ptr<Effect> CreateFlame() const;

    // ライトバケットBeginFrame後Resolve前に呼ぶ
    void UpdateLight(float dt);

private:
    XMFLOAT3 mWickPosition;
    float mScale;
    float mPhase;
    float mTime = 0.0f;
};
