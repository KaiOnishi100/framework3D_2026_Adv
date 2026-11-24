#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "mansionEntranceDoor.h"

//============================================================
// Initialize
//============================================================
bool MansionEntranceDoor::Initialize(
    vnScene* scene,
    InteractionSystem* interactionSystem,
    const std::string& persistentId,
    const XMFLOAT3& position,
    const XMFLOAT3& scale
)
{
    if (scene == nullptr || interactionSystem == nullptr)
    {
        return false;
    }

    mVisualWidthExtra = 0.2f;
    mVisualHeightExtra = 0.1f;
    //========================================================
    // CSV配置情報
    //========================================================
    mPersistentId = persistentId;

    // Positionは閉じた状態の両扉の中央
    mClosedPosition = position;

    // Interaction判定位置は固定
    mInteractionPosition = position;

    mScale = scale;

    //========================================================
    // ドア全体の幅から片扉の幅を計算
    //========================================================
    mLeafWidth = scale.x * 0.5f;

    //========================================================
    // ゲーム開始時は玄関が開いている
    //========================================================
    mState = State::Open;
    mCurrentAngle = mMaxOpenAngle;

    mLocked = false;
    mIntroControlled = true;
    mLockWhenClosed = false;

    //========================================================
    // 左右のVNM
    // まだ変換していない場合に分かるよう
    // 生成前にファイルの存在を確認する
    //========================================================
    const wchar_t* folder = L"data/model/mansion/dynamic/entrance/";

    const wchar_t* files[2] =
    {
        L"main_entrance_double_door_left_01.vnm",
        L"main_entrance_double_door_right_01.vnm"
    };

    for (int i = 0; i < 2; ++i)
    {
        const std::filesystem::path path = std::filesystem::path(folder) / files[i];

        if (!std::filesystem::is_regular_file(path))
        {
#if defined(_DEBUG)
            OutputDebugStringW(
                L"[MansionEntranceDoor] "
                L"Entrance VNM not found.\n"
            );
#endif
            return false;
        }
    }

    //========================================================
    // 左右2枚のドアを生成
    //========================================================
    for (int i = 0; i < 2; ++i)
    {
        mLeafModels[i] = new ModelBase(folder, files[i]);

        if (mLeafModels[i] == nullptr)
        {
            return false;
        }

        //------------------------------------------------------------
        // ドア全体の描画幅
        //------------------------------------------------------------
        const float visualTotalWidth = mScale.x + mVisualWidthExtra;

        //------------------------------------------------------------
        // 1枚分の描画幅
        //------------------------------------------------------------
        const float visualLeafWidth = visualTotalWidth * 0.5f;

        //------------------------------------------------------------
        // ドアの描画高さ
        //------------------------------------------------------------
        const float visualHeight = mScale.y + mVisualHeightExtra;
        const float visualScaleX = visualLeafWidth / 1.50f;
        const float visualScaleY = visualHeight / 2.20f;


        //====================================================
        // Blenderで作成した各ドア板の実寸
        //====================================================
        mLeafModels[i]->setScale(
            visualScaleX,
            visualScaleY,
            1.0f
        );

        //====================================================
        // Shadow
        //====================================================
        mLeafModels[i]->setCastShadow(true);
        mLeafModels[i]->setReceiveShadow(true);

        mLeafModels[i]->SetShadowMobility(ShadowMobility::Dynamic);

        //====================================================
        // OBB Collider
        //====================================================
        const float colliderHalfThickness =
            (std::min)(scale.z * 0.5f, 0.09f);

        mLeafColliders[i] =
            std::make_unique<BoxCollider>(
                mLeafModels[i],
                XMVectorSet(
                    mLeafWidth * 0.5f,
                    scale.y * 0.5f,
                    colliderHalfThickness,
                    0.0f
                )
            );

        mLeafColliders[i]->SetType(ColliderType::Body);

        mLeafColliders[i]->SetTag(ColliderTag::Door);

        //====================================================
        // プレイヤー接触通知
        // Intro中および施錠中は開かない
        // 終盤の解錠後は、Eキーでも接触でも開ける
        //====================================================
        mLeafColliders[i]->SetContactCallback(
            [this](const Collider* other)
            {
                OnPlayerContact(other);
            }
        );
    }

    //========================================================
    // 初期開扉位置へ移動
    //========================================================
    ApplyLeafTransforms();

    //========================================================
    // Scene登録
    //========================================================
    for (ModelBase* model : mLeafModels)
    {
        scene->registerObject(model);
    }

    //========================================================
    // EキーInteraction登録
    //========================================================
    interactionSystem->Register(this);

    return true;
}


//============================================================
// Terminate
//============================================================
void MansionEntranceDoor::Terminate(vnScene* scene,InteractionSystem* interactionSystem)
{
    //========================================================
    // Interactionから先に登録解除
    //========================================================
    if (interactionSystem != nullptr)
    {
        interactionSystem->Unregister(this);
    }

    //========================================================
    // Collider削除
    //========================================================
    for (auto& collider : mLeafColliders)
    {
        collider.reset();
    }

    //========================================================
    // Sceneに登録した左右のModelを削除
    //========================================================
    if (scene != nullptr)
    {
        for (ModelBase*& model : mLeafModels)
        {
            if (model != nullptr)
            {
                scene->deleteObject(model);
                model = nullptr;
            }
        }
    }

    //========================================================
    // 状態リセット
    //========================================================
    mState = State::Open;
    mOpenedCallback = {};
    mCurrentAngle = mMaxOpenAngle;

    mLocked = false;
    mIntroControlled = true;
    mLockWhenClosed = false;
}


void MansionEntranceDoor::RestoreFromProgress(bool enteredMansion, bool escapeUnlocked)
{
    //============================================================
   // ゲーム開始直後
   // 玄関イベントが発生していないため
   // 左右の扉を開いた状態にする
   //============================================================
    if (!enteredMansion)
    {
        mState = State::Open;
        mCurrentAngle = mMaxOpenAngle;

        mLocked = false;
        mIntroControlled = true;
    }
    else
    {
        //========================================================
        // 屋敷へ入った後
        // EntranceLocked中のデータを読み込んだ場合も
        // 閉鎖演出を再生し直さず閉じた状態へ復元する
        //========================================================
        mState = State::Closed;
        mCurrentAngle = 0.0f;

        // 最終脱出前は施錠 最終脱出中は解錠
        mLocked = !escapeUnlocked;

        // 最終脱出中はプレイヤーが操作できるようにする
        mIntroControlled = !escapeUnlocked;
    }

    //============================================================
    // ※復元処理で閉鎖アニメーションは行わないため
    // ※閉鎖完了後の追加施錠予約も解除する
    //============================================================
    mLockWhenClosed = false;

    //============================================================
    // ※重要
    // ※状態変数だけでは見た目は更新されない
    // ※左右のモデルとOBB Colliderを同時に更新する
    //============================================================
    ApplyLeafTransforms();
}

//============================================================
// Intro Close + Lock
//============================================================
bool MansionEntranceDoor::BeginCloseAndLock()
{
    //========================================================
    // 既に閉じている / Closing中なら開始しない
    //========================================================
    if (mState != State::Open)
    {
        return false;
    }


    //========================================================
    // Player操作は禁止したまま
    //========================================================
    mIntroControlled = true;


    //========================================================
    // 閉じ終わったら施錠する
    //========================================================
    mLockWhenClosed = true;


    //========================================================
    // Closing開始
    //========================================================
    mState = State::Closing;


    //========================================================
    // Door Noise
    //========================================================
    if (mNoiseSystem != nullptr)
    {
        mNoiseSystem->Emit(
            mInteractionPosition,
            mDoorNoiseRadius,
            NoiseType::Door,
            1.0f
        );
    }


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[MansionEntranceDoor] "
        L"Intro closing started.\n"
    );

#endif

    return true;
}


//============================================================
// Final Escape Unlock
//============================================================
void MansionEntranceDoor::UnlockForEscape()
{
    mLocked = false;

    //========================================================
    // ここからPlayerによるInteractionを許可する
    //========================================================
    mIntroControlled = false;


#if defined(_DEBUG)

    OutputDebugStringW(
        L"[MansionEntranceDoor] "
        L"Unlocked for final escape.\n"
    );

#endif
}


//============================================================
// Begin Opening
//============================================================
bool MansionEntranceDoor::BeginOpening()
{
    if (mLocked)
    {
        return false;
    }


    if (mState != State::Closed)
    {
        return false;
    }


    mState = State::Opening;

    SoundBank::getInstance()->PlaySfx(SoundID::AE_EntranceDoorOpen, 0.50f);

    if (mOpenedCallback)
    {
        // CSVのPositionは閉じた扉の中心 ・高さの半分を引いて敷居を求める
        const XMFLOAT3 thresholdPosition{
            mClosedPosition.x,
            mClosedPosition.y - mScale.y * 0.5f,
            mClosedPosition.z
        };

        mOpenedCallback(thresholdPosition);
    }

    if (mNoiseSystem != nullptr)
    {
        mNoiseSystem->Emit(
            mInteractionPosition,
            mDoorNoiseRadius,
            NoiseType::Door,
            1.0f
        );
    }


    return true;
}

void MansionEntranceDoor::ApplyLeafTransforms()
{

    const float visualTotalWidth = mScale.x + mVisualWidthExtra;
    const float visualLeafWidth = visualTotalWidth * 0.5f;

    //============================================================
    // 高さ方向の補正
    //============================================================
    const float visualRise = mVisualHeightExtra * 0.5f;

    //============================================================
    // 左右の扉を更新
    //============================================================
    for (int i = 0; i < 2; ++i)
    {
        if (mLeafModels[i] == nullptr)
        {
            continue;
        }

        //========================================================
        // 左右の識別
        // i=0 : 左扉
        // i=1 : 右扉
        //========================================================
        const bool left = (i == 0);

        const float sign = left ? 1.0f : -1.0f;

        //========================================================
        // 現在のドア回転角度
        //========================================================
        const float yaw = sign * mCurrentAngle;

        const XMMATRIX rotation = XMMatrixRotationY(yaw);

        //========================================================
        // Collider用の中心座標
        //========================================================
        const float physicsHingeX = mClosedPosition.x - sign * mScale.x * 0.5f;

        const XMVECTOR physicsHinge = XMVectorSet(
            physicsHingeX,
            mClosedPosition.y,
            mClosedPosition.z,
            0.0f
        );

        //========================================================
        // 物理ドアの蝶番 → 中心
        //========================================================
        const XMVECTOR physicsOffset = XMVectorSet(
            sign * mLeafWidth * 0.5f,
            0.0f,
            0.0f,
            0.0f
        );

        const XMVECTOR physicsCenter =
            physicsHinge + XMVector3TransformNormal(physicsOffset, rotation);

        //========================================================
        // Visual用の蝶番座標
        //========================================================
        const float visualHingeX = mClosedPosition.x - sign * visualTotalWidth * 0.5f;

        const XMVECTOR visualHinge = XMVectorSet(
            visualHingeX,
            mClosedPosition.y + visualRise,
            mClosedPosition.z,
            0.0f
        );

        //========================================================
        // Visualドアの蝶番 → 中心
        //========================================================
        const XMVECTOR visualOffset = XMVectorSet(
            sign * visualLeafWidth * 0.5f,
            0.0f,
            0.0f,
            0.0f
        );

        const XMVECTOR visualCenter =
            visualHinge + XMVector3TransformNormal(visualOffset, rotation);

        //========================================================
        // 描画モデルのTransform
        // Visual用の中心位置を設定する
        // Collider用の中心とは一致しなくてもよい
        //========================================================
        mLeafModels[i]->setPosition(
            XMVectorGetX(visualCenter),
            XMVectorGetY(visualCenter),
            XMVectorGetZ(visualCenter)
        );

        mLeafModels[i]->setRotation(
            0.0f,
            yaw,
            0.0f
        );

        //========================================================
        // Colliderの中心位置を元の場所に維持
        // 描画モデルを拡大してもColliderは従来の位置を維持できる
        //========================================================
        if (mLeafColliders[i] != nullptr)
        {
            const XMVECTOR colliderOffset = physicsCenter - visualCenter;

            mLeafColliders[i]->SetOffset(colliderOffset);

            //====================================================
            // OBBの回転角度はVisualと同じ
            //====================================================
            mLeafColliders[i]->SetYawRadians(yaw);

            //====================================================
            // 開いた状態でもColliderは有効
            //====================================================
            mLeafColliders[i]->SetEnabled(true);
        }
    }
}


void MansionEntranceDoor::OnPlayerContact(const Collider* other)
{
    if (other == nullptr)
    {
        return;
    }

    //========================================================
    // Playerだけに反応
    //========================================================
    if (!other->HasTag(ColliderTag::Player))
    {
        return;
    }

    //========================================================
    // Intro中はイベントによる制御を優先
    //========================================================
    if (mIntroControlled)
    {
        return;
    }

    //========================================================
    // 施錠中は開かない
    //========================================================
    if (mLocked)
    {
        return;
    }

    //========================================================
    // 閉じている場合だけ開く
    //========================================================
    if (mState != State::Closed)
    {
        return;
    }

    BeginOpening();
}


//============================================================
// Update
//============================================================
void MansionEntranceDoor::Update(float dt)
{
    if (mState != State::Opening && mState != State::Closing)
    {
        return;
    }

    dt = std::clamp(dt, 0.0f, 0.1f);

    //========================================================
    // 目標角度
    // Opening -> 85度
    // Closing -> 0度
    //========================================================
    const float targetAngle = (mState == State::Opening) ? mMaxOpenAngle : 0.0f;

    const float difference = targetAngle - mCurrentAngle;

    const float step = mAngularSpeed * dt;

    //========================================================
    // 目標角度まで回転
    //========================================================
    if (fabsf(difference) <= step)
    {
        mCurrentAngle = targetAngle;

        if (mState == State::Opening)
        {
            mState = State::Open;
        }
        else
        {
            mState = State::Closed;

            SoundBank::getInstance()->PlaySfx(SoundID::AE_EntranceDoorSlam, 0.80f);
            //================================================
            // 玄関進入イベントの場合は施錠する
            //================================================
            if (mLockWhenClosed)
            {
                mLocked = true;
                mLockWhenClosed = false;

#if defined(_DEBUG)
                OutputDebugStringW(
                    L"[MansionEntranceDoor] "
                    L"Closed and locked.\n"
                );
#endif
            }
        }
    }
    else
    {
        //====================================================
        // 一定角速度で開閉
        //====================================================
        mCurrentAngle += (difference > 0.0f ? step : -step);
    }

    //========================================================
    // 両扉とOBBを同期
    //========================================================
    ApplyLeafTransforms();
}


//============================================================
// Interaction Position
//============================================================
XMVECTOR MansionEntranceDoor::GetInteractionPosition() const
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
bool MansionEntranceDoor::CanInteract() const
{
    //========================================================
    // Intro中はPlayer操作禁止
    //========================================================
    if (mIntroControlled)
    {
        return false;
    }


    //========================================================
    // Locked
    //========================================================
    if (mLocked)
    {
        return false;
    }


    //========================================================
    // Final EscapeではClosed → Openだけ許可
    //========================================================
    return mState == State::Closed;
}


//============================================================
// Interact
//============================================================
void MansionEntranceDoor::Interact(ExplorationPlayer& player)
{
    (void)player;

    BeginOpening();
}


//============================================================
// Interaction Text
//============================================================
const wchar_t* MansionEntranceDoor::GetInteractionText() const
{
    if (mLocked)
    {
        return L"Locked";
    }


    if (mState == State::Closed)
    {
        return L"Open Door";
    }


    return L"";
}