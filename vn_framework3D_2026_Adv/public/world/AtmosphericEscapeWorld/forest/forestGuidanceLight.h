#pragma once

//============================================================
// ForestGuidanceLight
// 森の奥に配置する誘導灯
// 役割:
// ・赤い発光Modelを表示
// ・周囲に赤いPoint Lightを出す
//============================================================
class ForestGuidanceLight
{
public:

    bool Initialize(vnScene* scene, const XMFLOAT3& position);

    void Terminate(vnScene* scene);

    void Update();


private:

    ModelBase* mLightModel = nullptr;


    XMFLOAT3 mPosition =
    {
        0.0f,
        0.0f,
        0.0f
    };


    float mRange = 8.0f;

    float mIntensity = 12.0f;
};