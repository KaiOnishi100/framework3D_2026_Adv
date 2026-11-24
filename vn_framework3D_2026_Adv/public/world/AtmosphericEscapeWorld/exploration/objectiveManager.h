#pragma once

#include "explorationObjective.h"

//============================================================
// ObjectiveManager
// 現在のObjectiveを一元管理する
//============================================================
class ObjectiveManager
{
public:

    ObjectiveManager() = default;

    ~ObjectiveManager() = default;


    //========================================================
    // Initialize
    //========================================================
    void Initialize();


    //========================================================
    // Terminate
    //========================================================
    void Terminate();


    //========================================================
    // Definition登録
    //========================================================
    bool RegisterObjective(const ExplorationObjective& objective);

    //========================================================
    // Current Objective変更
    //========================================================
    bool SetCurrentObjective(const std::string& objectiveId);


    //========================================================
    // Clear
    //========================================================
    void ClearCurrentObjective();


    //========================================================
    // Current Objective
    //========================================================
    const ExplorationObjective* GetCurrentObjective() const;

    const std::string& GetCurrentObjectiveId() const
    {
        return mCurrentObjectiveId;
    }

private:

    std::unordered_map<std::string, ExplorationObjective> mObjectives;

    std::string  mCurrentObjectiveId;
};