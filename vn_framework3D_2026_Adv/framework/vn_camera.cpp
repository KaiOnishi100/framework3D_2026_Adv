//--------------------------------------------------------------//
//	"vn_camera.cpp"												//
//		カメラ													//
//													2026/01/01	//
//														Ichii	//
//--------------------------------------------------------------//
#include "../framework.h"
#include "vn_environment.h"

#include "AtmosphericEscapeOptions.h"

//============================================================
// Static members
//============================================================
vnCamera::GameType vnCamera::gameType = GameType::AtmosphericEscape;

XMVECTOR		vnCamera::Position;
XMVECTOR		vnCamera::Target;
XMVECTOR		vnCamera::Up;

float			vnCamera::FovY;
float			vnCamera::Aspect;
float			vnCamera::zNear;
float			vnCamera::zFar;

float           vnCamera::BaseDist = 10.0f;
float           vnCamera::MinDist = 4.0f;   

float           vnCamera::sCameraRadius = 0.5f;
float           vnCamera::sLookAtHeight = 0.75f;

float           vnCamera::BaseFov = XMConvertToRadians(45.0f);
float           vnCamera::MaxFov = XMConvertToRadians(95.0f);     //最大視野角


float           vnCamera::FollowYaw = 0.0f;
float           vnCamera::FollowPitch = 0.3f;   // 少し上から見る
float           vnCamera::FollowDist = 10.0f;

float           vnCamera::PadYawSpeed = 2.2f;   // rad/sec 相当のスケール（後で dt と掛ける）
float           vnCamera::PadPitchSpeed = 2.0f; // rad/sec
float           vnCamera::PadDeadZone = 0.20f;  // 0.0～1.0
bool            vnCamera::InvertPadY = true;   // 上下反転したいなら true

float           vnCamera::ShakeTimer = 0.0f;
float           vnCamera::ShakePower = 0.0f;
float           vnCamera::ShakeDuration = 0.0f;

bool            vnCamera::IsLockOn = false;
XMVECTOR        vnCamera::LockOnTarget = XMVectorZero();

XMMATRIX		vnCamera::View;
XMMATRIX		vnCamera::Proj;
XMMATRIX		vnCamera::Screen;

D3D12_VIEWPORT	vnCamera::Viewport;

//============================================================
// Lifecycle
//============================================================

bool vnCamera::Camera_SweepSphereVsBox(const XMVECTOR& start, const XMVECTOR& end, float radius, const BoxCollider* box, float* outHitT)
{
	if (box == nullptr || !box->IsEnabled())
	{
		return false;
	}

	//========================================================
	// BoxをCamera Radius分膨張させる
	// Sphere vs Box
	//      ↓
	// Point vs Expanded Box
	// として処理できる
	//========================================================
	const XMVECTOR expand =
		XMVectorSet(
			radius,
			radius,
			radius,
			0.0f
		);


	const XMVECTOR boxCenter =
		box->GetCenter();

	const XMVECTOR halfSize =
		box->GetHalfSize();


	const XMVECTOR boxMin =
		boxCenter - halfSize - expand;

	const XMVECTOR boxMax =
		boxCenter + halfSize + expand;


	XMFLOAT3 startF;
	XMFLOAT3 endF;
	XMFLOAT3 minF;
	XMFLOAT3 maxF;


	XMStoreFloat3(&startF, start);
	XMStoreFloat3(&endF, end);
	XMStoreFloat3(&minF, boxMin);
	XMStoreFloat3(&maxF, boxMax);


	const XMFLOAT3 direction =
	{
		endF.x - startF.x,
		endF.y - startF.y,
		endF.z - startF.z
	};


	//========================================================
	// Segment Parameter
	// P(t) = Start + Direction * t
	// 0 <= t <= 1
	//========================================================
	float tMin = 0.0f;
	float tMax = 1.0f;


	//========================================================
	// 1軸ずつSlab Test
	//========================================================
	auto TestAxis = [&tMin, &tMax](float startValue, float directionValue, float minValue, float maxValue) -> bool
		{
			//====================================================
			// この軸へほぼ移動していない
			//====================================================
			if (fabsf(directionValue) < 0.000001f)
			{
				// SegmentがSlab外にあれば絶対に当たらない
				return
					startValue >= minValue &&
					startValue <= maxValue;
			}


			const float inverseDirection = 1.0f / directionValue;

			float t1 = (minValue - startValue) * inverseDirection;
			float t2 = (maxValue - startValue) * inverseDirection;


			if (t1 > t2)
			{
				std::swap(t1, t2);
			}


			tMin = (std::max)(tMin, t1);
			tMax = (std::min)(tMax, t2);
			return tMin <= tMax;
		};


	//========================================================
	// X
	//========================================================
	if (!TestAxis(
		startF.x,
		direction.x,
		minF.x,
		maxF.x))
	{
		return false;
	}


	//========================================================
	// Y
	//========================================================
	if (!TestAxis(
		startF.y,
		direction.y,
		minF.y,
		maxF.y))
	{
		return false;
	}


	//========================================================
	// Z
	//========================================================
	if (!TestAxis(
		startF.z,
		direction.z,
		minF.z,
		maxF.z))
	{
		return false;
	}


	//========================================================
	// Segment範囲外
	//========================================================
	if (tMax < 0.0f || tMin > 1.0f)
	{
		return false;
	}


	if (outHitT != nullptr)
	{
		*outHitT = std::clamp(tMin, 0.0f, 1.0f);
	}


	return true;
}

XMVECTOR vnCamera::Camera_FindSafePosOnBoxes(const XMVECTOR& lookAt, const XMVECTOR& desired, float radius, const std::vector<Collider*>& colliders)
{
	//========================================================
    // 1.0ならDesiredまで問題なく進める
    //========================================================
	float closestHitT = 1.0f;

	bool hitAny = false;

	//========================================================
	// World Colliderを調べる
	//========================================================
	for (Collider* collider : colliders)
	{
		if (collider == nullptr ||
			!collider->IsEnabled())
		{
			continue;
		}

		const BoxCollider* box = dynamic_cast<const BoxCollider*>(collider);

		if (box == nullptr)
		{
			continue;
		}


		float hitT = 1.0f;


		if (Camera_SweepSphereVsBox(
			lookAt,
			desired,
			radius,
			box,
			&hitT))
		{
			if (!hitAny || hitT < closestHitT)
			{
				closestHitT = hitT;

				hitAny = true;
			}
		}
	}


	//========================================================
	// 障害物なし
	//========================================================
	if (!hitAny)
	{
		return desired;
	}


	//========================================================
	// 壁から少し手前で止める
	// 2cm程度余裕を取って、
	// Surface上でのちらつきを防ぐ
	//========================================================
	const float segmentLength = XMVectorGetX(XMVector3Length(desired - lookAt));


	float safeT =closestHitT;


	if (segmentLength > 0.0001f)
	{
		const float marginT =0.02f / segmentLength;

		safeT -= marginT;
	}


	safeT =std::clamp(safeT,0.0f,1.0f);

	return XMVectorLerp(lookAt,desired,safeT);
}

bool vnCamera::initialize()
{
	Position = XMVectorSet(5.0f, 5.0f, -10.0f, 0.0f);

	Target = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

	Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	FovY = XMConvertToRadians(45.0f);
	Aspect = (float)vnMainFrame::screenWidth / (float)vnMainFrame::screenHeight;
	zNear = 0.1f;
	zFar = 1000.0f;

	FovY = BaseFov;
	FollowDist = BaseDist;

	Viewport.TopLeftX = 0;
	Viewport.TopLeftY = 0;
	Viewport.Width = (float)vnMainFrame::screenWidth;
	Viewport.Height = (float)vnMainFrame::screenHeight;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	switch (gameType)
	{
	case vnCamera::EyeKillYou:
		break;
	case vnCamera::AtmosphericEscape:
		vnCamera::BaseDist = 3.5f;
		vnCamera::MinDist = 1.2f;
		vnCamera::sCameraRadius = 0.20f;
		vnCamera::sLookAtHeight = 1.35f;
		vnCamera::BaseFov =XMConvertToRadians(55.0f);
		vnCamera::MaxFov =XMConvertToRadians(70.0f);
		vnCamera::FollowYaw = 0.0f;
		vnCamera::FollowPitch = -0.20f;
		vnCamera::FollowDist = 3.5f;

		break;
	default:
		break;
	}

	return true;
}

void vnCamera::terminate()
{

}

//============================================================
// Input update
//============================================================

void vnCamera::UpdateFollowAngles(float dt)
{
	if (IsLockOn)
		return;

	// マウス入力
	UpdateRotationByMouse();
	// 右スティック入力（Pad）
	UpdateRotationByPad(dt);

	// 上下角の制限
	ClampFollowPitch();
	// 正規化
	NormalizeFollowYaw();
}

//============================================================
// Matrices / viewport
//============================================================

XMMATRIX* vnCamera::getView()
{
	View = XMMatrixLookAtLH(Position, Target, Up);
	return &View;
}

XMMATRIX* vnCamera::getProj()
{
	Proj = XMMatrixPerspectiveFovLH(FovY, Aspect, zNear, zFar);
	return &Proj;
}

XMMATRIX* vnCamera::getScreen()
{
	Screen = XMMatrixMultiply(*getView(), *getProj());
	return &Screen;
}

D3D12_VIEWPORT* vnCamera::getViewport()
{
	return &Viewport;
}

//============================================================
// Getters
//============================================================

//取得
const XMVECTOR* vnCamera::getPosition()
{
	return &Position;
}

float vnCamera::getPositionX()
{
	return XMVectorGetX(Position);
}

float vnCamera::getPositionY()
{
	return XMVectorGetY(Position);
}

float vnCamera::getPositionZ()
{
	return XMVectorGetZ(Position);
}

const XMVECTOR* vnCamera::getTarget()
{
	return &Target;
}

float vnCamera::getTargetX()
{
	return XMVectorGetX(Target);
}

float vnCamera::getTargetY()
{
	return XMVectorGetY(Target);
}

float vnCamera::getTargetZ()
{
	return XMVectorGetZ(Target);
}

const XMVECTOR* vnCamera::getUp()
{
	return &Up;
}

float vnCamera::getUpX()
{
	return XMVectorGetX(Up);
}

float vnCamera::getUpY()
{
	return XMVectorGetY(Up);
}

float vnCamera::getUpZ()
{
	return XMVectorGetZ(Up);
}

float vnCamera::getFovY()
{
	return FovY;
}

float vnCamera::getAspect()
{
	return Aspect;
}

float vnCamera::getNearZ()
{
	return zNear;
}

float vnCamera::getFarZ()
{
	return zFar;
}

XMVECTOR vnCamera::getForward()
{
	XMVECTOR forward = XMVectorSubtract(Target, Position);
	return XMVector3Normalize(forward);
}

XMVECTOR vnCamera::getRight()
{
	XMVECTOR forward = getForward();
	XMVECTOR right = XMVector3Cross(Up, forward);
	return XMVector3Normalize(right);
}

//============================================================
// Setters
//============================================================

void vnCamera::setPosition(float x, float y, float z)
{
	Position = XMVectorSet(x, y, z, 0.0f);
}

void vnCamera::setPosition(const XMVECTOR* v)
{
	Position = *v;
}

void vnCamera::setPositionX(float value)
{
	Position = XMVectorSetX(Position, value);
}

void vnCamera::setPositionY(float value)
{
	Position = XMVectorSetY(Position, value);
}

void vnCamera::setPositionZ(float value)
{
	Position = XMVectorSetZ(Position, value);
}

void vnCamera::setTarget(float x, float y, float z)
{
	Target = XMVectorSet(x, y, z, 0.0f);
}

void vnCamera::setTarget(const XMVECTOR* v)
{
	Target = *v;
}

void vnCamera::setTargetX(float value)
{
	Target = XMVectorSetX(Target, value);
}

void vnCamera::setTargetY(float value)
{
	Target = XMVectorSetY(Target, value);
}

void vnCamera::setTargetZ(float value)
{
	Target = XMVectorSetZ(Target, value);
}

void vnCamera::setUp(float x, float y, float z)
{
	Up = XMVectorSet(x, y, z, 0.0f);
}

void vnCamera::setUp(const XMVECTOR* v)
{
	Up = *v;
}

void vnCamera::setUpX(float value)
{
	Up = XMVectorSetX(Up, value);
}

void vnCamera::setUpY(float value)
{
	Up = XMVectorSetY(Up, value);
}

void vnCamera::setUpZ(float value)
{
	Up = XMVectorSetZ(Up, value);
}


void vnCamera::setFovY(float value)
{
	FovY = value;
}

void vnCamera::setAspect(float value)
{
	Aspect = value;
}

//============================================================
// Adders
//============================================================

void vnCamera::addPosition(float x, float y, float z)
{
	Position = XMVectorAdd(Position, XMVectorSet(x, y, z, 0.0f));
}

void vnCamera::addPosition(const XMVECTOR* v)
{
	Position += *v;
}

void vnCamera::addPositionX(float value)
{
	Position = XMVectorAdd(Position, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
}

void vnCamera::addPositionY(float value)
{
	Position = XMVectorAdd(Position, XMVectorSet(0.0f, value, 0.0f, 0.0f));
}

void vnCamera::addPositionZ(float value)
{
	Position = XMVectorAdd(Position, XMVectorSet(0.0f, 0.0f, value, 0.0f));
}

void vnCamera::addTarget(float x, float y, float z)
{
	Target = XMVectorAdd(Target, XMVectorSet(x, y, z, 0.0f));
}

void vnCamera::addTarget(const XMVECTOR* v)
{
	Target += *v;
}

void vnCamera::addTargetX(float value)
{
	Target = XMVectorAdd(Target, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
}

void vnCamera::addTargetY(float value)
{
	Target = XMVectorAdd(Target, XMVectorSet(0.0f, value, 0.0f, 0.0f));
}

void vnCamera::addTargetZ(float value)
{
	Target = XMVectorAdd(Target, XMVectorSet(0.0f, 0.0f, value, 0.0f));
}


void vnCamera::addFovY(float value)
{
	FovY += value;
}

void vnCamera::addAspect(float value)
{
	Aspect += value;
}

//============================================================
// Camera behaviors
//============================================================

void vnCamera::FollowFirstPerson(const XMVECTOR& playerPosition, float eyeHeight, float pitchOffset)
{
	// 衝突処理後の足元座標に、補間済みの目の高さを加える
	Position = playerPosition + XMVectorSet(0.0f, eyeHeight, 0.0f, 0.0f);
	Target = Position + BuildFollowDirection(pitchOffset);
	Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	if (gameType == AtmosphericEscape)
	{
		const auto& options = AtmosphericEscapeOptions::Get();
		FovY = XMConvertToRadians(static_cast<float>(options.fovYDegrees));
	}

	// 一人称の被弾演出はピッチ補正で表現する
	ShakeTimer = 0.0f;
}

void vnCamera::FollowTarget(const XMVECTOR& targetPos, float dt, const MeshCollider* stageMesh)
{
	//========================================
	// 注視点
	//========================================
	XMVECTOR playerPos = targetPos;
	XMVECTOR lookAt = BuildLookAt(playerPos);

	//========================================
	// 理想位置
	//========================================
	XMVECTOR desiredPos = Position;

	if (IsLockOn)
	{
		desiredPos = BuildDesiredPosLockOn(playerPos);

		XMVECTOR bossLook =
			LockOnTarget + XMVectorSet(0.0f, sLookAtHeight, 0.0f, 0.0f);

		Target = XMVectorLerp(Target, bossLook, 0.2f);
	}
	else
	{
		desiredPos = BuildDesiredPosNormal(lookAt);
		Target = XMVectorLerp(Target, lookAt, 0.2f);
	}

	//========================================
	// スムーズ追従
	//========================================
	const float followRate = 0.15f;
	Position = XMVectorLerp(Position, desiredPos, followRate);

	//========================================
	// メッシュ衝突解決
	//========================================
	if (stageMesh)
	{
		Camera_ResolveSphereVsMesh(
			Position,
			sCameraRadius,
			stageMesh);
	}

	//========================================
	// 地面補正
	//========================================
	ClampCameraAboveGround(stageMesh);

	//========================================
	// シェイク
	//========================================
	ApplyShake(dt);

	//========================================
	// 地面補正（追従後）
	//========================================
	if (stageMesh)
	{
		Camera_ResolveSphereVsMesh(
			Position,
			sCameraRadius,
			stageMesh);
	}
	ClampCameraAboveGround(stageMesh);
}

void vnCamera::FollowDeathTarget(const XMVECTOR& focusPos, float dt, float backDist, float height, const MeshCollider* stageMesh)
{
	//========================================
   // 注視点
   //========================================
	XMVECTOR lookAt = focusPos + XMVectorSet(0.0f, height * 0.35f, 0.0f, 0.0f);

	//========================================
	// 今の前方向から後方位置を作る
	//========================================
	XMVECTOR forward = XMVector3Normalize(Target - Position);
	forward = XMVectorSetY(forward, 0.0f);

	if (XMVectorGetX(XMVector3LengthSq(forward)) < 0.0001f)
		forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	else
		forward = XMVector3Normalize(forward);

	XMVECTOR desiredPos =
		lookAt - forward * backDist + XMVectorSet(0.0f, height, 0.0f, 0.0f);

	//========================================
	// スムーズ追従
	//========================================
	const float posRate = 0.08f;
	const float targetRate = 0.10f;

	Position = XMVectorLerp(Position, desiredPos, posRate);
	Target = XMVectorLerp(Target, lookAt, targetRate);

	//========================================
	// FOV
	//========================================
	FovY = SmoothDampF(FovY, XMConvertToRadians(60.0f), 8.0f, dt);

	//========================================
	// 地面補正
	//========================================
	ClampCameraAboveGround(stageMesh);

	//========================================
	// シェイク
	//========================================
	ApplyShake(dt);

	//========================================
	// シェイク後の地面補正
	//========================================
	ClampCameraAboveGround(stageMesh);
}

void vnCamera::FollowTargetWithColliders(
	const XMVECTOR& targetPos,
	float dt, 
	const std::vector<Collider*>& colliders,
	bool snap
)
{
	//========================================================
	// Player Root Position
	//========================================================
	const XMVECTOR playerPos = targetPos;


	//========================================================
	// 注視点
	//========================================================
	const XMVECTOR lookAt =
		BuildLookAt(playerPos);


	//========================================================
	// 理想Camera位置
	//========================================================
	XMVECTOR desiredPos;


	if (IsLockOn)
	{
		desiredPos = BuildDesiredPosLockOn(playerPos);
	}
	else
	{
		desiredPos = BuildDesiredPosNormal(lookAt);
	}


	//========================================================
	// Camera Sweep
	// Playerと理想Camera位置の間に
	// ・木
	// ・施設壁
	// ・天井
	// などが存在する場合、
	// 最初のColliderの手前までCameraを寄せる
	//========================================================
	desiredPos =
		Camera_FindSafePosOnBoxes(
			lookAt,
			desiredPos,
			sCameraRadius,
			colliders
		);


	//========================================================
	// Target追従
	//========================================================
	if (IsLockOn)
	{
		const XMVECTOR bossLook =
			LockOnTarget +
			XMVectorSet(
				0.0f,
				sLookAtHeight,
				0.0f,
				0.0f
			);


		Target =
			XMVectorLerp(
				Target,
				bossLook,
				0.20f
			);
	}
	else
	{
		Target =
			XMVectorLerp(
				Target,
				lookAt,
				0.20f
			);
	}


	//========================================================
	// Camera Position追従
	//========================================================
	const float followRate = 1.0f - expf(-10.0f * dt);


	Position =
		XMVectorLerp(
			Position,
			desiredPos,
			followRate
		);


	//========================================================
	// 最終Resolve
	// Lerpや急激なCamera回転で微妙にBoxへ入った場合も
	// 最後に確実に押し出す
	//========================================================
	Camera_ResolveSphereVsBoxes(
		Position,
		sCameraRadius,
		colliders
	);


	//========================================================
	// Shake
	//========================================================
	ApplyShake(dt);

	Camera_ResolveSphereVsBoxes(
		Position,
		sCameraRadius,
		colliders
	);
}

void vnCamera::UpdateSpeedCamera(float speedRate, float dt)
{
	//============================================================
	// speedRate
	// 0.0f = 通常速度
	// 1.0f = 最大速度演出
	//============================================================
	speedRate = std::clamp(speedRate, 0.0f, 1.0f);

	//============================================================
	// 速度が上がった時の目標値
	//============================================================
	const float normalDist = BaseDist;              // 通常距離
	const float speedDist = BaseDist + 5.0f;        // 速い時は少し引く

	const bool isEscape = gameType == AtmosphericEscape;
	const float normalFov = isEscape
		? XMConvertToRadians(static_cast<float>(AtmosphericEscapeOptions::Get().fovYDegrees)): BaseFov;
	const float speedFov = isEscape
		? normalFov + XMConvertToRadians(7.0f) : XMConvertToRadians(62.0f);

	//============================================================
	// 目標値を計算
	//============================================================
	float targetDist = normalDist + (speedDist - normalDist) * speedRate;
	float targetFov = normalFov + (speedFov - normalFov) * speedRate;

	//============================================================
	// 滑らかに補間
	//============================================================
	FollowDist = SmoothDampF(FollowDist, targetDist, 6.0f, dt);
	FovY = SmoothDampF(FovY, targetFov, 6.0f, dt);
}

void vnCamera::StartShake(float power, float duration)
{
	ShakePower = power;
	ShakeDuration = duration;
	ShakeTimer = duration;
}

void vnCamera::StartLockOn(const XMVECTOR& target)
{
	IsLockOn = true;
	LockOnTarget = target;
}

void vnCamera::EndLockOn()
{
	// 現在の向きからYaw/Pitchを再計算
	XMVECTOR dir = XMVector3Normalize(Target - Position);

	float x = XMVectorGetX(dir);
	float y = XMVectorGetY(dir);
	float z = XMVectorGetZ(dir);

	FollowYaw = atan2f(x, z);
	FollowPitch = asinf(y);

	IsLockOn = false;
}

//============================================================
// Internal update helpers
//============================================================

void vnCamera::UpdateRotationByMouse()
{
	const auto& options = AtmosphericEscapeOptions::Get();
	const bool isEscape = gameType == AtmosphericEscape;
	const float scale = isEscape ? options.mouseSensitivity / 100.0f : 1.0f;
	const float sensitivity = 0.003f * scale;
	const float verticalSign = isEscape && options.invertLookY ? -1.0f : 1.0f;

	FollowYaw += vnMouse::getDX() * sensitivity;
	FollowPitch -= vnMouse::getDY() * sensitivity * verticalSign;
}

void vnCamera::UpdateRotationByPad(float dt)
{
	const int padId = 0;
	if (!Pad::isValid(padId)) return;

	const auto& options = AtmosphericEscapeOptions::Get();
	const bool isEscape = gameType == AtmosphericEscape;
	const float scale = isEscape ? options.padSensitivity / 100.0f : 1.0f;
	const float verticalSign = isEscape && options.invertLookY ? -1.0f : 1.0f;
	const float rx = Pad::rightX(padId, PadDeadZone);
	const float ry = Pad::rightY(padId, PadDeadZone, InvertPadY);

	FollowYaw += rx * PadYawSpeed * scale * dt;
	FollowPitch += ry * PadPitchSpeed * scale * dt * verticalSign;
}

void vnCamera::ClampFollowPitch()
{
	FollowPitch = std::clamp(
		FollowPitch,
		-XM_PIDIV2 + 0.1f,
		XM_PIDIV2 - 0.1f
	);
}

void vnCamera::NormalizeFollowYaw()
{
	if (FollowYaw > XM_PI)
		FollowYaw -= XM_2PI;
	if (FollowYaw < -XM_PI)
		FollowYaw += XM_2PI;
}

XMVECTOR vnCamera::BuildFollowDirection(float pitchOffset)
{
	// 演出分は入力で更新した FollowPitch に書き戻さない
	// アクションが終わるとプレイヤーが向いていた方向へ自然に戻る
	const float pitch = std::clamp(
		FollowPitch + pitchOffset,
		-XM_PIDIV2 + 0.1f,
		XM_PIDIV2 - 0.1f
	);

	const float cosPitch = cosf(pitch);

	return XMVectorSet(
		cosPitch * sinf(FollowYaw),
		sinf(pitch),
		cosPitch * cosf(FollowYaw),
		0.0f
	);
}

//============================================================
// Internal follow helpers
//============================================================
XMVECTOR vnCamera::BuildLookAt(const XMVECTOR& targetPos)
{
	return targetPos + XMVectorSet(0.0f, sLookAtHeight, 0.0f, 0.0f);
}

XMVECTOR vnCamera::BuildDesiredPosNormal(const XMVECTOR& lookAt)
{
	// 三人称の理想位置 既存と同じ角度・距離の計算を使う
	return lookAt - BuildFollowDirection() * FollowDist;
}

XMVECTOR vnCamera::BuildDesiredPosLockOn(const XMVECTOR& playerPos)
{
	XMVECTOR toTarget = LockOnTarget - playerPos;
	toTarget = XMVectorSetY(toTarget, 0.0f);

	if (XMVectorGetX(XMVector3LengthSq(toTarget)) > 0.0001f)
		toTarget = XMVector3Normalize(toTarget);
	else
		toTarget = XMVectorSet(0, 0, 1, 0);

	XMVECTOR desiredPos = playerPos - toTarget * FollowDist;

	// ロックオン時は少し持ち上げる
	desiredPos += XMVectorSet(0.0f, sLookAtHeight * 0.5f, 0.0f, 0.0f);

	return desiredPos;
}

void vnCamera::ApplyShake(float dt)
{
	if (ShakeTimer <= 0.0f)
		return;

	ShakeTimer -= dt;

	float currentPower = ShakePower * (ShakeTimer / ShakeDuration);

	float offsetX = ((rand() % 200 - 100) / 100.0f) * currentPower;
	float offsetY = ((rand() % 200 - 100) / 100.0f) * currentPower;
	float offsetZ = ((rand() % 200 - 100) / 100.0f) * currentPower;

	Position = XMVectorAdd(Position, XMVectorSet(offsetX, offsetY, offsetZ, 0));
}

//============================================================
// Internal utility helpers
//============================================================
float vnCamera::Saturate(float x)
{
	// 0～1に丸める
	if (x < 0.0f) return 0.0f;
	if (x > 1.0f) return 1.0f;
	return x;
}

float vnCamera::SmoothDampF(float current, float target, float speed, float dt)
{
	float t = 1.0f - expf(-speed * dt);
	return current + (target - current) * t;
}

// 現在は未使用。近距離時の自動FOV拡張用
void vnCamera::UpdateAutoFovByDistance(float actualDist, float dt)
{
	// 近づき具合を 0～1 に正規化する
	//  BaseDist のとき 0、MinDist のとき 1 になる
	const float base = vnCamera::BaseDist;
	const float minD = vnCamera::MinDist;

	float denom = (base - minD);
	if (fabsf(denom) < 1e-6f) denom = 1e-6f;

	// actualDist が小さいほど ratio が大きくなるようにする
	float ratio = (base - actualDist) / denom;
	ratio = Saturate(ratio);

	// 補間で目標FOVを決める（BaseFov → maxFov）
	float targetFov = vnCamera::BaseFov + (vnCamera::MaxFov - vnCamera::BaseFov) * ratio;

	targetFov = std::clamp(targetFov, vnCamera::BaseFov, vnCamera::MaxFov);

	vnCamera::FovY = SmoothDampF(vnCamera::FovY, targetFov, 10.0f, dt);
}

float vnCamera::ApplyDeadZone(float v, float dead)
{
	if (fabsf(v) < dead) return 0.0f;

	// deadを超えた分を 0..1 に再正規化
	float sign = (v >= 0.0f) ? 1.0f : -1.0f;
	float a = (fabsf(v) - dead) / (1.0f - dead);
	if (a < 0.0f) a = 0.0f;
	if (a > 1.0f) a = 1.0f;
	return a * sign;
}
