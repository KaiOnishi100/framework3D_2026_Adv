#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "mansionDoor.h"

//============================================================
// Initialize
//============================================================
bool MansionDoor::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const XMFLOAT3& position,
    const XMFLOAT3& scale,
    const XMFLOAT3& rotation

)
{
    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }

    //============================================================
    // CSVから取得した位置・サイズ
    //============================================================
    mClosedPosition = position;
    mInteractionPosition = position;
    mScale = scale;
    mPersistentId = persistentId;

    //============================================================
    // MansionDoor Model Selection
    // persistentIdを見てドアのモデルを選択する
    //============================================================
    //============================================================
    // ドアの設置エリアを判定
    //============================================================
    const bool isMain1F = persistentId.rfind("Main1F_", 0) == 0;
    const bool isMain2F = persistentId.rfind("Main2F_", 0) == 0;
    const bool isMainB1 = persistentId.rfind("MainB1_", 0) == 0;
    const bool isAnnex1F = persistentId.rfind("Annex1F_", 0) == 0;
    const bool isAnnexB1 = persistentId.rfind("AnnexB1_", 0) == 0;

    //============================================================
    // CSV Rotation.xから蝶番の左右を取得
    // 0   : 左蝶番
    // 180 : 右蝶番
    //============================================================
    const bool rightHinge = std::fabs(rotation.x) >= 90.0f;

    mHingeSign = (persistentId == "Main1F_StudyDoor" && rightHinge) ? -1.0f : 1.0f;

    //============================================================
    // CSVから蝶番・開閉方向を取得
    // Rotation.x:
    //   0   = 左蝶番
    //   180 = 右蝶番
    // Rotation.y:
    //   0   = 自動
    //   90  = 正方向固定
    //   -90 = 逆方向固定
    //============================================================

    // 開閉方向を固定するか
    mUseFixedOpenDirection = std::fabs(rotation.y) >= 45.0f;
    // デフォルトの開閉方向
    mConfiguredOpenDirection = rotation.y < 0.0f ? -1.0f : 1.0f;
    mOpenDirection = mConfiguredOpenDirection;


    const std::wstring sideSuffix = rightHinge ? L"_right_01.vnm" : L"_left_01.vnm";

    //============================================================
    // モデルのFolderとFileを決定
    //============================================================
    std::wstring folder;
    std::wstring modelFile;

    if (persistentId == "Main1F_NorthAccessDoor")
    {
        folder = L"data/model/mansion/dynamic/door/";

        modelFile = L"main_north_access_swing_door_01.vnm";
    }
    else if (isMain1F || isMain2F)
    {
        //========================================================
        // 本館1F / 2F：既存の木製ドア
        //========================================================
        folder = L"data/model/mansion/dynamic/door/";

        modelFile = L"main_wooden_swing_door" + sideSuffix;
    }
    else if (isMainB1)
    {
        //========================================================
        // 本館B1：古い鉄製ドア
        //========================================================
        folder = L"data/model/mansion/dynamic/door/";

        modelFile = L"main_b1_metal_swing_door" + sideSuffix;
    }
    else if (isAnnex1F)
    {
        //========================================================
        // 別館1F：塗装金属ドア
        //========================================================
        folder = L"data/model/mansion/dynamic/door/";

        modelFile = L"annex_1f_painted_swing_door" + sideSuffix;
    }
    else if (isAnnexB1)
    {
        //========================================================
        // 別館B1：セキュリティドア
        //========================================================
        folder = L"data/model/mansion/dynamic/door/";

        modelFile = L"annex_b1_security_swing_door" + sideSuffix;
    }
    else
    {
        //========================================================
        // 未対応エリアはCubeで表示
        //========================================================
        folder = L"data/model/primitive/";
        modelFile = L"cube.vnm";
    }

    //============================================================
    // VNMを読み込む
    //============================================================
    const std::filesystem::path modelPath =
        std::filesystem::path(folder) / modelFile;

    if (!std::filesystem::is_regular_file(modelPath))
    {
#if defined(_DEBUG)

        const std::wstring message =
            L"[MansionDoor] VNM not found : " +
            modelPath.wstring() + L"\n";

        OutputDebugStringW(message.c_str());

#endif
        return false;
    }

    mModel = new ModelBase(folder.c_str(), modelFile.c_str());

    if (mModel == nullptr)
    {
        return false;
    }

    //============================================================
    // Modelの基準寸法
    //============================================================
    const float width = (std::max)(scale.x, scale.z);

    const float thickness = (std::min)(scale.x, scale.z);

    //============================================================
    // 見た目の高さだけを調整
    //============================================================
    const bool isMainBuilding = isMain1F || isMain2F || isMainB1;

    mVisualHeightExtra = isMainBuilding ? 0.30f : 0.0f;

    const float visualHeight = scale.y + mVisualHeightExtra;

    //============================================================
    // Visual Scale
    //============================================================
    const bool isDetailedVnmDoor = isMainBuilding || isAnnex1F || isAnnexB1;

    if (isDetailedVnmDoor)
    {
        //========================================================
        // VNMは幅2.0m / 高さ2.2m
        //========================================================
        mModel->setScale(
            width / 2.0f,
            visualHeight / 2.2f,
            1.0f
        );
    }
    else
    {
        //========================================================
        // Cubeの場合は従来のFull Sizeを適用
        //========================================================
        mModel->setScale(
            width,
            visualHeight,
            thickness
        );
    }

    //============================================================
    // Shadow設定
    //============================================================
    mModel->setCastShadow(true);
    mModel->setReceiveShadow(true);

    mModel->SetShadowMobility(
        ShadowMobility::Dynamic
    );

    //============================================================
    // 横壁・縦壁を判定
    // モデルのローカルX方向をドアの幅とする
    //============================================================
    const bool horizontalDoor = scale.x >= scale.z;

    mHalfDoorWidth = width * 0.5f;

    //============================================================
    // 閉じた状態のYaw
    // X方向に長いドアは0度
    // Z方向に長いドアは-90度
    //============================================================
    mBaseYaw = horizontalDoor ? 0.0f : -XM_PIDIV2;

    mCurrentAngle = 0.0f;
    mPushCooldown = 0.0f;

    //============================================================
    // 蝶番位置を計算
    // CSVのPositionはドア中心
    // ドア半幅分だけ左へ移動した地点を蝶番とする
    //============================================================
    const XMVECTOR center = XMVectorSet(
        position.x,
        position.y,
        position.z,
        1.0f
    );

    const XMVECTOR hingeToCenter = XMVectorSet(
        mHalfDoorWidth * mHingeSign,
        0.0f,
        0.0f,
        0.0f
    );

    const XMVECTOR rotatedOffset =
        XMVector3TransformNormal(
            hingeToCenter,
            XMMatrixRotationY(mBaseYaw)
        );

    const XMVECTOR hinge = center - rotatedOffset;

    XMStoreFloat3(&mHingePosition, hinge);

    //============================================================
    // Colliderは元のCSV寸法を維持する
    //============================================================
    mCollider = std::make_unique<BoxCollider>(
        mModel,
        XMVectorSet(
            width * 0.5f,
            scale.y * 0.5f,
            thickness * 0.5f,
            0.0f
        )
    );

    mCollider->SetType(ColliderType::Body);

    // DoorであることをCollisionManagerへ通知
    mCollider->SetTag(ColliderTag::Door);

    //============================================================
    // 接触Callback
    //============================================================
    mCollider->SetContactCallback([this](const Collider* other)
        {
            OnPlayerContact(other);
        }
    );

    //============================================================
    // 初期Transformを同期
    //============================================================
    ApplyDoorTransform();

    //========================================================
    // Scene / Interaction
    //========================================================
    scene->registerObject(mModel);

    interactionSystem->Register(this);

    //========================================================
    // Initial State
    //========================================================
    mState = State::Closed;

    mLocked = false;

    mCanClose = true;


    return true;
}


//============================================================
// Terminate
//============================================================
void MansionDoor::Terminate(vnScene* scene, InteractionSystem* interactionSystem)
{
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }


    mCollider.reset();


    if (scene != nullptr && mModel != nullptr)
    {
        scene->deleteObject(mModel);

        mModel = nullptr;
    }

    mOpenedCallback = {};

    mState = State::Closed;

    mLocked = false;

    mNoiseSystem = nullptr;
}


//============================================================
// Update
//============================================================
void MansionDoor::Update(float dt)
{
    if (mModel == nullptr)
    {
        return;
    }

    //========================================================
    // DeltaTimeの異常値を防ぐ
    //========================================================
    dt = std::clamp(dt, 0.0f, 0.1f);

    //========================================================
    // 接触開扉のCooldown
    //========================================================
    if (mPushCooldown > 0.0f)
    {
        mPushCooldown = (std::max)(0.0f, mPushCooldown - dt);
    }

    //========================================================
    // Opening / Closing中だけ角度更新
    //========================================================
    if (mState != State::Opening && mState != State::Closing)
    {
        return;
    }

    //========================================================
    // 目標角度
    // Opening: ±85度
    // Closing: 0度
    //========================================================
    const float targetAngle = (mState == State::Opening) ? mOpenDirection * mMaxOpenAngle : 0.0f;

    const float difference = targetAngle - mCurrentAngle;
    const float maxStep = mAngularSpeed * dt;

    //========================================================
    // 目標角度に到達
    //========================================================
    if (fabsf(difference) <= maxStep)
    {
        mCurrentAngle = targetAngle;

        if (mState == State::Opening)
        {
            mState = State::Open;
        }
        else
        {
            mState = State::Closed;
            SoundBank::getInstance()->PlaySfx(SoundID::AE_DoorClose, 0.44f);
        }
    }
    else
    {
        //====================================================
        // 一定角速度で回転
        //====================================================
        mCurrentAngle += (difference > 0.0f ? maxStep : -maxStep);
    }

    //========================================================
    // VisualとColliderの両方を更新
    //========================================================
    ApplyDoorTransform();
}

//============================================================
// AI Open Request
//============================================================
bool MansionDoor::RequestOpenFromAI()
{
    return BeginOpening();
}


bool MansionDoor::GetOpenDetourPoints(float clearance, XMFLOAT3& pointA, XMFLOAT3& pointB) const
{
    if (!IsOpen() || mCollider == nullptr || !mCollider->IsEnabled())
    {
        return false;
    }

    // 蝶番から、取っ手のある扉板の先端へ向かう方向。
    const XMVECTOR alongDoor = XMVector3TransformNormal(
        XMVectorSet(mHingeSign, 0.0f, 0.0f, 0.0f),
        XMMatrixRotationY(mBaseYaw + mCurrentAngle)
    );

    const XMVECTOR side = XMVector3Cross( XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), alongDoor);

    // 先端からカプセル半径分だけ先に出て、扉板の両側に候補を置く
    const XMVECTOR beyondTip = XMLoadFloat3(&mHingePosition)
        + alongDoor * (2.0f * mHalfDoorWidth + clearance);
    const float sideClearance = XMVectorGetZ(mCollider->GetHalfSize()) + clearance;

    XMStoreFloat3(&pointA, beyondTip + side * sideClearance);
    XMStoreFloat3(&pointB, beyondTip - side * sideClearance);
    return true;
}

//============================================================
// Interaction Position
//============================================================
XMVECTOR MansionDoor::GetInteractionPosition() const
{
    if (mState == State::Open && mCollider != nullptr)
    {
        return mCollider->GetCenter();
    }

    //============================================================
    // 開ける操作には、CSVから読み込んだ入口中央を使う
    //============================================================
    return XMLoadFloat3(&mInteractionPosition);
}


//============================================================
// Can Interact
//============================================================
bool MansionDoor::CanInteract() const
{
    if (mState == State::Opening || mState == State::Closing)
    {
        return false;
    }

    if (mState == State::Closed)
    {
        return true;
    }

    if (mState == State::Open && mCanClose)
    {
        return true;
    }

    return false;
}


//============================================================
// Player Interaction
//============================================================
void MansionDoor::Interact(ExplorationPlayer& player)
{
    (void)player;

    //============================================================
    // Door Animation中は操作不可
    //============================================================
    if (mState == State::Opening || mState == State::Closing)
    {
        return;
    }


    //============================================================
    // Locked
    //============================================================
    if (mLocked)
    {
        //========================================================
        // Required Itemが設定されているDoor
        //========================================================
        if (!mRequiredItemId.empty())
        {
            //====================================================
            // InventoryManager未設定
            //====================================================
            if (mInventoryManager == nullptr)
            {
#if defined(_DEBUG)

                OutputDebugStringA(
                    "[MansionDoor] "
                    "InventoryManager is null.\n"
                );

#endif

                return;
            }


            //====================================================
            // 必要Itemを持っているか
            //====================================================
            if (!mInventoryManager->HasItem(mRequiredItemId))
            {
#if defined(_DEBUG)

                std::string message =
                    "[MansionDoor] Required item not found : " +
                    mRequiredItemId +
                    "\n";

                OutputDebugStringA(message.c_str());

#endif

                return;
            }

            SetLocked(false);

#if defined(_DEBUG)

            std::string message =
                "[MansionDoor] Door unlocked by item : " +
                mRequiredItemId +
                "\n";

            OutputDebugStringA(message.c_str());
#endif
        }
        else
        {
            //====================================================
            // Itemでは解除できないLock
            //====================================================
#if defined(_DEBUG)

            OutputDebugStringA(
                "[MansionDoor] "
                "Door is locked.\n"
            );

#endif
            return;
        }
    }

    switch (mState)
    {
    case State::Closed:
    {
        SetOpenDirectionFromPosition(*player.getPosition());
        BeginOpening();

        break;
    }

    case State::Open:
    {
        if (mCanClose)
        {
            BeginClosing();
            mPushCooldown = 1.2f;
        }

        break;
    }

    case State::Opening:
    case State::Closing:
    default:
        break;
    }
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* MansionDoor::GetInteractionText() const
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
        return mCanClose ? L"Close Door" : L"";


    case State::Closing:
        return L"Closing...";


    default:
        return L"";
    }
}


//============================================================
// Begin Opening
//============================================================
bool MansionDoor::BeginOpening()
{
    //========================================================
    // LockedならPlayer / AIとも開けない
    //========================================================
    if (mLocked)
    {
        return false;
    }

    //========================================================
    // Closing中ならOpeningへ反転可能
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

    mState = State::Opening;

    SoundBank::getInstance()->PlaySfx(SoundID::AE_DoorOpen, 0.2f);


    if (mOpenedCallback)
    {
        // CSV の Position は閉じた扉の中心 ・高さの半分を引いて敷居を求める
        const XMFLOAT3 thresholdPosition{
            mClosedPosition.x,
            mClosedPosition.y - mScale.y * 0.5f,
            mClosedPosition.z
        };

        mOpenedCallback(thresholdPosition);
    }

    //========================================================
    // Gameplay Noise
    // 実サウンドとは別
    // PursuerのHearing用
    //========================================================
    if (mNoiseSystem != nullptr)
    {
        mNoiseSystem->Emit(
            mInteractionPosition,
            mDoorNoiseRadius,
            NoiseType::Door,
            1.20f
        );
    }

    return true;
}


//============================================================
// Begin Closing
//============================================================
bool MansionDoor::BeginClosing()
{
    if (!mCanClose)
    {
        return false;
    }

    if (mState != State::Open)
    {
        return false;
    }

    mState = State::Closing;

    if (mNoiseSystem != nullptr)
    {
        mNoiseSystem->Emit(
            mInteractionPosition,
            mDoorNoiseRadius,
            NoiseType::Door,
            1.20f
        );
    }

    return true;
}


//============================================================
// Save
//============================================================
WorldObjectSaveState MansionDoor::CaptureSaveState() const
{
    WorldObjectSaveState data;

    data.Id = mPersistentId;

    data.Type = PersistentObjectType::Door;


    //========================================================
    // 0 = Closed
    // 1 = Open
    // Opening / Closing中にSaveされた場合は
    // 最終的に近いStateではなく現在Open判定だけを保存する
    //========================================================
    data.State = IsOpen() ? 1u : 0u;

    //========================================================
    // bit0 = Locked
    //========================================================
    if (mLocked)
    {
        data.Flags |= 1u << 0;
    }

    return data;
}


//============================================================
// Load
//============================================================
void MansionDoor::RestoreSaveState(const WorldObjectSaveState& data)
{
    if (data.Id != mPersistentId)
    {
        return;
    }

    //========================================================
    // Locked
    //========================================================
    const bool locked = (data.Flags & (1u << 0)) != 0;


    //========================================================
    // Stateを先にClosedへ戻してからLock設定
    // SetLocked()はClosed時だけ有効なため
    //========================================================
    SetClosedImmediate();

    SetLocked(locked);


    //========================================================
    // Open
    //========================================================
    if (data.State == 1u)
    {
        SetOpenImmediate();
    }
}


//============================================================
// Load Immediate Open
//============================================================
void MansionDoor::SetOpenImmediate()
{
    mState = State::Open;

    //========================================================
    // 選択済みの開閉方向で最大角度にする
    //========================================================
    mCurrentAngle = mOpenDirection * mMaxOpenAngle;

    ApplyDoorTransform();
}


//============================================================
// Load Immediate Closed
//============================================================
void MansionDoor::SetClosedImmediate()
{
    mState = State::Closed;

    mCurrentAngle = 0.0f;

    ApplyDoorTransform();
}

void MansionDoor::ApplyDoorTransform()
{
    if (mModel == nullptr)
    {
        return;
    }

    //========================================================
    // 閉じたときのYaw + 現在の開閉角度
    //========================================================
    const float yaw = mBaseYaw + mCurrentAngle;

    //========================================================
    // 蝶番World座標
    //========================================================
    const XMVECTOR hinge = XMLoadFloat3(&mHingePosition);

    //========================================================
    // 蝶番からドア中心への距離
    //========================================================
    const XMVECTOR localOffset = XMVectorSet(
        mHalfDoorWidth * mHingeSign,
        0.0f,
        0.0f,
        0.0f
    );

    const XMVECTOR rotatedOffset =
        XMVector3TransformNormal(
            localOffset,
            XMMatrixRotationY(yaw)
        );

    //========================================================
    // 回転後のドア中心座標
    //========================================================
    const XMVECTOR newCenter = hinge + rotatedOffset;

    const float visualRise = mVisualHeightExtra * 0.5f;

    mModel->setPosition(
        XMVectorGetX(newCenter),
        XMVectorGetY(newCenter) + visualRise,
        XMVectorGetZ(newCenter)
    );

    //========================================================
    // 見た目の回転 ラジアンを渡す
    //========================================================
    mModel->setRotation(0.0f, yaw, 0.0f);

    //========================================================
    // ColliderのOBB回転を同期
    // mModelが移動したのでCollider中心も追従する
    //========================================================
    if (mCollider != nullptr)
    {
        mCollider->SetOffset(
            XMVectorSet(
                0.0f,
                -visualRise,
                0.0f,
                0.0f
            )
        );

        mCollider->SetYawRadians(yaw);

        //====================================================
        // 開いた状態でもColliderは有効にする
        //====================================================
        mCollider->SetEnabled(true);
    }
}

void MansionDoor::SetOpenDirectionFromPosition(const XMVECTOR& worldPosition)
{
    //============================================================
    // CSVで開く方向が固定されている場合、プレイヤーの位置で上書きしない
    //============================================================
    if (mUseFixedOpenDirection)
    {
        mOpenDirection = mConfiguredOpenDirection;
        return;
    }

    //========================================================
    // 閉じた状態のドア中心
    //========================================================
    const XMVECTOR doorCenter = XMVectorSet(
        mClosedPosition.x,
        mClosedPosition.y,
        mClosedPosition.z,
        1.0f
    );

    //========================================================
    // 閉じたドアの法線（Local +Z）
    //========================================================
    const XMVECTOR doorNormal =
        XMVector3TransformNormal(
            XMVectorSet(0, 0, 1, 0),
            XMMatrixRotationY(mBaseYaw)
        );

    //========================================================
    // プレイヤーがドアのどちら側にいるか
    //========================================================
    const XMVECTOR toPlayer = worldPosition - doorCenter;

    const float side = XMVectorGetX(XMVector3Dot(toPlayer, doorNormal));

    //========================================================
    // プレイヤーから遠ざかる方向へ開く
    // 蝶番の左右も考慮する
    //========================================================
    const float playerSide = side >= 0.0f ? 1.0f : -1.0f;

    mOpenDirection = playerSide * mHingeSign;
}

void MansionDoor::OnPlayerContact(const Collider* other)
{
    if (other == nullptr)
    {
        return;
    }

    //========================================================
    // Playerだけを対象とする
    // AIや別の物理オブジェクトでは開かない
    //========================================================
    if (!other->HasTag(ColliderTag::Player))
    {
        return;
    }

    const auto* capsule = dynamic_cast<const CapsuleCollider*>(other);

    if (capsule == nullptr)
    {
        return;
    }

    //========================================================
    // 閉じているドアのみ接触開扉する
    //========================================================
    if (mState != State::Closed)
    {
        //====================================================
        // 閉じている途中でプレイヤーに接触した場合
        // 挟み込み防止のため再び開く
        //====================================================
        if (mState == State::Closing)
        {
            BeginOpening();
        }

        return;
    }

    //========================================================
    // 施錠中は接触で開かない
    //========================================================
    if (mLocked)
    {
        return;
    }

    if (mPushCooldown > 0.0f)
    {
        return;
    }

    //========================================================
    // プレイヤー側と反対方向へ開く
    //========================================================
    SetOpenDirectionFromPosition(capsule->GetCenter());

    if (BeginOpening())
    {
        //====================================================
        // 接触による重複開扉を抑制
        //====================================================
        mPushCooldown = 0.3f;
    }
}
