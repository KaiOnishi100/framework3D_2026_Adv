#pragma once

#include "../importer/ImportedModel.h"


//============================================================
// AssetScaleProcessor
// 外部Assetの座標単位を
// Engine World Unitへ統一するためのProcessor
// Engine側ルール: 1.0f = 1 meter
//============================================================
class AssetScaleProcessor
{
public:

    //========================================================
    // Uniform Scaleを適用
    // scale:
    //     1.0  = 変更なし
    //     0.01 = 1/100
    //     0.005 = 1/200
    // Static Modelだけでなく、
    // Skinned Modelの場合はSkeleton / InverseBindも
    // 同じ単位系へ変換する
    //========================================================
    static bool Apply(ImportedModel& model, float scale);
};