#include "../../framework.h"
#include "../vn_environment.h"

#include "pointLightShadowManager.h"

//============================================================
// Static Member
//============================================================
bool PointLightShadowManager::sInitialized =false;
bool PointLightShadowManager::sRendering =false;
ID3D12Resource*PointLightShadowManager::sShadowResource =nullptr;
ID3D12DescriptorHeap*PointLightShadowManager::sDSVHeap =nullptr;
D3D12_CPU_DESCRIPTOR_HANDLE PointLightShadowManager::sDSVHandles[PointLightShadowManager::TotalSliceCount] = {};
ID3D12Resource*PointLightShadowManager::sObjectBuffer =nullptr;
BYTE*PointLightShadowManager::sMappedObjectBuffer =nullptr;
UINT PointLightShadowManager::sObjectBufferStride =0u;
UINT PointLightShadowManager::sObjectBufferIndex =0u;
UINT PointLightShadowManager::sLightCount =0u;
UINT PointLightShadowManager::sRenderingLightIndex =PointLightShadowManager::InvalidIndex;
UINT PointLightShadowManager::sRenderingFaceIndex =PointLightShadowManager::InvalidIndex;
XMFLOAT3 PointLightShadowManager::sLightPositions[PointLightShadowManager::MaxShadowLights] ={};
float PointLightShadowManager::sLightRanges[PointLightShadowManager::MaxShadowLights] ={};
XMFLOAT4 PointLightShadowManager::sShadowParameters[PointLightShadowManager::MaxShadowLights] ={};
XMMATRIX PointLightShadowManager::sLightViewProjection[PointLightShadowManager::TotalSliceCount];
XMMATRIX PointLightShadowManager::sIdentityMatrix =XMMatrixIdentity();
D3D12_RESOURCE_STATES PointLightShadowManager::sSliceResourceStates[PointLightShadowManager::TotalSliceCount];
D3D12_VIEWPORT PointLightShadowManager::sViewport ={};
D3D12_RECT PointLightShadowManager::sScissorRect ={};
BoundingFrustum PointLightShadowManager::sWorldFrustums[PointLightShadowManager::TotalSliceCount] = {};
PointLightShadowManager::UpdateMode PointLightShadowManager::sUpdateModes[PointLightShadowManager::MaxShadowLights] =
{
	PointLightShadowManager::UpdateMode::EveryFrame,
	PointLightShadowManager::UpdateMode::EveryFrame,
	PointLightShadowManager::UpdateMode::EveryFrame
};
bool PointLightShadowManager::sFaceDirty[PointLightShadowManager::TotalSliceCount] =
{
	true,
	true,
	true,
	true,
	true,
	true,

	true,
	true,
	true,
	true,
	true,
	true,

	true,
	true,
	true,
	true,
	true,
	true
};
bool PointLightShadowManager::sHasCachedLightData[PointLightShadowManager::MaxShadowLights] =
{
	false,
	false,
	false,
};

//============================================================
// Point Shadow実効解像度
//============================================================
UINT PointLightShadowManager::sShadowResolutions[PointLightShadowManager::MaxShadowLights] =
{
	PointLightShadowManager::ShadowMapSize,
	PointLightShadowManager::ShadowMapSize,
	PointLightShadowManager::ShadowMapSize,
};

//============================================================
// Point Shadow SlotごとのStable Light ID
//============================================================
std::uint32_t PointLightShadowManager::sCachedStableLightIDs[PointLightShadowManager::MaxShadowLights] =
{
	PointLightShadowManager::InvalidStableLightID,
	PointLightShadowManager::InvalidStableLightID
};

#if defined(_DEBUG)

namespace
{
	//========================================================
	// BeginFrameが呼び出された累計回数
	// シャドウ描画が進んでいるのにこの値が変わらなければ
	// BeginFrameの呼び出しが止まっている
	//========================================================
	uint64_t gPointShadowBeginFrameCount = 0;

	//========================================================
	// 同じBeginFrame期間中に一度だけOverflowを出力
	//========================================================
	bool gPointShadowOverflowReported = false;
}

#endif

#if defined(_DEBUG)
namespace
{
	//========================================================
	// 実際にObjectBufferIndexがリセットされた回数
	//========================================================
	uint64_t gPointShadowResetCount = 0;

	//========================================================
	// 容量不足のログを大量出力しないためのフラグ
	//========================================================
	bool gPointShadowOverflowLogged = false;
}
#endif

//============================================================
// Point Light Shadow Manager初期化
//============================================================
bool PointLightShadowManager::Initialize()
{
	if (sInitialized)
	{
		return true;
	}

#if defined(_DEBUG)

	//============================================================
	// BeginFrameが実際に呼ばれたことを記録
	//============================================================
	++gPointShadowBeginFrameCount;

	//============================================================
	// リセット前の使用数
	// ここが毎回約1,000〜2,000であれば、
	// 少なくともリセットは定期的に機能している
	//============================================================
	const UINT previousDrawCount = sObjectBufferIndex;

	// 次のOverflowを検出できるようにする
	gPointShadowOverflowReported = false;

	//============================================================
	// 60回に1回だけ出力
	//============================================================
	if (gPointShadowBeginFrameCount % 60 == 0)
	{
		WCHAR buffer[256] = {};

		swprintf_s(
			buffer,
			L"[PointShadowBeginFrame] "
			L"ResetID=%llu "
			L"PreviousDraws=%u\n",
			static_cast<unsigned long long>(gPointShadowBeginFrameCount),
			previousDrawCount
		);

		OutputDebugStringW(buffer);
	}

#endif

	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		assert(false);

		return false;
	}

	//============================================================
	// Point Shadow Texture2DArray Resource
	// Resource: R32_TYPELESS
	// DSV: D32_FLOAT
	// SRV: R32_FLOAT
	// Slice数: pointshadowCnt × 6面 = 12
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 1u;
	heapProperties.VisibleNodeMask = 1u;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0u;
	resourceDesc.Width = ShadowMapSize;
	resourceDesc.Height = ShadowMapSize;
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(TotalSliceCount);
	resourceDesc.MipLevels = 1u;
	resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	resourceDesc.SampleDesc.Count = 1u;
	resourceDesc.SampleDesc.Quality = 0u;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0u;

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(&sShadowResource)
	);

	if (FAILED(hr) || sShadowResource == nullptr)
	{
		assert(false);
		Terminate();
		return false;
	}

	sShadowResource->SetName(L"Point Light Shadow Texture2DArray");

	//============================================================
	// SliceごとのDSVを作成するHeap
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NumDescriptors = TotalSliceCount;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0u;

	hr = device->CreateDescriptorHeap(
		&dsvHeapDesc,
		IID_PPV_ARGS(&sDSVHeap)
	);

	if (FAILED(hr) || sDSVHeap == nullptr)
	{
		assert(false);
		Terminate();
		return false;
	}

	sDSVHeap->SetName(L"Point Light Shadow DSV Heap");

	const UINT dsvDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	const D3D12_CPU_DESCRIPTOR_HANDLE dsvHeapStart = sDSVHeap->GetCPUDescriptorHandleForHeapStart();

	//============================================================
	// 12Sliceすべてに個別のDSVを作成
	//============================================================
	for (UINT sliceIndex = 0u; sliceIndex < TotalSliceCount; ++sliceIndex)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeapStart;

		dsvHandle.ptr +=
			static_cast<SIZE_T>(sliceIndex) *
			static_cast<SIZE_T>(dsvDescriptorSize);

		sDSVHandles[sliceIndex] = dsvHandle;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2DArray.MipSlice = 0u;
		dsvDesc.Texture2DArray.FirstArraySlice = sliceIndex;
		//========================================================
		// 1つのDSVでは1つのSliceだけ参照する
		//========================================================
		dsvDesc.Texture2DArray.ArraySize = 1u;

		device->CreateDepthStencilView(
			sShadowResource,
			&dsvDesc,
			dsvHandle
		);
	}

	//============================================================
	// Object Constant Buffer
	//============================================================
	sObjectBufferStride = (sizeof(ObjectBuffer) + 255u) & ~255u;

	const UINT64 objectBufferSize =
		static_cast<UINT64>(sObjectBufferStride) *
		static_cast<UINT64>(MaxShadowDrawCalls);

	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapProperties.CreationNodeMask = 1u;
	uploadHeapProperties.VisibleNodeMask = 1u;

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = objectBufferSize;
	bufferDesc.Height = 1u;
	bufferDesc.DepthOrArraySize = 1u;
	bufferDesc.MipLevels = 1u;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1u;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sObjectBuffer)
	);

	if (FAILED(hr) || sObjectBuffer == nullptr)
	{
		assert(false);
		Terminate();
		return false;
	}

	sObjectBuffer->SetName(L"Point Light Shadow Object Buffer");

	//============================================================
	// Upload Bufferを常時Map
	//============================================================
	D3D12_RANGE readRange = {};
	readRange.Begin = 0u;
	readRange.End = 0u;

	hr = sObjectBuffer->Map(
		0u,
		&readRange,
		reinterpret_cast<void**>(&sMappedObjectBuffer)
	);

	if (FAILED(hr) || sMappedObjectBuffer == nullptr)
	{
		assert(false);
		Terminate();
		return false;
	}

	//============================================================
	// Viewport・Scissor
	//============================================================
	sViewport.TopLeftX = 0.0f;
	sViewport.TopLeftY = 0.0f;
	sViewport.Width = static_cast<float>(ShadowMapSize);
	sViewport.Height = static_cast<float>(ShadowMapSize);
	sViewport.MinDepth = 0.0f;
	sViewport.MaxDepth = 1.0f;

	sScissorRect.left = 0;
	sScissorRect.top = 0;
	sScissorRect.right = static_cast<LONG>(ShadowMapSize);
	sScissorRect.bottom = static_cast<LONG>(ShadowMapSize);

	//============================================================
	// 全Slot初期化
	//============================================================
	for (UINT shadowIndex = 0u; shadowIndex < MaxShadowLights; ++shadowIndex)
	{
		sLightPositions[shadowIndex] =
			XMFLOAT3(
				0.0f,
				0.0f,
				0.0f
			);

		sLightRanges[shadowIndex] = 1.0f;

		sShadowParameters[shadowIndex] =
			XMFLOAT4(
				0.0f,
				1.0f,
				0.0003f,
				0.0020f
			);

		sUpdateModes[shadowIndex] = UpdateMode::EveryFrame;
		sHasCachedLightData[shadowIndex] = false;
		sShadowResolutions[shadowIndex] = ShadowMapSize;
		sCachedStableLightIDs[shadowIndex] = InvalidStableLightID;
		sExcludedCasters[shadowIndex] = nullptr;
	}

	for (UINT sliceIndex = 0u; sliceIndex < TotalSliceCount; ++sliceIndex)
	{
		sLightViewProjection[sliceIndex] = XMMatrixIdentity();
		sSliceResourceStates[sliceIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		sWorldFrustums[sliceIndex] = BoundingFrustum();
		sFaceDirty[sliceIndex] = true;
	}

	sObjectBufferIndex = 0u;
	sLightCount = 0u;
	sRenderingLightIndex = InvalidIndex;
	sRenderingFaceIndex = InvalidIndex;
	sRendering = false;
	sInitialized = true;

	return true;
}

//============================================================
// Point Shadow Manager終了
//============================================================
void PointLightShadowManager::Terminate()
{
	sInitialized =false;
	sRendering =false;

	sLightCount =0u;
	sRenderingLightIndex =InvalidIndex;
	sRenderingFaceIndex =InvalidIndex;
	sObjectBufferIndex =0u;

	if (sObjectBuffer != nullptr &&sMappedObjectBuffer != nullptr)
	{
		sObjectBuffer->Unmap(
			0u,
			nullptr
		);

		sMappedObjectBuffer =nullptr;
	}

	SAFE_RELEASE(sObjectBuffer);
	SAFE_RELEASE(sDSVHeap);
	SAFE_RELEASE(sShadowResource);

	for (UINT sliceIndex = 0u; sliceIndex < TotalSliceCount; ++sliceIndex)
	{
		sDSVHandles[sliceIndex] = {};
		sLightViewProjection[sliceIndex] = XMMatrixIdentity();
		sSliceResourceStates[sliceIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		sWorldFrustums[sliceIndex] = BoundingFrustum();
	}

	for (UINT shadowIndex = 0u; shadowIndex < MaxShadowLights; ++shadowIndex)
	{
		sLightPositions[shadowIndex] =
			XMFLOAT3(
				0.0f,
				0.0f,
				0.0f
			);
		sLightRanges[shadowIndex] = 1.0f;

		sShadowParameters[shadowIndex] =
			XMFLOAT4(
				0.0f,
				1.0f,
				0.0003f,
				0.0020f
			);
		sUpdateModes[shadowIndex] =UpdateMode::EveryFrame;
		sHasCachedLightData[shadowIndex] = false;
		sShadowResolutions[shadowIndex] = ShadowMapSize;
		sExcludedCasters[shadowIndex] = nullptr;
	}

	sObjectBufferStride = 0u;
}

//============================================================
// Point Shadow Frame開始
//============================================================
void PointLightShadowManager::BeginFrame()
{
	if (!sInitialized)
	{
		return;
	}

#if defined(_DEBUG)

	//========================================================
	// Point Shadow使用量確認
	// リセットする前に前フレームの使用数を取得する
	//========================================================
	static UINT debugFrameCount = 0u;
	++debugFrameCount;

	// 60フレームごとにログを出力
	if (debugFrameCount % 60u == 0u)
	{
		WCHAR buffer[256] = {};

		swprintf_s(
			buffer,
			L"[PointShadowPerf] "
			L"PreviousDraws=%u / %u\n",
			sObjectBufferIndex,
			MaxShadowDrawCalls
		);

		OutputDebugStringW(buffer);
	}

#endif

	//============================================================
	// 前フレームの登録数をリセット
	//============================================================
	sLightCount = 0u;

#if defined(_DEBUG)
	{
		//========================================================
		// 実際のリセット処理と同時に計測する
		// ほかの場所でカウンターを0へ戻している場合も、
		// その箇所を確認する必要がある。
		//========================================================
		++gPointShadowResetCount;

		const UINT previousDraws = sObjectBufferIndex;

		// 次のフレームでOverflowを検出可能にする
		gPointShadowOverflowLogged = false;

		//========================================================
		// 120回ごとに実際のリセット状況を出力する
		//========================================================
		if (gPointShadowResetCount % 120 == 0)
		{
			WCHAR buffer[256] = {};

			swprintf_s(
				buffer,
				L"[PointShadowReset] "
				L"Count=%llu PreviousDraws=%u\n",
				static_cast<unsigned long long>(gPointShadowResetCount),
				previousDraws
			);

			OutputDebugStringW(buffer);
		}
	}
#endif

	//============================================================
	// Object Buffer使用位置をリセット
	// 各FaceのBeginRender()ではリセットしない
	//============================================================
	sRendering = false;
	sRenderingLightIndex = InvalidIndex;
	sRenderingFaceIndex = InvalidIndex;

	//============================================================
	// Shadow設定を無効化
	//============================================================
	for (UINT shadowIndex = 0u; shadowIndex < MaxShadowLights; ++shadowIndex)
	{
		sShadowParameters[shadowIndex] =
			XMFLOAT4(
				0.0f,
				1.0f,
				0.0003f,
				0.0020f
			);
	}
}

void PointLightShadowManager::BeginShadowDrawPass()
{
	//========================================================
	// ShadowManagerが初期化されていない場合
	//========================================================
	if (!sInitialized)
	{
		return;
	}

	//========================================================
	// Shadow Object Bufferの使用位置をリセット
	//========================================================
	sObjectBufferIndex = 0u;

#if defined(_DEBUG)

	//========================================================
	// 実際のShadow描画バッファリセット回数
	//========================================================
	++gPointShadowResetCount;

	//========================================================
	// Overflowログの出力制限を解除
	//========================================================
	gPointShadowOverflowLogged = false;

#endif
}

int PointLightShadowManager::AddLight(
	const XMFLOAT3& position, 
	float range, 
	float pcfRadius, 
	float minimumBias,
	float slopeBias, 
	UpdateMode updateMode,
	ShadowResolution shadowResolution, 
	std::uint32_t stableLightID,
	const ObjectBase* excludedCaster
)
{
	if (!sInitialized)
	{
		return -1;
	}

	if (sLightCount >= MaxShadowLights)
	{
		OutputDebugStringW(L"[PointLightShadowManager] Maximum shadow light count reached.\n");
		return -1;
	}

	//============================================================
	// Near Planeより大きいRangeが必要
	//============================================================
	if (range <= 0.1f)
	{
		return -1;
	}

	//============================================================
	// Enumから要求解像度を取得
	//============================================================
	const UINT requestedResolution = static_cast<UINT>(shadowResolution);

	//============================================================
	// Resource本体より大きな解像度は使用できない
	//============================================================
	UINT safeResolution = (std::min)(requestedResolution, ShadowMapSize);

	//============================================================
	// 0や不正値が渡された場合は最大解像度へ戻す
	//============================================================
	if (safeResolution == 0u)
	{
		safeResolution = ShadowMapSize;
	}

	//============================================================
	// ライト個別の要求解像度を重視しオプションの上限だけを適用する
	// 既存のresolutionChangedがキャッシュ無効化と再描画を処理する
	//============================================================
	safeResolution = (std::min)(safeResolution, static_cast<UINT>(sResolutionCap));

	const UINT shadowIndex = sLightCount;

	assert(shadowIndex < MaxShadowLights);

	//============================================================
	// 前フレームからLight位置が変化したか確認
	//============================================================
	bool positionChanged = true;
	bool rangeChanged = true;
	bool updateModeChanged = true;
	bool resolutionChanged = true;
	bool stableLightChanged = true;

	//============================================================
	// 前フレームに同じSlotが使用されていた場合だけ比較する
	//============================================================
	if (sHasCachedLightData[shadowIndex])
	{
		const XMFLOAT3& previousPosition = sLightPositions[shadowIndex];

		const float differenceX = position.x - previousPosition.x;
		const float differenceY = position.y - previousPosition.y;
		const float differenceZ = position.z - previousPosition.z;

		const float positionDifferenceSquared =
			differenceX *
			differenceX +
			differenceY *
			differenceY +
			differenceZ *
			differenceZ;

		//========================================================
		// 非常に小さな浮動小数点誤差では
		// Shadow Mapを再生成しない
		//========================================================
		positionChanged = positionDifferenceSquared > 0.000001f;

		rangeChanged = fabsf(range - sLightRanges[shadowIndex]) > 0.0001f;

		updateModeChanged = sUpdateModes[shadowIndex] != updateMode;

		resolutionChanged = sShadowResolutions[shadowIndex] != safeResolution;

		stableLightChanged = sCachedStableLightIDs[shadowIndex] != stableLightID;
	}

	//============================================================
	// Light情報を更新する前に、差分に応じてDirtyにする
	//============================================================
	const bool lightDataChanged =
		!sHasCachedLightData[shadowIndex] ||
		stableLightChanged ||
		positionChanged ||
		rangeChanged ||
		updateModeChanged ||
		resolutionChanged ||
		sExcludedCasters[shadowIndex] != excludedCaster;

	//============================================================
	// EveryFrame: 毎フレーム6面すべてをDirtyにする
	// OnDemand: Light情報が変化した場合だけ6面をDirtyにする
	//============================================================
	if (updateMode == UpdateMode::EveryFrame || lightDataChanged)
	{
		MarkAllFacesDirty(shadowIndex);
	}

	//============================================================
	// 現在フレームのLight情報を保存
	//============================================================
	sLightPositions[shadowIndex] = position;
	sLightRanges[shadowIndex] = range;
	sUpdateModes[shadowIndex] = updateMode;
	sHasCachedLightData[shadowIndex] = true;
	sCachedStableLightIDs[shadowIndex] = stableLightID;
	sExcludedCasters[shadowIndex] = excludedCaster;

	const XMVECTOR lightPosition =
		XMLoadFloat3(&position);

	//============================================================
	// Point Lightの6方向
	//============================================================
	static const XMVECTORF32 faceDirections[FaceCount] =
	{
		// +X
		{ 1.0f,  0.0f,  0.0f, 0.0f },

		// -X
		{-1.0f,  0.0f,  0.0f, 0.0f },

		// +Y
		{ 0.0f,  1.0f,  0.0f, 0.0f },

		// -Y
		{ 0.0f, -1.0f,  0.0f, 0.0f },

		// +Z
		{ 0.0f,  0.0f,  1.0f, 0.0f },

		// -Z
		{ 0.0f,  0.0f, -1.0f, 0.0f }
	};

	//============================================================
	// 各方向のUp Vector
	// +Y・-Y面では通常のY Upを使用できないため
	// Z方向をUpとして使う
	//============================================================
	static const XMVECTORF32 faceUpDirections[FaceCount] =
	{
		// +X
		{ 0.0f, 1.0f,  0.0f, 0.0f },

		// -X
		{ 0.0f, 1.0f,  0.0f, 0.0f },

		// +Y
		{ 0.0f, 0.0f, -1.0f, 0.0f },

		// -Y
		{ 0.0f, 0.0f,  1.0f, 0.0f },

		// +Z
		{ 0.0f, 1.0f,  0.0f, 0.0f },

		// -Z
		{ 0.0f, 1.0f,  0.0f, 0.0f }
	};

	//============================================================
	// Point Shadow各Face共通のProjection
	// 90度より少しだけ広げ、Cube Face境界を重ねる
	//============================================================
	constexpr float fieldOfViewRadians = XMConvertToRadians(FaceFieldOfViewDegrees);

	const XMMATRIX projection =
		XMMatrixPerspectiveFovLH(
			fieldOfViewRadians,
			1.0f,
			0.1f,// Near Plane
			range// Far Plane
		);

	for (UINT faceIndex = 0u; faceIndex < FaceCount; ++faceIndex)
	{
		//========================================================
		// Point Shadow Light番号とFace番号から
		// Texture2DArrayのSlice番号を取得
		//========================================================
		const UINT sliceIndex = CalculateSliceIndex(shadowIndex, faceIndex);

		if (sliceIndex == InvalidIndex ||
			sliceIndex >= TotalSliceCount)
		{
			assert(false);
			continue;
		}

		//========================================================
		// このCube Face用のView行列
		//========================================================
		const XMMATRIX view =
			XMMatrixLookToLH(
				lightPosition,
				faceDirections[faceIndex],
				faceUpDirections[faceIndex]
			);

		//========================================================
		// Shadow Map描画とシェーダー転送に使用する
		// ViewProjection行列
		//========================================================
		sLightViewProjection[sliceIndex] =
			XMMatrixMultiply(
				view,
				projection
			);

		//========================================================
		// Projection行列からView空間Frustumを作成
		// 第3引数false: Left-Handed座標系として生成する
		// Point Shadowで使用している行列もLHなので
		// falseを指定する
		//========================================================
		BoundingFrustum viewSpaceFrustum = {};

		BoundingFrustum::CreateFromMatrix(
			viewSpaceFrustum,
			projection,
			false
		);

		//========================================================
		// View空間からWorld空間へ変換するため
		// View行列の逆行列を求める
		//========================================================
		XMVECTOR determinant = XMVectorZero();

		const XMMATRIX inverseView =
			XMMatrixInverse(
				&determinant,
				view
			);

		//========================================================
		// View行列が正常に逆行列化できたか確認
		//========================================================
		const float determinantValue = XMVectorGetX(determinant);

		if (!std::isfinite(determinantValue) ||
			fabsf(determinantValue) <= 0.000001f)
		{
			//====================================================
			// 通常は発生しないが、不正なFrustumを使わない
			//====================================================
			sWorldFrustums[sliceIndex] = BoundingFrustum();
			continue;
		}

		//========================================================
		// View空間FrustumをWorld空間へ変換
		// vnModelが返すWorld空間Sphereと直接交差判定できる
		//========================================================
		viewSpaceFrustum.Transform(
			sWorldFrustums[sliceIndex],
			inverseView
		);
	}

	const float safePCFRadius = (std::clamp)(pcfRadius, 0.0f, 4.0f);
	const float safeMinimumBias = (std::max)(minimumBias, 0.0f);
	const float safeSlopeBias = (std::max)(slopeBias, 0.0f);

	//============================================================
	// 現在フレームの解像度を保存
	//============================================================
	sShadowResolutions[shadowIndex] = safeResolution;

	const float shadowUVScale =
		static_cast<float>(safeResolution) /
		static_cast<float>(ShadowMapSize);

	sShadowParameters[shadowIndex] =
		XMFLOAT4(
			shadowUVScale,
			safePCFRadius,
			safeMinimumBias,
			safeSlopeBias
		);

	++sLightCount;

	return static_cast<int>(shadowIndex);
}

bool PointLightShadowManager::BeginRender(UINT shadowIndex, UINT faceIndex)
{
	//============================================================
	// Manager未初期化
	//============================================================
	if (!sInitialized)
	{
		return false;
	}

	//============================================================
	// 既に別のFaceを描画中
	// EndRender()の呼び忘れを検出する
	//============================================================
	if (sRendering)
	{
		assert(false);

		return false;
	}

	//============================================================
	// 登録されていないPoint Shadow番号
	//============================================================
	if (shadowIndex >= sLightCount)
	{
		return false;
	}

	//============================================================
	// Face番号確認
	//============================================================
	if (faceIndex >= FaceCount)
	{
		return false;
	}

	if (sShadowResource == nullptr || sDSVHeap == nullptr)
	{
		return false;
	}

	//============================================================
	// キャッシュ済みで更新不要なFace
	// falseを返すことで、renderShadowPass()側の
	// continueへ進ませる
	//============================================================
	if (!ShouldRenderFace(shadowIndex, faceIndex))
	{
		return false;
	}

	//============================================================
	// Light番号とFace番号からTexture Array Sliceを算出
	//============================================================
	const UINT sliceIndex =
		CalculateSliceIndex(
			shadowIndex,
			faceIndex
		);

	if (sliceIndex == InvalidIndex || sliceIndex >= TotalSliceCount)
	{
		assert(false);

		return false;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return false;
	}

	//============================================================
	// 今回描画するSliceだけDepth Writeへ変更
	//============================================================
	ChangeSliceResourceState(sliceIndex, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	//============================================================
	// 現在のPoint Shadow Lightが使用する実効解像度
	//============================================================
	const UINT activeResolution = sShadowResolutions[shadowIndex];

	//============================================================
	// ライトごとのViewport
	// Texture2DArrayの左上領域だけを使用する
	//============================================================
	D3D12_VIEWPORT activeViewport = {};
	activeViewport.TopLeftX = 0.0f;
	activeViewport.TopLeftY = 0.0f;
	activeViewport.Width = static_cast<float>(activeResolution);
	activeViewport.Height = static_cast<float>(activeResolution);
	activeViewport.MinDepth = 0.0f;
	activeViewport.MaxDepth = 1.0f;

	//============================================================
	// ライトごとのScissor Rect
	//============================================================
	D3D12_RECT activeScissorRect = {};
	activeScissorRect.left = 0;
	activeScissorRect.top = 0;
	activeScissorRect.right = static_cast<LONG>(activeResolution);
	activeScissorRect.bottom = static_cast<LONG>(activeResolution);

	//============================================================
	// Shadow Map用Viewport
	//============================================================
	commandList->RSSetViewports(
		1u,
		&activeViewport
	);

	commandList->RSSetScissorRects(
		1u,
		&activeScissorRect
	);

	//============================================================
	// 今回描画するSlice専用DSV
	//============================================================
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = sDSVHandles[sliceIndex];

	//============================================================
	// Color Render Targetは使用せず、Depthだけ設定
	//============================================================
	commandList->OMSetRenderTargets(
		0u,
		nullptr,
		FALSE,
		&dsvHandle
	);

	const D3D12_RECT clearRect = activeScissorRect;

	//============================================================
	// 指定FaceのShadow MapをDepth 1.0でクリア
	//============================================================
	commandList->ClearDepthStencilView(
		dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0u,
		1u,
		&clearRect
	);

	//============================================================
	// Directional・Spot Shadowと同じ
	// Shadow描画用Root Signatureを使用
	//============================================================
	commandList->SetGraphicsRootSignature(ShadowMapManager::GetRootSignature());

	//============================================================
	// 現在描画中のLightとFaceを記録
	// SetObjectMatrix()がこの情報から
	// 使用するViewProjectionを選択する
	//============================================================
	sRendering = true;
	sRenderingLightIndex = shadowIndex;
	sRenderingFaceIndex = faceIndex;

	return true;
}


//============================================================
// Point Shadow Face描画終了
//============================================================
void PointLightShadowManager::EndRender()
{
	if (!sRendering)
	{
		return;
	}

	//============================================================
	// 現在描画中の情報を退避
	// この後InvalidIndexへ戻すため、先に保存する
	//============================================================
	const UINT finishedShadowIndex = sRenderingLightIndex;
	const UINT finishedFaceIndex = sRenderingFaceIndex;

	const UINT sliceIndex = CalculateSliceIndex(
		finishedShadowIndex,
		finishedFaceIndex
	);

	if (sliceIndex != InvalidIndex && sliceIndex < TotalSliceCount)
	{
		//========================================================
		// 描画したFaceだけPixel Shader Resourceへ戻す
		//========================================================
		ChangeSliceResourceState(
			sliceIndex,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
	}
	else
	{
		assert(false);
	}

	if (sliceIndex != InvalidIndex && sliceIndex < TotalSliceCount)
	{
		ChangeSliceResourceState(sliceIndex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		//========================================================
		// このFaceのShadow Map更新が完了した
		//========================================================
		sFaceDirty[sliceIndex] = false;
	}

	sRendering = false;

	sRenderingLightIndex = InvalidIndex;
	sRenderingFaceIndex = InvalidIndex;
}

//============================================================
// Point Shadow用Object行列設定
// 現在描画中のPoint Light・Faceに対応した
// LightViewProjectionを自動で選択する
//============================================================
bool PointLightShadowManager::SetObjectMatrix(const XMMATRIX& world)
{
	//============================================================
	// Point Shadow描画中でなければ設定しない
	//============================================================
	if (!sRendering)
	{
		return false;
	}

	if (sRenderingLightIndex == InvalidIndex || sRenderingFaceIndex == InvalidIndex)
	{
		return false;
	}

	if (sRenderingLightIndex >= sLightCount || sRenderingFaceIndex >= FaceCount)
	{
		return false;
	}

	if (sObjectBuffer == nullptr || sMappedObjectBuffer == nullptr)
	{
		return false;
	}

	//============================================================
	// 1フレームで使用可能な最大Draw数を超えた
	//============================================================
	if (sObjectBufferIndex >= MaxShadowDrawCalls)
	{
#if defined(_DEBUG)

		//========================================================
		// 同じBeginFrame期間では最初の1回だけ記録
		//========================================================
		if (!gPointShadowOverflowReported)
		{
			gPointShadowOverflowReported = true;

			WCHAR buffer[256] = {};

			swprintf_s(
				buffer,
				L"[PointShadowOverflow] "
				L"ResetID=%llu "
				L"Light=%u Face=%u "
				L"Draws=%u Capacity=%u\n",
				static_cast<unsigned long long>(gPointShadowBeginFrameCount),
				sRenderingLightIndex,
				sRenderingFaceIndex,
				sObjectBufferIndex,
				MaxShadowDrawCalls
			);

			OutputDebugStringW(buffer);
		}

#endif
		return false;
	}

	const UINT sliceIndex = CalculateSliceIndex(
		sRenderingLightIndex,
		sRenderingFaceIndex
	);

	if (sliceIndex == InvalidIndex || sliceIndex >= TotalSliceCount)
	{
		return false;
	}

	//============================================================
	// 今回使用するConstant Buffer領域
	// ※Faceが変わってもIndexはリセットしない
	// GPU実行前に別Faceの行列で上書きされるのを防ぐ
	//============================================================
	const UINT64 byteOffset =
		static_cast<UINT64>(sObjectBufferIndex) *
		static_cast<UINT64>(sObjectBufferStride);

	ObjectBuffer* objectBuffer =
		reinterpret_cast<ObjectBuffer*>(sMappedObjectBuffer + byteOffset);

	//============================================================
	// ObjectのWorld行列
	// Shader側でmul(vector, matrix)を使用するためTranspose
	//============================================================
	XMStoreFloat4x4(
		&objectBuffer->World,
		XMMatrixTranspose(world)
	);

	//============================================================
	// 現在描画中のPoint Light Face用ViewProjection
	//============================================================
	XMStoreFloat4x4(
		&objectBuffer->LightViewProjection,
		XMMatrixTranspose(sLightViewProjection[sliceIndex])
	);

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return false;
	}

	//============================================================
	// Shadow Root SignatureのRoot Parameter 0へ設定
	// Spot Shadowで使用している位置と同じ
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		0u,
		sObjectBuffer->GetGPUVirtualAddress() + byteOffset
	);

#if defined(_DEBUG)

	//============================================================
	// 通常は約1,000〜1,500回なので、
	// 2,048回を超えた時点で異常増加の原因を追跡する。
	// 以降、使用量が増えた場合も段階的に記録する
	//============================================================
	if (sObjectBufferIndex == 2048u ||
		sObjectBufferIndex == 4096u ||
		sObjectBufferIndex == 8192u ||
		sObjectBufferIndex == 16384u ||
		sObjectBufferIndex == 32768u)
	{
		WCHAR buffer[256] = {};

		swprintf_s(
			buffer,
			L"[PointShadowHighUsage] "
			L"ResetCount=%llu "
			L"Draws=%u Light=%u Face=%u\n",
			static_cast<unsigned long long>(gPointShadowResetCount),
			sObjectBufferIndex,
			sRenderingLightIndex,
			sRenderingFaceIndex
		);

		OutputDebugStringW(buffer);
	}

#endif

	//============================================================
	// 次のDraw用領域へ進める
	//============================================================
	++sObjectBufferIndex;

	return true;
}

//============================================================
// Point Shadow Texture2DArray SRV作成
//============================================================
void PointLightShadowManager::WriteSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle)
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || sShadowResource == nullptr || destinationHandle.ptr == 0)
	{
		return;
	}

	const D3D12_RESOURCE_DESC resourceDesc = sShadowResource->GetDesc();

	assert(resourceDesc.DepthOrArraySize == TotalSliceCount);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0u;
	srvDesc.Texture2DArray.MipLevels = 1u;
	srvDesc.Texture2DArray.FirstArraySlice = 0u;
	srvDesc.Texture2DArray.ArraySize = TotalSliceCount;
	srvDesc.Texture2DArray.PlaneSlice = 0u;
	srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(
		sShadowResource,
		&srvDesc,
		destinationHandle
	);
}

UINT PointLightShadowManager::CalculateSliceIndex(UINT shadowIndex, UINT faceIndex)
{
	if (shadowIndex >= MaxShadowLights)
	{
		return InvalidIndex;
	}

	if (faceIndex >= FaceCount)
	{
		return InvalidIndex;
	}

	return shadowIndex * FaceCount + faceIndex;
}

bool PointLightShadowManager::AreAllSlicesShaderReadable()
{
	for (UINT sliceIndex = 0u; sliceIndex < TotalSliceCount; ++sliceIndex)
	{
		if (sSliceResourceStates[sliceIndex] != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		{
			return false;
		}
	}

	return true;
}

void PointLightShadowManager::MarkLightDirty(UINT shadowIndex)
{
	if (!sInitialized)
	{
		return;
	}

	MarkAllFacesDirty(shadowIndex);
}

void PointLightShadowManager::MarkAllLightsDirty()
{
	if (!sInitialized)
	{
		return;
	}

	for (UINT shadowIndex = 0u;shadowIndex < MaxShadowLights;++shadowIndex)
	{
		MarkAllFacesDirty(shadowIndex);
	}
}

bool PointLightShadowManager::ShouldSkipCaster(UINT shadowIndex, const ObjectBase* object)
{
	return shadowIndex < sLightCount && object != nullptr && sExcludedCasters[shadowIndex] == object;
}

bool PointLightShadowManager::ShouldRenderFace(UINT shadowIndex, UINT faceIndex)
{
	if (!sInitialized)
	{
		return false;
	}

	if (shadowIndex >= sLightCount ||
		shadowIndex >= MaxShadowLights)
	{
		return false;
	}

	if (faceIndex >= FaceCount)
	{
		return false;
	}

	const UINT sliceIndex = CalculateSliceIndex(
		shadowIndex,
		faceIndex
	);

	if (sliceIndex == InvalidIndex || sliceIndex >= TotalSliceCount)
	{
		return false;
	}

	//============================================================
	// 毎フレーム更新モードなら常に描画する
	//============================================================
	if (sUpdateModes[shadowIndex] == UpdateMode::EveryFrame)
	{
		return true;
	}

	//============================================================
	// OnDemandではDirtyなFaceだけ描画する
	//============================================================
	return sFaceDirty[sliceIndex];
}

bool PointLightShadowManager::IsSphereInsideShadowFrustum(UINT shadowIndex, UINT faceIndex, FXMVECTOR worldCenter, float worldRadius)
{
	//============================================================
	// Managerが未初期化の場合
	// 判定できない場合にObjectの影を消さないよう
	// 安全側としてtrueを返す
	//============================================================
	if (!sInitialized)
	{
		return true;
	}

	//============================================================
	// 未登録のPoint Shadow番号
	//============================================================
	if (shadowIndex >= sLightCount ||
		shadowIndex >= MaxShadowLights)
	{
		return true;
	}

	//============================================================
	// Face番号が範囲外
	//============================================================
	if (faceIndex >= FaceCount)
	{
		return true;
	}

	//============================================================
	// Bounding Sphere半径が不正
	// Boundsが壊れていても影を欠落させないため
	// Cullingせずに描画する
	//============================================================
	if (!std::isfinite(worldRadius) ||worldRadius <= 0.0f)
	{
		return true;
	}

	//============================================================
	// World座標が有限値か確認
	//============================================================
	XMFLOAT3 center = {};

	XMStoreFloat3(
		&center,
		worldCenter
	);

	if (!std::isfinite(center.x) ||!std::isfinite(center.y) ||!std::isfinite(center.z))
	{
		return true;
	}

	//============================================================
	// Point ShadowのSlice番号を取得
	//============================================================
	const UINT sliceIndex = CalculateSliceIndex(
		shadowIndex,
		faceIndex
	);

	if (sliceIndex == InvalidIndex ||sliceIndex >= TotalSliceCount)
	{
		return true;
	}

	//============================================================
	// DirectXCollision用Bounding Sphereを作成
	//============================================================
	BoundingSphere boundingSphere = {};

	boundingSphere.Center = center;

	//============================================================
	// Face境界付近でObjectの影が欠けないように、
	// ※少しだけSphereを広げて判定する
	// ※特にPoint Shadowでは、Cube Face同士の境界上に
	// Objectが存在する可能性があるため重要
	//============================================================
	boundingSphere.Radius =
		worldRadius *
		1.05f +
		0.02f;

	//============================================================
	// FrustumとSphereの包含状態
	// DISJOINT: 完全にFrustum外
	// INTERSECTS: Frustum境界と交差
	// CONTAINS: 完全にFrustum内
	//============================================================
	const ContainmentType containment = sWorldFrustums[sliceIndex].Contains(boundingSphere);

	//============================================================
	// 完全に外側の場合だけ描画を省略する
	//============================================================
	return containment != DISJOINT;
}

bool PointLightShadowManager::IsSphereInsideLightRange(UINT shadowIndex, FXMVECTOR worldCenter, float worldRadius)
{
	//============================================================
	// 判定できない場合は安全側として描画する
	// Shadow Cullingでは
	// False Positive: 少し余計に描画する
	// False Negative: 必要な影そのものが消える
	//============================================================
	if (!sInitialized)
	{
		return true;
	}


	//============================================================
	// Light番号確認
	//============================================================
	if (shadowIndex >= sLightCount || shadowIndex >= MaxShadowLights)
	{
		return true;
	}


	//============================================================
	// Bounds Radius確認
	//============================================================
	if (!std::isfinite(worldRadius) || worldRadius <= 0.0f)
	{
		return true;
	}


	//============================================================
	// Object Center確認
	//============================================================
	XMFLOAT3 center = {};

	XMStoreFloat3(
		&center,
		worldCenter
	);


	if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
	{
		return true;
	}


	//============================================================
	// Point Light位置
	//============================================================
	const XMFLOAT3& lightPosition = sLightPositions[shadowIndex];

	//============================================================
	// Light Range
	//============================================================
	const float lightRange = sLightRanges[shadowIndex];


	if (!std::isfinite(lightRange) || lightRange <= 0.0f)
	{
		return true;
	}


	//============================================================
	// Light → Object Bounding Sphere中心までの距離
	//============================================================
	const float dx =
		center.x - lightPosition.x;

	const float dy =
		center.y - lightPosition.y;

	const float dz =
		center.z - lightPosition.z;


	const float distanceSquared =
		dx * dx + dy * dy + dz * dz;


	//============================================================
	// Bounding Sphereの一部でも
	// Light Rangeへ入っていれば描画対象
	// Object Centerだけで判定すると
	// 大きな壁などで
	// Center = Range外
	// Geometry = Range内
	//============================================================
	const float conservativeRange = lightRange + worldRadius;

	const float conservativeRangeSquared = conservativeRange * conservativeRange;


	//============================================================
	// Range Sphere同士が完全に離れている場合だけCulling
	//============================================================
	return distanceSquared <= conservativeRangeSquared;
}

bool PointLightShadowManager::IsActive()
{
	return sInitialized && sLightCount > 0u;
}

bool PointLightShadowManager::IsRendering()
{
	return sRendering;
}

UINT PointLightShadowManager::GetLightCount()
{
	if (!sInitialized)
	{
		return 0u;
	}

	return sLightCount;
}

const XMMATRIX& PointLightShadowManager::GetLightViewProjection(UINT shadowIndex, UINT faceIndex)
{
	const UINT sliceIndex = CalculateSliceIndex(
		shadowIndex,
		faceIndex
	);

	if (!sInitialized ||
		shadowIndex >= sLightCount ||
		sliceIndex == InvalidIndex ||
		sliceIndex >= TotalSliceCount)
	{
		return sIdentityMatrix;
	}

	return sLightViewProjection[sliceIndex];
}

XMFLOAT4 PointLightShadowManager::GetShadowParameters(UINT shadowIndex)
{
	if (!sInitialized || shadowIndex >= sLightCount)
	{
		return XMFLOAT4(
			0.0f,
			1.0f,
			0.0003f,
			0.0020f
		);
	}

	return sShadowParameters[shadowIndex];
}

UINT PointLightShadowManager::GetRenderingLightIndex()
{
	return sRenderingLightIndex;
}

UINT PointLightShadowManager::GetRenderingFaceIndex()
{
	return sRenderingFaceIndex;
}

void PointLightShadowManager::MarkAllFacesDirty(UINT shadowIndex)
{
	if (shadowIndex >= MaxShadowLights)
	{
		return;
	}

	for (UINT faceIndex = 0u; faceIndex < FaceCount; ++faceIndex)
	{
		const UINT sliceIndex = CalculateSliceIndex(
			shadowIndex,
			faceIndex
		);

		if (sliceIndex == InvalidIndex || sliceIndex >= TotalSliceCount)
		{
			continue;
		}

		sFaceDirty[sliceIndex] = true;
	}
}

//============================================================
// Point Shadow Texture2DArrayの指定Sliceだけ
// Resource Stateを変更
//============================================================
void PointLightShadowManager::ChangeSliceResourceState(UINT sliceIndex, D3D12_RESOURCE_STATES afterState)
{
	//============================================================
	// Resourceが作られていない
	//============================================================
	if (sShadowResource == nullptr)
	{
		return;
	}

	//============================================================
	// Slice番号確認
	// 有効範囲: 0 ～ TotalSliceCount - 1
	//============================================================
	if (sliceIndex >= TotalSliceCount)
	{
		assert(false);
		return;
	}

	//============================================================
	// 現在のResource Stateを取得
	//============================================================
	const D3D12_RESOURCE_STATES beforeState = sSliceResourceStates[sliceIndex];

	//============================================================
	// 同じ状態ならBarrier不要
	//============================================================
	if (beforeState == afterState)
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return;
	}

	//============================================================
	// Texture2DArrayのSubresource番号
	// 現在のResource構成:
	//   MipLevels = 1 PlaneCount = 1のため
	//   Subresource番号 = Array Slice番号になる
	//============================================================
	const UINT subresourceIndex = sliceIndex;

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = sShadowResource;
	//============================================================
	// Resource全体ではなく,指定Sliceだけ変更する
	//============================================================
	barrier.Transition.Subresource = subresourceIndex;
	barrier.Transition.StateBefore = beforeState;
	barrier.Transition.StateAfter = afterState;

	commandList->ResourceBarrier(
		1u,
		&barrier
	);

	//============================================================
	// CPU側で管理している状態も更新
	//============================================================
	sSliceResourceStates[sliceIndex] = afterState;
}