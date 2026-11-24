#pragma once

//============================================================
// Point Light Shadow定数
// ※HLSL側と必ず同じ値にする
//============================================================
static constexpr UINT POINT_SHADOW_FACE_COUNT = 6u;

static constexpr UINT MAX_POINT_SHADOW_LIGHTS = 3u;

static constexpr UINT POINT_SHADOW_TOTAL_SLICES = MAX_POINT_SHADOW_LIGHTS * POINT_SHADOW_FACE_COUNT;

//============================================================
// Point Shadow各Faceの視野角
// ※理論上は90度だが、浮動小数点誤差やRasterizerの境界により
// ※Face同士の境界でわずかな隙間が発生することがある
// ※そのため、少しだけ広い視野角で各Faceを描画し
// ※隣接Faceとわずかに重なるようにする
//============================================================
static constexpr float FaceFieldOfViewDegrees = 90.5f;

//============================================================
// Point Light Shadow Manager
// Point Lightは全方向へ光を放つため
// 1灯につき次の6方向へShadow Mapを生成する
//   Face 0 : +X
//   Face 1 : -X
//   Face 2 : +Y
//   Face 3 : -Y
//   Face 4 : +Z
//   Face 5 : -Z
//============================================================
class PointLightShadowManager final
{
public:
	//============================================================
	// Point Shadow更新方式
	//============================================================
	enum class UpdateMode : UINT
	{
		//========================================================
		// 毎フレームShadow Mapを更新する
		//========================================================
		EveryFrame = 0u,

		//========================================================
		// 必要なときだけShadow Mapを更新する
		//========================================================
		OnDemand
	};

	//============================================================
	// Point Shadowの実効解像度
	// Texture2DArray自体はShadowMapSizeで作成するが
	// Viewport・Scissor・Clear範囲を変更することで
	// ライトごとに描画する領域を制限する
	//============================================================
	enum class ShadowResolution : UINT
	{
		//========================================================
		// 遠距離・重要度の低いPoint Light向け
		//========================================================
		Low = 256u,
		//========================================================
		// 通常の動的Point Light向け
		//========================================================
		Medium = 512u,
		//========================================================
		// 近距離・重要度の高いPoint Light向け
		//========================================================
		High = 1024u
	};

	//============================================================
	// Point Lightを識別するStable ID
	// 0は無効IDとして扱う
	// Scene内の各Point Lightには異なるIDを割り当てる
	//============================================================
	static constexpr std::uint32_t InvalidStableLightID = 0u;

	//============================================================
	// Shadow Map解像度
	//============================================================
	static constexpr UINT ShadowMapSize = 1024u;

	//============================================================
	// Point Light 1灯あたりのShadow面数
	//============================================================
	static constexpr UINT FaceCount = 6u;

	//============================================================
	// Shadowを持てるPoint Lightの最大数
	//============================================================
	static constexpr UINT MaxShadowLights = 3u;

	//============================================================
	// Texture2DArrayの総Slice数
	//============================================================
	static constexpr UINT TotalSliceCount = MaxShadowLights * FaceCount;

	//============================================================
	// 1面あたりに描画できる最大Object数
	//============================================================
	static constexpr UINT MaxShadowObjectsPerFace = 2048u;

	//============================================================
	// 全Point Shadow Passで使用できる最大Draw数
	//============================================================
	static constexpr UINT MaxShadowDrawCalls = TotalSliceCount * MaxShadowObjectsPerFace;

	//============================================================
	// 無効なIndex
	//============================================================
	static constexpr UINT InvalidIndex = 0xffffffffu;

public:

	//============================================================
	// 初期化
	//============================================================
	static bool Initialize();

	//============================================================
	// 終了
	//============================================================
	static void Terminate();

	//============================================================
	// フレーム開始
	// 前フレームに登録したPoint Shadow Lightをリセットする
	//============================================================
	static void BeginFrame();

	//============================================================
	// Point Shadow描画開始時の初期化
	// ・Shadow描画用Object CBVの使用位置をリセットする
	// ・PointLight登録情報は維持する
	//============================================================
	static void BeginShadowDrawPass();

	//============================================================
	// Shadow付きPoint Lightを登録
	//
	// position: Point LightのWorld座標
	// range: Lightの有効範囲であり、Shadow CameraのFar Plane
	// pcfRadius: PCFのサンプル間隔
	// minimumBias: 最小Shadow Bias
	// slopeBias: 法線角度に応じて追加するBias
	// 戻り値:
	//   0～MaxShadowLights -1
	//     登録されたPoint Shadow番号
	//   -1
	//     登録失敗
	//============================================================
	static int AddLight(
		const XMFLOAT3& position,
		float range,
		float pcfRadius = 1.0f,
		float minimumBias = 0.0003f,
		float slopeBias = 0.0020f,
		UpdateMode updateMode = UpdateMode::EveryFrame,
		ShadowResolution shadowResolution = ShadowResolution::High,
		std::uint32_t stableLightID = InvalidStableLightID,
		const ObjectBase* excludedCaster = nullptr
	);

	//============================================================
	// 指定Point Light・Faceへの描画開始
	//============================================================
	static bool BeginRender(UINT shadowIndex, UINT faceIndex);

	//============================================================
	// 現在のPoint Shadow描画終了
	//============================================================
	static void EndRender();

	//============================================================
	// 現在描画中のFace用Object行列を設定
	//============================================================
	static bool SetObjectMatrix(const XMMATRIX& world);

	//============================================================
	// Texture2DArray SRVを書き込む
	// Material Descriptorのt15へ登録する
	//============================================================
	static void WriteSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle);

	//============================================================
	// 1灯以上登録されているか
	//============================================================
	static bool IsActive();

	//============================================================
	// Shadow Map描画中か
	//============================================================
	static bool IsRendering();

	//============================================================
	// 登録済みPoint Shadow Light数
	//============================================================
	static UINT GetLightCount();

	//============================================================
	// 指定Point Light・FaceのViewProjection
	//============================================================
	static const XMMATRIX& GetLightViewProjection(UINT shadowIndex, UINT faceIndex);

	//============================================================
	// 指定Point LightのShadow設定
	// x : Enable
	// y : PCF Radius
	// z : Minimum Bias
	// w : Slope Bias
	//============================================================
	static XMFLOAT4 GetShadowParameters(UINT shadowIndex);

	//============================================================
	// 現在描画中のPoint Shadow番号
	//============================================================
	static UINT GetRenderingLightIndex();

	//============================================================
	// 現在描画中のFace番号
	//============================================================
	static UINT GetRenderingFaceIndex();

	//============================================================
	// Point Shadow番号とFace番号から
	// Texture2DArrayのSlice番号を計算する
	//============================================================
	static UINT CalculateSliceIndex(UINT shadowIndex, UINT faceIndex);

	static void SetResolutionCap(ShadowResolution cap) { sResolutionCap = cap; }

	//============================================================
	// 全Point Shadow SliceがShader参照可能状態か
	//============================================================
	static bool AreAllSlicesShaderReadable();

	//============================================================
	// 指定Point Shadow Lightの6面すべてを更新対象にする
	// 固定ライトの周辺でObjectが移動した場合などに使用する
	//============================================================
	static void MarkLightDirty(UINT shadowIndex);

	//============================================================
	// すべてのPoint Shadow Lightを更新対象にする
	// Scene構成が大きく変化した場合に使用する
	//============================================================
	static void MarkAllLightsDirty();

	//============================================================
	// 指定オブジェクトをShadow Mapから除外するか判定
	//============================================================
	static bool ShouldSkipCaster(UINT shadowIndex, const ObjectBase* object);

	//============================================================
	// 指定Faceを今フレーム描画する必要があるか
	//============================================================
	static bool ShouldRenderFace(UINT shadowIndex, UINT faceIndex);

	//============================================================
	// 指定Point Shadow FaceのFrustumと
	// World空間Bounding Sphereが交差しているか判定
	// shadowIndex:Point Shadow Light番号
	// faceIndex:
	//   0 = +X
	//   1 = -X
	//   2 = +Y
	//   3 = -Y
	//   4 = +Z
	//   5 = -Z
	// worldCenter: Bounding SphereのWorld空間中心
	// worldRadius: Bounding SphereのWorld空間半径
	// 戻り値:
	//   true:  Frustum内または境界と交差しているため描画する
	//   false: Frustumから完全に外れているため描画しない
	//============================================================
	static bool IsSphereInsideShadowFrustum(
		UINT shadowIndex,
		UINT faceIndex,
		FXMVECTOR worldCenter,
		float worldRadius
	);

	//============================================================
	// Point Lightの有効Rangeと
	// World空間Bounding Sphereが交差しているか
	// true:
	//   Light Range内にObjectが存在する可能性がある
	// false:
	//   Light Rangeから完全に離れているので
	//   6Faceすべてで描画不要
	//============================================================
	static bool IsSphereInsideLightRange(
		UINT shadowIndex,
		FXMVECTOR worldCenter,
		float worldRadius
	);

private:

	//============================================================
	// Shadow Vertex Shaderへ渡すObject Constant Buffer
	//============================================================
	struct alignas(16) ObjectBuffer
	{
		XMFLOAT4X4 World;

		XMFLOAT4X4 LightViewProjection;
	};

private:

	//============================================================
	// 指定Point Shadow Lightの6面をDirtyにする内部関数
	//============================================================
	static void MarkAllFacesDirty(UINT shadowIndex);

	//============================================================
	// 指定SliceだけResource Stateを変更
	//============================================================
	static void ChangeSliceResourceState(UINT sliceIndex, D3D12_RESOURCE_STATES afterState);

private:

	static bool sInitialized;
	static bool sRendering;

	inline static ShadowResolution sResolutionCap = ShadowResolution::High;

	//============================================================
	// 12Sliceを持つPoint Shadow Texture2DArray
	//============================================================
	static ID3D12Resource* sShadowResource;

	//============================================================
	// SliceごとのDSVを持つDescriptor Heap
	//============================================================
	static ID3D12DescriptorHeap* sDSVHeap;
	static D3D12_CPU_DESCRIPTOR_HANDLE sDSVHandles[TotalSliceCount];

	//============================================================
	// Object Constant Buffer
	//============================================================
	static ID3D12Resource* sObjectBuffer;
	static BYTE* sMappedObjectBuffer;
	static UINT sObjectBufferStride;
	static UINT sObjectBufferIndex;

	//============================================================
	// 登録済みPoint Shadow Light数
	//============================================================
	static UINT sLightCount;

	//============================================================
	// 現在描画中のLight・Face
	//============================================================
	static UINT sRenderingLightIndex;

	static UINT sRenderingFaceIndex;

	//============================================================
	// Point Light情報
	//============================================================
	static XMFLOAT3 sLightPositions[MaxShadowLights];

	static float sLightRanges[MaxShadowLights];

	//============================================================
	// Shadow設定
	//
	// x : Enable
	// y : PCF Radius
	// z : Minimum Bias
	// w : Slope Bias
	//============================================================
	static XMFLOAT4 sShadowParameters[MaxShadowLights];

	//============================================================
	// 各Point Lightの6面ViewProjection
	//
	// Index:
	//   shadowIndex * FaceCount + faceIndex
	//============================================================
	static XMMATRIX sLightViewProjection[TotalSliceCount];

	static XMMATRIX sIdentityMatrix;

	//============================================================
	// 各SliceのResource State
	//============================================================
	static D3D12_RESOURCE_STATES sSliceResourceStates[TotalSliceCount];

	static D3D12_VIEWPORT sViewport;

	static D3D12_RECT sScissorRect;

	//============================================================
	// 各Point Shadow Lightの更新方式
	//============================================================
	static UpdateMode sUpdateModes[MaxShadowLights];

	//============================================================
	// 各Texture Array Sliceの更新フラグ
	// true: Shadow Mapを再描画する必要がある
	// false: 前回描画したShadow Mapを再利用できる
	// 配列Index: shadowIndex * FaceCount + faceIndex
	//============================================================
	static bool sFaceDirty[TotalSliceCount];

	//============================================================
	// スロットごとに前回の登録で使った除外対象を保持する
	//============================================================
	inline static const ObjectBase* sExcludedCasters[MaxShadowLights] = {};

	//============================================================
	// 過去フレームのLight情報を保持しているか
	// ※初回登録時は必ずShadow Mapを描画する
	//============================================================
	static bool sHasCachedLightData[MaxShadowLights];

	//============================================================
	// 各Point Shadow Lightの実効解像度
	// 1024 512 256
	//============================================================
	static UINT sShadowResolutions[MaxShadowLights];

	//============================================================
	// 各Shadow Slotが前回保持していたPoint LightのStable ID
	// Shadow Index0と1が、前フレームにどのライトを
	// 描画していたかを判定する
	//============================================================
	static std::uint32_t sCachedStableLightIDs[MaxShadowLights];

	//============================================================
	// Point Shadow各FaceのWorld空間Frustum
	// 配列Index: shadowIndex * FaceCount + faceIndex
	// Point Light 0: Slice 0～5
	// Point Light 1: Slice 6～11
	//============================================================
	static BoundingFrustum sWorldFrustums[TotalSliceCount];

};