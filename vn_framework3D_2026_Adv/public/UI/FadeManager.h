#pragma once

class vnScene;

// フェードの「状態」
enum class FadeState
{
    None,
    FadeOut,
    FadeIn
};

enum class FadeColor
{
    Black,
    White
};

// フェード管理
class FadeManager
{
public:
    static FadeManager& Get()
    {
        static FadeManager inst;
        return inst;
    }

    // sceneが切り替わっても残るように毎フレーム更新したい
    void Update(float dt);

    // 描画用：内部で vnSprite を使うので、initialize時に一度だけ呼ぶ
    void Initialize(vnScene* scene);
    void Terminate(vnScene* scene);

    // フェードアウト開始（完了時に callback を呼ぶ）
    void StartFadeOut(float duration, std::function<void()> onFinished , FadeColor color = FadeColor::Black);

    // フェードイン開始
    void StartFadeIn(float duration, FadeColor color = FadeColor::Black);

    bool IsFading() const { return mState != FadeState::None; }

private:
    FadeManager() = default;

    void SetRender(bool on);
    void SetAlpha(float a);
    void SetFadeColor(FadeColor color);

private:
    FadeState mState = FadeState::None;

    float mTimer = 0.0f;
    float mDuration = 0.5f;

    // FadeOut完了時に呼ぶ（シーン切替など）
    std::function<void()> mOnFadeOutFinished;

    // 全画面黒板
    vnSprite* mFadePlate = nullptr;
};