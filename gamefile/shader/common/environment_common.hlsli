#ifndef ENVIRONMENT_COMMON_HLSLI
#define ENVIRONMENT_COMMON_HLSLI

//============================================================
// エンジンのワールド方向をUE由来CubeMapの参照方向へ変換する
//
// このエンジン:
//   X = 右
//   Y = 上
//   Z = 前
//
// UE側:
//   X = 前
//   Y = 右
//   Z = 上
//
// 対応:
//   UE X = Engine Z
//   UE Y = Engine X
//   UE Z = Engine Y
//============================================================
float3 ConvertWorldDirectionToEnvironment(float3 worldDirection)
{
    const float3 direction =
		normalize(worldDirection);

	//========================================================
	// 軸を並べ替える
	//========================================================
    const float3 environmentDirection =
		float3(
			direction.z,
			direction.x,
			direction.y
		);

    return normalize(environmentDirection);
}

//============================================================
// エンジンのY軸を中心に方向を回転する
// CubeMap全体が90度・180度ずれている場合に使用する
//============================================================
float3 RotateEnvironmentDirectionY(float3 direction,float rotationRadians)
{
    float sineValue;
    float cosineValue;

    sincos(
		rotationRadians,
		sineValue,
		cosineValue
	);

    return float3(
		direction.x * cosineValue + direction.z * sineValue,
		direction.y,
		-direction.x * sineValue + direction.z * cosineValue
	);
}

#endif