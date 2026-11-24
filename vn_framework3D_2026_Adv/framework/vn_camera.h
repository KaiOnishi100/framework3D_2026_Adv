//--------------------------------------------------------------//
//	"vn_camera.h"												//
//		カメラ													//
//													2026/01/01	//
//														Ichii	//
//--------------------------------------------------------------//
#pragma once

class MeshCollider;
class Collider;
class BoxCollider;


class vnCamera
{

    //============================================================
    // Camera state
    //============================================================
    static XMVECTOR Position;   // カメラ座標
    static XMVECTOR Target;     // 注視点
    static XMVECTOR Up;         // 上方向

    //============================================================
    // Projection
    //============================================================
    static float FovY;          // 視野角
    static float Aspect;        // アスペクト比
    static float zNear;         // 前方クリッピング面
    static float zFar;          // 後方クリッピング面

    static float BaseFov;       // 通常FOV
    static float MaxFov;        // 最大FOV

    //============================================================
    // Follow settings
    //============================================================
    static float BaseDist;      // 通常距離
    static float MinDist;       // 最小距離

    static float FollowYaw;     // 水平角
    static float FollowPitch;   // 上下角
    static float FollowDist;    // プレイヤーからの距離

    //============================================================
    // Pad input settings
    //============================================================
    static float PadYawSpeed;   // 水平回転速度
    static float PadPitchSpeed; // 垂直回転速度
    static float PadDeadZone;   // デッドゾーン
    static bool  InvertPadY;    // Y反転

    //============================================================
    // Collision settings
    //============================================================
    static float sCameraRadius; // カメラ球半径
    static float sLookAtHeight; // 注視点の持ち上げ量

    //============================================================
    // Shake
    //============================================================
    static float ShakeTimer;
    static float ShakePower;
    static float ShakeDuration;

    //============================================================
    // Lock-on
    //============================================================
    static bool     IsLockOn;
    static XMVECTOR LockOnTarget;

    //============================================================
    // Matrices / viewport
    //============================================================
    static XMMATRIX View;
    static XMMATRIX Proj;
    static XMMATRIX Screen;

    static D3D12_VIEWPORT Viewport;

private:
    //============================================================
    // Internal update helpers
    //============================================================
    static void UpdateRotationByMouse();
    static void UpdateRotationByPad(float dt);
    static void ClampFollowPitch();
    static void NormalizeFollowYaw();

    //============================================================
    // Internal follow helpers
    //============================================================
    static XMVECTOR BuildLookAt(const XMVECTOR& targetPos);
    static XMVECTOR BuildDesiredPosNormal(const XMVECTOR& lookAt);
    static XMVECTOR BuildDesiredPosLockOn(const XMVECTOR& playerPos);
    static XMVECTOR BuildFollowDirection(float pitchOffset = 0.0f);    // FollowYaw / FollowPitchを一人称と三人称で共通の前方ベクトルに変換する


    //============================================================
    // Internal utility helpers
    //============================================================
    static void  ClampCameraAboveGround(const MeshCollider* stageMesh);
    static void  ApplyShake(float dt);

    static float Saturate(float x);
    static float SmoothDampF(float current, float target, float speed, float dt);
    static void  UpdateAutoFovByDistance(float actualDist, float dt);// 現在は未使用 近距離時の自動FOV拡張用
    static float ApplyDeadZone(float v, float dead);

    //============================================================
    // Collision helpers
    //============================================================
    static bool Camera_CheckSphereVsMesh(
        const XMVECTOR& center,
        float radius,
        const MeshCollider* mesh,
        float* outPush,
        XMVECTOR* outNormal
    );

    static void Camera_ResolveSphereVsMesh(
        XMVECTOR& ioCenter,
        float radius,
        const MeshCollider* mesh
    );

    static XMVECTOR Camera_FindSafePosOnSegment(
        const XMVECTOR& lookAt,
        const XMVECTOR& desired,
        float radius,
        const MeshCollider* mesh
    );

    static bool Camera_RaycastDownGroundY(
        const XMVECTOR& fromPos,
        const MeshCollider* mesh,
        float* outGroundY
    );

    //============================================================
    // AtmosphericEscape用 Camera Collision
    // CameraをSphereとして扱い
    // BoxColliderとの衝突を調べる
    //============================================================
    static bool Camera_CheckSphereVsBox(
        const XMVECTOR& center,
        float radius,
        const BoxCollider* box,
        float* outPush,
        XMVECTOR* outNormal
    );


    //============================================================
    // Camera Sphere vs Box の押し出し
    // Cameraが壁の中へ入ってしまった場合の最終安全処理
    //============================================================
    static void Camera_ResolveSphereVsBoxes(
        XMVECTOR& ioCenter,
        float radius,
        const std::vector<Collider*>& colliders
    );


    //============================================================
    // Sphere Sweep vs Box
    // start → end の移動経路上にBoxがあるか判定する
    // BoxをCamera Radius分だけ膨張させることで
    // Sphereを動かす問題をPointを膨張Boxへ飛ばす問題へ変換する
    //============================================================
    static bool Camera_SweepSphereVsBox(
        const XMVECTOR& start,
        const XMVECTOR& end,
        float radius,
        const BoxCollider* box,
        float* outHitT
    );


    //============================================================
    // Player注視点から理想Camera位置までの間で、
    // 最初にぶつかるBoxの手前へCameraを移動する
    //============================================================
    static XMVECTOR Camera_FindSafePosOnBoxes(
        const XMVECTOR& lookAt,
        const XMVECTOR& desired,
        float radius,
        const std::vector<Collider*>& colliders
    );

public:
    //============================================================
    // Lifecycle
    //============================================================
    static bool initialize();
    static void terminate();

    //============================================================
    // Input update
    //============================================================
    static void UpdateFollowAngles(float dt);

    //============================================================
    // Matrices / viewport
    //============================================================
    static XMMATRIX* getView();
    static XMMATRIX* getProj();
    static XMMATRIX* getScreen();
    static D3D12_VIEWPORT* getViewport();

    //============================================================
    // Getters
    //============================================================
    static const XMVECTOR* getPosition();
    static float getPositionX();
    static float getPositionY();
    static float getPositionZ();

    static const XMVECTOR* getTarget();
    static float getTargetX();
    static float getTargetY();
    static float getTargetZ();

    static const XMVECTOR* getUp();
    static float getUpX();
    static float getUpY();
    static float getUpZ();

    static float getFovY();
    static float getAspect();
    static float getNearZ();
    static float getFarZ();


    static XMVECTOR getForward();
    static XMVECTOR getRight();

    //============================================================
    // Setters
    //============================================================
    static void setPosition(float x, float y, float z);
    static void setPosition(const XMVECTOR* v);
    static void setPositionX(float value);
    static void setPositionY(float value);
    static void setPositionZ(float value);

    static void setTarget(float x, float y, float z);
    static void setTarget(const XMVECTOR* v);
    static void setTargetX(float value);
    static void setTargetY(float value);
    static void setTargetZ(float value);

    static void setUp(float x, float y, float z);
    static void setUp(const XMVECTOR* v);
    static void setUpX(float value);
    static void setUpY(float value);
    static void setUpZ(float value);

    static void setFovY(float value);
    static void setAspect(float value);

    //============================================================
    // Adders
    //============================================================
    static void addPosition(float x, float y, float z);
    static void addPosition(const XMVECTOR* v);
    static void addPositionX(float value);
    static void addPositionY(float value);
    static void addPositionZ(float value);

    static void addTarget(float x, float y, float z);
    static void addTarget(const XMVECTOR* v);
    static void addTargetX(float value);
    static void addTargetY(float value);
    static void addTargetZ(float value);

    static void addFovY(float value);
    static void addAspect(float value);

    //============================================================
    // Camera behaviors
    //============================================================
    static void FollowFirstPerson(const XMVECTOR& playerPosition, float eyeHeight, float pitchOffset = 0.0f);
    static void FollowTarget(const XMVECTOR& targetPos, float dt, const MeshCollider* stageMesh);
    static void FollowDeathTarget(const XMVECTOR& focusPos, float dt, float backDist, float height, const MeshCollider* stageMesh);

    static void FollowTargetWithColliders(
        const XMVECTOR& targetPos,
        float dt,
        const std::vector<Collider*>& colliders,
        bool snap = false
    );

    static void UpdateSpeedCamera(float speedRate, float dt);

    static void StartShake(float power, float duration);
    static void StartLockOn(const XMVECTOR& target);
    static void EndLockOn();

public:

    enum GameType
    {
        EyeKillYou = 0,
        AtmosphericEscape = 1
    };

    static GameType gameType;
};