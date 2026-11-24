#pragma once

#include "../../../interaction/IInteractable.h"
#include "../../../interface/INavigationDoor.h"
#include "../../../inventory/inventoryManager.h"
#include "../save/atmosphericEscapeSaveData.h"

class vnScene;
class InteractionSystem;
class ExplorationPlayer;
class NoiseSystem;

//============================================================
// MansionDoor
// 屋敷内で使用する通常Door
// Player:
// ・Open
// ・Close
// Pursuer:
// ・INavigationDoor経由でOpen可能
// Save:
// ・Open / Closed
// ・Locked
//============================================================
class MansionDoor : public IInteractable, public IPersistentWorldObject, public INavigationDoor
{
public:

    //========================================================
    // Door State
    //========================================================
    enum class State
    {
        Closed,
        Opening,
        Open,
        Closing
    };


public:

    MansionDoor() = default;

    ~MansionDoor() override = default;


    //========================================================
    // Initialize
    // scale:
    // CSVのFull Size
    // 横壁用: (2.0, 2.2, 0.25)
    // 縦壁用: (0.25, 2.2, 2.0)
    // ScaleからDoorのSliding方向を自動判定する
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const XMFLOAT3& position,
        const XMFLOAT3& scale,
        const XMFLOAT3& rotation
    );


    //========================================================
    // Terminate
    //========================================================
    void Terminate(
        vnScene* scene,
        InteractionSystem* interactionSystem
    );


    //========================================================
    // Update
    //========================================================
    void Update(float dt);


    //========================================================
    // Collider
    //========================================================
    Collider* GetCollider() const
    {
        return mCollider.get();
    }


    //========================================================
    // State
    //========================================================
    State GetState() const
    {
        return mState;
    }


    bool IsClosed() const
    {
        return mState == State::Closed;
    }


    //========================================================
    // Lock
    //========================================================
    void SetLocked(bool locked)
    {
        //====================================================
        // 動作中のDoorを途中でLockしない
        //====================================================
        if (mState != State::Closed)
        {
            return;
        }

        mLocked = locked;
    }


    bool IsLocked() const
    {
        return mLocked;
    }


    //========================================================
    // Playerが閉められるか
    //========================================================
    void SetCanClose(bool canClose)
    {
        mCanClose = canClose;
    }


    //========================================================
    // Noise
    //========================================================
    void SetNoiseSystem(NoiseSystem* noiseSystem )
    {
        mNoiseSystem = noiseSystem;
    }


public:

    //========================================================
    // INavigationDoor
    //========================================================
    bool IsOpen() const override
    {
        return mState == State::Open;
    }

    //============================================================
    // AI Navigation
    //============================================================
    bool IsPassableForAI() const override
    {
        //========================================================
        // Open中なら当然通行可能
        //========================================================
        if (IsOpen())
        {
            return true;
        }

        //========================================================
        // Unlockされている通常Doorなら
        // ClosedでもAIがRequestOpenFromAI()で開けられる
        // LockedならNavigation上から通行不可
        //========================================================
        return !mLocked;
    }

    const XMFLOAT3& GetDoorPosition() const override
    {
        //====================================================
        // ModelはOpening時に移動するが
        // Navigation上では入口中央を返す
        //====================================================
        return mInteractionPosition;
    }


    bool RequestOpenFromAI() override;

    //====================================================
    // World の初期化時に、開扉開始の通知先を設定する
    //====================================================
    void SetOpenedCallback(std::function<void(const XMFLOAT3&)> callback)
    {
        mOpenedCallback = std::move(callback);
    }

    bool GetOpenDetourPoints(float clearance, XMFLOAT3& pointA, XMFLOAT3& pointB) const;


public:

    //========================================================
    // IInteractable
    //========================================================
    XMVECTOR GetInteractionPosition() const override;

    bool CanInteract() const override;

    void Interact(ExplorationPlayer& player) override;

    const wchar_t* GetInteractionText() const override;


public:

    //========================================================
    // IPersistentWorldObject
    //========================================================
    const std::string& GetPersistentId() const override
    {
        return mPersistentId;
    }

    WorldObjectSaveState CaptureSaveState() const override;

    void RestoreSaveState(const WorldObjectSaveState& data) override;

public:

    //============================================================
    // Inventory Manager
    //============================================================
    void SetInventoryManager(InventoryManager* inventoryManager)
    {
        mInventoryManager = inventoryManager;
    }

    //============================================================
    // Required Item
    //============================================================
    void SetRequiredItemId( const std::string& itemId)
    {
        mRequiredItemId = itemId;

        if (!mRequiredItemId.empty())
        {
            mLocked = true;
        }
    }


    bool RequiresItem() const
    {
        return !mRequiredItemId.empty();
    }

private:

    bool BeginOpening();

    bool BeginClosing();

    void SetOpenImmediate();

    void SetClosedImmediate();

    //============================================================
    // ドアのTransform更新
    //============================================================
    void ApplyDoorTransform();

    //============================================================
    // 接触者の位置から開く方向を選択
    //============================================================
    void SetOpenDirectionFromPosition(const XMVECTOR& worldPosition);

    //============================================================
    // Player Colliderの接触通知
    //============================================================
    void OnPlayerContact(const Collider* other);


private:

    ModelBase* mModel = nullptr;

    std::unique_ptr<BoxCollider> mCollider;


    //========================================================
    // Fixed Door Position
    //========================================================
    XMFLOAT3 mClosedPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    //========================================================
    // Door格納位置
    //========================================================
    XMFLOAT3 mOpenPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    //========================================================
    // Interaction / Navigation固定位置
    //========================================================
    XMFLOAT3 mInteractionPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    XMFLOAT3 mScale =
    {
        1.0f,
        1.0f,
        1.0f
    };


    State mState = State::Closed;


    bool mLocked = false;

    //========================================================
    // ホラーゲームなので通常Doorは
    // Playerが再び閉められるようにする
    //========================================================
    bool mCanClose = true;

    //========================================================
    // Movement
    //========================================================
    float mMoveSpeed = 1.8f;

    //========================================================
    // Noise
    //========================================================
    NoiseSystem* mNoiseSystem = nullptr;

    float mDoorNoiseRadius = 7.0f;

    //========================================================
    // Save ID
    //========================================================
    std::string mPersistentId;

    //============================================================
    // 鍵付きDoor用
    //============================================================
    InventoryManager* mInventoryManager = nullptr;

    std::string mRequiredItemId;

    //============================================================
    // Hinged Door
    //============================================================
    //============================================================
    // 閉じたときのドアYaw
    // 横壁用ドア：0度
    // 縦壁用ドア：-90度
    // DirectXMathのラジアンで管理する
    //============================================================
    float mBaseYaw = 0.0f;

    //============================================================
    // ドアの現在の開閉角度
    // 0 = 閉じた状態
    // ±最大角度 = 開いた状態
    //============================================================
    float mCurrentAngle = 0.0f;

    //============================================================
    // 最大開閉角度・回転速度
    // 85度まで開く
    // 回転速度は90度/秒
    //============================================================
    float mMaxOpenAngle = XMConvertToRadians(85.0f);
    float mAngularSpeed = XMConvertToRadians(90.0f);

    //============================================================
    // 開く方向
    // +1: 正方向
    // -1: 逆方向
    //============================================================
    float mOpenDirection = 1.0f;

    //============================================================
    // 蝶番の取り付け側
    // +1: ローカルXのマイナス側に蝶番
    // -1: ローカルXのプラス側に蝶番
    // 初期値では全ドア同じ側
    // 部屋ごとに変更可能
    //============================================================
    float mHingeSign = 1.0f;

    //============================================================
    // ドアの幅の半分
    //============================================================
    float mHalfDoorWidth = 1.0f;

    //============================================================
    // 蝶番のWorld座標
    //============================================================
    XMFLOAT3 mHingePosition = {};

    //============================================================
    // 接触開扉の再発動防止
    // Eキーで閉めた直後接触して即座に再び開く
    // 現象を軽減するためのCooldown
    //============================================================
    float mPushCooldown = 0.0f;

    //============================================================
    // 開閉方向をCSVで固定するか
    // false：プレイヤーの立ち位置から自動決定
    // true ：CSVのRotYで固定
    //============================================================
    bool mUseFixedOpenDirection = false;

    //============================================================
    // CSVに指定した開閉方向
    //============================================================
    float mConfiguredOpenDirection = 1.0f;

    //============================================================
    // ドアの描画モデルだけに追加する高さ
    //============================================================
    float mVisualHeightExtra = 0.0f;

    std::function<void(const XMFLOAT3&)> mOpenedCallback;
};