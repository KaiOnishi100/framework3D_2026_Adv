#pragma once

class EffectManager
{
private:
    // Effectをまとめて保持
    std::vector<std::unique_ptr<Effect>> mEffects;

public:
    // 新しいエフェクトを追加
    void AddEffect(std::unique_ptr<Effect> effect);


    void StopAllByOwner(Character* owner);

    // 全エフェクト更新
    void Update(float dt);

    // 全エフェクト描画
    void Render();

    // 全削除（ステージ終了時など）
    void Clear();

    void SetAllOpacity(float a);

    void RenderFullResolution();

    void RenderHalfResolutionAdditive();
};