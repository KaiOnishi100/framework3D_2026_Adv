#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "puzzleBase.h"


//============================================================
// Initialize
//============================================================
bool PuzzleBase::Initialize(const std::string& persistentId)
{
    //========================================================
    // PuzzleはSave / Load対象なのでID必須
    //========================================================
    if (persistentId.empty())
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[PuzzleBase] "
            L"Persistent ID is empty.\n"
        );

#endif

        return false;
    }

    mPersistentId = persistentId;

    //========================================================
    // Initial State
    //========================================================
    mState = PuzzleState::Unsolved;

    return true;
}


//============================================================
// Update
//============================================================
void PuzzleBase::Update(float dt)
{
    (void)dt;
}


//============================================================
// Solve
//============================================================
void PuzzleBase::Solve()
{
    //========================================================
    // 二重Solve防止
    //========================================================
    if (mState == PuzzleState::Solved)
    {
        return;
    }

    //========================================================
    // Solved
    //========================================================
    mState = PuzzleState::Solved;


    //========================================================
    // 派生Puzzle側の処理
    //========================================================
    OnSolved();


#if defined(_DEBUG)

    std::string message =
        "[PuzzleBase] Solved : " +
        mPersistentId +
        "\n";

    OutputDebugStringA(message.c_str());

#endif
}


//============================================================
// Save
//============================================================
WorldObjectSaveState PuzzleBase::CaptureSaveState() const
{
    WorldObjectSaveState data;

    data.Id = mPersistentId;
    data.Type = PersistentObjectType::Puzzle;


    //========================================================
    // Base State
    //
    // 0 = Unsolved
    // 1 = Solved
    //========================================================
    data.State = IsSolved() ? 1u : 0u;


    //========================================================
    // 派生Puzzle固有データ
    //========================================================
    CaptureCustomState(data);


    return data;
}


//============================================================
// Load
//============================================================
void PuzzleBase::RestoreSaveState(const WorldObjectSaveState& data)
{
    //========================================================
    // ID Check
    //========================================================
    if (data.Id != mPersistentId)
    {
        return;
    }


    //========================================================
    // Puzzle固有状態を先に戻す
    //========================================================
    RestoreCustomState(data);

    //========================================================
    // Base State
    //========================================================
    SetSolvedImmediate(data.State == 1u);
}


//============================================================
// Load用 即時状態設定
//============================================================
void PuzzleBase::SetSolvedImmediate(bool solved)
{
    mState = solved ? PuzzleState::Solved : PuzzleState::Unsolved;


    //========================================================
    // Load時にSolved状態なら、
    // 見た目やRewardの派生状態だけ再構築する
    //========================================================
    if (solved)
    {
        OnSolvedRestored();
    }
}