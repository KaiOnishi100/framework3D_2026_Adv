#pragma once

class IInteractable;
class ExplorationPlayer;

//============================================================
// InteractionSystem
// Scene内に存在するInteractableを管理する
//============================================================
class InteractionSystem
{
public:

    //========================================================
    // Register
    //========================================================
    void Register(IInteractable* interactable);


    //========================================================
    // Unregister
    //========================================================
    void Unregister(IInteractable* interactable);


    //========================================================
    // 最も適切なInteraction Targetを探す
    // position: Player位置
    // forward: Player正面方向
    // maxDistance: 最大Interaction距離
    // minFacingDot: 正面判定
    //========================================================
    IInteractable* FindBestTarget(
        const XMVECTOR& position,
        const XMVECTOR& forward,
        float maxDistance,
        float minFacingDot,
        const ExplorationPlayer* player = nullptr
    ) const;


private:

    //========================================================
    // 非所有Pointer
    // ObjectのLifetimeはScene側が管理する
    //========================================================
    std::vector<IInteractable*> mInteractables;
};