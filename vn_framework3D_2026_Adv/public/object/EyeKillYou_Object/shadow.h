#pragma once

class Shadow : public Object
{
public:
	Shadow(const WCHAR* folder, const WCHAR* file);

    // 追従させたい対象と、影の基準サイズなど
    void SetTarget(Object* target) { mTarget = target; }

    void SetBaseScale(float s) { mBaseScale = s; }
    void SetMinScale(float s) { mMinScale = s; }

    // ボス用：yがこの範囲で小さくなる
    void SetHeightRange(float minY, float maxY) { mMinY = minY; mMaxY = maxY; }

    // 毎フレーム呼ぶ
    void UpdateShadow(float groundY);

private:
    Object* mTarget = nullptr;

    float mBaseScale = 1.0f; // 地面にいる時の影サイズ
    float mMinScale = 0.3f; // 最大高度の時の影サイズ

    float mMinY = 0.0f;      // この高さから
    float mMaxY = 45.0f;     // この高さまでで縮む
};