#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "sequencePuzzle.h"


bool SequencePuzzle::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const std::vector<XMFLOAT3>& positions,
    const std::vector<XMFLOAT3>& rotations,
    const std::vector<std::wstring>& interactionTexts,
    const std::wstring& modelFolder,
    const std::vector<std::wstring>& modelFiles,
    const std::vector<std::uint32_t>& correctOrder

)
{
    //========================================================
    // Safety
    //========================================================
    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }

    //========================================================
    // Switchが存在しないPuzzleは成立しない
    //========================================================
    if (positions.empty())
    {
        return false;
    }

    //========================================================
    // ※PositionとInteraction Textは
    // ※必ず同じ個数必要
    //========================================================
    if (positions.size() != interactionTexts.size())
    {
        return false;
    }

    //============================================================
    // ※Switchの数とモデル数は一致する必要がある
    //============================================================
    if (positions.size() != modelFiles.size())
    {
        return false;
    }

    //============================================================
    // ※正解順は空にできない
    //============================================================
    if (correctOrder.empty())
    {
        return false;
    }

    //============================================================
    // 正解Indexが有効で、同じスイッチを重複して
    // 登録していないことを確認する
    //============================================================
    std::vector<bool> used(positions.size(), false);

    for (std::uint32_t index : correctOrder)
    {
        if (index >= positions.size() || used[index])
        {
            return false;
        }

        used[index] = true;
    }

    //========================================================
    // Base
    //========================================================
    if (!PuzzleBase::Initialize(persistentId))
    {
        return false;
    }


    //========================================================
    // 初期状態
    //========================================================
    mProgress = 0;

    mCorrectOrder.clear();

    mCorrectOrder = correctOrder;

    mSwitches.clear();


    //========================================================
    // 再確保回数を減らす
    //========================================================
    mCorrectOrder.reserve(positions.size());
    mSwitches.reserve(positions.size());


    //========================================================
    // Switch生成
    // positionsの配列順を正解順として使用する
    //========================================================
    for (std::size_t i = 0; i < positions.size(); ++i)
    {
        //====================================================
        // Switch生成
        //====================================================
        auto puzzleSwitch = std::make_unique<PuzzleSwitch>();


        if (!puzzleSwitch->Initialize(
            scene,
            interactionSystem,
            this,
            static_cast<std::uint32_t>(i),
            positions[i],
            rotations[i],
            interactionTexts[i].c_str(),
            modelFolder,
            modelFiles[i]
        ))
        {
            //========================================================
            // 失敗時の既存クリーンアップ処理を維持する
            //========================================================
            for (auto& createdSwitch : mSwitches)
            {
                if (createdSwitch != nullptr)
                {
                    createdSwitch->Terminate(scene, interactionSystem);
                }
            }

            mSwitches.clear();
            mCorrectOrder.clear();

            return false;
        }

        mSwitches.push_back(std::move(puzzleSwitch));
    }


    //========================================================
    // 初期Visual
    //========================================================
    RefreshSwitchVisuals();

    return true;
}


//============================================================
// Terminate
//============================================================
void SequencePuzzle::Terminate(vnScene* scene,InteractionSystem* interactionSystem)
{
    for (auto& puzzleSwitch : mSwitches)
    {
        if (puzzleSwitch != nullptr)
        {
            puzzleSwitch->Terminate(scene, interactionSystem);
        }
    }

    mSwitches.clear();
    mCorrectOrder.clear();

    mSolvedCallback = nullptr;
    mProgress = 0;
}


//============================================================
// Press Switch
//============================================================
void SequencePuzzle::PressSwitch(std::uint32_t switchIndex)
{
    //========================================================
    // 解決済み
    //========================================================
    if (!CanAcceptInput())
    {
        return;
    }

    //========================================================
    // Puzzle自体にSwitchが存在しない
    //========================================================
    if (mSwitches.empty() || mCorrectOrder.empty())
    {
        return;
    }


    //========================================================
    // Invalid Index
    //========================================================
    if (switchIndex >= mSwitches.size())
    {
        return;
    }

    if (mProgress >= mCorrectOrder.size())
    {
        return;
    }

    //========================================================
    // 現在期待しているSwitch
    //========================================================
    const std::uint32_t expected = mCorrectOrder[mProgress];


    //========================================================
    // 正解
    //========================================================
    if (switchIndex == expected)
    {
        ++mProgress;


#if defined(_DEBUG)

        wchar_t buffer[128];

        swprintf_s(
            buffer,
            L"[SequencePuzzle] Correct. Progress = %u / %zu\n",
            mProgress,
            mCorrectOrder.size()
        );

        OutputDebugStringW(buffer);

#endif


        //====================================================
        // 全問正解
        //====================================================
        if (mProgress >= mCorrectOrder.size())
        {
            mProgress = static_cast<std::uint32_t>(mCorrectOrder.size());

            RefreshSwitchVisuals();

            SoundBank::getInstance()->PlaySfx(SoundID::AE_PuzzleSolved, 0.70f);

            //================================================
            // PuzzleBase側でSolvedへ変更
            //================================================
            Solve();

            return;
        }

        RefreshSwitchVisuals();

        SoundBank::getInstance()->PlaySfx(SoundID::AE_PuzzleInteract, 0.55f);

        return;
    }


    //========================================================
    // ※ただし押したSwitchが最初の正解なら
    // ※新しいSequence開始として扱える
    //========================================================
    if (switchIndex == mCorrectOrder[0])
    {
        mProgress = 1;
    }
    else
    {
        mProgress = 0;
    }


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[SequencePuzzle] "
        L"Wrong sequence. Reset.\n"
    );

#endif


    RefreshSwitchVisuals();

    SoundBank::getInstance()->PlaySfx(SoundID::AE_PuzzleError, 0.67f);
}


//============================================================
// Save Custom State
//============================================================
void SequencePuzzle::CaptureCustomState(WorldObjectSaveState& data) const
{
    //========================================================
    // Valueへ現在Progressを保存
    //
    // 0 / 1 / 2 / 3
    //========================================================
    data.Value = static_cast<float>(mProgress);
}


//============================================================
// Load Custom State
//============================================================
void SequencePuzzle::RestoreCustomState(const WorldObjectSaveState& data)
{
    //========================================================
    // Save値を整数へ戻す
    //========================================================
    int progress =
        static_cast<int>(std::lround(data.Value));


    progress = std::clamp(progress, 0, static_cast<int>(mCorrectOrder.size()));

    mProgress = static_cast<std::uint32_t>(progress);

    //========================================================
    // Load直後のSwitch Visualを復元
    //========================================================
    RefreshSwitchVisuals();
}


//============================================================
// GameplayでSolved
//============================================================
void SequencePuzzle::OnSolved()
{
    mProgress = static_cast<std::uint32_t>(mCorrectOrder.size());

    RefreshSwitchVisuals();

    //========================================================
    // Reward
    //========================================================
    if (mSolvedCallback)
    {
        mSolvedCallback(false);
    }


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[SequencePuzzle] "
        L"Puzzle solved.\n"
    );

#endif
}


//============================================================
// SaveからSolved復元
//============================================================
void SequencePuzzle::OnSolvedRestored()
{
    mProgress = static_cast<std::uint32_t>(mCorrectOrder.size());

    RefreshSwitchVisuals();

    //========================================================
    // Reward状態も復元
    // restored=trueなので
    // 呼び出し側はSEや演出を鳴らさず
    // Door Unlockなど状態だけ反映できる
    //========================================================
    if (mSolvedCallback)
    {
        mSolvedCallback(true);
    }
}


//============================================================
// Visual Sync
//============================================================
void SequencePuzzle::RefreshSwitchVisuals()
{
    //========================================================
    // 一旦すべて未入力へ戻す
    //========================================================
    for (auto& puzzleSwitch : mSwitches)
    {
        if (puzzleSwitch != nullptr)
        {
            puzzleSwitch->SetAccepted(false);
        }
    }


    //========================================================
    // 現在までの正解数
    //========================================================
    const std::uint32_t count =
        (std::min)(mProgress, static_cast<std::uint32_t>(mCorrectOrder.size()));


    //========================================================
    // 正解済みSwitchのみAcceptedへ
    //========================================================
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const std::uint32_t switchIndex = mCorrectOrder[i];

        if (switchIndex >= mSwitches.size())
        {
            continue;
        }

        if (mSwitches[switchIndex] != nullptr)
        {
            mSwitches[switchIndex]->SetAccepted(true);
        }
    }
}