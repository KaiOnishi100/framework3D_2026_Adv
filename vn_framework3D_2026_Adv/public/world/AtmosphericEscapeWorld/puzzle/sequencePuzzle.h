#pragma once

#include "puzzleBase.h"
#include "puzzleSwitch.h"

class vnScene;
class InteractionSystem;


//============================================================
// SequencePuzzle
// 複数のSwitchを決められた順番で操作するPuzzle
// Save:
//      State = Solved / Unsolved
//      Value = 現在の正解数
//============================================================
class SequencePuzzle : public PuzzleBase
{
public:

    using SolvedCallback = std::function<void(bool restored)>;


public:

    SequencePuzzle() = default;

    ~SequencePuzzle() override = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const std::vector<XMFLOAT3>& positions,
        const std::vector<XMFLOAT3>& rotations,
        const std::vector<std::wstring>& interactionTexts,
        const std::wstring& modelFolder,
        const std::vector<std::wstring>& modelFiles,
        const std::vector<std::uint32_t>& correctOrder
    );


    //========================================================
    // Terminate
    //========================================================
    void Terminate(
        vnScene* scene,
        InteractionSystem* interactionSystem
    );


    //========================================================
    // PuzzleSwitchから呼ばれる
    //========================================================
    void PressSwitch(std::uint32_t switchIndex);


    //========================================================
    // Reward Callback
    // restored:
    //      false = GameplayでSolve
    //      true  = Save LoadからSolve状態復元
    //========================================================
    void SetSolvedCallback(SolvedCallback callback)
    {
        mSolvedCallback = std::move(callback);
    }


    std::uint32_t GetProgress() const
    {
        return mProgress;
    }


    //========================================================
    // 現在のSwitch数
    //========================================================
    std::size_t GetSwitchCount() const
    {
        return mSwitches.size();
    }

    bool CanAcceptInput() const
    {
        return mInteractionEnabled && !IsSolved();
    }

    void SetInteractionEnabled(bool enabled)
    {
        mInteractionEnabled = enabled;
    }

protected:

    void CaptureCustomState(WorldObjectSaveState& data) const override;

    void RestoreCustomState(const WorldObjectSaveState& data) override;


    void OnSolved() override;

    void OnSolvedRestored() override;


private:

    void RefreshSwitchVisuals();


private:

    //========================================================
    // 正解順
    //========================================================
    std::vector<std::uint32_t> mCorrectOrder;


    //========================================================
    // 現在何個まで正解しているか
    //========================================================
    std::uint32_t mProgress = 0;


    //========================================================
    // Puzzle Switch
    //========================================================
    std::vector<std::unique_ptr<PuzzleSwitch>> mSwitches;

    //========================================================
    // Reward
    //========================================================
    SolvedCallback mSolvedCallback;

    bool mInteractionEnabled = true;
};