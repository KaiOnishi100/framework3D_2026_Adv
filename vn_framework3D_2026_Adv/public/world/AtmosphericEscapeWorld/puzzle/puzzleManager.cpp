#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "puzzleManager.h"
#include "puzzleBase.h"


bool PuzzleManager::Register(PuzzleBase* puzzle)
{
    if (puzzle == nullptr)
    {
        return false;
    }


    const std::string& id = puzzle->GetPersistentId();


    if (id.empty())
    {
        return false;
    }


    //========================================================
    // ID重複は禁止
    //========================================================
    const auto [it, inserted] = mPuzzles.emplace(id, puzzle);


#if defined(_DEBUG)

    if (!inserted)
    {
        std::string message =
            "[PuzzleManager] Duplicate Puzzle ID : " +
            id +
            "\n";


        OutputDebugStringA(message.c_str());
    }

#endif


    return inserted;
}


void PuzzleManager::Clear()
{
    mPuzzles.clear();
}


PuzzleBase* PuzzleManager::Find(const std::string& id)
{
    const auto found = mPuzzles.find(id);

    if (found == mPuzzles.end())
    {
        return nullptr;
    }

    return found->second;
}


const PuzzleBase* PuzzleManager::Find(const std::string& id) const
{
    const auto found = mPuzzles.find(id);

    if (found == mPuzzles.end())
    {
        return nullptr;
    }

    return found->second;
}


bool PuzzleManager::IsSolved(const std::string& id) const
{
    const PuzzleBase* puzzle = Find(id);

    if (puzzle == nullptr)
    {
        return false;
    }

    return puzzle->IsSolved();
}