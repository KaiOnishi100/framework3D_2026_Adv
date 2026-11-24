#pragma once

#include "../../../interaction/IInteractable.h"

class ExplorationPlayer;


//============================================================
// DebugInteractObject
// InteractionSystem動作確認用のObject
// 役割:
// ・Scene上に表示する
// ・IInteractableを実装する
// ・Interactされたら状態を切り替える
// ・Debug Outputを出す
//============================================================
class DebugInteractObject : public ModelBase, public IInteractable
{
public:

    //========================================================
    // Constructor
    //========================================================
    DebugInteractObject(const WCHAR* folder, const WCHAR* modelFile);

    virtual ~DebugInteractObject() = default;


    //========================================================
    // Interaction Position
    // 今回はStatic Debug Objectなので、
    // 専用のInteraction Positionを保持して返す
    //========================================================
    XMVECTOR GetInteractionPosition() const override
    {
        return mInteractionPosition;
    }


    //========================================================
    // Interaction可能か
    //========================================================
    bool CanInteract() const override
    {
        return true;
    }


    //========================================================
    // Interaction実行
    //========================================================
    void Interact(ExplorationPlayer& player) override;


    //========================================================
    // UI表示用Text
    //========================================================
    const wchar_t* GetInteractionText() const override
    {
        return mActivated ? L"Deactivate Test Object" : L"Activate Test Object";
    }


    //========================================================
    // Position設定
    // vnObject側Positionと
    // Interaction判定位置を同時に更新する
    //========================================================
    void SetDebugPosition(
        float x,
        float y,
        float z
    );


    //========================================================
    // Debug State
    //========================================================
    bool IsActivated() const
    {
        return mActivated;
    }


private:

    //========================================================
    // Interaction判定に使用する位置
    //========================================================
    XMVECTOR mInteractionPosition = XMVectorZero();


    //========================================================
    // Eを押すたびにON/OFFする
    //========================================================
    bool mActivated = false;
};