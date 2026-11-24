#include "../../../framework.h"
#include "../../vn_environment.h"

#include "AtmosphericEscapeLightBudget.h"
#include "../../../public/world/AtmosphericEscapeWorld/exploration/explorationLocationSystem.h"

std::vector<AtmosphericEscapeLightBudget::PointLightRequest> AtmosphericEscapeLightBudget::sPointRequests = {};
std::vector<AtmosphericEscapeLightBudget::SpotLightRequest> AtmosphericEscapeLightBudget::sSpotRequests = {};
AtmosphericEscapeLightBudget::DebugStats AtmosphericEscapeLightBudget::sDebugStats = {};

void AtmosphericEscapeLightBudget::BeginFrame()
{
    sPointRequests.clear();
    sSpotRequests.clear();
    sDebugStats = {};
}


void AtmosphericEscapeLightBudget::SubmitPointLight(const PointLightRequest& request)
{
    sPointRequests.push_back(request);
    ++sDebugStats.SubmittedPointLights;
}


void AtmosphericEscapeLightBudget::SubmitSpotLight(const SpotLightRequest& request)
{
    sSpotRequests.push_back(request);
    ++sDebugStats.SubmittedSpotLights;
}

void AtmosphericEscapeLightBudget::ResolveAndRegister(const XMFLOAT3& playerPosition, const ExplorationLocationSystem& locationSystem)
{
    //============================================================
    // Playerが現在いるRoom
    //============================================================
    const RoomDefinition* currentRoom = locationSystem.GetCurrentRoom();

    const auto canRegisterInCurrentRoom = [&](bool limitToCurrentRoom, const XMFLOAT3& lightPosition)
        {
            if (!limitToCurrentRoom)
            {
                return true;
            }

            // 固定室内灯は部屋を特定できないフレームでも建物全体へ漏らさない
            if (currentRoom == nullptr)
            {
                return false;
            }

            const RoomDefinition* lightRoom = locationSystem.FindRoomAtPosition(lightPosition);

            return lightRoom != nullptr && lightRoom->Id == currentRoom->Id;
        };

    //============================================================
    // Point / SpotをまとめたPriority Table
    //============================================================
    std::vector<ScoredCandidate> candidates;

    candidates.reserve(sPointRequests.size() + sSpotRequests.size());


    //============================================================
    // Point Light Priority
    //============================================================
    for (std::size_t index = 0u; index < sPointRequests.size(); ++index)
    {
        const PointLightRequest& light = sPointRequests[index];

        const RoomDefinition* lightRoom = light.LimitToRoomBounds
            ? locationSystem.FindRoomAtPosition(light.Position) : nullptr;

        // 制限対象の部屋を特定できないときこのライトを候補から外す
        if (light.LimitToRoomBounds && lightRoom == nullptr)
        {
            continue;
        }

        const float score =
            CalculatePriority(
                light.Position,
                light.Range,
                light.PriorityBias,
                false,
                playerPosition,
                currentRoom,
                locationSystem
            );


        candidates.push_back(
            {
                CandidateType::Point,
                index,
                score,
                lightRoom
            }
        );
    }


    //============================================================
    // Spot Light Priority
    //============================================================
    for (std::size_t index = 0u; index < sSpotRequests.size(); ++index)
    {
        const SpotLightRequest& light = sSpotRequests[index];

        const RoomDefinition* lightRoom = light.LimitToRoomBounds
            ? locationSystem.FindRoomAtPosition(light.Position) : nullptr;

        // 制限対象の部屋を特定できないときはこのライトを候補から外す
        if (light.LimitToRoomBounds && lightRoom == nullptr)
        {
            continue;
        }

        const float score =
            CalculatePriority(
                light.Position,
                light.Range,
                light.PriorityBias,
                light.ForceShadow,
                playerPosition,
                currentRoom,
                locationSystem
            );


        candidates.push_back(
            {
                CandidateType::Spot,
                index,
                score,
                lightRoom
            }
        );
    }


    //============================================================
    // Priority降順
    // stable_sortにしておくことで
    // 同一Scoreなら元の登録順をなるべく維持する
    // Shadow Slotの入れ替わりを抑えられる
    //============================================================
    std::stable_sort(candidates.begin(), candidates.end(), [](const ScoredCandidate& a, const ScoredCandidate& b)
        {
            return a.Score > b.Score;
        }
    );


    //============================================================
    // 通常Lightingに使用するLight
    // Shadowとは別
    // LocalLightManagerへ登録できる数だけを選択する
    //============================================================
    std::vector<bool> activePointLights(sPointRequests.size(), false);

    std::vector<bool> activeSpotLights(sSpotRequests.size(), false);

    UINT activeLocalLightCount = 0u;

    for (const ScoredCandidate& candidate : candidates)
    {

        //========================================================
        // GPUへ送れるLocal Light上限に到達
        //========================================================
        if (activeLocalLightCount >= LocalLightManager::MaxLocalLights)
        {
            break;
        }


        switch (candidate.Type)
        {
        case CandidateType::Point:
        {
            activePointLights[candidate.Index] = true;

            ++sDebugStats.ActivePointLights;

            break;
        }


        case CandidateType::Spot:
        {
            activeSpotLights[candidate.Index] = true;

            ++sDebugStats.ActiveSpotLights;

            break;
        }
        }


        ++activeLocalLightCount;
    }


    //============================================================
    // Point Shadow選択結果
    //============================================================
    std::vector<bool> pointShadowLights(sPointRequests.size(), false);


    //============================================================
    // Shadow専用候補
    // 通常Lightingとは別にScoreを作る
    //============================================================
    std::vector<PointShadowCandidate> pointShadowCandidates;

    pointShadowCandidates.reserve(sPointRequests.size());


    //============================================================
    // Active Point Lightの中から
    // Shadow候補を作る
    //============================================================
    for (std::size_t pointIndex = 0u; pointIndex < sPointRequests.size(); ++pointIndex)
    {
        const PointLightRequest& light = sPointRequests[pointIndex];

        //========================================================
        // Local Lightとして選ばれていないLightには
        // Shadowを与えない
        //========================================================
        if (!activePointLights[pointIndex])
        {
            continue;
        }

        //========================================================
        // Shadowを要求していない
        //========================================================
        if (!light.CastsShadow)
        {
            continue;
        }


        //========================================================
        // 元のPriorityを取得
        // CalculatePriorityを再利用する
        //========================================================
        float shadowScore =
            CalculatePriority(
                light.Position,
                light.Range,
                light.PriorityBias,
                false,
                playerPosition,
                currentRoom,
                locationSystem
            );


        //========================================================
        // 前フレームもShadow付きだった場合
        // 少しだけBonusを加える
        //========================================================
        if (WasPreviousPointShadowLight(light.StableLightId))
        {
            shadowScore += PointShadowRetentionBonus;
        }

        pointShadowCandidates.push_back({ pointIndex, shadowScore });
    }


    //============================================================
    // Shadow Score降順
    //============================================================
    std::stable_sort(pointShadowCandidates.begin(), pointShadowCandidates.end(), [](const PointShadowCandidate& a, const PointShadowCandidate& b)
        {
            return a.Score > b.Score;
        }
    );


    //============================================================
    // 最大4灯選択
    //============================================================
    const std::size_t pointShadowCount =
        (std::min)(pointShadowCandidates.size(), static_cast<std::size_t>(PointLightShadowManager::MaxShadowLights));


    for (std::size_t selectionIndex = 0u; selectionIndex < pointShadowCount; ++selectionIndex)
    {
        const std::size_t requestIndex = pointShadowCandidates[selectionIndex].RequestIndex;

        pointShadowLights[requestIndex] = true;

        ++sDebugStats.PointShadowLights;
    }

    //============================================================
    // Point Shadow登録順
    // 前フレームから残っているLightは
    // 前フレームのShadow Slot順をなるべく維持する
    // 新しくShadow対象になったLightは
    // その後ろへPriority順で追加する
    //============================================================
    std::vector<std::size_t> orderedPointShadowRequests;

    orderedPointShadowRequests.reserve(PointLightShadowManager::MaxShadowLights);

    //============================================================
    // 前フレームにも存在したShadow Lightを
    // 前フレームのSlot順に並べる
    //============================================================
    for (UINT previousSlot = 0u; previousSlot < PointLightShadowManager::MaxShadowLights; ++previousSlot )
    {
        const uint32_t previousStableId =
            sPreviousPointShadowIds[previousSlot];

        //========================================================
        // 前フレームでこのSlotが未使用だった
        //========================================================
        if (previousStableId == 0u)
        {
            continue;
        }

        //========================================================
        // 今フレーム選ばれたPoint Shadow候補から
        // 同じStableLightIdを探す
        //========================================================
        for (std::size_t selectionIndex = 0u; selectionIndex < pointShadowCount; ++selectionIndex)
        {
            const std::size_t requestIndex =
                pointShadowCandidates[selectionIndex].RequestIndex;

            const PointLightRequest& light = sPointRequests[requestIndex];

            //====================================================
            // 前フレームのLightとは別物
            //====================================================
            if (light.StableLightId != previousStableId)
            {
                continue;
            }

            //====================================================
            // 重複登録を防止
            //====================================================
            const bool alreadyRegistered =
                std::find(orderedPointShadowRequests.begin(), orderedPointShadowRequests.end(), requestIndex)
                != orderedPointShadowRequests.end();


            if (alreadyRegistered)
            {
                continue;
            }

            //====================================================
            // 前フレームと同じ相対位置を維持する
            //====================================================
            orderedPointShadowRequests.push_back(requestIndex);

            break;
        }
    }


    //============================================================
    // 今フレーム新しくShadow対象になったLightを
    // Priority順に末尾へ追加する
    //============================================================
    for (std::size_t selectionIndex = 0u; selectionIndex < pointShadowCount; ++selectionIndex)
    {
        const std::size_t requestIndex =
            pointShadowCandidates[selectionIndex].RequestIndex;


        //========================================================
        // 登録済みなら何もしない
        //========================================================
        const bool alreadyRegistered =
            std::find(orderedPointShadowRequests.begin(), orderedPointShadowRequests.end(), requestIndex)
            != orderedPointShadowRequests.end();

        if (alreadyRegistered)
        {
            continue;
        }

        //========================================================
        // 新規Shadow Light
        //========================================================
        orderedPointShadowRequests.push_back(requestIndex);
    }

    //============================================================
    // Spot Shadow対象
    //============================================================
    std::vector<bool> spotShadowLights(sSpotRequests.size(), false);

    UINT spotShadowCount = 0u;

    for (const ScoredCandidate& candidate : candidates)
    {
        if (spotShadowCount >= SpotLightShadowManager::MaxShadowLights)
        {
            break;
        }


        if (candidate.Type != CandidateType::Spot)
        {
            continue;
        }


        if (!activeSpotLights[candidate.Index])
        {
            continue;
        }

        const SpotLightRequest& light = sSpotRequests[candidate.Index];

        if (!light.CastsShadow)
        {
            continue;
        }


        spotShadowLights[candidate.Index] = true;

        ++spotShadowCount;

        ++sDebugStats.SpotShadowLights;
    }

    //============================================================
    // Point Light RequestごとのShadow Index
    // -1: Shadowなし
    // 0～3: PointLightShadowManagerの実際のShadow Slot
    //============================================================
    std::vector<int> pointShadowIndices(sPointRequests.size(), -1);


    //============================================================
    // Point Shadowを登録する
    //============================================================
    for (std::size_t orderIndex = 0u; orderIndex < orderedPointShadowRequests.size(); ++orderIndex)
    {
        const std::size_t requestIndex = orderedPointShadowRequests[orderIndex];

        const PointLightRequest& light = sPointRequests[requestIndex];

        //========================================================
        // Point Shadow Managerへ登録
        //========================================================
        const int shadowIndex =
            PointLightShadowManager::AddLight(
                light.Position,
                light.Range,
                1.0f,
                0.0003f,
                0.0020f,
                light.UpdateMode,
                light.Resolution,
                light.StableLightId,
                light.ExcludedShadowCaster
            );



        //========================================================
        // 後のLocalLight登録時に使うため
        // Request IndexごとにShadow Indexを保存する
        //========================================================
        pointShadowIndices[requestIndex] = shadowIndex;


#if defined(_DEBUG)

        //========================================================
        // Shadow Slotが安定しているか確認するログ
        //========================================================
        //wchar_t buffer[256] = {};

        //swprintf_s(
        //    buffer,
        //    L"[LightBudget] "
        //    L"PointOrder=%zu "
        //    L"StableID=%u "
        //    L"ShadowIndex=%d\n",

        //    orderIndex,
        //    light.StableLightId,
        //    shadowIndex
        //);

        //OutputDebugStringW(buffer);

#endif
    }

    //============================================================
    // 実際のManagerへ登録
    // Priority順に登録する
    //============================================================
    for (const ScoredCandidate& candidate : candidates)
    {
        LocalLightRoomBounds roomBounds{};

        if (const RoomDefinition* room = candidate.LightRoom)
        {
            // 描画する表面や霧のサンプルがこのXYZ範囲にあるかをGPUで判定する
            roomBounds.Min = XMFLOAT4(
                room->MinBounds.x, room->MinBounds.y, room->MinBounds.z, 1.0f
            );
            roomBounds.Max = XMFLOAT4(
                room->MaxBounds.x, room->MaxBounds.y, room->MaxBounds.z, 0.0f
            );
        }


        //========================================================
        // Point
        //========================================================
        if (candidate.Type == CandidateType::Point)
        {
            //========================================================
            // 通常Lighting対象でないPoint Lightは登録しない
            //========================================================
            if (!activePointLights[candidate.Index])
            {
                continue;
            }

            const PointLightRequest& light = sPointRequests[candidate.Index];

            //========================================================
            // すでにPointLightShadowManagerへ登録済み
            // Shadow対象: 0～3
            // Shadow対象外: -1
            //========================================================
            const int shadowIndex = pointShadowIndices[candidate.Index];

            //========================================================
            // Local Light本体を登録
            //========================================================
            LocalLightManager::AddPointLight(
                light.Position,
                light.Range,
                light.Color,
                light.Intensity,
                shadowIndex,
                roomBounds
            );

            continue;
        }



        if (candidate.Type == CandidateType::Spot)
        {
            //============================================================
            // 通常Lighting対象として選ばれていないSpotLightは除外
            //============================================================
            if (!activeSpotLights[candidate.Index])
            {
                continue;
            }

            //============================================================
            // Spot Light Request取得
            // ExcludedShadowCasterも保持されている
            //============================================================
            const SpotLightRequest& light = sSpotRequests[candidate.Index];

            //============================================================
            // Shadow Index
            // -1: Shadowなし
            // 0以上: SpotLightShadowManagerが割り当てた実際のSlice
            //============================================================
            int shadowIndex = -1;

            //============================================================
            // Shadow Budgetで選ばれたSpotLightのみ
            // Shadow Mapを登録する
            //============================================================
            if (spotShadowLights[candidate.Index])
            {
                //========================================================
                // Shadow Map登録
                //========================================================
                shadowIndex =
                    SpotLightShadowManager::AddLight(
                        light.Position,
                        light.Direction,
                        light.Range,
                        light.OuterAngleDegrees,
                        light.PCFRadius,
                        light.MinimumBias,
                        light.SlopeBias
                    );

                //========================================================
                // SpotLight専用Shadow Caster Exclusion
                // このSpotLightのShadow Mapから除外する
                // Objectが設定されている場合だけ処理する
                // Player Flashlightの場合: Player Meshを除外
                // 他のSpotLightの場合: nullptrなら従来どおり全Casterを描画
                //========================================================
                if (shadowIndex >= 0 && light.ExcludedShadowCaster != nullptr)
                {
                    SpotLightShadowManager::SetExcludedCaster(
                        static_cast<UINT>(shadowIndex),
                        light.ExcludedShadowCaster
                    );

#if defined(_DEBUG)
                    //====================================================
                    // Shadow除外設定が正しいSliceに登録されたか確認
                    //====================================================
                    static bool debugPrinted = false;

                    if (!debugPrinted)
                    {
                        wchar_t buffer[256] = {};

                        swprintf_s(
                            buffer,
                            L"[LightBudget] "
                            L"Spot Shadow Exclusion Registered "
                            L"ShadowIndex=%d\n",
                            shadowIndex
                        );

                        OutputDebugStringW(buffer);

                        debugPrinted = true;
                    }
#endif
                }
            }

            //============================================================
            // 通常SpotLightの登録
            // Shadow IndexをLocalLightManagerへ渡す
            //============================================================
            LocalLightManager::AddSpotLight(
                light.Position,
                light.Direction,
                light.Range,
                light.Color,
                light.Intensity,
                light.InnerAngleDegrees,
                light.OuterAngleDegrees,
                shadowIndex,
                roomBounds
            );
        }
    }

    //============================================================
    // 次フレーム用Point Shadow Slot履歴
    //============================================================
    sPreviousPointShadowIds.fill(0u);

    for (std::size_t shadowIndex = 0u; shadowIndex < orderedPointShadowRequests.size(); ++shadowIndex)
    {
        const std::size_t requestIndex = orderedPointShadowRequests[shadowIndex];

        //========================================================
        // 配列Index = 実際のShadowIndex
        //========================================================
        sPreviousPointShadowIds[shadowIndex] = sPointRequests[requestIndex].StableLightId;
    }
}

float AtmosphericEscapeLightBudget::CalculatePriority(
    const XMFLOAT3& lightPosition,
    float range,
    float priorityBias,
    bool forceShadow,
    const XMFLOAT3& playerPosition,
    const RoomDefinition* currentRoom,
    const ExplorationLocationSystem& locationSystem
)
{
    float score = priorityBias;

    //========================================================
    // Flashlightなど強制Shadow対象
    // 他の環境Lightがどれだけ近くても、
    // 基本的にこちらを優先する
    //========================================================
    if (forceShadow)
    {
        score += 1000000.0f;
    }


    //========================================================
    // Lightが所属しているRoomを取得
    //========================================================
    const RoomDefinition* lightRoom =
        locationSystem.FindRoomAtPosition(lightPosition);


    if (currentRoom != nullptr && lightRoom != nullptr)
    {
        //====================================================
        // Playerと同じRoom
        // 最重要
        //====================================================
        if (lightRoom->Id == currentRoom->Id)
        {
            score += 10000.0f;
        }

        //====================================================
        // 同じArea + Floor
        //====================================================
        else if (lightRoom->Area == currentRoom->Area && lightRoom->Floor == currentRoom->Floor)
        {
            score += 2500.0f;
        }

        //====================================================
        // 同じ建物
        //====================================================
        else if (lightRoom->Area == currentRoom->Area)
        {
            score += 500.0f;
        }
    }


    //========================================================
    // Playerとの距離
    //========================================================
    const float dx =
        lightPosition.x - playerPosition.x;

    const float dy =
        lightPosition.y - playerPosition.y;

    const float dz =
        lightPosition.z - playerPosition.z;

    const float distance =
        sqrtf(dx * dx + dy * dy + dz * dz);

    if (distance <= range)
    {
        score += 1000.0f;
    }


    //========================================================
    // 遠いほどPriorityを下げる
    //========================================================
    score -= distance * 20.0f;


    return score;
}

bool AtmosphericEscapeLightBudget::WasPreviousPointShadowLight(uint32_t stableLightId)
{
    //============================================================
    // 0はInvalid Stable IDとして扱う
    //============================================================
    if (stableLightId == 0u)
    {
        return false;
    }

    //============================================================
    // 前フレームの4Slotを検索
    //============================================================
    for (const uint32_t previousId : sPreviousPointShadowIds)
    {
        if (previousId == stableLightId)
        {
            return true;
        }
    }
    return false;
}
