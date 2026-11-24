#include "../framework.h"
#include "vn_environment.h"

#include "cinematicCamera.h"

void CinematicCamera::Start(float duration)
{
    mActive = true;
    mT = 0.0f;

    // 0除算防止
    mDuration = (duration > 0.001f) ? duration : 0.001f;

    // ------------------------------------------------------------
    // 開始時点の vnCamera 状態を保存
    // ------------------------------------------------------------
    mStartPos = *vnCamera::getPosition();
    mStartTarget = *vnCamera::getTarget();

    // ついでにLastにも入れておく（安全）
    mLastPos = mStartPos;
    mLastTarget = mStartTarget;
}

void CinematicCamera::Stop()
{
    // ここでは単にOFF
    // ※Scene側が通常カメラへ戻す
    mActive = false;
}

void CinematicCamera::Update(float dt, const XMVECTOR& bossPos, float bossRadius, const XMVECTOR& playerPos)
{
    if (!mActive) return;

    // ------------------------------------------------------------
    // 0～1補間（ゆっくり入る）
    // ------------------------------------------------------------
    mT += dt;
    float t = mT / mDuration;
    if (t > 1.0f) t = 1.0f;

    // ゆっくり入ってゆっくり止まる
    float s = Calculate::SmoothStep01(t);

    // ------------------------------------------------------------
    // 注視点（Target）
    // BossとPlayerの中間：少しBoss寄りだと “ボスが主役” になりやすい
    // ------------------------------------------------------------
    XMVECTOR desiredTarget = XMVectorLerp(playerPos, bossPos, 0.65f);

    // ちょっと上を見ると気持ち良い（巨大ボスだと特に）
    desiredTarget = desiredTarget + XMVectorSet(0.0f, bossRadius * 0.25f, 0.0f, 0.0f);

    // ------------------------------------------------------------
    // カメラの向きの基準ベクトル
    // “プレイヤー→ボス” を水平成分で作る（Yは殺す）
    // ------------------------------------------------------------
    XMVECTOR dir = bossPos - playerPos;
    dir = XMVectorSetY(dir, 0.0f);

    // 距離がゼロに近い場合の保険（Normalize事故回避）
    if (XMVectorGetX(XMVector3LengthSq(dir)) < 0.0001f)
        dir = XMVectorSet(0, 0, 1, 0);

    dir = XMVector3Normalize(dir);

    // ------------------------------------------------------------
    // 距離と高さ（ボスサイズでスケール）
    // ・巨大ボスほど引く
    // ・少し高めにして “見下ろし＋主役感” を作る
    // ------------------------------------------------------------
    const float baseDist = 25.0f + bossRadius * 2.2f; // 好みで調整
    const float height = 10.0f + bossRadius * 0.8f; // 好みで調整

    XMVECTOR desiredPos =
        desiredTarget
        - dir * baseDist
        + XMVectorSet(0.0f, height, 0.0f, 0.0f);

    // ------------------------------------------------------------
    // ここが重要：開始時点（mStart）→ 演出目標（desired）へ補間
    // vnCamera は “blend付きSet” が無いのでこちらでLerpして渡す
    // ------------------------------------------------------------
    XMVECTOR outPos = XMVectorLerp(mStartPos, desiredPos, s);
    XMVECTOR outTarget = XMVectorLerp(mStartTarget, desiredTarget, s);

    // vnCameraへ反映
    vnCamera::setPosition(&outPos);
    vnCamera::setTarget(&outTarget);

    // 最後に保存（Stop時のつなぎ等に使える）
    mLastPos = outPos;
    mLastTarget = outTarget;

    // ------------------------------------------------------------
    // 100%到達したら終了
    // ここは好み：
    // ・「一回寄って終わり」ならこのままでOK
    // ・「寄ったあと少しキープ」したいなら “Hold時間” を足して段階制に
    // ------------------------------------------------------------
    if (t >= 1.0f)
    {
        mActive = false;
    }
}