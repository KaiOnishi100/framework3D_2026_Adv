#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "AtmosphericChaseVignette.h"

namespace
{
    float Smooth01(float value)
    {
        const float t = (std::clamp)(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // 指定時刻でピークに達しその後なだらかに減衰する1つの山
    float SamplePulse(float age, float peakTime, float fallTime)
    {
        if (age < peakTime)
        {
            return Smooth01(age / peakTime);
        }

        return 1.0f - Smooth01((age - peakTime) / fallTime);
    }
}

void AtmosphericChaseVignette::Initialize(vnScene* scene)
{
    if (scene == nullptr || mSprite != nullptr)
    {
        return;
    }

    const float width = static_cast<float>(vnMainFrame::screenWidth);
    const float height = static_cast<float>(vnMainFrame::screenHeight);

    // PNGの透明度で画面中央を抜き縁だけに色を重ねる
    mSprite = new vnSprite(
        width * 0.5f, height * 0.5f, width, height,
        L"data/image/UI/AtmosphericEscape/ChaseVignette.png");

    // 通常のシーンより手前に描く 3000番台のポーズ・リザルトUIより後方
    mSprite->setRenderPriority(2000);
    scene->registerObject(mSprite);
    Reset();
}

void AtmosphericChaseVignette::Terminate(vnScene* scene)
{
    if (mSprite != nullptr && scene != nullptr)
    {
        scene->deleteObject(mSprite);
        mSprite = nullptr;
    }

    Reset();
}

void AtmosphericChaseVignette::Update(float dt, bool chasing, bool emittedBeat)
{
    if (!std::isfinite(dt) || dt <= 0.0f)
    {
        return;
    }

    constexpr float kBaseOpacity = 0.22f;  // 拍の間も残す薄い赤
    constexpr float kPulseOpacity = 0.42f; // 拍のときに追加する赤

    const float elapsed = (std::min)(dt, 0.1f);
    const float target = chasing ? 1.0f : 0.0f;
    const float fadeRate = chasing ? 8.0f : 5.0f;
    mPresence += (target - mPresence) * (1.0f - std::exp(-fadeRate * elapsed));

    mTimeSinceBeat = (std::min)(mTimeSinceBeat + elapsed, 1.0f);
    if (chasing && emittedBeat)
    {
        // 同じフレームで心音が鳴る 画像側もここから拍を始める
        mTimeSinceBeat = 0.0f;
    }

    if (chasing)
    {
        // Heartbeat.wavの2つの打音に合わせて強・弱の2回だけ膨らませる
        // 山の位置は再生開始から約0.03秒と0.17秒
        const float firstBeat = SamplePulse(mTimeSinceBeat, 0.030f, 0.105f);
        const float secondBeat = 0.57f * SamplePulse(mTimeSinceBeat - 0.135f, 0.035f, 0.105f);
        mPulse = (std::max)(firstBeat, secondBeat);
    }
    else
    {
        // 見失った後は新しい拍を作らず残っている色を薄くする
        mPulse *= std::exp(-10.0f * elapsed);
    }

    mOpacity = mPresence * (kBaseOpacity + kPulseOpacity * mPulse);
}

void AtmosphericChaseVignette::Reset()
{
    mPresence = 0.0f;
    mTimeSinceBeat = 1.0f;
    mPulse = 0.0f;
    mOpacity = 0.0f;
    Apply(false);
}

void AtmosphericChaseVignette::Apply(bool gameplayVisible)
{
    if (mSprite == nullptr)
    {
        return;
    }

    const bool visible = gameplayVisible && mOpacity > 0.005f;
    mSprite->setRenderEnable(visible);
    mSprite->setExecuteEnable(visible);

    if (visible)
    {
        // PNGのアルファマスクに現在の拍のアルファを掛け合わせる
        for (int i = 0; i < vnSprite::VertexNum; ++i)
        {
            mSprite->vtx[i].a = mOpacity;
        }
    }
}
