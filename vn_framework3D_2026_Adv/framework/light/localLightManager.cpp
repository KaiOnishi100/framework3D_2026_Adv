#include "../../framework.h"
#include "../vn_environment.h"

#include "localLightManager.h"

//============================================================
// Static Member
//============================================================
bool LocalLightManager::sInitialized = false;
bool LocalLightManager::sSpotShadowCasterRegistered = false;
UINT LocalLightManager::sLightCount = 0;
LocalLightManager::LocalLightGPU LocalLightManager::sCPUData[LocalLightManager::MaxLocalLights] = {};
LocalLightManager::LocalLightGPU* LocalLightManager::sMappedData = nullptr;
ID3D12Resource*LocalLightManager::sLightBuffer =nullptr;

//============================================================
// 初期化
//============================================================
bool LocalLightManager::Initialize()
{
	if (sInitialized)
	{
		return true;
	}

	ID3D12Device* device =
		Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	//============================================================
	// Upload Heap
	// 局所光源は毎フレーム変化する可能性があるため
	// CPUから直接更新できるUpload Heapを使用する
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	//============================================================
	// StructuredBuffer Resource
	//============================================================
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(LocalLightGPU) * MaxLocalLights;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	const HRESULT createResult = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sLightBuffer)
	);

	if (FAILED(createResult) || sLightBuffer == nullptr)
	{
		OutputDebugStringW(L"[LocalLightManager] Failed to create light buffer.\n");
		assert(false);
		return false;
	}

	sLightBuffer->SetName(L"Local Light StructuredBuffer");

	//============================================================
	// Upload Bufferを永続Map
	//============================================================
	D3D12_RANGE readRange =
	{
		0,
		0
	};

	const HRESULT mapResult = sLightBuffer->Map(
		0,
		&readRange,
		reinterpret_cast<void**>(&sMappedData)
	);

	if (FAILED(mapResult) || sMappedData == nullptr)
	{
		OutputDebugStringW(L"[LocalLightManager] Failed to map light buffer.\n");
		assert(false);
		SAFE_RELEASE(sLightBuffer);

		return false;
	}

	//============================================================
	// 初期状態
	//============================================================
	std::memset(
		sMappedData,
		0,
		sizeof(LocalLightGPU) *
		MaxLocalLights
	);

	std::memset(
		sCPUData,
		0,
		sizeof(sCPUData)
	);

	sLightCount = 0;
	sInitialized = true;

	return true;
}

//============================================================
// 解放
//============================================================
void LocalLightManager::Terminate()
{
	sInitialized = false;
	sLightCount = 0;

	if (sLightBuffer != nullptr)
	{
		if (sMappedData != nullptr)
		{
			sLightBuffer->Unmap(0, nullptr);
			sMappedData = nullptr;
		}
	}

	SAFE_RELEASE(sLightBuffer);
	std::memset(
		sCPUData,
		0,
		sizeof(sCPUData)
	);
}

//============================================================
// フレーム開始
//============================================================
void LocalLightManager::BeginFrame()
{
	//============================================================
	// 古いライト登録数をリセット
	//============================================================
	sLightCount = 0;
	sSpotShadowCasterRegistered = false;
}

//============================================================
// Point Light追加
//============================================================
bool LocalLightManager::AddPointLight(
	const XMFLOAT3& position,
	float range,
	const XMFLOAT3& color,
	float intensity,
	int shadowIndex,
	const LocalLightRoomBounds& roomBounds
)
{
	//============================================================
	// Managerが初期化されていない場合は登録できない
	//============================================================
	if (!sInitialized)
	{
		OutputDebugStringW(L"[LocalLightManager] Manager is not initialized.\n");
		return false;
	}

	//============================================================
	// 登録可能なLocal Light数を超えている
	//============================================================
	if (sLightCount >= MaxLocalLights)
	{
		OutputDebugStringW(L"[LocalLightManager] Maximum local light count reached.\n");
		return false;
	}

	//============================================================
	// Point Lightの有効範囲
	// 0以下では距離減衰やShadow Projectionを
	// 正しく計算できないため登録しない
	//============================================================
	if (range <= 0.0f)
	{
		OutputDebugStringW(L"[LocalLightManager] Point Light range must be greater than zero.\n");
		return false;
	}

	//============================================================
	// 負のIntensityは使用しない
	//============================================================
	if (intensity < 0.0f)
	{
		OutputDebugStringW(L"[LocalLightManager] Point Light intensity must not be negative.\n");
		return false;
	}

	//============================================================
	// 今回書き込むGPU用Lightデータ
	// LocalLightDataやsLightsではなく、
	// 現在のクラスで定義されている
	//   LocalLightGPU
	//   sCPUData
	// を使用する
	//============================================================
	LocalLightGPU& light = sCPUData[sLightCount];

	//============================================================
	// xyz : Point Lightのワールド座標
	// w   : Point Lightの有効範囲
	//============================================================
	light.PositionRange =
		XMFLOAT4(
			position.x,
			position.y,
			position.z,
			range
		);

	//============================================================
	// rgb : Light Color
	// w   : HDR Intensity
	// 色が負数にならないように制限する
	//============================================================
	light.ColorIntensity =
		XMFLOAT4(
			(std::max)(color.x, 0.0f),
			(std::max)(color.y, 0.0f),
			(std::max)(color.z, 0.0f),
			intensity
		);

	//============================================================
	// Point Lightでは方向とSpot Coneを使用しない
	//============================================================
	light.DirectionOuterCos =
		XMFLOAT4(
			0.0f,
			0.0f,
			0.0f,
			0.0f
		);

	//============================================================
	// Point Shadow Indexを検証
	// 有効な値:
	//   -1: Shadowなし
	//   0～MAX_POINT_SHADOW_LIGHTS - 1: 有効なPoint Shadow番号
	//============================================================
	int safeShadowIndex = shadowIndex;

	if (safeShadowIndex < -1 || safeShadowIndex >= static_cast<int>(MAX_POINT_SHADOW_LIGHTS))
	{
		//========================================================
		// 不正なIndexはShadowなしとして扱う
		//========================================================
		safeShadowIndex = -1;
	}

	//============================================================
	// Shader用Shadow Indexへ変換
	// Parameters.w == 0: Shadowなし
	// Parameters.w == 1: Point Shadow 0
	// Parameters.w == 2: Point Shadow 1
	//============================================================
	const float encodedShadowIndex = safeShadowIndex >= 0 ?
		static_cast<float>(safeShadowIndex + 1) : 0.0f;

	//============================================================
	// Parameters
	// x: LightType
	// y: Point Lightでは未使用
	// z: Light Enable
	// w: EncodeされたPoint Shadow Index
	//============================================================
	light.Parameters =
		XMFLOAT4(static_cast<float>(LightType::Point),
			0.0f,
			1.0f,
			encodedShadowIndex
		);

	light.RoomMin = roomBounds.Min;
	light.RoomMax = roomBounds.Max;

	//============================================================
	// すべての設定完了後に登録数を増やす
	//============================================================
	++sLightCount;

	return true;
}

//============================================================
// GPU Bufferへ転送
//============================================================
void LocalLightManager::Upload()
{
	if (!sInitialized ||sMappedData == nullptr)
	{
		return;
	}

	if (sLightCount == 0)
	{
		return;
	}

	std::memcpy(
		sMappedData,
		sCPUData,
		sizeof(LocalLightGPU) * sLightCount
	);
}

//============================================================
// Light数
//============================================================
UINT LocalLightManager::GetLightCount()
{
	return sLightCount;
}

void LocalLightManager::WriteSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle)
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	//============================================================
	// StructuredBufferなのでFormatはUNKNOWN
	//============================================================
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = MaxLocalLights;
	srvDesc.Buffer.StructureByteStride = sizeof(LocalLightGPU);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	//============================================================
	// Resourceが未作成ならNull Buffer SRV
	//============================================================
	device->CreateShaderResourceView(
		sLightBuffer,
		&srvDesc,
		destinationHandle
	);
}

ID3D12Resource* LocalLightManager::GetBuffer()
{
	return sLightBuffer;
}

bool LocalLightManager::AddSpotLight(const XMFLOAT3& position, 
	const XMFLOAT3& direction, 
	float range, 
	const XMFLOAT3& color,
	float intensity, 
	float innerAngleDegrees,
	float outerAngleDegrees,
	int shadowIndex,
	const LocalLightRoomBounds& roomBounds
)
{
	//============================================================
	// Managerが初期化されていない場合は登録できない
	//============================================================
	if (!sInitialized)
	{
		return false;
	}

	//============================================================
	// 最大ライト数を超えないようにする
	//============================================================
	if (sLightCount >= MaxLocalLights)
	{
		OutputDebugStringW(L"[LocalLightManager] Maximum local light count reached.\n");
		return false;
	}

	//============================================================
	// Rangeは0より大きい必要がある
	//============================================================
	if (range <= 0.0f)
	{
		OutputDebugStringW(L"[LocalLightManager] Spot Light range must be greater than zero.\n");
		return false;
	}

	//============================================================
	// 負の強度は使用しない
	//============================================================
	if (intensity < 0.0f)
	{
		OutputDebugStringW(L"[LocalLightManager] Spot Light intensity must not be negative.\n");
		return false;
	}

	//============================================================
	// Spot Lightの角度を確認
	// ここで指定する角度はコーン全体の角度ではなく
	// 中心軸から端までの半角
	// inner < outerでなければならない
	//============================================================
	if (innerAngleDegrees < 0.0f ||
		outerAngleDegrees <=
		innerAngleDegrees ||
		outerAngleDegrees >= 90.0f)
	{
		OutputDebugStringW(L"[LocalLightManager] Invalid Spot Light cone angles.\n");
		return false;
	}

	//============================================================
	// ライト方向を読み込む
	//============================================================
	const XMVECTOR directionVector = XMLoadFloat3(&direction);

	//============================================================
	// ゼロベクトルを正規化するとNaNになるため
	// 方向ベクトルの長さを確認する
	//============================================================
	const float directionLengthSquared = XMVectorGetX(XMVector3LengthSq(directionVector));

	if (directionLengthSquared <= 0.000001f)
	{
		OutputDebugStringW(L"[LocalLightManager] Spot Light direction is zero.\n");
		return false;
	}

	//============================================================
	// ライトが照らす方向を正規化
	//============================================================
	const XMVECTOR normalizedDirectionVector = XMVector3Normalize(directionVector);

	XMFLOAT3 normalizedDirection = {};

	XMStoreFloat3(
		&normalizedDirection,
		normalizedDirectionVector
	);

	//============================================================
	// Shaderでは角度そのものではなくcos値を使う
	// 角度が小さいほどcos値は大きい
	// したがってinnerCos > outerCosになる
	//============================================================
	const float innerCos = cosf(XMConvertToRadians(innerAngleDegrees));

	const float outerCos = cosf(XMConvertToRadians(outerAngleDegrees));

	LocalLightGPU& light = sCPUData[sLightCount];

	//============================================================
	// Shadow Index確認
	// -1: Shadowなし
	// 0～MaxShadowLights-1: 有効なShadow Texture Array Slice
	//============================================================
	int safeShadowIndex = shadowIndex;

	if (safeShadowIndex < -1 || safeShadowIndex >= static_cast<int>(SpotLightShadowManager::MaxShadowLights))
	{
		safeShadowIndex = -1;
	}

	//============================================================
	// xyz : ライト位置
	// w   : ライト範囲
	//============================================================
	light.PositionRange =
		XMFLOAT4(
			position.x,
			position.y,
			position.z,
			range
		);

	//============================================================
	// rgb : ライト色
	// w   : HDR強度
	//============================================================
	light.ColorIntensity =
		XMFLOAT4(
			(std::max)(color.x, 0.0f),
			(std::max)(color.y, 0.0f),
			(std::max)(color.z, 0.0f),
			intensity
		);

	//============================================================
	// xyz : ライトから照射先へ向かう方向
	// w   : 外側コーンのcos値
	//============================================================
	light.DirectionOuterCos =
		XMFLOAT4(
			normalizedDirection.x,
			normalizedDirection.y,
			normalizedDirection.z,
			outerCos
		);

	const float encodedShadowIndex =
		safeShadowIndex >= 0 ?
		static_cast<float>(safeShadowIndex + 1) : 0.0f;


	//============================================================
	// x : LightType::Spot
	// y : 内側コーンのcos値
	// z : Enable
	// w : 未使用
	//============================================================
	light.Parameters =
		XMFLOAT4(
			static_cast<float>(LightType::Spot),
			innerCos,
			1.0f,
			encodedShadowIndex
		);

	light.RoomMin = roomBounds.Min;
	light.RoomMax = roomBounds.Max;

	++sLightCount;

	return true;
}
