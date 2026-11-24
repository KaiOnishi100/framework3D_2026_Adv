#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "annexSafetyPuzzle.h"

//============================================================
// Panel Initialize
//============================================================
bool AnnexSafetyPuzzle::Panel::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    AnnexSafetyPuzzle* owner,
    PanelType type,
    const XMFLOAT3& position,
    const XMFLOAT3& scale,
    const XMFLOAT3& rotation
)
{
    if (scene == nullptr || interactionSystem == nullptr || owner == nullptr)
    {
        return false;
    }

    mOwner = owner;
    mType = type;
    mPosition = position;

    const wchar_t* modelFile = nullptr;

    //============================================================
    // 各VNMの基準寸法
    //============================================================
    float baseWidth = 0.45f;
    float baseHeight = 0.45f;
    float baseDepth = 0.20f;

    switch (mType)
    {
    case PanelType::Vent:
        modelFile = L"annex_safety_vent_01.vnm";
        break;

    case PanelType::Cooling:
        modelFile = L"annex_safety_cooling_01.vnm";
        break;

    case PanelType::Controller:
        modelFile = L"annex_safety_controller_01.vnm";

        baseWidth = 0.65f;
        baseHeight = 0.55f;
        baseDepth = 0.25f;
        break;

    default:
        return false;
    }

    //============================================================
    // 専用VNMを生成
    //============================================================
    mModel = new ModelBase(L"data/model/mansion/dynamic/annexPuzzle/", modelFile);

    if (mModel == nullptr)
    {
        return false;
    }

    mModel->setPosition(
        position.x,
        position.y,
        position.z
    );

    mModel->setRotation(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );

    mModel->setScale(
        scale.x / baseWidth,
        scale.y / baseHeight,
        scale.z / baseDepth
    );

    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);
    mModel->SetShadowMobility(ShadowMobility::Static);

    SetIndicator(false);

    //========================================================
    // Scene / Interactionへ登録
    //========================================================
    scene->registerObject(mModel);

    interactionSystem->Register(this);

    mRegistered = true;

    return true;
}

//============================================================
// Panel Terminate
//============================================================
void AnnexSafetyPuzzle::Panel::Terminate(vnScene* scene, InteractionSystem* interactionSystem)
{
    //========================================================
    // 先に操作対象から取り除く
    // モデルだけ破棄すると、InteractionSystemに
    // 無効なポインタが残る可能性がある
    //========================================================
    if (mRegistered && interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }

    mRegistered = false;

    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);
    }

    mModel = nullptr;
    mOwner = nullptr;
}

//============================================================
// Panel Visual
//============================================================
void AnnexSafetyPuzzle::Panel::SetIndicator(bool active)
{
    if (mModel == nullptr)
    {
        return;
    }

    //========================================================
    // Material ID 3 = Indicator
    //========================================================
    constexpr int kIndicatorMaterialId = 3;

    if (active)
    {
        //====================================================
        // ON：緑色
        //====================================================
        mModel->SetEmissive(
            0.05f,
            1.0f,
            0.12f,
            3.5f,
            kIndicatorMaterialId
        );
    }
    else
    {
        //====================================================
        // OFF：暗い橙
        //====================================================
        mModel->SetEmissive(
            0.55f,
            0.15f,
            0.02f,
            1.2f,
            kIndicatorMaterialId
        );
    }
}

//============================================================
// Panel Interaction Position
//============================================================
XMVECTOR AnnexSafetyPuzzle::Panel::GetInteractionPosition() const
{
    return XMVectorSet(mPosition.x, mPosition.y, mPosition.z, 1.0f);
}

//============================================================
// Panel CanInteract
//============================================================
bool AnnexSafetyPuzzle::Panel::CanInteract() const
{
    //========================================================
    // 解決後は再操作できない
    //========================================================
    return
        mRegistered && mOwner != nullptr && !mOwner->IsSolved();
}

//============================================================
// Panel Interact
//============================================================
void AnnexSafetyPuzzle::Panel::Interact(ExplorationPlayer& player)
{
    (void)player;

    if (!CanInteract())
    {
        return;
    }

    //========================================================
    // 各Panelは状態を持たず、
    // Puzzle本体に操作を通知する
    //========================================================
    mOwner->Operate(mType);
}

//============================================================
// Panel Interaction Text
//============================================================
const wchar_t* AnnexSafetyPuzzle::Panel::GetInteractionText() const
{
    if (mOwner == nullptr)
    {
        return L"";
    }

    switch (mType)
    {
    case PanelType::Vent:
        return mOwner->IsVentReady()
            ? L"排気弁を閉じる" : L"排気弁を開く";

    case PanelType::Cooling:
        return mOwner->IsCoolingReady()
            ? L"冷却装置を停止する" : L"冷却装置を起動する";

    case PanelType::Controller:
        return L"安全装置の復旧を実行する";

    default:
        return L"装置を操作する";
    }
}

//============================================================
// Puzzle Initialize
//============================================================
bool AnnexSafetyPuzzle::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const std::array<XMFLOAT3, 3>& positions,
    const std::array<XMFLOAT3, 3>& scales,
    const std::array<XMFLOAT3, 3>& rotations
)
{
    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }

    //========================================================
    // PuzzleBase初期化
    // Persistent IDとSolved状態を初期化する
    //========================================================
    if (!PuzzleBase::Initialize(persistentId))
    {
        return false;
    }

    mVentReady = false;
    mCoolingReady = false;

    //========================================================
    // 3個の操作パネル生成
    // 0: 排気弁
    // 1: 冷却装置
    // 2: 復旧制御盤
    //========================================================
    for (std::size_t i = 0; i < mPanels.size(); ++i)
    {
        const PanelType type = static_cast<PanelType>(i);

        if (!mPanels[i].Initialize(
            scene,
            interactionSystem,
            this,
            type,
            positions[i],
            scales[i],
            rotations[i]
            
        ))
        {
            //================================================
            // 一部だけ作成できた場合も後片付けする
            //================================================
            Terminate(scene, interactionSystem);
            return false;
        }
    }

    RefreshVisuals();

    return true;
}

//============================================================
// Puzzle Terminate
//============================================================
void AnnexSafetyPuzzle::Terminate(vnScene* scene,InteractionSystem* interactionSystem)
{
    for (Panel& panel : mPanels)
    {
        panel.Terminate(scene, interactionSystem);
    }

    mVentReady = false;
    mCoolingReady = false;

    mSolvedCallback = nullptr;
}

//============================================================
// Operate
//============================================================
void AnnexSafetyPuzzle::Operate(PanelType type)
{
    if (IsSolved())
    {
        return;
    }

    auto* sound = SoundBank::getInstance();

    switch (type)
    {
    case PanelType::Vent:
    {
        //====================================================
        // 排気弁の状態を反転
        //====================================================
        mVentReady = !mVentReady;
        sound->PlaySfx(
            mVentReady ? SoundID::AE_SafetyVent : SoundID::AE_Interact,
            mVentReady ? 0.65f : 0.40f
        );
        break;
    }

    case PanelType::Cooling:
    {
        //====================================================
        // 冷却装置の状態を反転
        //====================================================
        mCoolingReady = !mCoolingReady;
        sound->PlaySfx(
            mCoolingReady ? SoundID::AE_SafetyCooling : SoundID::AE_Interact,
            mCoolingReady ? 0.65f : 0.40f
        );
        break;
    }

    case PanelType::Controller:
    {
        //====================================================
        // 両方の装置がONの場合だけ復旧成功
        // 片方でもOFFなら状態は維持する
        // プレイヤーは不足分だけ修正して再挑戦できる
        //====================================================
        if (mVentReady && mCoolingReady)
        {
            //================================================
            // PuzzleBaseのSolve()を使用
            // StateをSolvedへ変更しOnSolved()を呼び出す
            //================================================
            sound->PlaySfx(SoundID::AE_SafetyController, 0.60f);
            Solve();
        }
        else
        {
            sound->PlaySfx(SoundID::AE_PuzzleError, 0.67f);

#if defined(_DEBUG)
            OutputDebugStringW(
                L"[AnnexSafetyPuzzle] "
                L"Safety recovery failed. "
                L"Vent or Cooling is inactive.\n"
            );
        
#endif
        }
        break;
    }

    default:
        break;
    }

    RefreshVisuals();
}

//============================================================
// Refresh Visuals
//============================================================
void AnnexSafetyPuzzle::RefreshVisuals()
{
    //========================================================
    // ON状態のパネルは緑色にする
    // 制御盤はPuzzleがSolvedになった場合のみ緑色
    //========================================================
    mPanels[0].SetIndicator(mVentReady || IsSolved());

    mPanels[1].SetIndicator(mCoolingReady || IsSolved());

    mPanels[2].SetIndicator(IsSolved());
}

//============================================================
// Capture Save State
//============================================================
void AnnexSafetyPuzzle::CaptureCustomState(WorldObjectSaveState& data) const
{
    //========================================================
    // 1つのFlagsに2つのboolを保存する
    // bit0: 排気弁
    // bit1: 冷却装置
    // PuzzleのSolved状態はPuzzleBaseが
    // data.Stateに保存する
    //========================================================
    data.Flags = 0;

    if (mVentReady)
    {
        data.Flags |= (1u << 0);
    }

    if (mCoolingReady)
    {
        data.Flags |= (1u << 1);
    }
}

//============================================================
// Restore Save State
//============================================================
void AnnexSafetyPuzzle::RestoreCustomState(const WorldObjectSaveState& data)
{
    //========================================================
    // 保存されていた設備状態を復元する
    // PuzzleBaseはこの関数の後にSolved状態を戻す
    //========================================================
    mVentReady = (data.Flags & (1u << 0)) != 0;

    mCoolingReady = (data.Flags & (1u << 1)) != 0;

    //========================================================
    // 復元途中でも表示が正しくなるようにする
    //========================================================
    mPanels[0].SetIndicator(mVentReady);

    mPanels[1].SetIndicator(mCoolingReady);

    mPanels[2].SetIndicator(data.State == 1u);
}

//============================================================
// 通常プレイで解決した場合
//============================================================
void AnnexSafetyPuzzle::OnSolved()
{
    RefreshVisuals();

    if (mSolvedCallback)
    {
        //====================================================
        // false: 通常操作で解決
        //====================================================
        mSolvedCallback(false);
    }

#if defined(_DEBUG)
    OutputDebugStringW(
        L"[AnnexSafetyPuzzle] "
        L"Safety system recovered.\n"
    );
#endif
}

//============================================================
// Save Loadで解決済みへ復元した場合
//============================================================
void AnnexSafetyPuzzle::OnSolvedRestored()
{
    RefreshVisuals();

    if (mSolvedCallback)
    {
        //====================================================
        // true: セーブデータからの復元
        // 演出は再生せず、扉などの結果状態だけ反映する
        //====================================================
        mSolvedCallback(true);
    }
}