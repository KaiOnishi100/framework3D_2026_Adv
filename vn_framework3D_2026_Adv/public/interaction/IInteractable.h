#pragma once

#include "../../framework/vn_environment.h"

class ExplorationPlayer;


//============================================================
// IInteractable
// Playerから操作可能なObjectの共通Interface
//============================================================
class IInteractable
{
public:

    virtual ~IInteractable() = default;


    //========================================================
    // Interaction Position
    // Playerとの距離判定に使用する位置
    //========================================================
    virtual XMVECTOR GetInteractionPosition() const = 0;

    // プレイヤーの位置や状態に応じた条件を持つクラスが上書きする
    virtual bool CanInteractFrom(const ExplorationPlayer& player) const
    {
        (void)player;
        return CanInteract();
    }

    //========================================================
    // 現在Interaction可能か
    //========================================================
    virtual bool CanInteract() const
    {
        return true;
    }


    //========================================================
    // Interaction実行
    //========================================================
    virtual void Interact(ExplorationPlayer& player) = 0;


    //========================================================
    // UI表示用
    //========================================================
    virtual const wchar_t* GetInteractionText() const
    {
        return L"Interact";
    }
};