#pragma once

#include <DirectXMath.h>

//============================================================
// INavigationDoor
// PursuerなどのAIがNavigation中に扱えるDoorのInterface
//============================================================
class INavigationDoor
{
public:

    virtual ~INavigationDoor() = default;


    //========================================================
    // Doorが完全にOpenか
    // true: AIはそのまま通過してよい
    // false: Closed / Opening / Closing / Locked等
    //========================================================
    virtual bool IsOpen() const = 0;

    //========================================================
    // AIがこのDoorを通行可能か
    // true:
    // ・すでにOpen
    // ・ClosedだがUnlockされておりAIが開けられる
    // false:
    // ・Locked
    // ・その他AIが通行してはいけないDoor
    //========================================================
    virtual bool IsPassableForAI() const = 0;

    //========================================================
    // Navigation上のDoor位置
    // Door ModelがOpening中に横へ移動しても、
    // 通路中央の固定位置を返す
    //========================================================
    virtual const DirectX::XMFLOAT3& GetDoorPosition() const = 0;


    //========================================================
    // AIからDoorを開ける
    // return:
    // true
    //      この呼び出しでOpening開始
    // false
    //      ・すでにOpening中
    //      ・Open済み
    //      ・Locked
    //      ・その他Opening不可
    //========================================================
    virtual bool RequestOpenFromAI() = 0;
};