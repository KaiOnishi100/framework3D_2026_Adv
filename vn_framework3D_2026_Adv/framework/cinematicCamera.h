#pragma once

//============================================================
// 演出用カメラ（Cinematic）
// ・Start() した瞬間の vnCamera のPos/Targetを保持
// ・Update() で “演出カメラのPos/Target” を計算し、
//   そこへスムーズに補間して反映する
// ・FinishしたらActive=false にして Scene側で通常カメラへ戻す
//============================================================
class CinematicCamera
{
public:
    // duration: 演出カメラへ寄せる（ブレンド）時間
    void Start(float duration);

    // 強制停止（途中でも通常カメラへ戻す用途）
    void Stop();

    bool IsActive() const { return mActive; }

    // bossPos/playerPos からカメラを作る
    void Update(float dt, const XMVECTOR& bossPos, float bossRadius, const XMVECTOR& playerPos);

private:
    bool  mActive = false;

    // 経過時間
    float mT = 0.0f;

    // ブレンド時間
    float mDuration = 1.0f;

    // Startした瞬間の元のカメラ状態を保持
    XMVECTOR mStartPos = XMVectorZero();
    XMVECTOR mStartTarget = XMVectorZero();

    // 最後に演出で反映した値
    XMVECTOR mLastPos = XMVectorZero();
    XMVECTOR mLastTarget = XMVectorZero();
};