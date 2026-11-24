#pragma once

class ExplorationPlayer;
class InteractionSystem;
class IInteractable;


//============================================================
// PlayerInteraction
// PlayerのInteraction機能を担当
//============================================================
class PlayerInteraction
{
public:

    //========================================================
    // Interaction System設定
    //========================================================
    void SetInteractionSystem(InteractionSystem* system )
    {
        mSystem = system;
    }


    //========================================================
    // 入力受付
    //========================================================
    void RequestInteract()
    {
        mInteractRequested = true;
    }


    //========================================================
    // Update
    //========================================================
    void Update(ExplorationPlayer& player);


    //========================================================
    // 現在選択されているTarget
    //========================================================
    IInteractable* GetCurrentTarget() const
    {
        return mCurrentTarget;
    }


private:

    InteractionSystem* mSystem = nullptr;

    IInteractable* mCurrentTarget =  nullptr;

    bool mInteractRequested = false;

    //========================================================
    // Interaction Parameters
    //========================================================

    // Playerから操作できる距離
    float mMaxDistance = 2.5f;
    // 正面Cone
    float mMinFacingDot = 0.5f;
};