#include "../../framework.h"
#include "../vn_environment.h"

#include "spotLightShadowManager.h"

//============================================================
// Static Member
//============================================================
bool SpotLightShadowManager::sInitialized =false;
bool SpotLightShadowManager::sRendering =false;
ID3D12Resource*SpotLightShadowManager::sShadowResource =nullptr;
ID3D12DescriptorHeap*SpotLightShadowManager::sDSVHeap =nullptr;
D3D12_CPU_DESCRIPTOR_HANDLE SpotLightShadowManager::sDSVHandles[SpotLightShadowManager::MaxShadowLights] = {};
ID3D12Resource*SpotLightShadowManager::sObjectBuffer =nullptr;
BYTE*SpotLightShadowManager::sMappedObjectBuffer =nullptr;
UINT SpotLightShadowManager::sObjectBufferStride =0;
UINT SpotLightShadowManager::sObjectBufferIndex =0;
UINT SpotLightShadowManager::sLightCount = 0;
UINT SpotLightShadowManager::sRenderingLightIndex = SpotLightShadowManager::InvalidShadowIndex;
D3D12_VIEWPORT SpotLightShadowManager::sViewport ={};
D3D12_RECT SpotLightShadowManager::sScissorRect ={};
XMMATRIX SpotLightShadowManager::sLightView[SpotLightShadowManager::MaxShadowLights];
XMMATRIX SpotLightShadowManager::sLightProjection[SpotLightShadowManager::MaxShadowLights];
XMMATRIX SpotLightShadowManager::sLightViewProjection[SpotLightShadowManager::MaxShadowLights];
XMMATRIX SpotLightShadowManager::sIdentityMatrix = XMMatrixIdentity();
XMFLOAT4 SpotLightShadowManager::sShadowParameters[SpotLightShadowManager::MaxShadowLights] = {};
BoundingFrustum SpotLightShadowManager::sWorldFrustums[SpotLightShadowManager::MaxShadowLights] = {};

D3D12_RESOURCE_STATES SpotLightShadowManager::sSliceResourceStates[SpotLightShadowManager::MaxShadowLights] = {};
const ObjectBase* SpotLightShadowManager::sExcludedCasters[SpotLightShadowManager::MaxShadowLights] = {};
//============================================================
// 初期化
//============================================================
bool SpotLightShadowManager::Initialize()
{
	if (sInitialized)
	{
		return true;
	}

	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr)
	{
		return false;
	}

	HRESULT hr = S_OK;

	//============================================================
	// ShadowMap Resource
	// Resource: R32_TYPELESS
	// DSV: D32_FLOAT
	// SRV: R32_FLOAT
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = ShadowMapSize;
	resourceDesc.Height = ShadowMapSize;
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(MaxShadowLights);
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,// 通常描画から参照できる状態で開始
		&clearValue,
		IID_PPV_ARGS(&sShadowResource)
	);

	if (FAILED(hr) || sShadowResource == nullptr)
	{
		assert(false);
		return false;
	}

	sShadowResource->SetName(L"Spot Light Shadow Resource");

	for (UINT shadowIndex = 0u; shadowIndex < MaxShadowLights; ++shadowIndex)
		sSliceResourceStates[shadowIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//============================================================
	// DSV Heap
	//============================================================
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NumDescriptors = MaxShadowLights;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

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

	sDSVHeap->SetName(L"Spot Light Shadow DSV Heap");

	//============================================================
	// DSV Descriptor 1個分のサイズ
	//============================================================
	const UINT dsvDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	const D3D12_CPU_DESCRIPTOR_HANDLE dsvHeapStart = sDSVHeap->GetCPUDescriptorHandleForHeapStart();

	for (UINT shadowIndex = 0u; shadowIndex < MaxShadowLights; ++shadowIndex)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeapStart;

		dsvHandle.ptr +=
			static_cast<SIZE_T>(shadowIndex) *
			static_cast<SIZE_T>(dsvDescriptorSize);

		sDSVHandles[shadowIndex] = dsvHandle;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		//========================================================
		// Texture2DArray
		//========================================================
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2DArray.MipSlice = 0u;
		//========================================================
		// このDSVが参照するArray Slice
		//========================================================
		dsvDesc.Texture2DArray.FirstArraySlice = shadowIndex;
		//========================================================
		// 1つのDSVで1Sliceだけを描画する
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
	// ※1オブジェクトにつき256byte境界へ揃える
	//============================================================
	sObjectBufferStride = (sizeof(ObjectBuffer) + 255u) & ~255u;

	const UINT64 objectBufferSize =
		static_cast<UINT64>(sObjectBufferStride) *
		static_cast<UINT64>(MaxShadowDrawCalls);

	D3D12_HEAP_PROPERTIES objectHeapProperties = {};
	objectHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	objectHeapProperties.CreationNodeMask = 1;
	objectHeapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC objectBufferDesc = {};
	objectBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	objectBufferDesc.Width = objectBufferSize;
	objectBufferDesc.Height = 1;
	objectBufferDesc.DepthOrArraySize = 1;
	objectBufferDesc.MipLevels = 1;
	objectBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	objectBufferDesc.SampleDesc.Count = 1;
	objectBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	hr = device->CreateCommittedResource(
		&objectHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&objectBufferDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&sObjectBuffer)
	);

	if (FAILED(hr) || sObjectBuffer == nullptr)
	{
		assert(false);
		Terminate();
		return false;
	}

	sObjectBuffer->SetName(L"Spot Light Shadow Object Buffer");

	D3D12_RANGE readRange =
	{
		0,
		0
	};

	hr = sObjectBuffer->Map(
		0,
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
	// Viewport
	//============================================================
	sViewport.TopLeftX = 0.0f;
	sViewport.TopLeftY = 0.0f;
	sViewport.Width = static_cast<float>(ShadowMapSize);
	sViewport.Height = static_cast<float>(ShadowMapSize);
	sViewport.MinDepth = 0.0f;
	sViewport.MaxDepth = 1.0f;

	sScissorRect =
	{
		0,
		0,
		static_cast<LONG>(ShadowMapSize),
		static_cast<LONG>(ShadowMapSize)
	};

	//============================================================
	// Light Matrix初期化
	//============================================================
	for (UINT shadowIndex = 0u; shadowIndex < MaxShadowLights; ++shadowIndex)
	{
		sLightView[shadowIndex] = XMMatrixIdentity();
		sLightProjection[shadowIndex] = XMMatrixIdentity();
		sLightViewProjection[shadowIndex] = XMMatrixIdentity();
		sShadowParameters[shadowIndex] =
			XMFLOAT4(
				0.0f,      // Enable
				1.0f,      // PCF Radius
				0.0003f,   // Minimum Bias
				0.0020f    // Slope Bias
			);
		sSliceResourceStates[shadowIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	sLightCount = 0u;
	sRenderingLightIndex = InvalidShadowIndex;
	sObjectBufferIndex = 0u;

	sInitialized = true;

	return true;
}

//============================================================
// 終了
//============================================================
void SpotLightShadowManager::Terminate()
{
	sInitialized = false;
	sRendering = false;

	if (sObjectBuffer != nullptr && sMappedObjectBuffer != nullptr)
	{
		sObjectBuffer->Unmap(
			0,
			nullptr
		);

		sMappedObjectBuffer = nullptr;
	}

	SAFE_RELEASE(sObjectBuffer);
	SAFE_RELEASE(sDSVHeap);
	SAFE_RELEASE(sShadowResource);
	for (UINT shadowIndex = 0u; shadowIndex < MaxShadowLights; ++shadowIndex)
	{
		sSliceResourceStates[shadowIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		sDSVHandles[shadowIndex] = {};

		sLightView[shadowIndex] = XMMatrixIdentity();
		sLightProjection[shadowIndex] = XMMatrixIdentity();
		sLightViewProjection[shadowIndex] = XMMatrixIdentity();
	}

	sObjectBufferStride = 0;
	sObjectBufferIndex = 0;
}

//============================================================
// フレーム開始
//============================================================
void SpotLightShadowManager::BeginFrame()
{
	sLightCount = 0;
	sRendering = false;
	sObjectBufferIndex = 0;
	sRenderingLightIndex = InvalidShadowIndex;

	// 全Shadow Slotを無効化
	for (UINT shadowIndex = 0u;shadowIndex < MaxShadowLights;++shadowIndex)
	{
		sShadowParameters[shadowIndex] =
			XMFLOAT4(
				0.0f,      // Disable
				1.0f,      // PCF Radius
				0.0003f,   // Minimum Bias
				0.0020f    // Slope Bias
			);

		sExcludedCasters[shadowIndex] = nullptr;
	}
}


void SpotLightShadowManager::ChangeSliceResourceState(UINT shadowIndex,D3D12_RESOURCE_STATES afterState)
{
	if (sShadowResource == nullptr || shadowIndex >= MaxShadowLights)
	{
		assert(false);
		return;
	}

	//============================================================
	// 現在状態を取得
	//============================================================
	const D3D12_RESOURCE_STATES beforeState = sSliceResourceStates[shadowIndex];

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

	const UINT subresourceIndex =
		Calculate::CalculateD3D12Subresource(
			0u,// Mip Slice
			shadowIndex,// Array Slice
			0u,// Plane Slice
			1u,// Mip Level数
			MaxShadowLights// Array Slice数
		);


	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = sShadowResource;
	barrier.Transition.StateBefore = beforeState;
	barrier.Transition.StateAfter = afterState;
	barrier.Transition.Subresource = subresourceIndex;

	commandList->ResourceBarrier(
		1,
		&barrier
	);

	sSliceResourceStates[shadowIndex] = afterState;
}

int SpotLightShadowManager::AddLight(
	const XMFLOAT3& position,
	const XMFLOAT3& direction,
	float range,
	float outerAngleDegrees,
	float pcfRadius,
	float minimumBias,
	float slopeBias
)
{
	if (!sInitialized)
	{
		return -1;
	}

	if (sLightCount >= sShadowLightBudget)
	{
		return -1; // このライトの影を割り当てない
	}

	//============================================================
	// 最大Shadow Light数を超えた
	//============================================================
	if (sLightCount >= MaxShadowLights)
	{
		OutputDebugStringW(L"[SpotLightShadowManager] Maximum shadow light count reached.\n");
		return -1;
	}

	//============================================================
	// Far PlaneはNear Planeより大きい必要がある
	//============================================================
	if (range <= 0.1f)
	{
		return -1;
	}

	const XMVECTOR directionVector = XMLoadFloat3(&direction);

	const float directionLengthSquared = XMVectorGetX(XMVector3LengthSq(directionVector));

	//============================================================
	// ゼロ方向は使用できない
	//============================================================
	if (directionLengthSquared <= 0.000001f)
	{
		return -1;
	}

	const XMVECTOR normalizedDirection = XMVector3Normalize(directionVector);

	const XMVECTOR lightPosition = XMLoadFloat3(&position);

	//============================================================
	// View Matrix用Up Vector
	// Spot方向とY Upが平行に近い場合は
	// Z軸をUpとして使用する
	//============================================================
	XMVECTOR upDirection =
		XMVectorSet(
			0.0f,
			1.0f,
			0.0f,
			0.0f
		);

	const float directionDotUp =
		fabsf(XMVectorGetX(XMVector3Dot(normalizedDirection, upDirection)));

	if (directionDotUp > 0.99f)
	{
		upDirection = XMVectorSet(
			0.0f,
			0.0f,
			1.0f,
			0.0f
		);
	}

	//============================================================
	// 今回使用するTexture2DArrayのSlice番号
	//============================================================
	const UINT shadowIndex = sLightCount;
	assert(shadowIndex < MaxShadowLights);

	//============================================================
	// Outer Angleを安全な範囲へ制限
	//============================================================
	const float safeOuterAngle =
		(std::clamp)(outerAngleDegrees, 1.0f, 89.0f);

	//============================================================
	// ※outerAngleは半角なのでFOVには2倍した値を使う
	//============================================================
	const float fieldOfView =
		XMConvertToRadians(safeOuterAngle * 2.0f);

	//============================================================
	// Light View Matrix
	//============================================================
	sLightView[shadowIndex] = XMMatrixLookToLH(
		lightPosition,
		normalizedDirection,
		upDirection
	);

	//============================================================
	// Light Projection Matrix
	//============================================================
	sLightProjection[shadowIndex] =
		XMMatrixPerspectiveFovLH(
			fieldOfView,
			1.0f,// Shadow Mapは正方形
			0.1f,// Near
			range// Far
		);

	//============================================================
	// Light ViewProjection
	//============================================================
	sLightViewProjection[shadowIndex] =
		XMMatrixMultiply(
			sLightView[shadowIndex],
			sLightProjection[shadowIndex]
		);

	//============================================================
	// View空間のSpot Shadow Frustumを生成
	// Projection行列から、Near・Far・左右・上下の
	// 視錐台情報を生成する
	// false: Left-Handed座標系として生成する
	//============================================================
	BoundingFrustum viewSpaceFrustum = {};

	BoundingFrustum::CreateFromMatrix(
		viewSpaceFrustum,
		sLightProjection[shadowIndex],
		false
	);

	//============================================================
	// Light View行列の逆行列
	// View空間のFrustumをWorld空間へ変換するために使用する
	//============================================================
	XMVECTOR inverseViewDeterminant = {};

	const XMMATRIX inverseLightView =
		XMMatrixInverse(
			&inverseViewDeterminant,
			sLightView[shadowIndex]
		);

	//============================================================
	// View空間FrustumをWorld空間へ変換
	// World空間のBounding Sphereと直接交差判定できる
	//============================================================
	viewSpaceFrustum.Transform(
		sWorldFrustums[shadowIndex],
		inverseLightView
	);

	//============================================================
	// PCF Radiusを安全な範囲へ制限
	// ※値を大きくしすぎると サンプル間隔が空き、輪郭が不自然になる
	//============================================================
	const float safePCFRadius = (std::clamp)(pcfRadius, 0.0f, 4.0f);

	//============================================================
	// Biasは負数にならないようにする
	//============================================================
	const float safeMinimumBias = (std::max)(minimumBias, 0.0f);
	const float safeSlopeBias = (std::max)(slopeBias, 0.0f);

	//============================================================
	// このSpot Shadow固有の設定を保存
	// HLSL側: SpotShadowParam[shadowIndex]
	//============================================================
	sShadowParameters[shadowIndex] =
		XMFLOAT4(
			1.0f,              // Enable
			safePCFRadius,      // PCF Radius
			safeMinimumBias,    // Minimum Bias
			safeSlopeBias       // Slope Bias
		);


	++sLightCount;

	return static_cast<int>(shadowIndex);
}

//============================================================
// Shadow Pass開始
//============================================================
bool SpotLightShadowManager::BeginRender(UINT shadowIndex)
{
	if (!sInitialized)
	{
		return false;
	}

	//============================================================
	// 登録されていないShadow Index
	//============================================================
	if (shadowIndex >= sLightCount)
	{
		return false;
	}

	if (sShadowResource == nullptr || sDSVHeap == nullptr)
	{
		return false;
	}

	if (sRendering)
	{
		//========================================================
		// EndRender()を呼ばずに次のShadow描画へ進んでいる
		//========================================================
		assert(false);

		return false;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return false;
	}

	//============================================================
	// 指定SliceだけDepth WriteへTransition
	//============================================================
	ChangeSliceResourceState(
		shadowIndex,
		D3D12_RESOURCE_STATE_DEPTH_WRITE
	);

	commandList->RSSetViewports(
		1u,
		&sViewport
	);

	commandList->RSSetScissorRects(
		1u,
		&sScissorRect
	);

	const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = sDSVHandles[shadowIndex];

	//============================================================
	// Color Render Targetは使用せず、
	// 指定SliceのDepthだけを設定
	//============================================================
	commandList->OMSetRenderTargets(
		0u,
		nullptr,
		FALSE,
		&dsvHandle
	);

	//============================================================
	// 指定したSliceだけDepth 1.0でクリア
	//============================================================
	commandList->ClearDepthStencilView(
		dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0u,
		0u,
		nullptr
	);

	commandList->SetGraphicsRootSignature(ShadowMapManager::GetRootSignature());

	sRendering = true;
	sRenderingLightIndex = shadowIndex;

	return true;
}


//============================================================
// Shadow Pass終了
//============================================================
void SpotLightShadowManager::EndRender()
{
	if (!sRendering)
	{
		return;
	}

	//============================================================
	// 現在描画中のSlice番号を保存
	// この後sRenderingLightIndexを無効化するため
	// 先にローカル変数へ退避する
	//============================================================
	const UINT finishedShadowIndex = sRenderingLightIndex;

	if (finishedShadowIndex >= MaxShadowLights)
	{
		assert(false);
		sRendering = false;
		sRenderingLightIndex = InvalidShadowIndex;

		return;
	}

	//============================================================
	// 描画したSliceだけPixel Shader Resourceへ戻す
	//============================================================
	ChangeSliceResourceState(finishedShadowIndex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	sRendering = false;
	sRenderingLightIndex = InvalidShadowIndex;
}

void SpotLightShadowManager::BeginShadowDrawPass()
{
	if (!sInitialized)
	{
		return;
	}

	// 各ライトのBeginRender()よりも前に呼ぶ
	assert(!sRendering);

	// 今回のシャドウ描画で使用するObject CBVの先頭へ戻す
	sObjectBufferIndex = 0u;
}

//============================================================
// オブジェクト行列設定
//============================================================
void SpotLightShadowManager::SetObjectMatrix(const XMMATRIX& world)
{
	if (!sRendering)
	{
		return;
	}

	if (sRenderingLightIndex == InvalidShadowIndex)
	{
		return;
	}

	if (sRenderingLightIndex >= sLightCount)
	{
		return;
	}

	if (sMappedObjectBuffer == nullptr || sObjectBuffer == nullptr)
	{
		return;
	}

	//============================================================
	// 全Spot Shadow Passを含めた最大Draw数
	//============================================================
	if (sObjectBufferIndex >= MaxShadowDrawCalls)
	{
		OutputDebugStringW(L"[SpotLightShadowManager] Object buffer overflow.\n");
		return;
	}

	const UINT64 byteOffset =
		static_cast<UINT64>(sObjectBufferIndex) *
		static_cast<UINT64>(sObjectBufferStride);

	ObjectBuffer* objectBuffer =
		reinterpret_cast<ObjectBuffer*>(sMappedObjectBuffer + byteOffset);

	//============================================================
	// World Matrix
	//============================================================
	XMStoreFloat4x4(
		&objectBuffer->World,
		XMMatrixTranspose(world)
	);

	//============================================================
	// 現在描画中のSpot Light行列
	//============================================================
	XMStoreFloat4x4(
		&objectBuffer->LightViewProjection,
		XMMatrixTranspose(sLightViewProjection[sRenderingLightIndex])
	);

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return;
	}

	commandList->SetGraphicsRootConstantBufferView(
		0u,
		sObjectBuffer->GetGPUVirtualAddress() +
		byteOffset
	);

	++sObjectBufferIndex;
}

//============================================================
// SRV作成
//============================================================
void SpotLightShadowManager::WriteSRV(D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle)
{
	ID3D12Device* device = Direct3D::getDevice();

	if (device == nullptr || sShadowResource == nullptr)
	{
		return;
	}

	//============================================================
	// 作成されたResourceがTexture2DArrayか確認
	//============================================================
	const D3D12_RESOURCE_DESC resourceDesc = sShadowResource->GetDesc();

	assert(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
	assert(resourceDesc.DepthOrArraySize == MaxShadowLights);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0u;
	srvDesc.Texture2DArray.ArraySize = MaxShadowLights; // 4つのSliceすべてをShaderから参照可能にする
	srvDesc.Texture2DArray.PlaneSlice = 0;
	srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(
		sShadowResource,
		&srvDesc,
		destinationHandle
	);
}

bool SpotLightShadowManager::IsActive()
{
	return sInitialized && sLightCount > 0u;
}

bool SpotLightShadowManager::IsRendering()
{
	return sRendering;
}

UINT SpotLightShadowManager::GetLightCount()
{
	//========================================================
	// 初期化されていない場合は0灯
	//========================================================
	if (!sInitialized)
	{
		return 0u;
	}

	return sLightCount;
}

const XMMATRIX& SpotLightShadowManager::GetLightViewProjection(UINT shadowIndex)
{
	if (!sInitialized ||shadowIndex >=sLightCount)
	{
		return sIdentityMatrix;
	}
	return sLightViewProjection[shadowIndex];
}

UINT SpotLightShadowManager::GetRenderingLightIndex()
{
	return sRenderingLightIndex;
}


XMFLOAT4 SpotLightShadowManager::GetShadowParameters(UINT shadowIndex)
{
	//============================================================
	// Manager未初期化または未登録Index
	//============================================================
	if (!sInitialized || shadowIndex >= sLightCount)
	{
		return XMFLOAT4(
			0.0f,      // Disable
			1.0f,      // PCF Radius
			0.0003f,   // Minimum Bias
			0.0020f    // Slope Bias
		);
	}

	return sShadowParameters[shadowIndex];
}

void SpotLightShadowManager::SetShadowLightBudget(UINT count)
{
	sShadowLightBudget = (std::clamp)(count, 1u, MaxShadowLights);
}

bool SpotLightShadowManager::IsSphereInsideShadowFrustum(UINT shadowIndex, FXMVECTOR worldCenter, float worldRadius)
{
	//============================================================
	// Manager未初期化
	// ※安全側としてtrueを返し、描画を省略しない
	//============================================================
	if (!sInitialized)
	{
		return true;
	}

	//============================================================
	// 未登録または範囲外のShadow Index
	// 判定できない場合は描画する
	//============================================================
	if (shadowIndex >=
		sLightCount ||
		shadowIndex >=
		MaxShadowLights)
	{
		return true;
	}

	//============================================================
	// 不正な半径
	// Boundsが壊れていてもObjectを消さないようにする
	//============================================================
	if (!std::isfinite(worldRadius) || worldRadius <= 0.0f)
	{
		return true;
	}

	//============================================================
	// DirectXCollision用Bounding Sphereを作成
	//============================================================
	BoundingSphere boundingSphere = {};

	XMStoreFloat3(&boundingSphere.Center,worldCenter);

	boundingSphere.Radius = worldRadius;

	//============================================================
	// Frustumに対する包含状態
	// DISJOINT: 完全に視錐台外
	// INTERSECTS: 視錐台境界と交差
	// CONTAINS: 完全に視錐台内
	//============================================================
	const ContainmentType containment =
		sWorldFrustums[shadowIndex].Contains(boundingSphere);

	//============================================================
	// 完全に外側の場合だけ描画を省略する
	// 境界と交差するObjectはShadow Mapへ描画する
	//============================================================
	return containment != DirectX::DISJOINT;
}

void SpotLightShadowManager::SetExcludedCaster(UINT shadowIndex, const ObjectBase* object)
{
	//========================================================
	// Manager未初期化・不正なShadow Index
	//========================================================
	if (!sInitialized || shadowIndex >= sLightCount || shadowIndex >= MaxShadowLights)
	{
		return;
	}

	//========================================================
	// Shadow Map描画開始後の変更は許可しない
	//========================================================
	if (sRendering)
	{
		assert(false);
		return;
	}

	//========================================================
	// このSliceの除外対象を記録
	//========================================================
	sExcludedCasters[shadowIndex] = object;
}

bool SpotLightShadowManager::ShouldSkipCaster(UINT shadowIndex, const ObjectBase* object)
{
	if (!sInitialized || object == nullptr || shadowIndex >= sLightCount || shadowIndex >= MaxShadowLights)
	{
		return false;
	}

	//========================================================
	// 登録された除外対象と一致するか
	//========================================================
	return sExcludedCasters[shadowIndex] == object;
}
