#pragma once

//--------------------------------------------
// UI操作可能オブジェクトのインターフェイス
//--------------------------------------------
class IUIControllable
{
public:
    virtual ~IUIControllable() = default;

    // 選択移動（-1/0/+1）
    virtual void MoveUIHorizontal(int dir) = 0; // -1=左 +1=右
    virtual void MoveUIVertical(int dir) = 0; // -1=上 +1=下

    // 決定/キャンセル
    virtual void Submit() = 0;
    virtual void Cancel() = 0;
};