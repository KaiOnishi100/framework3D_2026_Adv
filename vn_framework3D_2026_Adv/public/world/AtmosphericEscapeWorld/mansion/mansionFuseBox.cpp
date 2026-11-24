#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "mansionFuseBox.h"

//============================================================
// Initialize
//============================================================
bool MansionFuseBox::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const XMFLOAT3& position,
    const XMFLOAT3& scale,
    const XMFLOAT3& rotation

)
{
    //============================================================
    // Safety
    //============================================================
    if (scene == nullptr || interactionSystem == nullptr || persistentId.empty())
    {
        return false;
    }


    //============================================================
    // Persistent ID
    //============================================================
    mPersistentId = persistentId;

    //============================================================
    // Interaction Position
    //============================================================
    mInteractionPosition = position;

    //============================================================
    // Initial State
    //============================================================
    mState = State::MissingFuse;


    //============================================================
    // Model
    //============================================================
    mModel = new ModelBase(L"data/model/mansion/dynamic/gimmick/", L"main_b1_fuse_box_01.vnm");


    if (mModel == nullptr)
    {
        return false;
    }


    //============================================================
    // Transform
    //============================================================
    mModel->setPosition(position.x, position.y, position.z);

    mModel->setRotation(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );


    mModel->setScale(
        scale.x / 0.45f,
        scale.y / 1.00f,
        scale.z / 0.80f
    );


    //============================================================
    // Shadow
    //============================================================
    mModel->setCastShadow(true);

    mModel->setReceiveShadow(true);

    mModel->SetShadowMobility(ShadowMobility::Static);


    //============================================================
    // Collider
    //============================================================
    const XMFLOAT3 halfExtents =
    {
        scale.x * 0.5f,
        scale.y * 0.5f,
        scale.z * 0.5f
    };


    mCollider =
        std::make_unique<BoxCollider>(
            mModel,
            XMVectorSet(
                halfExtents.x,
                halfExtents.y,
                halfExtents.z,
                0.0f
            )
        );


    if (mCollider == nullptr)
    {
        scene->deleteObject(mModel);

        mModel = nullptr;

        return false;
    }


    mCollider->SetType(ColliderType::Body);

    //============================================================
    // Scene Register
    //============================================================
    scene->registerObject(mModel);


    //============================================================
    // Interaction Register
    //============================================================
    interactionSystem->Register(this);

    mInspected = false;

    //============================================================
    // Visual
    //============================================================
    RefreshVisual();

    return true;
}


//============================================================
// Terminate
//============================================================
void MansionFuseBox::Terminate(
    vnScene* scene,
    InteractionSystem* interactionSystem
)
{
    //============================================================
    // Interactionから先に外す
    // Object削除後にInteractionSystemから参照されることを防ぐ
    //============================================================
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }


    //============================================================
    // Collider
    //============================================================
    mCollider.reset();


    //============================================================
    // Model
    //============================================================
    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);

        mModel = nullptr;
    }


    //============================================================
    // 非所有Pointerを解除
    //============================================================
    mInventoryManager = nullptr;
    mPersistentId.clear();
    mState = State::MissingFuse;
    mInspected = false;
}


//============================================================
// Interaction Position
//============================================================
XMVECTOR MansionFuseBox::GetInteractionPosition() const
{
    return XMVectorSet(
        mInteractionPosition.x,
        mInteractionPosition.y,
        mInteractionPosition.z,
        0.0f
    );
}


//============================================================
// Can Interact
//============================================================
bool MansionFuseBox::CanInteract() const
{
    //============================================================
    // Activated後は再操作させない
    // Fuseを持っていなくてもtrueにしているのは
    // PlayerがFuse Boxを調べられるようにするため
    //============================================================
    return
        mState == State::MissingFuse;
}


//============================================================
// Interaction
//============================================================
void MansionFuseBox::Interact(ExplorationPlayer& player)
{

    (void)player;


    if (!CanInteract())
    {
        return;
    }


    //============================================================
    // InventoryManager未設定
    //============================================================
    if (mInventoryManager == nullptr)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[MansionFuseBox] "
            L"InventoryManager is null.\n"
        );

#endif

        return;
    }

    //============================================================
    // Fuse Boxを調査済みにする
    //============================================================
    mInspected = true;

#if defined(_DEBUG)

    OutputDebugStringW(
        L"[MansionFuseBox] "
        L"Fuse Box inspected.\n"
    );

#endif

    //============================================================
    // Fuse所持確認
    //============================================================
    if (!mInventoryManager->HasItem(mRequiredItemId, 1))
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[MansionFuseBox] "
            L"Fuse is required.\n"
        );

#endif
        SoundBank::getInstance()->PlaySfx(SoundID::AE_FuseBoxInspect, 0.45f);

        return;
    }


    //============================================================
    // Fuseを1個消費
    // ここでRemoveに成功した時だけ設備をActivatedにする
    //============================================================
    if (!mInventoryManager->RemoveItem(mRequiredItemId, 1))
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[MansionFuseBox] "
            L"Failed to consume Fuse.\n"
        );

#endif
        return;
    }

    //============================================================
    // Activate
    //============================================================
    mState = State::Activated;

    SoundBank::getInstance()->PlaySfx(SoundID::AE_FuseSet, 0.75f);

    RefreshVisual();

#if defined(_DEBUG)

    OutputDebugStringW(
        L"[MansionFuseBox] "
        L"Fuse installed. Activated.\n"
    );

#endif
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* MansionFuseBox::GetInteractionText() const
{
    //============================================================
    // Activated後はCanInteract() == falseなので
    // 基本表示されない。
    //============================================================
    if (mState == State::Activated)
    {
        return L"";
    }


    //============================================================
    // Inventory未接続時
    //============================================================
    if (mInventoryManager == nullptr)
    {
        return L"ヒューズボックスを調べる";
    }


    //============================================================
    // Fuseを持っている
    //============================================================
    if (mInventoryManager->HasItem(mRequiredItemId,1))
    {
        return L"ヒューズを取り付ける";
    }


    //============================================================
    // Fuseなし
    //============================================================
    return L"ヒューズが必要";
}


//============================================================
// Save
//============================================================
WorldObjectSaveState MansionFuseBox::CaptureSaveState() const
{
    WorldObjectSaveState data;


    data.Id = mPersistentId;


    //============================================================
    // 復元自体はPersistent IDで対象Objectを特定する
    //============================================================
    data.Type = PersistentObjectType::Terminal;


    //============================================================
    // State
    // 0 = Fuse未装着
    // 1 = Activated
    //============================================================
    data.State = IsActivated() ? 1u : 0u;

    //============================================================
    // 調査状態を汎用Valueへ保存
    // 0.0f = 未調査
    // 1.0f = 調査済み
    //============================================================
    data.Value = mInspected ? 1.0f : 0.0f;

    return data;
}


//============================================================
// Load
//============================================================
void MansionFuseBox::RestoreSaveState(const WorldObjectSaveState& data)
{
    //============================================================
    // 別ObjectのSave Dataは無視する
    //============================================================
    if (data.Id != mPersistentId)
    {
        return;
    }

    //============================================================
    // 調査状態を復元
    //============================================================
    mInspected = (data.Value >= 0.5f);

    //============================================================
    // 配電盤の起動状態を復元
    // FuseBoxがActivatedなら、調査も完了済みとして扱う
    //============================================================
    SetActivatedImmediate(data.State == 1u);
}


//============================================================
// Immediate Restore
//============================================================
void MansionFuseBox::SetActivatedImmediate(bool activated)
{
    mState = activated ? State::Activated : State::MissingFuse;

    if (activated)
    {
        mInspected = true;
    }

    RefreshVisual();
}


//============================================================
// Visual State
//============================================================
void MansionFuseBox::RefreshVisual()
{
    if (mModel == nullptr)
    {
        return;
    }

    //========================================================
    // 本体ではなくIndicatorだけを発光させる
    //========================================================
    constexpr int kIndicatorMaterialId = 2;

    if (mState == State::Activated)
    {
        // ヒューズ取り付け完了：緑色
        mModel->SetEmissive(
            0.02f, 1.0f, 0.08f, 3.0f,
            kIndicatorMaterialId
        );
    }
    else
    {
        // ヒューズ未装着：赤色
        mModel->SetEmissive(
            0.8f, 0.02f, 0.01f, 2.0f,
            kIndicatorMaterialId
        );
    }
}