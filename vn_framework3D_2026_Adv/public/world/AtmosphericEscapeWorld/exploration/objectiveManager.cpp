#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "objectiveManager.h"


//============================================================
// Initialize
//============================================================
void ObjectiveManager::Initialize()
{
    mObjectives.clear();

    mCurrentObjectiveId.clear();
}


//============================================================
// Terminate
//============================================================
void ObjectiveManager::Terminate()
{
    mObjectives.clear();

    mCurrentObjectiveId.clear();
}


//============================================================
// Register
//============================================================
bool ObjectiveManager::RegisterObjective(const ExplorationObjective& objective)
{
    if (objective.Id.empty())
    {
        return false;
    }

    const auto [iterator, inserted] = mObjectives.emplace(objective.Id, objective);


#if defined(_DEBUG)

    if (!inserted)
    {
        std::string message =
            "[ObjectiveManager] Duplicate ID : " +
            objective.Id +
            "\n";

        OutputDebugStringA(message.c_str());
    }

#endif

    return inserted;
}


//============================================================
// Set Current
//============================================================
bool ObjectiveManager::SetCurrentObjective(const std::string& objectiveId)
{
    const auto found = mObjectives.find(objectiveId);


    if (found == mObjectives.end())
    {
#if defined(_DEBUG)

        std::string message =
            "[ObjectiveManager] Objective not found : " +
            objectiveId +
            "\n";

        OutputDebugStringA(message.c_str());

#endif

        return false;
    }


    //========================================================
    // 同じObjectiveなら何もしない
    //========================================================
    if (mCurrentObjectiveId == objectiveId)
    {
        return true;
    }


    mCurrentObjectiveId = objectiveId;


#if defined(_DEBUG)

    std::string message =
        "[ObjectiveManager] Current Objective : " +
        objectiveId +
        "\n";

    OutputDebugStringA(message.c_str());

#endif


    return true;
}

//============================================================
// Clear
//============================================================
void ObjectiveManager::ClearCurrentObjective()
{
    mCurrentObjectiveId.clear();
}

//============================================================
// Get Current
//============================================================
const ExplorationObjective* ObjectiveManager::GetCurrentObjective() const
{
    if (mCurrentObjectiveId.empty())
    {
        return nullptr;
    }

    const auto found = mObjectives.find(mCurrentObjectiveId);

    if (found == mObjectives.end())
    {
        return nullptr;
    }

    return &found->second;
}