#pragma once

struct LocalLightRoomBounds
{
	// xyz はワールド座標の最小値 wが1のときだけ部屋境界を使う
	XMFLOAT4 Min = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	// xyz はワールド座標の最大値 wは予約
	XMFLOAT4 Max = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
};

//============================================================
// シーン内に複数存在できる局所光源
// ・Point Light
// ・Spot Light
// GPUへStructuredBufferとして送る
//============================================================
class LocalLightManager final
{
public:

	//============================================================
	// GPUへ送れる最大局所光源数
	//============================================================
	static constexpr UINT MaxLocalLights = 16;

	//============================================================
	// 光源種類
	//============================================================
	enum class LightType : UINT
	{
		Point = 0,
		Spot = 1
	};

	//============================================================
	// GPUへ転送する1ライト分のデータ
	// StructuredBufferで使用するため
	// float4単位で構成してパッキングずれを防ぐ
	//============================================================
	struct alignas(16) LocalLightGPU
	{
		//========================================================
		// xyz : ワールド座標
		// w   : 有効範囲
		//========================================================
		XMFLOAT4 PositionRange;

		//========================================================
		// rgb : 光源色
		// w   : 光源強度
		//========================================================
		XMFLOAT4 ColorIntensity;

		//========================================================
		// xyz : Spot Light方向
		// w   : Spot Outer Coneのcos値
		//========================================================
		XMFLOAT4 DirectionOuterCos;

		//========================================================
		// x : LightType
		// y : Spot Inner Coneのcos値
		// z : Enable
		// w : Encode済みShadow Index
		//========================================================
		XMFLOAT4 Parameters;

		XMFLOAT4 RoomMin;
		XMFLOAT4 RoomMax;
	};

public:

	//============================================================
	// StructuredBuffer Resource作成
	// モデルのDescriptorを作る前に呼び出す
	//============================================================
	static bool Initialize();

	//============================================================
	// Resource解放
	//============================================================
	static void Terminate();

	//============================================================
	// 1フレーム分のライト登録を開始
	// 毎フレーム、AddPointLight()より前に呼び出す
	//============================================================
	static void BeginFrame();

	//============================================================
	// Point Lightを登録
	// 戻り値:
	//   true  = 登録成功
	//   false = 上限到達または値が不正
	//============================================================
	static bool AddPointLight(
		const XMFLOAT3& position,
		float range,
		const XMFLOAT3& color,
		float intensity,
		int shadowIndex = -1,
		const LocalLightRoomBounds& roomBounds = {}
	);

	//============================================================
	// CPU側配列をGPUのUpload Bufferへ転送
	//============================================================
	static void Upload();

	//============================================================
	// 現在登録されているライト数
	//============================================================
	static UINT GetLightCount();

	//============================================================
	// 指定Descriptor位置へStructuredBuffer SRVを作成
	//============================================================
	static void WriteSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle);

	static ID3D12Resource* GetBuffer();

	//============================================================
	// Spot Lightを登録
	//
	// position:Spot Lightのワールド座標
	// direction:ライトから照射先へ向かう方向
	// range:ライトの最大到達距離
	// color:Linear空間のライト色
	// intensity: HDR強度
	// innerAngleDegrees: 内側コーンの半角この角度以内は最大強度
	// outerAngleDegrees:外側コーンの半角この角度を超えると完全に0
	//   0～15度  : 最大強度
	//   15～25度 : 滑らかに減衰
	//   25度以上 : 影響なし
	//============================================================
	static bool AddSpotLight(
		const XMFLOAT3& position,
		const XMFLOAT3& direction,
		float range,
		const XMFLOAT3& color,
		float intensity,
		float innerAngleDegrees,
		float outerAngleDegrees,
		int shadowIndex = -1,
		const LocalLightRoomBounds& roomBounds = {}
	);

private:

	static bool sInitialized;
	static bool sSpotShadowCasterRegistered;

	static UINT sLightCount;

	static LocalLightGPU sCPUData[MaxLocalLights];

	static LocalLightGPU* sMappedData;

	static ID3D12Resource* sLightBuffer;
};

static_assert(sizeof(LocalLightManager::LocalLightGPU) == 96, "LocalLightGPU must be 96 bytes.");