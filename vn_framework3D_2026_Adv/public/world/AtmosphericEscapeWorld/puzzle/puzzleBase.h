#pragma once


//============================================================
// Puzzle State
//============================================================
enum class PuzzleState : std::uint32_t
{
    //========================================================
    // まだ解かれていない
    //========================================================
    Unsolved = 0,


    //========================================================
    // 解決済み
    //========================================================
    Solved
};


//============================================================
// PuzzleBase
// AtmosphericEscape内の謎解き共通基底
// PuzzleBase自身はIInteractableにしない
// 理由:
// ・1つのPuzzleに複数の操作Objectが存在できる
// ・自動判定型Puzzleも作れる
// ・UI型Puzzleも作れる
// Save / LoadについてはPuzzleそのものがPersistent Objectになる
//============================================================
class PuzzleBase :
    public IPersistentWorldObject
{
public:

    PuzzleBase() = default;

    virtual ~PuzzleBase() = default;


public:

    //========================================================
    // Initialize
    //========================================================
    virtual bool Initialize(const std::string& persistentId);


    //========================================================
    // Update
    //========================================================
    virtual void Update(float dt);


    //========================================================
    // State
    //========================================================
    PuzzleState GetPuzzleState() const
    {
        return mState;
    }


    bool IsSolved() const
    {
        return mState == PuzzleState::Solved;
    }


    //========================================================
    // Persistent Object
    //========================================================
    const std::string&
        GetPersistentId() const override
    {
        return mPersistentId;
    }


    WorldObjectSaveState CaptureSaveState() const override;


    void RestoreSaveState(const WorldObjectSaveState& data) override;


protected:

    //========================================================
    // Puzzle解決
    // ※派生Puzzleは正解条件を満たしたらこれを呼ぶ
    //========================================================
    void Solve();


    //========================================================
    // Load用
    //========================================================
    void SetSolvedImmediate(bool solved);


protected:

    //========================================================
    // Puzzle固有Save
    // 例えば:
    // DialPuzzle  現在のDial番号
    // StatuePuzzle 現在の像向き
    // SequencePuzzle 現在何番目まで正解しているか
    // 基底SaveのState / Flags / Valueを必要に応じて
    // 派生側で拡張する
    //========================================================
    virtual void CaptureCustomState( WorldObjectSaveState& data) const
    {
        (void)data;
    }


    virtual void RestoreCustomState(const WorldObjectSaveState& data)
    {
        (void)data;
    }


    //========================================================
    // 解決した瞬間
    // Door Unlockや演出などは派生側またはWorld側で処理
    //========================================================
    virtual void OnSolved(){}

    //========================================================
    // LoadでSolvedへ復元された瞬間
    // SE・Noiseなどは発生させない
    //========================================================
    virtual void OnSolvedRestored(){}


private:

    std::string mPersistentId;

    PuzzleState mState =  PuzzleState::Unsolved;
};