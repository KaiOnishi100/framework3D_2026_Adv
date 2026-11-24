#include "../../framework.h"
#include "../vn_environment.h"

#include "pingPongPostRT.h"

//============================================================
// 初期化
//============================================================
bool PingPongPostRT::Initialize(
	UINT width,
	UINT height,
	DXGI_FORMAT format
)
{
	if (mInitialized) return true;

	HRESULT hr = S_OK;

	ID3D12Device* device = Direct3D::getDevice();
	assert(device != nullptr);

	mWidth = width;
	mHeight = height;
	mFormat = format;

	if (mWidth < 1)  mWidth = 1;
	if (mHeight < 1) mHeight = 1;

	mReadIndex = 0;
	mWriteIndex = 1;

	mState[0] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	mState[1] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	//============================================================
	// Descriptor size 取得
	//============================================================
	mRTVDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	mSRVDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//============================================================
	// RTV Heap 作成
	// ※2枚のRTへ描画するため、RTVは2個必要
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.NumDescriptors = 2;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 0;

		hr = device->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(&mRTVHeap)
		);
		assert(hr == S_OK);

		mRTVHeap->SetName(L"PingPongPostRT RTVHeap");
	}

	//============================================================
	// SRV Heap 作成
	// シェーダーから読むため、shader visible にする
	//============================================================
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = 2;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;

		hr = device->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(&mSRVHeap)
		);
		assert(hr == S_OK);

		mSRVHeap->SetName(L"PingPongPostRT SRVHeap");
	}

	//============================================================
	// ハンドルを作成
	//============================================================
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
			mRTVHeap->GetCPUDescriptorHandleForHeapStart();

		D3D12_CPU_DESCRIPTOR_HANDLE srvCPUHandle =
			mSRVHeap->GetCPUDescriptorHandleForHeapStart();

		D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle =
			mSRVHeap->GetGPUDescriptorHandleForHeapStart();

		for (int i = 0; i < 2; ++i)
		{
			mRTV[i] = rtvHandle;
			mSRVCPU[i] = srvCPUHandle;
			mSRVGPU[i] = srvGPUHandle;

			rtvHandle.ptr += mRTVDescriptorSize;
			srvCPUHandle.ptr += mSRVDescriptorSize;
			srvGPUHandle.ptr += mSRVDescriptorSize;
		}
	}

	//============================================================
	// RenderTarget 2枚作成
	//============================================================
	for (int i = 0; i < 2; ++i)
	{
		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProp.CreationNodeMask = 0;
		heapProp.VisibleNodeMask = 0;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Alignment = 0;
		resDesc.Width = mWidth;
		resDesc.Height = mHeight;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Format = mFormat;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		// RenderTargetとして使うので必須
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = mFormat;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 1.0f;

		// 初期状態はSRV
		// 書くときだけRTVへ遷移させる
		hr = device->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(&mRT[i])
		);
		assert(hr == S_OK);

		if (i == 0)
		{
			mRT[i]->SetName(L"PingPongPostRT RT[0]");
		}
		else
		{
			mRT[i]->SetName(L"PingPongPostRT RT[1]");
		}

		//========================================================
		// RTV作成
		//========================================================
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = mFormat;

		device->CreateRenderTargetView(
			mRT[i],
			&rtvDesc,
			mRTV[i]
		);

		//========================================================
		// SRV作成
		//========================================================
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = mFormat;
		srvDesc.Texture2D.MipLevels = 1;

		device->CreateShaderResourceView(
			mRT[i],
			&srvDesc,
			mSRVCPU[i]
		);
	}

	mInitialized = true;

	return true;
}

//============================================================
// 終了処理
//============================================================
void PingPongPostRT::Terminate()
{
	SAFE_RELEASE(mRT[0]);
	SAFE_RELEASE(mRT[1]);

	SAFE_RELEASE(mSRVHeap);
	SAFE_RELEASE(mRTVHeap);

	mReadIndex = 0;
	mWriteIndex = 1;

	mWidth = 0;
	mHeight = 0;

	mInitialized = false;
}

//============================================================
// Reset
//============================================================
void PingPongPostRT::Reset()
{
	mReadIndex = 0;
	mWriteIndex = 1;
}

//============================================================
// Swap
//============================================================
void PingPongPostRT::Swap()
{
	//============================================================
	// 直前に書き込んだRTを次の読み取り元にする
	//============================================================
	int temp = mReadIndex;
	mReadIndex = mWriteIndex;
	mWriteIndex = temp;
}

//============================================================
// 状態変更
//============================================================
void PingPongPostRT::ChangeResourceState(
	int index,
	D3D12_RESOURCE_STATES afterState
)
{
	assert(index >= 0 && index < 2);
	assert(mRT[index] != nullptr);

	//============================================================
	// すでに目的の状態なら何もしない
	// 余計なBarrierを減らす
	//============================================================
	if (mState[index] == afterState)
	{
		return;
	}

	ID3D12GraphicsCommandList* cmdList =Direct3D::getCommandList();

	assert(cmdList != nullptr);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = mRT[index];
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = mState[index];
	barrier.Transition.StateAfter = afterState;

	cmdList->ResourceBarrier(1, &barrier);

	//============================================================
	// 現在状態を更新
	//============================================================
	mState[index] = afterState;
}

//============================================================
// Read側をSRVにする
//============================================================
void PingPongPostRT::TransitionReadToSRV()
{
	ChangeResourceState(
		mReadIndex,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
}

//============================================================
// Write側をRTVにする
//============================================================
void PingPongPostRT::TransitionWriteToRTV()
{
	ChangeResourceState(
		mWriteIndex,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);
}

//============================================================
// Write側をSRVにする
// 書いた後、次のPassで読むために使う
//============================================================
void PingPongPostRT::TransitionWriteToSRV()
{
	ChangeResourceState(
		mWriteIndex,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
}

//============================================================
// Write先RTVをクリア
//============================================================
void PingPongPostRT::ClearWriteRTV(const float clearColor[4])
{
	ID3D12GraphicsCommandList* cmdList = Direct3D::getCommandList();

	assert(cmdList != nullptr);

	cmdList->ClearRenderTargetView(
		GetWriteRTV(),
		clearColor,
		0,
		nullptr
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE PingPongPostRT::GetReadSRVCPU() const
{
	return mSRVCPU[mReadIndex];
}

//============================================================
// Getter
//============================================================
ID3D12Resource* PingPongPostRT::GetReadResource() const
{
	return mRT[mReadIndex];
}

ID3D12Resource* PingPongPostRT::GetWriteResource() const
{
	return mRT[mWriteIndex];
}

D3D12_GPU_DESCRIPTOR_HANDLE PingPongPostRT::GetReadSRV() const
{
	return mSRVGPU[mReadIndex];
}

D3D12_GPU_DESCRIPTOR_HANDLE PingPongPostRT::GetWriteSRV() const
{
	return mSRVGPU[mWriteIndex];
}

D3D12_CPU_DESCRIPTOR_HANDLE PingPongPostRT::GetReadRTV() const
{
	return mRTV[mReadIndex];
}

D3D12_CPU_DESCRIPTOR_HANDLE PingPongPostRT::GetWriteRTV() const
{
	return mRTV[mWriteIndex];
}

ID3D12DescriptorHeap* PingPongPostRT::GetSRVHeap() const
{
	return mSRVHeap;
}