#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "facilityDoor.h"


//============================================================
// Initialize
//============================================================
bool FacilityDoor::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const XMFLOAT3& position
)
{
    //========================================================
    // Persistent ID
    //========================================================
    mPersistentId = persistentId;

    //========================================================
    // Safety
    //========================================================
    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }


    //========================================================
    // Door Position
    // position: 閉じているときのDoor中心
    //========================================================
    mClosedPosition = position;


    //========================================================
    // Interaction Position
    // Doorが右へスライドしても
    // Playerは入口中央で操作できるようにする
    //========================================================
    mInteractionPosition = position;


    //========================================================
    // Open Position
    // +X方向へスライド
    // Facility Front Right Wall内部へDoorを収納する
    //========================================================
    mOpenPosition = position;
    mOpenPosition.x += mOpenDistance;

    //========================================================
    // Door Model
    //========================================================
    mModel = new ModelBase(L"data/model/primitive/", L"cube.vnm");


    if (mModel == nullptr)
    {
        return false;
    }


    //========================================================
    // Position
    //========================================================
    mModel->setPosition(
        position.x,
        position.y,
        position.z
    );


    //========================================================
    // Door Size
    //========================================================
    mModel->setScale(
        1.80f,
        2.10f,
        0.16f
    );


    //========================================================
    // Material
    //========================================================
    mModel->SetPBREnabled(true);
    mModel->setDiffuse(0.10f, 0.11f, 0.12f, 1.0f);
    mModel->SetMetallic(0.65f);
    mModel->SetRoughness(0.55f);

    //========================================================
    // Shadow
    //========================================================
    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);
    mModel->SetShadowMobility(ShadowMobility::Dynamic);


    //========================================================
    // Door Collider
    //========================================================
    mCollider =
        std::make_unique<BoxCollider>(
            mModel,
            XMVectorSet(
                0.90f,
                1.05f,
                0.08f,
                0.0f
            )
        );


    mCollider->SetType(ColliderType::Body);


    //========================================================
    // Scene登録
    //========================================================
    scene->registerObject(mModel);


    //========================================================
    // Interaction登録
    //========================================================
    interactionSystem->Register(this);


    //========================================================
    // Initial State
    //========================================================
    mState = State::Closed;


    return true;
}


//============================================================
// Terminate
//============================================================
void FacilityDoor::Terminate(
    vnScene* scene,
    InteractionSystem* interactionSystem
)
{
    //========================================================
    // Interactionから先に外す
    // ※Object破棄後にInteractionSystemへPointerが残るのを防ぐ
    //========================================================
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }


    //========================================================
    // Collider
    //========================================================
    mCollider.reset();


    //========================================================
    // Model
    //========================================================
    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);

        mModel = nullptr;
    }


    mState = State::Closed;
}


//============================================================
// Update
//============================================================
void FacilityDoor::Update(float dt)
{
    if (mModel == nullptr)
    {
        return;
    }


    //========================================================
    // 動いていないDoor
    //========================================================
    if (mState != State::Opening && mState != State::Closing)
    {
        return;
    }


    //========================================================
    // Current Position
    //========================================================
    const XMVECTOR current = *mModel->getPosition();


    //========================================================
    // Target Position
    // Opening: Open Position
    // Closing: Closed Position
    //========================================================
    const XMFLOAT3& targetPosition =
        (mState == State::Opening) ? mOpenPosition : mClosedPosition;


    const XMVECTOR target =
        XMVectorSet(
            targetPosition.x,
            targetPosition.y,
            targetPosition.z,
            0.0f
        );


    //========================================================
    // Current → Target
    //========================================================
    const XMVECTOR difference = target - current;


    const float distance =
        XMVectorGetX(XMVector3Length(difference));


    //========================================================
    // 到着
    //========================================================
    if (distance <= 0.001f)
    {
        mModel->setPosition(
            targetPosition.x,
            targetPosition.y,
            targetPosition.z
        );


        //====================================================
        // Opening完了
        //====================================================
        if (mState == State::Opening)
        {
            mState = State::Open;


            //================================================
            // 開いたので通過可能
            //================================================
            if (mCollider != nullptr)
            {
                mCollider->SetEnabled(false);
            }
        }

        //====================================================
        // Closing完了
        //====================================================
        else
        {
            mState = State::Closed;

            //================================================
            // 完全に閉まったのでCollider復活
            //================================================
            if (mCollider != nullptr)
            {
                mCollider->SetEnabled(true);
            }
        }


        return;
    }
    //========================================================
    // Movement
    //========================================================
    const float moveDistance = mOpenSpeed * dt;

    //========================================================
    // Targetを通り越す場合
    //========================================================
    if (moveDistance >= distance)
    {
        mModel->setPosition(
            targetPosition.x,
            targetPosition.y,
            targetPosition.z
        );

        if (mState == State::Opening)
        {
            mState = State::Open;

            if (mCollider != nullptr)
            {
                mCollider->SetEnabled(false);
            }
        }
        else
        {
            mState = State::Closed;

            if (mCollider != nullptr)
            {
                mCollider->SetEnabled(true);
            }
        }


        return;
    }


    //========================================================
    // Normal Move
    //========================================================
    const XMVECTOR direction =
        XMVectorScale(difference, 1.0f / distance);

    const XMVECTOR delta =
        XMVectorScale(direction, moveDistance);

    mModel->Translate(delta);
}


bool FacilityDoor::RequestOpenFromAI()
{
    return BeginOpening();
}

//============================================================
// Interaction Position
//============================================================
XMVECTOR FacilityDoor::GetInteractionPosition() const
{
    return
        XMVectorSet(
            mInteractionPosition.x,
            mInteractionPosition.y,
            mInteractionPosition.z,
            0.0f
        );
}


//============================================================
// Can Interact
//============================================================
bool FacilityDoor::CanInteract() const
{
    if (mLocked)
    {
        return false;
    }

    //========================================================
    // Closedなら開けられる
    //========================================================
    if (mState == State::Closed)
    {
        return true;
    }

    //========================================================
    // OpenかつClose可能なら閉められる
    //========================================================
    if (mState == State::Open && mCanClose)
    {
        return true;
    }

    return false;
}


//============================================================
// Interact
//============================================================
void FacilityDoor::Interact(ExplorationPlayer& player)
{
    (void)player;


    //========================================================
    // Locked
    //========================================================
    if (mLocked)
    {
        return;
    }


    switch (mState)
    {
    //========================================================
    // Closed → Open
    //========================================================
    case State::Closed:
    {
        BeginOpening();

        break;
    }

    //========================================================
    // Open → Closed
    //========================================================
    case State::Open:
    {
        if (mCanClose)
        {
            BeginClosing();
        }

        break;
    }


    //========================================================
    // Animation中は操作不可
    //========================================================
    case State::Opening:
    case State::Closing:
    default:
        break;
    }
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* FacilityDoor::GetInteractionText() const
{
    if (mLocked)
    {
        return L"Locked";
    }

    switch (mState)
    {
    case State::Closed:
        return L"Open Door";

    case State::Opening:
        return L"Opening...";

    case State::Open:
        return  mCanClose ? L"Close Door" : L"";

    case State::Closing:
        return L"Closing...";

    default:
        return L"";
    }
}

bool FacilityDoor::BeginOpening()
{
    //========================================================
    // Locked DoorはPlayer / AIとも開けない。
    //========================================================
    if (mLocked)
    {
        return false;
    }


    //========================================================
    // Closed以外は基本的にOpening開始しない
    // Closing中なら、AIが来た場合などに
    // 開く方向へ反転できるようにする
    //========================================================
    if (mState == State::Closing)
    {
        mState = State::Opening;

        return true;
    }


    if (mState != State::Closed)
    {
        return false;
    }


    //========================================================
    // Opening開始
    //========================================================
    mState = State::Opening;


    //========================================================
    // Gameplay Noise
    //========================================================
    if (mNoiseSystem != nullptr)
    {
        mNoiseSystem->Emit(
            mInteractionPosition,
            mOpenNoiseRadius,
            NoiseType::Door,
            1.20f
        );
    }


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[FacilityDoor] Opening started.\n"
    );

#endif


    return true;
}

bool FacilityDoor::BeginClosing()
{
    //========================================================
   // 閉められないDoorなら何もしない
   //========================================================
    if (!mCanClose)
    {
        return false;
    }

    //========================================================
    // 完全にOpenしている場合のみClosing開始
    //========================================================
    if (mState != State::Open)
    {
        return false;
    }

    mState = State::Closing;

    //========================================================
    // Doorを閉める音もGameplay Noiseとして扱う
    //========================================================
    if (mNoiseSystem != nullptr)
    {
        mNoiseSystem->Emit(
            mInteractionPosition,
            mOpenNoiseRadius,
            NoiseType::Door,
            1.20f
        );
    }


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[FacilityDoor] Closing started.\n"
    );

#endif

    return true;
}

WorldObjectSaveState FacilityDoor::CaptureSaveState() const
{
    WorldObjectSaveState data;

    data.Id = mPersistentId;

    data.Type = PersistentObjectType::Door;

    //========================================================
    // Door State
    //========================================================
    data.State = IsOpen()? 1u: 0u;

    //========================================================
    // Locked Flag
    //========================================================
    if (IsLocked())
    {
        data.Flags |= 1u << 0;
    }

    return data;
}

void FacilityDoor::RestoreSaveState(const WorldObjectSaveState& data)
{
    //========================================================
    // ID Safety Check
    //========================================================
    if (data.Id != mPersistentId)
    {
        return;
    }

    //========================================================
    // Locked
    //========================================================
    const bool locked = (data.Flags & (1u << 0)) != 0;
    SetLocked(locked );

    //========================================================
    // Open / Closed
    // Load時にはAnimationを再生せず
    // 即座に最終状態へ設定する
    //========================================================
    if (data.State == 1u)
    {
        SetOpenImmediate();
    }
    else
    {
        SetClosedImmediate();
    }
}

void FacilityDoor::SetOpenImmediate()
{
    //============================================================
    // State
    //============================================================
    mState = State::Open;


    //============================================================
    // ModelをOpen Positionへ直接移動
    //============================================================
    if (mModel != nullptr)
    {
        mModel->setPosition(
            mOpenPosition.x,
            mOpenPosition.y,
            mOpenPosition.z
        );
    }

    //============================================================
    // Doorが開いているのでBlocking Collider無効
    //============================================================
    if (mCollider != nullptr)
    {
        mCollider->SetEnabled(false);
    }
}

void FacilityDoor::SetClosedImmediate()
{
    //============================================================
    // State
    //============================================================
    mState = State::Closed;


    //============================================================
    // ModelをOpen Positionへ直接移動
    //============================================================
    if (mModel != nullptr)
    {
        mModel->setPosition(
            mClosedPosition.x,
            mClosedPosition.y,
            mClosedPosition.z
        );
    }

    //============================================================
    // Doorが開いているのでBlocking Collider無効
    //============================================================
    if (mCollider != nullptr)
    {
        mCollider->SetEnabled(true);
    }
}
