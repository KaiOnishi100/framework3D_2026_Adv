#include "../../framework.h"
#include "../vn_environment.h"

#include "shadowMap.h"

//============================================================
// 初期化
//============================================================
bool ShadowMap::Initialize()
{
	CreateResource();
	CreateSRV();
	CreateDSV();

	CreateRootSignature();
	CreatePipelineState();
	CreateObjectConstantBuffer();

	//============================================================
	// GPU時間計測用Timestamp Queryを作成
	//============================================================
	CreateTimestampQueryResources();


	//============================================================
	// Cascade関連情報を初期化
	//============================================================
	for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
	{
		mLightView[cascadeIndex] = XMMatrixIdentity();
		mLightProj[cascadeIndex] = XMMatrixIdentity();
		mLightViewProj[cascadeIndex] = XMMatrixIdentity();

		//========================================================
		// Static ShadowMapキャッシュ初期化
		// ※初回はまだStatic ShadowMapが作られていないため
		// ※Dirty=true、行列未登録=falseにする
		//========================================================
		XMStoreFloat4x4(&mLastStaticLightViewProj[cascadeIndex], XMMatrixIdentity());
		mHasLastStaticLightViewProj[cascadeIndex] = false;
		mStaticCascadeDirty[cascadeIndex] = true;

		//========================================================
		// 初期状態では全Cascadeを毎フレーム更新
		//========================================================
		mCascadeUpdateIntervals[cascadeIndex] = 1;
		mCascadeShouldUpdate[cascadeIndex] = true;
		for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
		{
			mLayerCascadeUpdatedThisFrame[layerIndex][cascadeIndex] = false;
			mLayerCascadeRenderedObjectCounts[layerIndex][cascadeIndex] = 0;
			mLayerCascadeDrawCallCounts[layerIndex][cascadeIndex] = 0;
			mLayerCascadeTimestampIssued[layerIndex][cascadeIndex] = false;
			mLayerCascadeGpuTimeMs[layerIndex][cascadeIndex] = 0.0;
		}
	}

	//============================================================
	// 遠距離Cascadeだけ2フレームに1回更新
	//============================================================
	mCascadeUpdateIntervals[2] = 2;

	//============================================================
	// 最初のフレームは全Cascadeを必ず作成する
	//============================================================
	mForceUpdateAllCascades = true;

	mShadowFrameIndex = 0;

	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = static_cast<FLOAT>(mShadowMapSize);
	mViewport.Height = static_cast<FLOAT>(mShadowMapSize);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	mScissor.left = 0;
	mScissor.top = 0;
	mScissor.right = static_cast<LONG>(mShadowMapSize);
	mScissor.bottom = static_cast<LONG>(mShadowMapSize);

	mMaxShadowDistance = 150.0f;

	UpdateLightMatrix(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0));

	return true;
}

//============================================================
// 終了処理
//============================================================
void ShadowMap::Terminate()
{
	//============================================================
	// Timestamp Readback BufferのMap解除
	//============================================================
	if (mTimestampReadbackBuffer != nullptr)
	{
		if (mMappedTimestampData != nullptr)
		{
			mTimestampReadbackBuffer->Unmap(
				0,
				nullptr
			);

			mMappedTimestampData = nullptr;
		}
	}

	//============================================================
	// Timestamp関連Resource解放
	//============================================================
	SAFE_RELEASE(mTimestampReadbackBuffer);
	SAFE_RELEASE(mTimestampQueryHeap);

	mTimestampFrequency = 0;
	mHasPendingTimestampResult = false;

	if (mObjectCB)
	{
		mObjectCB->Unmap(0, nullptr);
		mMappedObjectCB = nullptr;
	}
	SAFE_RELEASE(mObjectCB);

	SAFE_RELEASE(mPipelineStateStatic);
	SAFE_RELEASE(mPipelineStateSkin);
	SAFE_RELEASE(mRootSignature);

	SAFE_RELEASE(mDSVHeap);
	SAFE_RELEASE(mSRVHeap);
	SAFE_RELEASE(mSRVCopyHeap);
	for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
	{
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			SAFE_RELEASE(mShadowResources[layerIndex][cascadeIndex]);

			mDSV[layerIndex][cascadeIndex] = {};
			mSRVCPU[layerIndex][cascadeIndex] = {};
			mSRVGPU[layerIndex][cascadeIndex] = {};
			mSRVCopyCPU[layerIndex][cascadeIndex] = {};
			mResourceStates[layerIndex][cascadeIndex] = D3D12_RESOURCE_STATE_COMMON;
		}
	}
}

void ShadowMap::UpdateTimestampResults()
{
	//============================================================
	// まだ1フレーム分のTimestampが記録されていない
	//============================================================
	if (!mHasPendingTimestampResult)
	{
		return;
	}

	//============================================================
	// 必要Resource確認
	//============================================================
	if (mMappedTimestampData == nullptr ||mTimestampFrequency == 0)
	{
		return;
	}

	//============================================================
	// 前回結果をリセット
	//============================================================
	mTotalGpuTimeMs = 0.0;

	for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
	{
		const ShadowMapLayer layer = static_cast<ShadowMapLayer>(layerIndex);

		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			//========================================================
			// 前フレームで更新されなかったCascade
			// ※Readback Bufferには以前の値が残っている可能性があるため
			// ※GPU時間として使用しない
			//========================================================
			if (!mLayerCascadeTimestampIssued[layerIndex][cascadeIndex])
			{
				mLayerCascadeGpuTimeMs[layerIndex][cascadeIndex] = 0.0;
				continue;
			}

			const UINT startIndex = GetTimestampStartIndex(
				layer,
				cascadeIndex
			);

			const UINT endIndex = startIndex + 1;

			//========================================================
			// GPU Timestamp値
			//========================================================
			const UINT64 startTimestamp = mMappedTimestampData[startIndex];
			const UINT64 endTimestamp = mMappedTimestampData[endIndex];

			//========================================================
			// 不正値対策
			//========================================================
			if (endTimestamp < startTimestamp)
			{
				mLayerCascadeGpuTimeMs[layerIndex][cascadeIndex] = 0.0;
				continue;
			}

			//========================================================
			// GPU Clockの差
			//========================================================
			const UINT64 timestampDelta = endTimestamp - startTimestamp;

			//========================================================
			// 秒へ変換
			//
			// delta / frequency
			//========================================================
			const double elapsedSeconds =
				static_cast<double>(timestampDelta) / static_cast<double>(mTimestampFrequency);

			//========================================================
			// 秒からミリ秒へ変換
			//========================================================
			const double elapsedMilliseconds = elapsedSeconds * 1000.0;

			mLayerCascadeGpuTimeMs[layerIndex][cascadeIndex] = elapsedMilliseconds;

			mTotalGpuTimeMs += elapsedMilliseconds;
		}

	}


	//============================================================
	// 今回の結果は読み取り済み
	// ※現在フレームのEndRender(Cascade 2)で再度trueになる
	//============================================================
	mHasPendingTimestampResult = false;
}

bool ShadowMap::IntersectsCascadeSphere(UINT cascadeIndex, FXMVECTOR worldCenter, float worldRadius) const
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	//============================================================
	// 半径が無効な場合はカリングしない
	//============================================================
	if (!std::isfinite(worldRadius) || worldRadius <= 0.0f)
	{
		return true;
	}

	//============================================================
	// アニメーションやBounds誤差への余裕
	// 完全に範囲外になったときだけカリングするため
	// 半径を少し広げて保守的に判定する
	//============================================================
	const float radiusPadding = 1.15f;

	const float paddedRadius = worldRadius * radiusPadding;

	//============================================================
	// Bounding Sphere中心をライトView空間へ変換
	// ※LightViewは回転と移動のみなので
	// ※Sphereの形状と半径は維持される
	//============================================================
	const XMVECTOR centerLightSpace =
		XMVector3TransformCoord(
			worldCenter,
			mLightView[cascadeIndex]
		);

	const float centerX =
		XMVectorGetX(centerLightSpace);

	const float centerY =
		XMVectorGetY(centerLightSpace);

	const float centerZ =
		XMVectorGetZ(centerLightSpace);

	const CascadeCullVolume& volume = mCascadeCullVolumes[cascadeIndex];

	//============================================================
	// X軸で完全に範囲外
	//============================================================
	if (centerX + paddedRadius < volume.minPosition.x)
	{
		return false;
	}

	if (centerX - paddedRadius > volume.maxPosition.x)
	{
		return false;
	}

	//============================================================
	// Y軸で完全に範囲外
	//============================================================
	if (centerY + paddedRadius < volume.minPosition.y)
	{
		return false;
	}

	if (centerY - paddedRadius > volume.maxPosition.y)
	{
		return false;
	}

	//============================================================
	// Z軸で完全に範囲外
	// casterDepthPaddingを含むため
	// カメラ視錐台外から影を落とす物体も残る
	//============================================================
	if (centerZ + paddedRadius < volume.minPosition.z)
	{
		return false;
	}

	if (centerZ - paddedRadius > volume.maxPosition.z)
	{
		return false;
	}

	//============================================================
	// 3軸すべてで完全な範囲外ではないため描画する
	//============================================================
	return true;
}

//============================================================
// ShadowMap用Resource作成
//============================================================
void ShadowMap::CreateResource()
{
	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();
	assert(device != nullptr);

	//============================================================
	// ShadowMap本体はTypelessで作る
	// DSVでは D32_FLOAT として使う
	// SRVでは R32_FLOAT として読む
	//============================================================
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Alignment = 0;
	resDesc.Width = mShadowMapSize;
	resDesc.Height = mShadowMapSize;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProp.CreationNodeMask = 1;
	heapProp.VisibleNodeMask = 1;

	//============================================================
	// Static / Dynamicレイヤーごとに作成
	//============================================================
	for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
	{
		//========================================================
		// デバッグ名に使用するレイヤー名
		//========================================================
		const wchar_t* layerName = (layerIndex == ToShadowLayerIndex(ShadowMapLayer::Static)) ? L"Static" : L"Dynamic";

		//============================================================
		// Cascade数分のResourceを作成
		//============================================================
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			assert(mShadowResources[layerIndex][cascadeIndex] == nullptr);

			hr = device->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&resDesc,
				//================================================
				// 通常描画から読み取れる状態で作成する
				// ShadowPass開始時にDEPTH_WRITEへ遷移させる
				//================================================
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&clearValue,
				IID_PPV_ARGS(&mShadowResources[layerIndex][cascadeIndex])
			);

			assert(SUCCEEDED(hr));

			//========================================================
			// 初期Resource Stateを記録
			//========================================================
			mResourceStates[layerIndex][cascadeIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			//========================================================
			// Graphics Debuggerで識別できる名前を付ける
			//========================================================
			WCHAR resourceName[128] = {};

			swprintf_s(resourceName, _countof(resourceName),
				L"ShadowMap %ls Cascade %u Resource",
				layerName,
				cascadeIndex
			);

			mShadowResources[layerIndex][cascadeIndex]->SetName(resourceName);
		}

	}

}

//============================================================
// DSV作成
//============================================================
void ShadowMap::CreateDSV()
{
	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();
	assert(device != nullptr);

	const UINT totalDescriptorCount = SHADOW_LAYER_COUNT * SHADOW_CASCADE_COUNT;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	heapDesc.NumDescriptors = totalDescriptorCount;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.NodeMask = 0;

	hr = device->CreateDescriptorHeap(
		&heapDesc,
		IID_PPV_ARGS(&mDSVHeap)
	);

	if (FAILED(hr))
	{
		wchar_t errorMessage[256] = {};

		swprintf_s(errorMessage, _countof(errorMessage),
			L"[ShadowMap DSV Heap Create Failed]\n"
			L"Descriptor Count : %u\n"
			L"HRESULT          : 0x%08X\n",
			totalDescriptorCount,
			static_cast<unsigned int>(hr)
		);
		OutputDebugStringW(errorMessage);
		assert(false);
		return;
	}

	mDSVHeap->SetName(L"ShadowMap Static Dynamic DSVHeap");

	//============================================================
	// DSV Descriptor 1個分のサイズ
	//============================================================
	const UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	D3D12_CPU_DESCRIPTOR_HANDLE heapStart = mDSVHeap->GetCPUDescriptorHandleForHeapStart();

	//============================================================
	// DSV共通設定
	//============================================================
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;

	//============================================================
	// Static / Dynamicレイヤーを順番に処理
	//============================================================
	for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
	{
		//========================================================
		// デバッグ出力用レイヤー名
		//========================================================
		const wchar_t* layerName = (layerIndex == ToShadowLayerIndex(ShadowMapLayer::Static)) ? L"Static" : L"Dynamic";

		//============================================================
		// CascadeごとのDSVを作成
		//============================================================
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			const UINT descriptorIndex = layerIndex * SHADOW_CASCADE_COUNT + cascadeIndex;

			//====================================================
			// Heap先頭Handleから指定Descriptor位置へ進める
			//====================================================
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = heapStart;
			dsvHandle.ptr += static_cast<SIZE_T>(descriptorIndex) * static_cast<SIZE_T>(descriptorSize);

			//====================================================
			// 後でBeginRender()から使用できるように保存
			//====================================================
			mDSV[layerIndex][cascadeIndex] = dsvHandle;

			//====================================================
			// 対応するDepth Resourceが作成済みか確認
			//====================================================
			ID3D12Resource* shadowResource = mShadowResources[layerIndex][cascadeIndex];

			if (shadowResource == nullptr)
			{
				wchar_t errorMessage[256] = {};

				swprintf_s(errorMessage, _countof(errorMessage),
					L"[ShadowMap DSV Create Failed]\n"
					L"Resource is nullptr\n"
					L"Layer   : %ls\n"
					L"Cascade : %u\n",
					layerName,
					cascadeIndex
				);
				OutputDebugStringW(errorMessage);
				assert(false);
				continue;
			}

			//====================================================
			// Static/DynamicそれぞれのDepth Resourceへ
			// DSVを作成する
			//====================================================
			device->CreateDepthStencilView(
				shadowResource,
				&dsvDesc,
				mDSV[layerIndex][cascadeIndex]
			);
		}

	}
}

//============================================================
// SRV作成
//============================================================
void ShadowMap::CreateSRV()
{
	HRESULT hr = S_OK;

	ID3D12Device* device =Direct3D::getDevice();

	assert(device != nullptr);

	const UINT totalDescriptorCount = SHADOW_LAYER_COUNT * SHADOW_CASCADE_COUNT;

	//============================================================
	// shader visible SRV heap
	// GPUから直接参照するためのHeap
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type =D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = totalDescriptorCount;
		heapDesc.Flags =D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;

		hr = device->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(&mSRVHeap)
		);

		if (FAILED(hr))
		{
			wchar_t errorMessage[256] = {};
			swprintf_s(errorMessage, _countof(errorMessage),
				L"[ShadowMap Shader Visible SRV Heap Create Failed]\n"
				L"Descriptor Count : %u\n"
				L"HRESULT          : 0x%08X\n",
				totalDescriptorCount,
				static_cast<unsigned int>(hr)
			);
			OutputDebugStringW(errorMessage);
			assert(false);
			return;
		}

		mSRVHeap->SetName(L"ShadowMap Static Dynamic SRVHeap ShaderVisible");
	}

	//============================================================
	// CPU only SRV heap
	// CopyDescriptorsSimple のコピー元として使うHeap
	// Flags は NONE にする
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type =D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = totalDescriptorCount;
		heapDesc.Flags =D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 0;

		hr = device->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(&mSRVCopyHeap)
		);

		if (FAILED(hr))
		{
			wchar_t errorMessage[256] = {};

			swprintf_s(errorMessage, _countof(errorMessage),
				L"[ShadowMap CPU Copy SRV Heap Create Failed]\n"
				L"Descriptor Count : %u\n"
				L"HRESULT          : 0x%08X\n",
				totalDescriptorCount,
				static_cast<unsigned int>(hr)
			);
			OutputDebugStringW(errorMessage);
			assert(false);
			return;
		}

		mSRVCopyHeap->SetName(L"ShadowMap Static Dynamic SRVHeap CopySource");
	}

	//============================================================
	// ShadowMap Resource は R32_TYPELESS
	// SRVでは R32_FLOAT として読む
	//============================================================
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension =D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping =D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	//============================================================
	// CBV/SRV/UAV Descriptorのサイズ
	//============================================================
	const UINT descriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//============================================================
	// 各Heapの先頭Handle
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE visibleCPUStart =
		mSRVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_GPU_DESCRIPTOR_HANDLE visibleGPUStart =
		mSRVHeap->GetGPUDescriptorHandleForHeapStart();

	D3D12_CPU_DESCRIPTOR_HANDLE copyCPUStart =
		mSRVCopyHeap->GetCPUDescriptorHandleForHeapStart();

	//============================================================
	// Static / DynamicレイヤーごとにSRVを作成
	//============================================================
	for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
	{
		//========================================================
		// デバッグメッセージ用のレイヤー名
		//========================================================
		const wchar_t* layerName = (layerIndex == ToShadowLayerIndex(ShadowMapLayer::Static)) ?
			L"Static" : L"Dynamic";
		//============================================================
		// CascadeごとのSRVを作成
		//============================================================
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			//====================================================
			// 2次元配列をDescriptor Heap内の
			// 1次元Descriptor番号へ変換する
			//====================================================
			const UINT descriptorIndex =
				layerIndex * SHADOW_CASCADE_COUNT + cascadeIndex;

			const SIZE_T descriptorOffset =
				static_cast<SIZE_T>(descriptorIndex) *
				static_cast<SIZE_T>(descriptorSize);

			//====================================================
			// Shader Visible Heap側CPU Handle
			// CreateShaderResourceView()は
			// CPU Handleを使用してDescriptorを書き込む
			//====================================================
			D3D12_CPU_DESCRIPTOR_HANDLE visibleCPUHandle = visibleCPUStart;
			visibleCPUHandle.ptr += descriptorOffset;
			mSRVCPU[layerIndex][cascadeIndex] = visibleCPUHandle;

			//====================================================
			// Shader Visible Heap側GPU Handle
			// SetGraphicsRootDescriptorTable()などで使用する
			//====================================================
			D3D12_GPU_DESCRIPTOR_HANDLE visibleGPUHandle = visibleGPUStart;
			visibleGPUHandle.ptr += descriptorOffset;
			mSRVGPU[layerIndex][cascadeIndex] = visibleGPUHandle;

			//====================================================
			// CPU Only Heap側Handle
			// 通常描画用マテリアルHeapへ
			// Descriptorをコピーするときに使用する
			//====================================================
			D3D12_CPU_DESCRIPTOR_HANDLE copyCPUHandle = copyCPUStart;
			copyCPUHandle.ptr += descriptorOffset;
			mSRVCopyCPU[layerIndex][cascadeIndex] = copyCPUHandle;

			//====================================================
			// 対応するShadowMap Resourceを取得
			//====================================================
			ID3D12Resource* shadowResource = mShadowResources[layerIndex][cascadeIndex];
			if (shadowResource == nullptr)
			{
				wchar_t errorMessage[256] = {};
				swprintf_s(errorMessage, _countof(errorMessage),
					L"[ShadowMap SRV Create Failed]\n"
					L"Resource is nullptr\n"
					L"Layer   : %ls\n"
					L"Cascade : %u\n",
					layerName,
					cascadeIndex
				);
				OutputDebugStringW(errorMessage);
				assert(false);
				continue;
			}
			//========================================================
			// Shader Visible Heap側へSRV作成
			//========================================================
			device->CreateShaderResourceView(
				shadowResource,
				&srvDesc,
				mSRVCPU[layerIndex][cascadeIndex]
			);

			//========================================================
			// Descriptorコピー元Heap側にも同じSRVを作成
			//========================================================
			device->CreateShaderResourceView(
				shadowResource,
				&srvDesc,
				mSRVCopyCPU[layerIndex][cascadeIndex]
			);
		}

	}
}

void ShadowMap::CreateRootSignature()
{
	HRESULT hr = S_OK;

	ID3D12Device* device =
		Direct3D::getDevice();

	assert(device != nullptr);

	//============================================================
	// RootParameter
	// [0] b0 : ShadowObjectCB
	//           world
	//           lightViewProj
	// [1] b3 : BoneBuffer
	//           スキニングモデルのボーン行列
	//============================================================
	D3D12_ROOT_PARAMETER rootParam[2] = {};

	//============================================================
	// RootParameter[0]
	// b0 : ShadowObjectCB
	//============================================================
	rootParam[0].ParameterType =D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParam[0].Descriptor.ShaderRegister =0; // b0
	rootParam[0].Descriptor.RegisterSpace =0;
	rootParam[0].ShaderVisibility =D3D12_SHADER_VISIBILITY_VERTEX;

	//============================================================
	// RootParameter[1]
	// b3 : BoneBuffer
	// 通常描画のVS_3D_Skinと同じShaderRegisterを使う
	//============================================================
	rootParam[1].ParameterType =D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParam[1].Descriptor.ShaderRegister =3; // b3
	rootParam[1].Descriptor.RegisterSpace =0;
	rootParam[1].ShaderVisibility =D3D12_SHADER_VISIBILITY_VERTEX;

	//============================================================
	// RootSignature全体
	//============================================================
	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};

	rsDesc.NumParameters =_countof(rootParam);
	rsDesc.pParameters =rootParam;
	rsDesc.NumStaticSamplers =0;
	rsDesc.pStaticSamplers =nullptr;
	rsDesc.Flags =D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* signatureBlob =nullptr;
	ID3DBlob* errorBlob =nullptr;

	hr = D3D12SerializeRootSignature(
		&rsDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);

	if (FAILED(hr))
	{
		if (errorBlob != nullptr)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}

		assert(false);
	}

	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)
	);

	assert(SUCCEEDED(hr));

	mRootSignature->SetName(L"ShadowMap RootSignature");

	SAFE_RELEASE(signatureBlob);
	SAFE_RELEASE(errorBlob);
}

void ShadowMap::CreatePipelineState()
{
	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();

	assert(device != nullptr);

	//============================================================
	// 通常モデル用InputLayout
	// ShadowMap生成ではPOSITIONだけを使用する
	//============================================================
	D3D12_INPUT_ELEMENT_DESC staticLayout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
	};

	//============================================================
	// スキニングモデル用InputLayout
	//
	// 頂点バッファ内の実際の並びと完全に合わせる。
	//
	// POSITION     offset  0
	// NORMAL       offset 12
	// COLOR        offset 24
	// TEXCOORD     offset 40
	// SKIN_INDEX   offset 48
	// SKIN_WEIGHT  offset 56
	//============================================================
	D3D12_INPUT_ELEMENT_DESC skinLayout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"NORMAL",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"TANGENT",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"SKIN_INDEX",
			0,
			DXGI_FORMAT_R16G16B16A16_UINT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"SKIN_WEIGHT",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
	};

	//============================================================
	// 両方のPSOで共通する設定
	//============================================================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.pRootSignature = mRootSignature;

	//============================================================
	// ShadowMapではPixelShaderを使わない
	//============================================================
	psoDesc.PS.pShaderBytecode = nullptr;
	psoDesc.PS.BytecodeLength = 0;

	//============================================================
	// Rasterizer
	//============================================================
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.RasterizerState.FrontCounterClockwise = false;
	psoDesc.RasterizerState.DepthBias = 1000;
	psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	psoDesc.RasterizerState.SlopeScaledDepthBias = 1.5f;
	psoDesc.RasterizerState.DepthClipEnable = true;
	psoDesc.RasterizerState.MultisampleEnable = false;
	psoDesc.RasterizerState.AntialiasedLineEnable = false;
	psoDesc.RasterizerState.ForcedSampleCount = 0;
	psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//============================================================
	// Blend
	// ※ShadowMapはRTVを持たずDepthだけ書く
	//============================================================
	psoDesc.BlendState.AlphaToCoverageEnable = false;
	psoDesc.BlendState.IndependentBlendEnable = false;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = false;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;

	//============================================================
	// Depth
	//============================================================
	psoDesc.DepthStencilState.DepthEnable = true;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState.StencilEnable = false;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//============================================================
	// ShadowMapではRTVを使わない
	//============================================================
	psoDesc.NumRenderTargets = 0;

	for (UINT i = 0; i < 8; ++i)
	{
		psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
	}

	//============================================================
	// その他の共通設定
	//============================================================
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	//============================================================
	// 通常モデル用PSO
	//============================================================
	psoDesc.InputLayout =
	{
		staticLayout,
		_countof(staticLayout)
	};

	psoDesc.VS.pShaderBytecode =
		Shader::getVShader(Shader::eVertexShader::VS_Shadow)->getCode();

	psoDesc.VS.BytecodeLength =
		Shader::getVShader(Shader::eVertexShader::VS_Shadow)->getLength();

	hr = device->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(&mPipelineStateStatic)
	);

	assert(SUCCEEDED(hr));

	mPipelineStateStatic->SetName(L"ShadowMap Static PipelineState");

	//============================================================
	// スキニングモデル用PSO
	// ※共通設定を維持したままInputLayoutとVSだけを変更する
	//============================================================
	psoDesc.InputLayout =
	{
		skinLayout,
		_countof(skinLayout)
	};

	psoDesc.VS.pShaderBytecode =
		Shader::getVShader(Shader::eVertexShader::VS_ShadowSkin)->getCode();

	psoDesc.VS.BytecodeLength =
		Shader::getVShader(Shader::eVertexShader::VS_ShadowSkin)->getLength();

	hr = device->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(&mPipelineStateSkin)
	);

	assert(SUCCEEDED(hr));

	mPipelineStateSkin->SetName(L"ShadowMap Skin PipelineState");
}

void ShadowMap::CreateObjectConstantBuffer()
{
	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();

	assert(device != nullptr);

	//============================================================
	// 1オブジェクト分を256バイトに揃える
	//============================================================
	mObjectCBStride =
		(sizeof(ShadowObjectCB) + 255) & ~255u;

	//============================================================
	// 全オブジェクト分の領域を確保する
	//============================================================
	const UINT64 totalBufferSize =
		static_cast<UINT64>(mObjectCBStride) *
		static_cast<UINT64>(MaxShadowObjects) *
		static_cast<UINT64>(SHADOW_CASCADE_COUNT) *
		static_cast<UINT64>(SHADOW_LAYER_COUNT);

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProp.CreationNodeMask = 1;
	heapProp.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = totalBufferSize;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	hr = device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mObjectCB)
	);

	assert(SUCCEEDED(hr));

	mObjectCB->SetName(L"ShadowMap Object ConstantBuffer");

	//============================================================
	// UploadHeapなのでMapしたままでよい
	//============================================================
	hr = mObjectCB->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&mMappedObjectCB)
	);

	assert(SUCCEEDED(hr));
}

//============================================================
// ライト行列更新
//============================================================
void ShadowMap::UpdateLightMatrix(FXMVECTOR target)
{
	//============================================================
	// ※現在のCSMはカメラ視錐台を基準にCascade範囲を作るため
	// ※targetは現段階では使用しない
	// ※引数自体は既存コードとの互換性のため残している
	//============================================================
	(void)target;

	//============================================================
	// カメラのProjection情報を取得
	//============================================================
	const float cameraFovY = vnCamera::getFovY();

	const float cameraAspect = vnCamera::getAspect();

	const float cameraNear = vnCamera::getNearZ();

	//============================================================
	// Shadowを表示する最大距離
	// カメラFarよりShadow最大距離の方が短い場合は
	// Shadow最大距離までをCascade分割対象にする
	//============================================================
	const float cameraFar = (std::min)(vnCamera::getFarZ(), mMaxShadowDistance);

	//============================================================
	// Cascade終了距離とブレンド開始距離を計算
	// ※通常、Near/Farが固定なら毎フレーム同じ値になる
	//============================================================
	UpdateCascadeSplitDepths(
		cameraNear,
		cameraFar
	);

	//============================================================
	// 各Cascadeの視錐台8頂点を作成
	//============================================================
	float splitNear = cameraNear;

	for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
	{
		//========================================================
		// 現在Cascadeの終了距離
		//========================================================
		const float splitFar = mCascadeSplitDepths[cascadeIndex];

		//========================================================
		// 現在フレームで更新しないCascade
		// 以前保存した
		// ・mCascadeFrustumCorners
		// ・mLightView
		// ・mLightProj
		// ・mLightViewProj
		// ・mCascadeCullVolumes
		// を維持する
		//========================================================
		if (!ShouldUpdateCascade(cascadeIndex))
		{
			//====================================================
			// 次のCascadeの論理的な開始位置だけ進める
			//====================================================
			splitNear = splitFar;

			continue;
		}

		//========================================================
		// ShadowMapを構築するときに使うNear距離
		// Cascade 1・2は、境界ブレンドのために
		// 前Cascadeのブレンド開始地点まで手前へ広げる
		//========================================================
		float cascadeBuildNear = splitNear;

		if (cascadeIndex > 0)
		{
			cascadeBuildNear = mCascadeBlendStartDepths[cascadeIndex - 1];
		}

		//========================================================
		// カメラNearより手前へ出ないように保護
		//========================================================
		cascadeBuildNear = (std::max)(cascadeBuildNear, cameraNear);

		//========================================================
		// 現在Cascadeのワールド空間視錐台8頂点
		//========================================================
		XMVECTOR frustumCorners[8] = {};

		BuildFrustumCornersWorld(
			cascadeBuildNear,
			splitFar,
			cameraFovY,
			cameraAspect,
			frustumCorners
		);

		//========================================================
		// 更新対象Cascadeの視錐台だけ保存
		// ※スキップしたCascadeの視錐台は上書きしない
		//========================================================
		for (UINT cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
		{
			XMStoreFloat3(&mCascadeFrustumCorners[cascadeIndex][cornerIndex], frustumCorners[cornerIndex]);
		}

		//========================================================
		// 次Cascadeの通常Near位置へ進める
		//========================================================
		splitNear = splitFar;
	}

	//============================================================
	// 更新対象Cascadeだけライト行列を作り直す
	// ※Cascade 2がCachedの場合は BuildCascadeLightMatrix(2)を呼ばない
	// ※ShadowMap TextureLightViewProjectionの両方が前回状態のまま維持される
	//============================================================
	for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
	{
		if (!ShouldUpdateCascade(cascadeIndex))
		{
			continue;
		}

		BuildCascadeLightMatrix(cascadeIndex);
	}
}

void ShadowMap::SetObjectMatrix(const XMMATRIX& world)
{
	if (mObjectCB == nullptr ||
		mMappedObjectCB == nullptr)
	{
		return;
	}

	if (mObjectCBIndex >= MaxShadowObjects)
	{
		// 確保数を超えた場合は描画しない
		assert(false);
		return;
	}

	ID3D12GraphicsCommandList* cmdList =
		Direct3D::getCommandList();

	assert(cmdList != nullptr);

	//============================================================
	// 一旦ローカル構造体へ行列を格納する
	//============================================================
	ShadowObjectCB data = {};

	XMStoreFloat4x4(
		&data.world,
		XMMatrixTranspose(world)
	);

	XMStoreFloat4x4(
		&data.lightViewProj,
		XMMatrixTranspose(mLightViewProj[mCurrentCascadeIndex])
	);

	//============================================================
	// 現在描画中のLayer番号
	//============================================================
	const UINT layerIndex = ToShadowLayerIndex(mCurrentLayer);
	assert(layerIndex < SHADOW_LAYER_COUNT);

	//============================================================
	// Layer・Cascadeの組み合わせ番号
	//============================================================
	const UINT layerCascadeIndex =
		layerIndex *
		SHADOW_CASCADE_COUNT +
		mCurrentCascadeIndex;

	//============================================================
	// Layer * Cascadeを含めた絶対Slot番号
	//============================================================
	const UINT absoluteSlotIndex =
		layerCascadeIndex *
		MaxShadowObjects +
		mObjectCBIndex;

	//============================================================
	// このオブジェクト専用の書き込み位置を計算
	//============================================================
	BYTE* destination =
		mMappedObjectCB +
		static_cast<size_t>(absoluteSlotIndex) *
		mObjectCBStride;

	//============================================================
	// 専用領域へコピー
	//============================================================
	memcpy(
		destination,
		&data,
		sizeof(ShadowObjectCB)
	);

	//============================================================
	// このオブジェクト専用のGPUアドレス
	//============================================================
	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress =
		mObjectCB->GetGPUVirtualAddress() +
		static_cast<UINT64>(absoluteSlotIndex) *
		static_cast<UINT64> (mObjectCBStride);

	cmdList->SetGraphicsRootConstantBufferView(
		0,
		gpuAddress
	);

	// 次のオブジェクト用領域へ進める
	++mObjectCBIndex;
}

void ShadowMap::PrepareCascadeUpdates()
{
	//============================================================
	// 更新判定に使用する現在のフレーム番号
	//============================================================
	const UINT64 currentFrameIndex = mShadowFrameIndex;

	for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
	{
		//========================================================
		// 更新間隔が0にならないように保護
		//========================================================
		const UINT updateInterval = (std::max)(mCascadeUpdateIntervals[cascadeIndex], 1u);

		//========================================================
		// 更新条件
		// ・強制更新が指定されている
		// ・最初のフレーム
		// ・更新間隔に該当するフレーム
		//========================================================
		const bool shouldUpdate =
			mForceUpdateAllCascades ||
			currentFrameIndex == 0 ||
			(currentFrameIndex % static_cast<UINT64>(updateInterval)) == 0;

		mCascadeShouldUpdate[cascadeIndex] = shouldUpdate;
		//========================================================
		// Layerごとの現在フレーム情報をリセット
		//========================================================
		for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
		{
			//========================================================
			// BeginRender()が呼ばれたときにtrueへ変更する
			//========================================================
			mLayerCascadeUpdatedThisFrame[layerIndex][cascadeIndex] = false;

			//========================================================
			// 現在フレームのTimestamp発行状態を初期化
			//========================================================
			mLayerCascadeTimestampIssued[layerIndex][cascadeIndex] = false;
		}
	}

	//============================================================
	// 強制更新要求を消費
	//============================================================
	mForceUpdateAllCascades = false;

	//============================================================
	// 次のフレーム番号へ進める
	//============================================================
	++mShadowFrameIndex;
}

bool ShadowMap::ShouldUpdateStaticCascade(UINT cascadeIndex) const
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	//============================================================
	// Cascade自体が更新対象外
	//
	// UpdateLightMatrix()でもこのCascadeの行列は更新されず
	// Static/Dynamicの両ShadowMapを前回状態で再利用する
	//============================================================
	if (!ShouldUpdateCascade(cascadeIndex))
	{
		return false;
	}

	//============================================================
	// Static Casterの追加・削除・移動などがあった
	//============================================================
	if (mStaticCascadeDirty[cascadeIndex])
	{
		return true;
	}

	//============================================================
	// Static ShadowMapをまだ一度も生成していない
	//============================================================
	if (!mHasLastStaticLightViewProj[cascadeIndex])
	{
		return true;
	}

	//============================================================
	// LightVPが変化した
	//
	// Staticオブジェクト自体が動いていなくても、
	// ShadowMapを生成した行列が変われば再描画が必要
	//============================================================
	if (HasStaticLightViewProjChanged(cascadeIndex))
	{
		return true;
	}

	//============================================================
	// Static ShadowMapをそのまま再利用できる
	//============================================================
	return false;
}

void ShadowMap::CommitStaticCascadeUpdate(UINT cascadeIndex)
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	//============================================================
	// 今回Static ShadowMap生成に使用したLightVPを保存
	//============================================================
	XMStoreFloat4x4(&mLastStaticLightViewProj[cascadeIndex], mLightViewProj[cascadeIndex]);

	//============================================================
	// 有効なStatic ShadowMapが作成された
	//============================================================
	mHasLastStaticLightViewProj[cascadeIndex] = true;

	//============================================================
	// 現在のStatic Caster状態がShadowMapへ反映された
	//============================================================
	mStaticCascadeDirty[cascadeIndex] = false;
}

void ShadowMap::InvalidateStaticShadowCache()
{
	for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
	{
		mStaticCascadeDirty[cascadeIndex] = true;
	}

	//============================================================
	// Cascade 2などが更新間隔によってスキップされると
	// Staticオブジェクト変更が反映されるまで遅延する
	// ※変更直後の次フレームですべて更新するため Cascade更新も強制する
	//============================================================
	ForceUpdateAllCascades();
}

void ShadowMap::SetCascadeUpdateInterval(UINT cascadeIndex, UINT interval)
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	//============================================================
	// 0フレーム間隔は無効なので最低1にする
	//============================================================
	mCascadeUpdateIntervals[cascadeIndex] = (std::max)(interval, 1u);

	//============================================================
	// 設定変更直後は一度更新しておく
	//============================================================
	mForceUpdateAllCascades = true;
}

void ShadowMap::MarkLayerCascadeSkipped(UINT cascadeIndex, ShadowMapLayer layer)
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);
	const UINT layerIndex = ToShadowLayerIndex(layer);
	assert(layerIndex < SHADOW_LAYER_COUNT);
	//============================================================
	// このフレームでは更新していない
	//============================================================
	mLayerCascadeUpdatedThisFrame[layerIndex][cascadeIndex] = false;
	//============================================================
	// このフレームでは描画していない
	//============================================================
	mLayerCascadeRenderedObjectCounts[layerIndex][cascadeIndex] = 0;
	mLayerCascadeDrawCallCounts[layerIndex][cascadeIndex] = 0;
	mLayerCascadeGpuTimeMs[layerIndex][cascadeIndex] = 0.0;
}

void ShadowMap::MarkCascadeSkipped(UINT cascadeIndex)
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
	{
		MarkLayerCascadeSkipped(cascadeIndex, static_cast<ShadowMapLayer>(layerIndex));
	}
}

void ShadowMap::FinishShadowFrame()
{
	//============================================================
	// 現在フレームで少なくとも1つのCascadeに対して
	// Timestamp Queryを発行したか
	//============================================================
	bool hasTimestampResult = false;

	for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
	{
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			if (mLayerCascadeTimestampIssued[layerIndex][cascadeIndex])
			{
				hasTimestampResult = true;
				break;
			}
		}
		if (hasTimestampResult)
		{
			break;
		}
	}

	//============================================================
	// 次フレームのUpdateTimestampResults()で
	// Readback Bufferを読み取るかどうかを記録する
	// Cascade 2をスキップしていても
	// Cascade 0または1が更新されていればtrueになる
	//============================================================
	mHasPendingTimestampResult = hasTimestampResult;
}

//============================================================
// Resource状態変更
//============================================================
void ShadowMap::ChangeResourceState(UINT cascadeIndex, ShadowMapLayer layer, D3D12_RESOURCE_STATES afterState)
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	ID3D12GraphicsCommandList* cmdList =Direct3D::getCommandList();

	assert(cmdList != nullptr);

	const UINT layerIndex = ToShadowLayerIndex(layer);
	assert(layerIndex < SHADOW_LAYER_COUNT);

	ID3D12Resource* resource = mShadowResources[layerIndex][cascadeIndex];
	assert(resource != nullptr);

	//============================================================
	// 現在の状態を取得
	//============================================================
	const D3D12_RESOURCE_STATES beforeState = mResourceStates[layerIndex][cascadeIndex];

	//============================================================
	// 同じ状態ならBarrierは不要
	//============================================================
	if (beforeState == afterState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = beforeState;
	barrier.Transition.StateAfter = afterState;

	cmdList->ResourceBarrier(1, &barrier);

	//============================================================
	// Barrier発行後の状態を記録
	//============================================================
	mResourceStates[layerIndex][cascadeIndex] = afterState;
}

void ShadowMap::CreateTimestampQueryResources()
{
	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();

	ID3D12CommandQueue* commandQueue = Direct3D::getCommandQueue();

	assert(device != nullptr);

	assert(commandQueue != nullptr);

	//============================================================
	// GPU Timestampの周波数を取得
	//============================================================
	hr = commandQueue->GetTimestampFrequency(&mTimestampFrequency);

	assert(SUCCEEDED(hr));

	assert(mTimestampFrequency > 0);

	//============================================================
	// Timestamp Query Heap作成
	//============================================================
	D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	queryHeapDesc.Count = TimestampQueryCount;
	queryHeapDesc.NodeMask = 0;

	hr = device->CreateQueryHeap(
		&queryHeapDesc,
		IID_PPV_ARGS(&mTimestampQueryHeap)
	);

	assert(SUCCEEDED(hr));

	mTimestampQueryHeap->SetName(L"ShadowMap Timestamp QueryHeap");

	//============================================================
	// Readback Bufferのサイズ
	// Timestamp 1個につきUINT64 1個
	//============================================================
	const UINT64 readbackBufferSize =
		static_cast<UINT64>(TimestampQueryCount) * sizeof(UINT64);

	//============================================================
	// CPU読み取り用Heap
	//============================================================
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	//============================================================
	// Timestamp結果格納用Buffer
	//============================================================
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = readbackBufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		//================================================
		// Readback HeapのBufferはCOPY_DESTで作成する
		//================================================
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mTimestampReadbackBuffer));

	assert(SUCCEEDED(hr));

	mTimestampReadbackBuffer->SetName(L"ShadowMap Timestamp ReadbackBuffer");

	//============================================================
	// Readback BufferをMap
	// ※Readback HeapなのでMapしたままでよい
	//============================================================
	D3D12_RANGE readRange = {};
	readRange.Begin = 0;
	readRange.End = static_cast<SIZE_T>(readbackBufferSize);

	hr = mTimestampReadbackBuffer->Map(
		0,
		&readRange,
		reinterpret_cast<void**>(&mMappedTimestampData)
	);

	assert(SUCCEEDED(hr));

	//============================================================
	// 初期値を0にする
	//============================================================
	memset(
		mMappedTimestampData,
		0,
		static_cast<size_t>(readbackBufferSize)
	);

	for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
	{
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			mLayerCascadeGpuTimeMs[layerIndex][cascadeIndex] = 0.0;
		}
	}

	mTotalGpuTimeMs = 0.0;

	mHasPendingTimestampResult = false;
}

void ShadowMap::BuildFrustumCornersWorld(float splitNear, float splitFar, float fovY, float aspect, XMVECTOR outCorners[8]) const
{
	//============================================================
	// カメラのワールド座標
	//============================================================
	const XMVECTOR cameraPosition = *vnCamera::getPosition();

	//============================================================
	// カメラの前方向
	// Target - Positionを正規化した値
	//============================================================
	const XMVECTOR cameraForward =
		XMVector3Normalize(vnCamera::getForward());

	//============================================================
	// カメラの右方向
	//============================================================
	const XMVECTOR cameraRight =
		XMVector3Normalize(vnCamera::getRight());

	//============================================================
	// カメラの上方向
	// 元のUpをそのまま使うよりForwardとRightから直交する軸を作り直す
	// ※カメラが傾いた場合でもRight / Up / Forwardの直交関係を維持しやすい
	//============================================================
	const XMVECTOR cameraUp =
		XMVector3Normalize(XMVector3Cross(cameraForward, cameraRight));

	//============================================================
	// 垂直FOVの半分に対するtan
	// Near/Far平面の高さを計算するために使用する
	//============================================================
	const float tanHalfFov = tanf(fovY * 0.5f);

	//============================================================
	// Near平面の半分の高さと幅
	//============================================================
	const float nearHalfHeight = tanHalfFov * splitNear;

	const float nearHalfWidth = nearHalfHeight * aspect;

	//============================================================
	// Far平面の半分の高さと幅
	//============================================================
	const float farHalfHeight = tanHalfFov * splitFar;

	const float farHalfWidth = farHalfHeight * aspect;

	//============================================================
	// Near平面中心
	//============================================================
	const XMVECTOR nearCenter =
		XMVectorAdd(cameraPosition, XMVectorScale(cameraForward, splitNear));

	//============================================================
	// Far平面中心
	//============================================================
	const XMVECTOR farCenter =
		XMVectorAdd(cameraPosition, XMVectorScale(cameraForward, splitFar));

	//============================================================
	// Near平面の横・縦方向ベクトル
	//============================================================
	const XMVECTOR nearRight =
		XMVectorScale(
			cameraRight,
			nearHalfWidth
		);

	const XMVECTOR nearUp =
		XMVectorScale(
			cameraUp,
			nearHalfHeight
		);

	//============================================================
	// Far平面の横・縦方向ベクトル
	//============================================================
	const XMVECTOR farRight =
		XMVectorScale(
			cameraRight,
			farHalfWidth
		);

	const XMVECTOR farUp =
		XMVectorScale(
			cameraUp,
			farHalfHeight
		);

	//============================================================
	// Near平面4頂点
	//============================================================

	// Near Top Left
	outCorners[0] =
		XMVectorAdd(XMVectorSubtract(nearCenter, nearRight), nearUp);

	// Near Top Right
	outCorners[1] =
		XMVectorAdd(XMVectorAdd(nearCenter, nearRight), nearUp);

	// Near Bottom Left
	outCorners[2] =
		XMVectorSubtract(XMVectorSubtract(nearCenter, nearRight), nearUp);

	// Near Bottom Right
	outCorners[3] =
		XMVectorSubtract(XMVectorAdd(nearCenter, nearRight), nearUp);

	//============================================================
	// Far平面4頂点
	//============================================================

	// Far Top Left
	outCorners[4] =
		XMVectorAdd(XMVectorSubtract(farCenter, farRight), farUp);

	// Far Top Right
	outCorners[5] =
		XMVectorAdd(XMVectorAdd(farCenter, farRight), farUp);

	// Far Bottom Left
	outCorners[6] =
		XMVectorSubtract(XMVectorSubtract(farCenter, farRight), farUp);

	// Far Bottom Right
	outCorners[7] =
		XMVectorSubtract(XMVectorAdd(farCenter, farRight), farUp);
}

void ShadowMap::UpdateCascadeSplitDepths(float cameraNear, float cameraFar)
{
	//============================================================
	// Nearが極端に小さいと対数分割が不安定になるため保護
	//============================================================
	cameraNear = (std::max)(cameraNear, 0.01f);
	cameraFar = (std::max)(cameraFar, cameraNear + 0.01f);

	//============================================================
	// 均等分割と対数分割の混合率
	//============================================================
	const float lambda = 0.65f;

	for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
	{
		//========================================================
		// Cascade 0: 1 / 3
		// Cascade 1: 2 / 3
		// Cascade 2: 3 / 3
		//========================================================
		const float ratio = static_cast<float>(cascadeIndex + 1) / static_cast<float>(SHADOW_CASCADE_COUNT);

		//========================================================
		// 均等分割
		//========================================================
		const float uniformSplit = cameraNear + (cameraFar - cameraNear) * ratio;

		//========================================================
		// 対数分割
		// 近距離側が細かく分かれる
		//========================================================
		const float logarithmicSplit = cameraNear * powf(cameraFar / cameraNear, ratio);

		//========================================================
		// 均等分割と対数分割を混ぜる
		//========================================================
		mCascadeSplitDepths[cascadeIndex] = uniformSplit * (1.0f - lambda) + logarithmicSplit * lambda;
	}

	//============================================================
	// 最後のCascadeは必ずカメラFarと一致させる
	// 浮動小数点誤差による範囲漏れを防ぐ
	//============================================================
	mCascadeSplitDepths[SHADOW_CASCADE_COUNT - 1] = cameraFar;

	for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
	{
		//========================================================
		// 現在Cascadeの開始距離
		// Cascade 0: Camera Near
		// Cascade 1: Cascade 0 Far
		// Cascade 2: Cascade 1 Far
		//========================================================
		const float cascadeStart =
			(cascadeIndex == 0) ? cameraNear : mCascadeSplitDepths[cascadeIndex - 1];

		//========================================================
		// 現在Cascadeの終了距離
		//========================================================
		const float cascadeEnd = mCascadeSplitDepths[cascadeIndex];

		//========================================================
		// Cascade全体の距離
		//========================================================
		const float cascadeLength = (std::max)(cascadeEnd - cascadeStart, 0.001f);

		//========================================================
		// 終端から一定割合だけ手前をブレンド開始距離にする
		//========================================================
		const float blendWidth = cascadeLength * mCascadeBlendRatio;

		mCascadeBlendStartDepths[cascadeIndex] = cascadeEnd - blendWidth;
	}
}

void ShadowMap::BuildCascadeLightMatrix(UINT cascadeIndex)
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	//============================================================
	// Cascade視錐台の8頂点を読み込む
	//============================================================
	XMVECTOR corners[8] = {};

	for (UINT cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
	{
		corners[cornerIndex] = XMLoadFloat3(&mCascadeFrustumCorners[cascadeIndex][cornerIndex]);
	}

	//============================================================
	// 視錐台8頂点の中心を計算
	//============================================================
	XMVECTOR cascadeCenter = XMVectorZero();

	for (UINT cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
	{
		cascadeCenter = XMVectorAdd(cascadeCenter, corners[cornerIndex]);
	}

	cascadeCenter = XMVectorScale(cascadeCenter, 1.0f / 8.0f);

	//============================================================
	// 中心から最も遠い頂点までの距離を求める
	// ※この半径をXY投影範囲に使用すると
	// ※カメラの回転によって投影幅が大きく変化しにくくなる
	//============================================================
	float cascadeRadius = 0.0f;

	for (UINT cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
	{
		const XMVECTOR difference =
			XMVectorSubtract(
				corners[cornerIndex],
				cascadeCenter
			);

		const float distance =
			XMVectorGetX(XMVector3Length(difference));

		cascadeRadius = (std::max)(cascadeRadius, distance);
	}

	//============================================================
	// 範囲の端ぎりぎりで切れないように少し広げる
	//============================================================
	const float cascadePadding = 1.05f;

	cascadeRadius *= cascadePadding;

	//============================================================
	// 半径を一定単位へ切り上げる
	// カメラのわずかな回転による投影サイズ変動を抑える
	//============================================================
	const float radiusStep = 1.0f / 16.0f;

	cascadeRadius = ceilf(cascadeRadius / radiusStep) * radiusStep;
	cascadeRadius = (std::max)(cascadeRadius, 0.1f);

	//============================================================
	// Directional Lightの方向
	//   lightPosition = center + lightDir * distance
	// として、中心方向を見る
	//============================================================
	XMVECTOR lightDir = XMVector3Normalize(*Light::getILightDir());

	//============================================================
	// ライトの上方向
	//============================================================
	XMVECTOR lightUp = XMVectorSet(
		0.0f,
		1.0f,
		0.0f,
		0.0f
	);

	//============================================================
	// ライト方向とY軸がほぼ平行なら、
	// LookAt行列が不安定になるためX軸を使用する
	//============================================================
	const float parallel =
		fabsf(XMVectorGetX(XMVector3Dot(lightDir, lightUp)));

	if (parallel > 0.99f)
	{
		lightUp = XMVectorSet(
			1.0f,
			0.0f,
			0.0f,
			0.0f
		);
	}

	//============================================================
	// ライト位置をCascade中心から十分離す
	// ※Z方向のShadow Casterも収められるよう半径に追加距離を加える
	//============================================================
	const float lightDepthPadding = 200.0f;

	const float receiverDepthPadding = 50.0f;

	const float lightDistance = cascadeRadius + lightDepthPadding + receiverDepthPadding;

	const XMVECTOR lightPosition =
		XMVectorAdd(cascadeCenter, XMVectorScale(lightDir, lightDistance));

	//============================================================
	// Cascade中心を見るライトView行列
	//============================================================
	XMMATRIX lightView = XMMatrixLookAtLH(
		lightPosition,
		cascadeCenter,
		lightUp
	);

	//============================================================
	// Cascade中心をライト空間へ変換
	//============================================================
	const XMVECTOR centerLightSpace = XMVector3TransformCoord(
		cascadeCenter,
		lightView
	);

	float centerX = XMVectorGetX(centerLightSpace);
	float centerY = XMVectorGetY(centerLightSpace);

	//============================================================
	// Orthographic Projectionの幅と高さ
	// Bounding Sphereを使用するため正方形になる
	//============================================================
	const float projectionWidth = cascadeRadius * 2.0f;
	const float projectionHeight = cascadeRadius * 2.0f;

	//============================================================
	// CascadeごとのTexel Snapping
	// 各Cascadeの投影範囲は異なるため
	// Cascadeごとに1テクセルのワールドサイズを計算する
	//============================================================
	if (mEnableTexelSnapping)
	{
		const float worldUnitsPerTexelX =
			projectionWidth /static_cast<float>(mShadowMapSize);

		const float worldUnitsPerTexelY =
			projectionHeight /static_cast<float>(mShadowMapSize);

		centerX =
			roundf(centerX / worldUnitsPerTexelX) * worldUnitsPerTexelX;

		centerY =
			roundf(centerY / worldUnitsPerTexelY) * worldUnitsPerTexelY;
	}

	//============================================================
	// ライト空間でのOrthographic範囲
	//============================================================
	const float minX = centerX - cascadeRadius;

	const float maxX = centerX + cascadeRadius;

	const float minY = centerY - cascadeRadius;

	const float maxY = centerY + cascadeRadius;

	//============================================================
	// Cascade視錐台8頂点をライト空間へ変換し
	// Z方向の最小値・最大値を求める
	//============================================================
	float minZ = FLT_MAX;

	float maxZ = -FLT_MAX;

	for (UINT cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
	{
		const XMVECTOR cornerLightSpace =
			XMVector3TransformCoord(
				corners[cornerIndex],
				lightView
			);

		const float cornerZ = XMVectorGetZ(cornerLightSpace);

		minZ =(std::min)(minZ,cornerZ);
		maxZ =(std::max)(maxZ,cornerZ);
	}

	//============================================================
	// カメラ視錐台の外側にある物体も
	// 視錐台内へ影を落とす可能性がある
	// ※Z方向へ余裕を追加する
	//============================================================
	const float casterDepthPadding = 300.0f;

	minZ -= casterDepthPadding;

	maxZ += casterDepthPadding;

	//============================================================
	// Nearは0より大きくする
	// ライト位置を十分後方に置いているため
	// 通常はminZも正になる
	//============================================================
	const float nearZ =(std::max)(minZ,0.01f);
	const float farZ = (std::max)(maxZ, nearZ + 0.01f);

	//============================================================
	// Cascade別カリングに使用するライト空間AABBを保存
	// IntersectsCascadeSphere()では
	// オブジェクトのBounding Sphere中心をライト空間へ変換し
	// この範囲との交差を調べる
	//============================================================
	mCascadeCullVolumes[cascadeIndex].minPosition =
	{
		minX,
		minY,
		nearZ
	};

	mCascadeCullVolumes[cascadeIndex].maxPosition =
	{
		maxX,
		maxY,
		farZ
	};

	//============================================================
	// Cascade専用Orthographic Projection
	// OffCenter版を使用し、
	// Texel Snapping後の中心位置を反映する
	//============================================================
	const XMMATRIX lightProjection =
		XMMatrixOrthographicOffCenterLH(
			minX,
			maxX,
			minY,
			maxY,
			nearZ,
			farZ
		);

	//============================================================
	// Cascadeごとの行列を保存
	//============================================================
	mLightView[cascadeIndex] = lightView;

	mLightProj[cascadeIndex] = lightProjection;

	mLightViewProj[cascadeIndex] =
		XMMatrixMultiply(
			lightView,
			lightProjection
		);
}

bool ShadowMap::HasStaticLightViewProjChanged(UINT cascadeIndex) const
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	//============================================================
	// 一度もStatic ShadowMapを描画していない
	// 比較対象が存在しないため、変化ありとして扱う
	//============================================================
	if (!mHasLastStaticLightViewProj[cascadeIndex])
	{
		return true;
	}

	//============================================================
	// 前回Static ShadowMapを描画したときの行列
	//============================================================
	const XMMATRIX previousLightViewProj =
		XMLoadFloat4x4(&mLastStaticLightViewProj[cascadeIndex]);

	//============================================================
	// 現在通常描画で使用するLightVP
	//============================================================
	const XMMATRIX& currentLightViewProj =
		mLightViewProj[cascadeIndex];

	//============================================================
	// 浮動小数点の微小誤差を許容する
	// ※値を大きくしすぎると
	// ※古いStatic ShadowMapを使い続けて影がずれる
	//============================================================
	const XMVECTOR epsilon = XMVectorReplicate(0.0000001f);

	//============================================================
	// 4×4行列を行ごとに比較
	//============================================================
	for (UINT rowIndex = 0; rowIndex < 4; ++rowIndex)
	{
		const bool rowIsEqual =
			XMVector4NearEqual(
				currentLightViewProj.r[rowIndex],
				previousLightViewProj.r[rowIndex],
				epsilon
			);

		if (!rowIsEqual)
		{
			return true;
		}
	}

	//============================================================
	// 全要素が許容誤差内
	//============================================================
	return false;
}

//============================================================
// ShadowMap描画開始
//============================================================
void ShadowMap::BeginRender(UINT cascadeIndex, ShadowMapLayer layer)
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	const UINT layerIndex = ToShadowLayerIndex(layer);

	assert(layerIndex < SHADOW_LAYER_COUNT);
	ID3D12GraphicsCommandList* cmdList = Direct3D::getCommandList();

	assert(cmdList != nullptr);

	//============================================================
	// 現在描画中のCascadeとLayerを保存
	//============================================================
	mCurrentCascadeIndex = cascadeIndex;
	mCurrentLayer = layer;

	//============================================================
	// このCascadeは現在フレームで実際に更新された
	//============================================================
	mLayerCascadeUpdatedThisFrame[layerIndex][cascadeIndex] = true;

	//============================================================
	// 新しいShadowMap描画を開始するので、
	// 定数バッファの使用位置を先頭へ戻す
	//============================================================
	mObjectCBIndex = 0;

	//============================================================
	// 現在のCascadeのDraw Call数をリセット
	//============================================================
	mLayerCascadeRenderedObjectCounts[layerIndex][cascadeIndex] = 0;
	mLayerCascadeDrawCallCounts[layerIndex][cascadeIndex] = 0;

	//============================================================
	// 現在CascadeのTimestamp開始番号
	// Cascade 0: 0
	// Cascade 1: 2
	// Cascade 2: 4
	//============================================================
	const UINT timestampStartIndex = GetTimestampStartIndex(layer, cascadeIndex);

	//============================================================
	// Shadow Pass開始Timestampを記録
	// この位置に置くことで
	// Resource Barrier、Clear、Draw処理を含めて計測する
	//============================================================
	if (mTimestampQueryHeap != nullptr)
	{
		cmdList->EndQuery(mTimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, timestampStartIndex);
		//========================================================
		// このCascadeのTimestampは現在フレームで有効
		//========================================================
		mLayerCascadeTimestampIssued[layerIndex][cascadeIndex] = true;
	}

	//============================================================
	// SRV状態からDepth書き込み状態へ
	//============================================================
	ChangeResourceState(
		cascadeIndex,
		layer,
		D3D12_RESOURCE_STATE_DEPTH_WRITE
	);

	//============================================================
	// ShadowMap描画用Viewport / Scissor
	//============================================================
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissor);

	//============================================================
	// RTVは使わず、DSVのみセット
	//============================================================
	cmdList->OMSetRenderTargets(
		0,
		nullptr,
		false,
		&mDSV[layerIndex][cascadeIndex]
	);

	//============================================================
	// Depthをクリア
	//============================================================
	cmdList->ClearDepthStencilView(
		mDSV[layerIndex][cascadeIndex],
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr
	);

	//============================================================
	// ShadowMap用RootSignature / PSO
	//============================================================
	cmdList->SetGraphicsRootSignature(mRootSignature);
	//cmdList->SetPipelineState(mPipelineStateStatic);
}

//============================================================
// ShadowMap描画終了
//============================================================
void ShadowMap::EndRender(UINT cascadeIndex, ShadowMapLayer layer)
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);
	const UINT layerIndex = ToShadowLayerIndex(layer);
	assert(layerIndex < SHADOW_LAYER_COUNT);

	ID3D12GraphicsCommandList* cmdList = Direct3D::getCommandList();

	assert(cmdList != nullptr);

	//============================================================
	// 今回のCascadeへ送ったオブジェクト数を保存
	// ※BeginRender()でmObjectCBIndexを0へ戻し、
	// ※SetObjectMatrix()ごとに1ずつ加算しているため、
	// ※現在値がこのCascadeの描画オブジェクト数になる
	//============================================================
	mLayerCascadeRenderedObjectCounts[layerIndex][cascadeIndex] = mObjectCBIndex;

	//============================================================
	// DepthWrite状態からPixelShaderで読める状態へ
	//============================================================
	ChangeResourceState(
		cascadeIndex,
		layer,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);

	//============================================================
	// Layer込みTimestamp番号
	//============================================================
	const UINT timestampStartIndex = GetTimestampStartIndex(layer, cascadeIndex);

	const UINT timestampEndIndex = timestampStartIndex + 1;

	if (mTimestampQueryHeap != nullptr && mTimestampReadbackBuffer != nullptr)
	{
		//========================================================
		// Shadow Pass終了Timestamp
		//========================================================
		cmdList->EndQuery(
			mTimestampQueryHeap,
			D3D12_QUERY_TYPE_TIMESTAMP,
			timestampEndIndex
		);

		//========================================================
		// 2個のTimestampをReadback BufferへResolve
		// Destination Offset:
		//   Cascade 0 = 0バイト
		//   Cascade 1 = 16バイト
		//   Cascade 2 = 32バイト
		//========================================================
		const UINT64 destinationOffset = static_cast<UINT64>(timestampStartIndex) * sizeof(UINT64);

		cmdList->ResolveQueryData(
			mTimestampQueryHeap,
			D3D12_QUERY_TYPE_TIMESTAMP,
			timestampStartIndex,
			2,
			mTimestampReadbackBuffer,
			destinationOffset
		);
	}
}