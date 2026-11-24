#pragma once

#include "../../../public/world/AtmosphericEscapeWorld/exploration/explorationLocationSystem.h"
#include "../../../public/world/AtmosphericEscapeWorld/exploration/explorationLocation.h"


class ObjectBase;
class ExplorationLocationSystem;

//============================================================
// Atmospheric Escape Local Light / Shadow Budget
// ・通常Lightingとして使用するLight
// ・Shadow Mapを生成するLight
// をPlayer位置・Room情報から選択する
//============================================================
class AtmosphericEscapeLightBudget final
{
public:

    //========================================================
    // Point Light Candidate
    //========================================================
    struct PointLightRequest
    {
        XMFLOAT3 Position =
        {
            0.0f,
            0.0f,
            0.0f
        };

        XMFLOAT3 Color =
        {
            1.0f,
            1.0f,
            1.0f
        };

        float Range = 1.0f;

        float Intensity = 1.0f;

        //====================================================
        // Shadowを生成する候補にするか
        // false: 通常Lightingだけ行う
        // true: Shadow Budget選抜対象になる
        //====================================================
        bool CastsShadow = false;

        //====================================================
        // 固有Priority
        // 特別に重要なLightへ加点したい場合に使用する
        //====================================================
        float PriorityBias = 0.0f;

        //====================================================
        // Point Shadow Cache用Stable ID
        // 毎フレーム同じLightであることを識別する
        //====================================================
        std::uint32_t StableLightId = 0u;

        PointLightShadowManager::UpdateMode UpdateMode =
            PointLightShadowManager::UpdateMode::EveryFrame;

        PointLightShadowManager::ShadowResolution Resolution =
            PointLightShadowManager::ShadowResolution::Medium;

        bool LimitToRoomBounds = false;

        const ObjectBase* ExcludedShadowCaster = nullptr;

    };


    //========================================================
    // Spot Light Candidate
    //========================================================
    struct SpotLightRequest
    {
        XMFLOAT3 Position =
        {
            0.0f,
            0.0f,
            0.0f
        };

        XMFLOAT3 Direction =
        {
            0.0f,
            -1.0f,
            0.0f
        };

        XMFLOAT3 Color =
        {
            1.0f,
            1.0f,
            1.0f
        };

        float Range = 1.0f;

        float Intensity = 1.0f;

        float InnerAngleDegrees = 15.0f;

        float OuterAngleDegrees = 25.0f;

        bool CastsShadow = false;

        //====================================================
        // Player Flashlightなど
        // 通常Lightより圧倒的に優先したいLight
        //====================================================
        bool ForceShadow = false;

        float PriorityBias = 0.0f;

        float PCFRadius = 1.0f;

        float MinimumBias = 0.0003f;

        float SlopeBias = 0.0020f;

        bool LimitToRoomBounds = false;

        const ObjectBase* ExcludedShadowCaster = nullptr;
    };

    //============================================================
    // Light Budget Debug 
    // Runtime Debug表示用
    //============================================================
    struct DebugStats
    {
        UINT SubmittedPointLights = 0u;
        UINT SubmittedSpotLights = 0u;

        UINT ActivePointLights = 0u;
        UINT ActiveSpotLights = 0u;

        UINT PointShadowLights = 0u;
        UINT SpotShadowLights = 0u;
    };

    static const DebugStats& GetDebugStats()
    {
        return sDebugStats;
    }

private:
    //============================================================
    // Point / Spotを同じPriority Tableで扱うための種類
    //============================================================
    enum class CandidateType
    {
        Point,
        Spot
    };

    //============================================================
    // Priority計算後のLight参照
    // 実体そのものはコピーせず
    // 元Request配列のIndexだけ保持する
    //============================================================
    struct ScoredCandidate
    {
        CandidateType Type = CandidateType::Point;
        std::size_t Index = 0u;
        float Score = 0.0f;
        const RoomDefinition* LightRoom = nullptr;
    };

    struct PointShadowCandidate
    {
        //============================================================
        // sPointRequests内のIndex
        //============================================================
        std::size_t RequestIndex = 0u;

        //============================================================
        // Shadow選抜用Score
        // 通常Priority + 前フレーム継続Bonus
        //============================================================
        float Score = 0.0f;
    };

public:

    //========================================================
    // Frame開始
    //========================================================
    static void BeginFrame();


    //========================================================
    // Candidate登録
    //========================================================
    static void SubmitPointLight(const PointLightRequest& request);
    static void SubmitSpotLight(const SpotLightRequest& request);


    //========================================================
    // Candidateを評価して実際のLightへ変換する
    //========================================================
    static void ResolveAndRegister(
        const XMFLOAT3& playerPosition,
        const ExplorationLocationSystem& locationSystem
    );


private:

    //========================================================
    // Shadow Priority計算
    //========================================================
    static float CalculatePriority(
        const XMFLOAT3& lightPosition,
        float range,
        float priorityBias,
        bool forceShadow,
        const XMFLOAT3& playerPosition,
        const RoomDefinition* currentRoom,
        const ExplorationLocationSystem& locationSystem
    );

    //============================================================
    // 指定Lightが前フレームで
    // Point Shadowを持っていたか
    //============================================================
    static bool WasPreviousPointShadowLight(uint32_t stableLightId);

private:

    static std::vector<PointLightRequest> sPointRequests;

    static std::vector<SpotLightRequest> sSpotRequests;

    //============================================================
    // 前フレームでPoint Shadowに選ばれていたLight
    // 目的:
    //   前フレームからShadowを持っているLightへ
    //   少しだけScore Bonusを与え
    //   小さな距離変化だけでShadowが切り替わるのを防ぐ
    //============================================================
    inline static std::array<uint32_t, PointLightShadowManager::MaxShadowLights> sPreviousPointShadowIds = {};

    //============================================================
    // Shadow継続Bonus
    // Room変更など大きなPriority差が発生すれば
    // この値を超えて正常に入れ替わる
    //============================================================
    static constexpr float PointShadowRetentionBonus = 300.0f;

    static DebugStats sDebugStats;
};