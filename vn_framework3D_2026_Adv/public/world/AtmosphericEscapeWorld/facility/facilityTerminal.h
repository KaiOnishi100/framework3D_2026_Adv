#pragma once

#include "../../../interaction/IInteractable.h"


class vnScene;
class InteractionSystem;
class ExplorationPlayer;
class NoiseSystem;

//============================================================
// FacilityTerminal
// 施設内部に配置する操作端末
// ・IInteractableを実装
// ・Playerが近づくと "[E] Use Terminal"
// ・一度操作するとActivated状態になる
// ・Activated後は再操作不可
//============================================================
class FacilityTerminal : public IInteractable, public IPersistentWorldObject
{
public:

    //========================================================
    // State
    //========================================================
    enum class State
    {
        Ready,
        Activated
    };


public:

    FacilityTerminal() = default;

    ~FacilityTerminal() override = default;


    //========================================================
    // Initialize
    // position: Terminal中心のWorld Position
    // scale: CSVから取得したFull Size
    //========================================================
    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const XMFLOAT3& position,
        const XMFLOAT3& scale
    );


    //========================================================
    // Terminate
    //========================================================
    void Terminate(vnScene* scene, InteractionSystem* interactionSystem);


    //========================================================
    // Collider
    //========================================================
    Collider* GetCollider() const
    {
        return mCollider.get();
    }


    //========================================================
    // Activated
    //========================================================
    bool IsActivated() const
    {
        return mState == State::Activated;
    }


public:

    //========================================================
    // IInteractable
    //========================================================
    XMVECTOR GetInteractionPosition() const override;


    bool CanInteract() const override;


    void Interact(ExplorationPlayer& player) override;


    const wchar_t* GetInteractionText() const override;

    //============================================================
    // Noise System
    //============================================================
    void SetNoiseSystem(NoiseSystem* noiseSystem)
    {
        mNoiseSystem = noiseSystem;
    }

    const std::string& GetPersistentId() const;

    WorldObjectSaveState CaptureSaveState() const;

    void RestoreSaveState(const WorldObjectSaveState& data);

    //============================================================
    // Load用即時設定
    //============================================================
    void SetActivatedImmediate(bool activated);


private:

    //========================================================
    // Model
    //========================================================
    ModelBase* mModel = nullptr;

    //============================================================
    // Save / Loadで使用する固有ID
    //============================================================
    std::string mPersistentId;

    //========================================================
    // Collision
    //========================================================
    std::unique_ptr<BoxCollider> mCollider;


    //========================================================
    // Interaction Position
    // ※Model Positionとは分離して保持しておく
    //========================================================
    XMFLOAT3 mInteractionPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    //========================================================
    // State
    //========================================================
    State mState = State::Ready;

    //============================================================
    // Noise System
    // Terminal操作時のGameplay Noiseを発生させるために使用
    //============================================================
    NoiseSystem* mNoiseSystem = nullptr;

    //============================================================
    // Terminal Noise Radius
    //============================================================
    float mActivationNoiseRadius = 14.0f;

};