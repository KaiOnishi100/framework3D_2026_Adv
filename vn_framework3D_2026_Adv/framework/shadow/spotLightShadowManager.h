#pragma once

class ObjectBase;

//============================================================
// Spot Light Shadow Manager
// 影を生成するSpot Lightは1灯だけ対応する
// 既存のDirectional Shadow用Shader、Root Signature、PSOを
// 再利用し、LightViewProjectionだけ透視投影へ変更する
//============================================================
class SpotLightShadowManager final
{
public:

	//============================================================
	// ShadowMap解像度
	//============================================================
	static constexpr UINT ShadowMapSize = 1024;

	//============================================================
	// ShadowMapへ描画できる最大オブジェクト数
	//============================================================
	static constexpr UINT MaxShadowObjectsPerLight = 2048;

	//============================================================
	// Shadowを持てるSpot Lightの最大数
	// ※HLSL側のMAX_SPOT_SHADOW_LIGHTSと一致させる
	//============================================================
	static constexpr UINT MaxShadowLights = 4;

	//============================================================
	// 1フレームで使用する最大Shadow Draw数
	// 同じUpload Buffer領域を複数ライト間で使い回すと、
	// GPU実行前に後続ライトのデータで上書きされる可能性がある
	// そのため、全ライト分の領域を確保する
	//============================================================
	static constexpr UINT MaxShadowDrawCalls = MaxShadowObjectsPerLight * MaxShadowLights;

	//============================================================
	// 無効なShadow Index
	//============================================================
	static constexpr UINT InvalidShadowIndex = 0xffffffffu;

public:

	static bool Initialize();

	static void Terminate();

	//============================================================
	// フレーム開始
	// 前フレームのSpot Light情報を無効化する
	//============================================================
	static void BeginFrame();

	//============================================================
	// Shadow付きSpot Lightを登録
	// position: Spot Lightのワールド座標
	// direction: Lightから照射先へ向かう方向
	// range: Shadow CameraのFar Plane
	// outerAngleDegrees: Spot Lightの外側半角
	// pcfRadius:
	//   PCFサンプル間隔
	//   1.0fで通常の3×3 PCF
	// minimumBias: 最低限適用するShadow Bias
	// slopeBias: 面がライトに対して斜めになるほど追加するBias
	// 戻り値:
	//   0～MaxShadowLights-1 = Shadow Slice番号
	//   -1                   = 登録失敗
	//============================================================
	static int AddLight(
		const XMFLOAT3& position,
		const XMFLOAT3& direction,
		float range,
		float outerAngleDegrees,
		float pcfRadius = 1.0f,
		float minimumBias = 0.0003f,
		float slopeBias = 0.0020f
	);

	//============================================================
	// 指定Shadow Sliceへの描画開始
	//============================================================
	static bool BeginRender(UINT shadowIndex);

	//============================================================
	// 現在のSpot Shadow描画終了
	//============================================================
	static void EndRender();
	
	//============================================================
	// そのフレームの全Spot Shadow描画を始める前に一度だけ呼ぶ
	// 登録済みライトの数や行列は保持する
	//============================================================
	static void BeginShadowDrawPass();

	//============================================================
	// 現在描画中のSpot Lightに対するObject行列を設定
	//============================================================
	static void SetObjectMatrix(const XMMATRIX& world);

	//============================================================
	// Material Descriptor HeapへTexture2DArray SRVを作成
	//============================================================
	static void WriteSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle);

	static bool IsActive();

	static bool IsRendering();

	static UINT GetLightCount();

	static const XMMATRIX& GetLightViewProjection(UINT shadowIndex);

	//============================================================
	// 現在描画中のShadow Index
	//============================================================
	static UINT GetRenderingLightIndex();

	//============================================================
	// 指定Spot Shadowの設定を取得
	// 無効なIndexの場合は、Shadow無効設定を返す
	//============================================================
	static XMFLOAT4 GetShadowParameters(UINT shadowIndex);

	static void SetShadowLightBudget(UINT count);

	//============================================================
	// 指定Spot LightのShadow Frustumと
	// World空間Bounding Sphereが交差しているか判定
	// shadowIndex:Spot Shadow Texture2DArrayのSlice番号
	// worldCenter:Bounding SphereのWorld空間中心
	// worldRadius:Bounding SphereのWorld空間半径
	// 戻り値:
	//   true  = Shadow Mapへ描画する
	//   false = 視錐台外なので描画を省略する
	//============================================================
	static bool IsSphereInsideShadowFrustum(
		UINT shadowIndex,
		FXMVECTOR worldCenter,
		float worldRadius
	);

	//============================================================
	// SpotLightごとのShadow Caster除外設定
	// shadowIndex: SpotLightのShadow Map Slice番号
	// object: Shadow Mapへ描画しないオブジェクト
	// nullptr: 除外対象なし
	//============================================================
	static void SetExcludedCaster(UINT shadowIndex, const ObjectBase* object);

	//============================================================
	// 指定オブジェクトをShadow Mapから除外するか判定
	//============================================================
	static bool ShouldSkipCaster(UINT shadowIndex, const ObjectBase* object);

private:

	//============================================================
	// Shadow Vertex Shaderへ送る定数
	//============================================================
	struct alignas(16) ObjectBuffer
	{
		XMFLOAT4X4 World;
		XMFLOAT4X4 LightViewProjection;
	};

private:
	//============================================================
	// 指定Shadow SliceだけResource Stateを変更
	// shadowIndex: Texture2DArrayのSlice番号
	// afterState: 変更後のResource State
	//============================================================
	static void ChangeSliceResourceState(UINT shadowIndex, D3D12_RESOURCE_STATES afterState);

private:

	static bool sInitialized;
	static bool sRendering;

	inline static UINT sShadowLightBudget = MaxShadowLights;

	static ID3D12Resource* sShadowResource;

	static ID3D12DescriptorHeap* sDSVHeap;

	static D3D12_CPU_DESCRIPTOR_HANDLE sDSVHandles[MaxShadowLights];

	static ID3D12Resource* sObjectBuffer;

	static BYTE* sMappedObjectBuffer;

	static UINT sObjectBufferStride;
	static UINT sObjectBufferIndex;
	static UINT sLightCount;
	static UINT sRenderingLightIndex;

	static D3D12_VIEWPORT sViewport;

	static D3D12_RECT sScissorRect;

	static XMMATRIX sLightView[MaxShadowLights];
	static XMMATRIX sLightProjection[MaxShadowLights];
	static XMMATRIX sLightViewProjection[MaxShadowLights];

	static XMMATRIX sIdentityMatrix;

	static D3D12_RESOURCE_STATES sSliceResourceStates[MaxShadowLights];// Texture2DArrayの各Sliceを別々に管理する

	//============================================================
	// Spot Shadow設定
	//
	// x:Shadow有効フラグ
	// y:PCF Radius
	// z:Minimum Bias	
	// w:Slope Bias
	//============================================================
	static XMFLOAT4 sShadowParameters[MaxShadowLights];

	//============================================================
	// Spot LightごとのWorld空間Shadow Frustum
	// sLightProjectionからView空間Frustumを生成し、
	// Light Viewの逆行列でWorld空間へ変換する
	//============================================================
	static BoundingFrustum sWorldFrustums[MaxShadowLights];

	static const ObjectBase* sExcludedCasters[MaxShadowLights];
};