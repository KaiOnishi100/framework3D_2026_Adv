#pragma once

#include "../../../interaction/IInteractable.h"


class vnScene;
class InteractionSystem;
class ExplorationPlayer;
class SequencePuzzle;

//============================================================
// PuzzleSwitch
// SequencePuzzleを構成する1個の操作スイッチ
//============================================================
class PuzzleSwitch :public IInteractable
{
public:

    PuzzleSwitch() = default;

    ~PuzzleSwitch() override = default;


    //========================================================
    // Initialize
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        SequencePuzzle* ownerPuzzle,
        std::uint32_t switchIndex,
        const XMFLOAT3& position,
        const XMFLOAT3& rotation,
        const wchar_t* interactionText,
        const std::wstring& modelFolder,
        const std::wstring& modelFile
    );


    //========================================================
    // Terminate
    //========================================================
    void Terminate(vnScene* scene, InteractionSystem* interactionSystem);


    //========================================================
    // Visual
    // true: 正解Sequenceとして入力済み
    // false: 未入力
    //========================================================
    void SetAccepted(bool accepted);


public:

    //========================================================
    // IInteractable
    //========================================================
    XMVECTOR GetInteractionPosition() const override;

    bool CanInteract() const override;

    void Interact(ExplorationPlayer& player) override;

    const wchar_t* GetInteractionText() const override;


private:

    //========================================================
    // 親Puzzle
    // 所有権なし
    // SequencePuzzle側が先に存在する
    //========================================================
    SequencePuzzle* mOwnerPuzzle = nullptr;

    //========================================================
    // Switch Index
    //
    // 0 = Moon
    // 1 = Sun
    // 2 = Bird
    //========================================================
    std::uint32_t mSwitchIndex = 0;

    //========================================================
    // Model
    //========================================================
    ModelBase* mModel = nullptr;

    //========================================================
    // Interaction
    //========================================================
    XMFLOAT3 mPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    std::wstring mInteractionText;

    //========================================================
    // Visual State
    //========================================================
    bool mAccepted = false;
};