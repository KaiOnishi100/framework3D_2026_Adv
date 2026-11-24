#pragma once


//============================================================
// PlayerFlashlight
// ExplorationPlayer専用のFlashlight機能
// ・ON / OFF
// ・Spot Light位置計算
// ・Spot Light方向計算
// ・LocalLightManagerへの登録
//============================================================
class PlayerFlashlight
{
public:

    //========================================================
    // Flashlight Settings
    //========================================================
    struct Settings
    {
        //====================================================
        // Spot Light到達距離
        //====================================================
        float Range = 8.0f;

        //====================================================
        // HDR Light強度
        //====================================================
        float Intensity = 80.0f;


        //====================================================
        // Spot Cone
        // Inner: 最大強度を維持する角度
        // Outer: 完全に光が消える角度
        //====================================================
        float InnerAngleDegrees = 12.0f;

        float OuterAngleDegrees = 22.0f;


        //====================================================
        // Player Rootからの位置Offset
        // Y: 胸～手あたりの高さ
        // Forward: Player中心から少し前へ
        // Side: 右手側へ少し寄せる
        //====================================================
        float HeightOffset = 1.30f;

        float CrouchHeightOffset = 0.85f;

        float ForwardOffset = 0.22f;

        float SideOffset = 0.20f;


        //====================================================
        // Light Color
        //====================================================
        XMFLOAT3 Color =
        {
            1.0f,
            0.92f,
            0.80f
        };
    };


public:

    PlayerFlashlight() = default;

    ~PlayerFlashlight() = default;


    //========================================================
    // 毎Frame更新
    // FlashlightがONの場合
    // LocalLightManagerへSpot Lightを登録する
    //========================================================
    void Update(ObjectBase& owner, bool crouching, const XMMATRIX* attachedBoneWorld = nullptr);


    //========================================================
    // ON / OFF Toggle
    //========================================================
    void Toggle()
    {
        mEnabled = !mEnabled;
    }


    //========================================================
    // 明示的なON / OFF
    //========================================================
    void SetEnabled(bool enabled)
    {
        mEnabled = enabled;
    }


    bool IsEnabled() const
    {
        return mEnabled;
    }


    //========================================================
    // Settings
    //========================================================
    void SetSettings(const Settings& settings)
    {
        mSettings = settings;
    }


    const Settings& GetSettings() const
    {
        return mSettings;
    }

    void SubmitSpotLight(const XMVECTOR& flashlightPosition, const XMVECTOR& forward) const;

    //============================================================
    // ライト本体モデルを登録
    //============================================================
    void SetVisualModel(ModelBase* model)
    {
        mVisualModel = model;
    }

    //============================================================
    // Flashlight専用Shadow除外対象を設定
    //============================================================
    void SetShadowExcludedCaster(const ObjectBase* caster)
    {
        mShadowExcludedCaster = caster;
    }

private:

    Settings mSettings;

    //============================================================
    // Flashlight Visual Model
    //============================================================
    ModelBase* mVisualModel = nullptr;

    //============================================================
    // Flashlight Shadow Exclusion
    //============================================================
    const ObjectBase* mShadowExcludedCaster = nullptr;

    //========================================================
    // 最初はOFF
    //========================================================
    bool mEnabled = false;
};