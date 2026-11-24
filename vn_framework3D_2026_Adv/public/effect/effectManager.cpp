#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "effectManager.h"

void EffectManager::AddEffect(std::unique_ptr<Effect> effect)
{
    if (effect)
    {
        WCHAR buf[128];
        swprintf_s(buf, L"AddEffect owner=%p effect=%p", effect->GetOwner(), effect.get());
        vnFont::output(buf); // 一時ログ
    }
    mEffects.push_back(std::move(effect));
}

void EffectManager::StopAllByOwner(Character* owner)
{
    for (auto& e : mEffects)
    {
        if (e && e->GetOwner() == owner)
            e->Stop();
    }
}

//更新処理
void EffectManager::Update(float dt)
{
    for (auto& effect : mEffects)
    {
        if (effect)
        {
            effect->Update(dt);
        }
    }

    // 死んだエフェクトを削除
    mEffects.erase(
        std::remove_if(
            mEffects.begin(),
            mEffects.end(),
            [](const std::unique_ptr<Effect>& effect)
            {
                return !effect->IsAlive();
            }
        ),
        mEffects.end()
    );
}

void EffectManager::Render()
{
    for (auto& effect : mEffects)
    {
        effect->Render();
    }
}

void EffectManager::Clear()
{
    mEffects.clear();
}

void EffectManager::SetAllOpacity(float a)
{
    for (auto& e : mEffects)if (e) e->SetOpacity(a);
}

void EffectManager::RenderFullResolution()
{
    for (auto& effect : mEffects)
    {
        if (!effect)
        {
            continue;
        }

        if (effect->GetRenderPath() != EffectRenderPath::FullResolution)
        {
            continue;
        }
        effect->Render();
    }
}

void EffectManager::RenderHalfResolutionAdditive()
{
    for (auto& effect : mEffects)
    {
        if (!effect)
        {
            continue;
        }
        if (effect->GetRenderPath() != EffectRenderPath::HalfResolutionAdditive)
        {
            continue;
        }
        effect->Render();
    }
}
