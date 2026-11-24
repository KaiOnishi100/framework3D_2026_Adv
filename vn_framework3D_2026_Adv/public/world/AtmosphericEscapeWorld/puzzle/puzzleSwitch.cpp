#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "puzzleSwitch.h"
#include "sequencePuzzle.h"

//============================================================
// Initialize
//============================================================
bool PuzzleSwitch::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    SequencePuzzle* ownerPuzzle,
    std::uint32_t switchIndex,
    const XMFLOAT3& position,
    const XMFLOAT3& rotation,
    const wchar_t* interactionText,
    const std::wstring& modelFolder,
    const std::wstring& modelFile
)
{
    //========================================================
    // Safety
    //========================================================
    if (scene == nullptr || interactionSystem == nullptr || ownerPuzzle == nullptr)
    {
        return false;
    }


    //========================================================
    // Owner
    //========================================================
    mOwnerPuzzle = ownerPuzzle;

    mSwitchIndex = switchIndex;

    mPosition = position;


    if (interactionText != nullptr)
    {
        mInteractionText = interactionText;
    }
    else
    {
        mInteractionText = L"Press Switch";
    }


    //========================================================
    // Model
    //========================================================
    mModel = new ModelBase(modelFolder.c_str(), modelFile.c_str());


    if (mModel == nullptr)
    {
        return false;
    }


    //========================================================
    // Transform
    //========================================================
    mModel->setPosition(position.x, position.y, position.z);

    mModel->setRotation(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );

    mModel->setScale(1.0f, 1.0f, 1.0f);

    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);
    mModel->SetShadowMobility(ShadowMobility::Static);

    //========================================================
    // 初期Visual
    //========================================================
    SetAccepted(false);

    //========================================================
    // Scene
    //========================================================
    scene->registerObject(mModel);

    //========================================================
    // Interaction
    //========================================================
    interactionSystem->Register(this);

    return true;
}


//============================================================
// Terminate
//============================================================
void PuzzleSwitch::Terminate( vnScene* scene, InteractionSystem* interactionSystem)
{
    //========================================================
    // Interactionから先に外す
    //========================================================
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister( this );
    }


    //========================================================
    // Model
    //========================================================
    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject( mModel);
        mModel = nullptr;
    }

    mOwnerPuzzle = nullptr;
    mAccepted = false;
}


//============================================================
// Interaction Position
//============================================================
XMVECTOR PuzzleSwitch::GetInteractionPosition() const
{
    return XMVectorSet(mPosition.x, mPosition.y, mPosition.z, 0.0f);
}


//============================================================
// Can Interact
//============================================================
bool PuzzleSwitch::CanInteract() const
{
    if (mOwnerPuzzle == nullptr)
    {
        return false;
    }

    //========================================================
    // 解決後は操作不可
    //========================================================
    return mOwnerPuzzle != nullptr && mOwnerPuzzle->CanAcceptInput();
}


//============================================================
// Interact
//============================================================
void PuzzleSwitch::Interact(ExplorationPlayer& player)
{
    (void)player;


    if (!CanInteract())
    {
        return;
    }


    //========================================================
    // 親Puzzleへ入力を通知
    //========================================================
    mOwnerPuzzle->PressSwitch(mSwitchIndex);
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* PuzzleSwitch::GetInteractionText() const
{
    return mInteractionText.c_str();
}


//============================================================
// Accepted Visual
//============================================================
void PuzzleSwitch::SetAccepted(bool accepted)
{
    mAccepted = accepted;

    if (mModel == nullptr)
    {
        return;
    }

    //========================================================
    // 4つ目のMaterial SlotがIndicator
    // ※VNM変換後のMaterial IDを確認すること
    //========================================================
    constexpr int kIndicatorMaterialId = 3;

    if (accepted)
    {
        //====================================================
        // 正しい順序で押されたスイッチ
        // 緑色のランプを点灯
        //====================================================
        mModel->SetEmissive(
            0.05f,
            1.0f,
            0.15f,
            3.5f,
            kIndicatorMaterialId
        );
    }
    else
    {
        //====================================================
        // 未入力
        // 暗い橙色で待機
        //====================================================
        mModel->SetEmissive(
            0.65f,
            0.20f,
            0.03f,
            1.5f,
            kIndicatorMaterialId
        );
    }
}