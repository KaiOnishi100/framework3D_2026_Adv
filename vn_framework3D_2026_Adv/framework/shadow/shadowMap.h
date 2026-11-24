#pragma once

//============================================================
// ShadowObjectCB
// ShadowMap描画用の定数バッファ
// world モデルのワールド行列
// lightViewProj: ライト視点のViewProjection行列
// 注意: HLSL側の cbuffer ShadowObjectCB と並びを合わせる
//============================================================
struct ShadowObjectCB
{
	XMFLOAT4X4 world;
	XMFLOAT4X4 lightViewProj;
};

//============================================================
// Cascadeカリング用のライト空間AABB
// BuildCascadeLightMatrix()で求めた
// Orthographic Projectionの範囲を保存する
//============================================================
struct CascadeCullVolume
{
	XMFLOAT3 minPosition =
	{
		0.0f,
		0.0f,
		0.0f
	};

	XMFLOAT3 maxPosition =
	{
		0.0f,
		0.0f,
		0.0f
	};
};

//============================================================
// ShadowMapの描画レイヤー
// Static: 建物、岩、フェンスなどの静的Caster用
// Dynamic: プレイヤー、敵、動くギミックなどの動的Caster用
//============================================================
enum class ShadowMapLayer : UINT
{
	Static = 0,
	Dynamic,

	//========================================================
	// レイヤー数
	// 配列サイズとして使用する
	//========================================================
	Count
};

//============================================================
// enum classを配列番号へ変換する補助関数
//============================================================
constexpr UINT ToShadowLayerIndex(ShadowMapLayer layer)
{
	return static_cast<UINT>(layer);
}

//============================================================
// ShadowMapレイヤー数
//============================================================
static constexpr UINT SHADOW_LAYER_COUNT = ToShadowLayerIndex(ShadowMapLayer::Count);
// ShadowMapのCascade数
static constexpr UINT SHADOW_CASCADE_COUNT = 3;

class ShadowMap
{
public:
	bool Initialize();
	void Terminate();

	//============================================================
	// シャドウマップ描画開始
	// ・ShadowMap用DepthBufferへ描画先を変更
	// ・Viewport / ScissorをShadowMapサイズに変更
	// ・Depthをクリア
	//============================================================
	void BeginRender(UINT cascadeIndex, ShadowMapLayer layer);

	//============================================================
	// シャドウマップ描画終了
	// ShadowMapをDepthWrite状態からPixelShaderResource状態へ戻す
	//============================================================
	void EndRender(UINT cascadeIndex, ShadowMapLayer layer);

	//============================================================
	// ライト行列更新
	// target:
	//   ShadowMapの中心にしたいワールド座標
	//   プレイヤーを中心にする場合はプレイヤーのワールド座標を渡す
	//============================================================
	void UpdateLightMatrix(FXMVECTOR target);

	//============================================================
	// ShadowMap描画用定数バッファ更新
	// world: 描画するモデルのワールド行列
	//============================================================
	void SetObjectMatrix(const XMMATRIX& world);

	//============================================================
	// 現在フレームで各Cascadeを更新するか決定する
	// UpdateLightMatrix()より前に、フレームごとに1回呼ぶ
	//============================================================
	void PrepareCascadeUpdates();

	//============================================================
	// 指定Cascadeを現在フレームで更新するか
	// true: ShadowMapとLightViewProjectionを更新する
	// false: 前回のShadowMapとLightViewProjectionを再利用する
	//============================================================
	bool ShouldUpdateCascade(UINT cascadeIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		return mCascadeShouldUpdate[cascadeIndex];
	}

	//============================================================
	// Static ShadowMapを更新する必要があるか
	// 判定条件:
	// ・Cascade自体が更新対象である
	// ・Static ShadowMapがDirtyである
	// ・Static ShadowMapをまだ一度も描画していない
	// ・最後にStaticを描画したときからLightVPが変化した
	//============================================================
	bool ShouldUpdateStaticCascade(UINT cascadeIndex) const;

	//============================================================
	// Static ShadowMapの描画完了を記録
	// Static ShadowMapを描画した直後に呼び
	// 今回使用したLightVPをキャッシュする
	//============================================================
	void CommitStaticCascadeUpdate(UINT cascadeIndex);

	//============================================================
	// Static ShadowMapキャッシュをすべて無効化
	// Static Casterの追加・削除・移動
	// CastShadowやMobility変更時に呼ぶ
	//============================================================
	void InvalidateStaticShadowCache();

	//============================================================
	// Static ShadowMapがDirtyか
	// デバッグ表示にも利用できる
	//============================================================
	bool IsStaticCascadeDirty(UINT cascadeIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		return mStaticCascadeDirty[cascadeIndex];
	}

	//============================================================
	// Cascadeの更新間隔を設定
	// interval = 1: 毎フレーム更新
	// interval = 2: 2フレームに1回更新
	// interval = 4: 4フレームに1回更新
	//============================================================
	void SetCascadeUpdateInterval(UINT cascadeIndex, UINT interval);

	//============================================================
	// Cascadeの更新間隔を取得
	//============================================================
	UINT GetCascadeUpdateInterval(UINT cascadeIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		return mCascadeUpdateIntervals[cascadeIndex];
	}

	//============================================================
	// 現在フレームで実際に更新されたか
	// デバッグ表示に使用する
	//============================================================
	bool WasCascadeUpdatedThisFrame(UINT cascadeIndex, ShadowMapLayer layer) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		const UINT layerIndex = ToShadowLayerIndex(layer);
		assert(layerIndex < SHADOW_LAYER_COUNT);
		return mLayerCascadeUpdatedThisFrame[layerIndex][cascadeIndex];
	}

	//============================================================
	// 指定Cascadeが更新されたか
	// StaticまたはDynamicのどちらかが更新されていればtrue
	//============================================================
	bool WasCascadeUpdatedThisFrame(UINT cascadeIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
		{
			if (mLayerCascadeUpdatedThisFrame[layerIndex][cascadeIndex])
			{
				return true;
			}
		}
		return false;
	}

	//============================================================
	// 指定Layer・Cascadeをスキップ
	//============================================================
	void MarkLayerCascadeSkipped(UINT cascadeIndex, ShadowMapLayer layer);

	//============================================================
	// Cascadeを更新しなかった場合の処理
	// オブジェクト数やDraw Call数を0へ戻す
	//============================================================
	void MarkCascadeSkipped(UINT cascadeIndex);

	//============================================================
	// 全Cascadeを次フレームで強制更新する
	// ライト方向変更、カメラのワープ、FOV変更などで使用する
	//============================================================
	void ForceUpdateAllCascades()
	{
		mForceUpdateAllCascades = true;
	}

	//============================================================
	// 1フレーム分のShadow処理が終了したことを通知
	// Timestamp Queryを使用している場合にも必要
	//============================================================
	void FinishShadowFrame();

	//============================================================
	// Getter
	//============================================================
	//============================================================
	// Cascade数取得
	//============================================================
	static constexpr UINT GetCascadeCount()
	{
		return SHADOW_CASCADE_COUNT;
	}

	ID3D12Resource* GetResource(UINT cascadeIndex, ShadowMapLayer layer) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		const UINT layerIndex = ToShadowLayerIndex(layer);
		assert(layerIndex < SHADOW_LAYER_COUNT);
		return mShadowResources[layerIndex][cascadeIndex];
	}

	ID3D12DescriptorHeap* GetSRVHeap() const { return mSRVHeap; }

	const XMMATRIX& GetLightView(UINT cascadeIndex = 0) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		return mLightView[cascadeIndex];
	}
	const XMMATRIX& GetLightProj(UINT cascadeIndex = 0) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		return mLightProj[cascadeIndex];
	}
	const XMMATRIX& GetLightViewProj(UINT cascadeIndex = 0) const
	{
		return mLightViewProj[cascadeIndex];
	}

	float GetCascadeSplitDepth(UINT cascadeIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		return mCascadeSplitDepths[cascadeIndex];
	}

	void SetMaxShadowDistance(float distance)
	{
		mMaxShadowDistance = (std::max)(distance, 1.0f);
	}

	float GetMaxShadowDistance() const
	{
		return mMaxShadowDistance;
	}

	//============================================================
	// 指定Cascadeのブレンド開始距離を取得
	//============================================================
	float GetCascadeBlendStartDepth(UINT cascadeIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);

		return mCascadeBlendStartDepths[cascadeIndex];
	}

	//============================================================
	// Cascade視錐台の頂点を取得
	//
	// cornerIndex:
	//
	//   0 Near TopLeft
	//   1 Near TopRight
	//   2 Near BottomLeft
	//   3 Near BottomRight
	//
	//   4 Far TopLeft
	//   5 Far TopRight
	//   6 Far BottomLeft
	//   7 Far BottomRight
	//============================================================
	XMVECTOR GetCascadeFrustumCorner(UINT cascadeIndex, UINT cornerIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		assert(cornerIndex < 8);
		return XMLoadFloat3(&mCascadeFrustumCorners[cascadeIndex][cornerIndex]);
	}


	UINT GetSize() const { return mShadowMapSize; }

	//============================================================
	// ShadowMap描画用
	//============================================================
	ID3D12RootSignature* GetRootSignature() const { return mRootSignature; }
	ID3D12PipelineState* GetStaticPipelineState() const { return mPipelineStateStatic; }
	ID3D12PipelineState* GetSkinPipelineState()const { return mPipelineStateSkin; }

	//============================================================
	// ShadowMap SRV CPU Handle 取得
	// ※vnModel::render() 側で
	// ※マテリアルDescriptorHeapの [1] へShadowMap SRVをコピーするために使う
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUHandle(UINT cascadeIndex, ShadowMapLayer layer) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		const UINT layerIndex = ToShadowLayerIndex(layer);
		assert(layerIndex < SHADOW_LAYER_COUNT);
		return mSRVCPU[layerIndex][cascadeIndex];
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle(UINT cascadeIndex, ShadowMapLayer layer) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		const UINT layerIndex = ToShadowLayerIndex(layer);
		assert(layerIndex < SHADOW_LAYER_COUNT);
		return mSRVGPU[layerIndex][cascadeIndex];
	}

	//============================================================
	// CopyDescriptorsSimple用のコピー元SRV
	// ※shader visible heapではなく、CPU only heapのhandleを返す
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCopyCPUHandle(UINT cascadeIndex, ShadowMapLayer layer) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		const UINT layerIndex = ToShadowLayerIndex(layer);
		assert(layerIndex < SHADOW_LAYER_COUNT);
		return mSRVCopyCPU[layerIndex][cascadeIndex];
	}

	//============================================================
	// LayerとCascadeの組み合わせを1次元番号へ変換
	//============================================================
	static constexpr UINT GetLayerCascadeIndex(ShadowMapLayer layer, UINT cascadeIndex)
	{
		return ToShadowLayerIndex(layer) * SHADOW_CASCADE_COUNT + cascadeIndex;
	}

	//============================================================
	// Timestamp Queryの開始番号を取得
	// 1Passにつき、開始・終了の2個を使用する
	// Static Cascade 0:
	//   start = 0
	//   end   = 1
	// Dynamic Cascade 0:
	//   start = 6
	//   end   = 7
	//============================================================
	static constexpr UINT GetTimestampStartIndex(ShadowMapLayer layer, UINT cascadeIndex)
	{
		return GetLayerCascadeIndex(layer, cascadeIndex) * 2;
	}

	//============================================================
	// デバック用
	//============================================================
	//============================================================
	// 指定Layer・Cascadeのオブジェクト数
	//============================================================
	UINT GetLayerCascadeRenderedObjectCount(UINT cascadeIndex, ShadowMapLayer layer) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		const UINT layerIndex = ToShadowLayerIndex(layer);
		assert(layerIndex < SHADOW_LAYER_COUNT);
		return mLayerCascadeRenderedObjectCounts[layerIndex][cascadeIndex];
	}
	//============================================================
	// 指定Cascadeへ描画したShadow Caster数を取得
	//============================================================
	UINT GetCascadeRenderedObjectCount(UINT cascadeIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		UINT totalCount = 0;
		for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
		{
			totalCount += mLayerCascadeRenderedObjectCounts[layerIndex][cascadeIndex];
		}
		return totalCount;
	}

	//============================================================
	// 全Cascadeの描画オブジェクト数合計を取得
	// 同じオブジェクトが複数Cascadeへ描画された場合は
	// その回数分だけ合計される
	//============================================================
	UINT GetTotalRenderedObjectCount() const
	{
		UINT totalCount = 0;
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			totalCount += GetCascadeRenderedObjectCount(cascadeIndex);
		}
		return totalCount;
	}

	//============================================================
	// 現在描画中のCascadeへShadow Draw Call数を加算
	// vnModel::renderShadow()で
	// DrawIndexedInstanced()を呼んだ直後に使用する
	//============================================================
	void AddShadowDrawCall(UINT count = 1)
	{
		assert(mCurrentCascadeIndex < SHADOW_CASCADE_COUNT);
		const UINT layerIndex = ToShadowLayerIndex(mCurrentLayer);
		assert(layerIndex < SHADOW_LAYER_COUNT);
		mLayerCascadeDrawCallCounts[layerIndex][mCurrentCascadeIndex] += count;
	}

	//============================================================
	// 指定Layer・CascadeのDraw Call数
	//============================================================
	UINT GetLayerCascadeDrawCallCount(UINT cascadeIndex, ShadowMapLayer layer) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		const UINT layerIndex = ToShadowLayerIndex(layer);
		assert(layerIndex < SHADOW_LAYER_COUNT);
		return mLayerCascadeDrawCallCounts[layerIndex][cascadeIndex];
	}

	//============================================================
	// 指定CascadeのStatic＋Dynamic合計
	//============================================================
	UINT GetCascadeDrawCallCount(UINT cascadeIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		UINT totalCount = 0;
		for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
		{
			totalCount += mLayerCascadeDrawCallCounts[layerIndex][cascadeIndex];
		}
		return totalCount;
	}

	//============================================================
	// 全CascadeのShadow Draw Call数を取得
	// 同じモデルが複数Cascadeに描画された場合は
	// Cascadeごとの描画回数をすべて合計する
	//============================================================
	UINT GetTotalDrawCallCount() const
	{
		UINT totalCount = 0;
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			totalCount += GetCascadeDrawCallCount(cascadeIndex);
		}
		return totalCount;
	}

	//============================================================
	// Timestamp Query関連Resource作成
	//============================================================
	void CreateTimestampQueryResources();

	//============================================================
	// 指定Layer・CascadeのGPU時間
	//============================================================
	double GetLayerCascadeGpuTimeMs(UINT cascadeIndex, ShadowMapLayer layer) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		const UINT layerIndex = ToShadowLayerIndex(layer);
		assert(layerIndex < SHADOW_LAYER_COUNT);
		return mLayerCascadeGpuTimeMs[layerIndex][cascadeIndex];
	}

	//============================================================
	// 指定CascadeのGPU処理時間を取得
	// 戻り値: ミリ秒
	//============================================================
	double GetCascadeGpuTimeMs(UINT cascadeIndex) const
	{
		assert(cascadeIndex < SHADOW_CASCADE_COUNT);
		double totalTimeMs = 0.0;
		for (UINT layerIndex = 0; layerIndex < SHADOW_LAYER_COUNT; ++layerIndex)
		{
			totalTimeMs += mLayerCascadeGpuTimeMs[layerIndex][cascadeIndex];
		}
		return totalTimeMs;
	}

	//============================================================
	// 全CascadeのGPU処理時間を取得
	//============================================================
	double GetTotalGpuTimeMs() const
	{
		return mTotalGpuTimeMs;
	}

	//============================================================
	// Timestamp結果からGPU処理時間を計算
	//============================================================
	void UpdateTimestampResults();


	//============================================================
	//============================================================

	//============================================================
	// ワールド空間Bounding SphereとCascade範囲の交差判定
	//============================================================
	bool IntersectsCascadeSphere(
		UINT cascadeIndex,
		FXMVECTOR worldCenter,
		float worldRadius) const;

	void SetTexelSnappingEnabled(bool enable) { mEnableTexelSnapping = enable; }
	bool IsTexelSnappingEnabled() const { return mEnableTexelSnapping; }
private:

	void CreateResource();
	void CreateDSV();
	void CreateSRV();
	void CreateRootSignature();
	void CreatePipelineState();

	void CreateObjectConstantBuffer();

	void ChangeResourceState(
		UINT cascadeIndex,
		ShadowMapLayer layer,
		D3D12_RESOURCE_STATES afterState
	);


	//============================================================
	// Near/Far間の視錐台8頂点をワールド座標で作る
	//============================================================
	void BuildFrustumCornersWorld(
		float splitNear,
		float splitFar,
		float fovY,
		float aspect,
		XMVECTOR outCorners[8]
	) const;

	//============================================================
	// Cascade分割距離を更新する
	//============================================================
	void UpdateCascadeSplitDepths(
		float cameraNear,
		float cameraFar
	);

	//============================================================
	// 指定したCascadeの視錐台8頂点から、
	// ライト用View / Projection行列を作成する
	//============================================================
	void BuildCascadeLightMatrix(UINT cascadeIndex);

	//============================================================
	// Static ShadowMap描画時からLightVPが変化したか
	// true:
	//   前回のStatic ShadowMapと現在のShadow座標が一致しないため
	//   Static ShadowMapの再描画が必要
	// false:
	//   前回のStatic ShadowMapを再利用できる
	//============================================================
	bool HasStaticLightViewProjChanged(UINT cascadeIndex) const;

private:
	//============================================================
	// ShadowMap本体
	// mShadowResources[Static][0]
	//   Cascade 0のStatic ShadowMap
	// mShadowResources[Dynamic][2]
	//   Cascade 2のDynamic ShadowMap
	//============================================================
	ID3D12Resource* mShadowResources[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// 各Resourceの現在State
	//============================================================
	D3D12_RESOURCE_STATES mResourceStates[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};
	//============================================================
	// DSV : 深度書き込み用
	// SRV : シェーダー読み取り用
	//============================================================
	ID3D12DescriptorHeap* mDSVHeap = nullptr;
	ID3D12DescriptorHeap* mSRVHeap = nullptr;
	ID3D12DescriptorHeap* mSRVCopyHeap = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE mDSV[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE mSRVCPU[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};// 他のDescriptorHeapへShadowMap SRVをコピーするために使う。
	D3D12_GPU_DESCRIPTOR_HANDLE mSRVGPU[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};// ShadowMap専用Heapを使うために確保する
	D3D12_CPU_DESCRIPTOR_HANDLE mSRVCopyCPU[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};
	//============================================================
	// ShadowMap描画用Viewport / Scissor
	//============================================================
	D3D12_VIEWPORT mViewport = {};
	D3D12_RECT mScissor = {};

	//============================================================
	// ライト視点行列
	//============================================================
	XMMATRIX mLightView[SHADOW_CASCADE_COUNT] = {};

	XMMATRIX mLightProj[SHADOW_CASCADE_COUNT] = {};

	XMMATRIX mLightViewProj[SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// Static ShadowMapを最後に描画したときのLightVP
	// ※Static ShadowMapのDepth値は、この行列を使って生成されている
	// ※通常描画側が異なるLightVPを使うと影の位置がずれるため
	// ※現在のLightVPと比較して再描画を判定する
	//============================================================
	XMFLOAT4X4 mLastStaticLightViewProj[SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// 各CascadeでStatic ShadowMapを一度でも作成したか
	// false: 初回なので必ずStatic ShadowMapを描画する
	//============================================================
	bool mHasLastStaticLightViewProj[SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// Static ShadowMapの内容が古くなっているか
	// Static Casterが変更された場合にtrueへする
	//============================================================
	bool mStaticCascadeDirty[SHADOW_CASCADE_COUNT] =
	{
		true,
		true,
		true
	};

	//============================================================
	// ShadowMapサイズ
	//============================================================
	UINT mShadowMapSize = 2048;

	//============================================================
	// ShadowMap描画専用RootSignature / PSO
	//============================================================
	ID3D12RootSignature* mRootSignature = nullptr;
	ID3D12PipelineState* mPipelineStateStatic = nullptr;
	ID3D12PipelineState* mPipelineStateSkin = nullptr;

	//============================================================
	// ShadowMap描画1回あたりの定数バッファサイズ
	// ※D3D12のCBVは256バイト境界が必要
	//============================================================
	UINT mObjectCBStride = 0;

	//============================================================
	// 今フレームで使用する定数バッファ番号
	//============================================================
	UINT mObjectCBIndex = 0;

	//============================================================
	// 最大Shadow描画オブジェクト数
	//============================================================
	static constexpr UINT MaxShadowObjects = 2048;

	//============================================================
	// ShadowMap描画用定数バッファ
	//============================================================
	ID3D12Resource* mObjectCB = nullptr;

	//============================================================
	// Mapした定数バッファの先頭
	// オブジェクトごとに256バイトずつずらして書き込む
	//============================================================
	BYTE* mMappedObjectCB = nullptr;

	//============================================================
	// Texel Snappingを使用するか
	//============================================================
	bool mEnableTexelSnapping = true;

	//============================================================
	// 現在描画中のCascade番号
	// SetObjectMatrix()が、どのCascade用領域へ
	// 書き込むか判断するために使用する
	//============================================================
	UINT mCurrentCascadeIndex = 0;

	//============================================================
	// Cascadeごとのカメラ視錐台8頂点
	// ※XMVECTORをメンバー配列にするとアライメント管理が必要なので
	// ※保存用にはXMFLOAT3を使用する
	//============================================================
	XMFLOAT3 mCascadeFrustumCorners[SHADOW_CASCADE_COUNT][8] = {};

	//============================================================
	// Cascadeごとのカリング範囲
	// 座標空間はライトView空間
	//============================================================
	CascadeCullVolume mCascadeCullVolumes[SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// Shadowを描画する最大距離
	// この距離を超えたオブジェクトには、
	// ShadowMapによる動的な影を適用しない
	//============================================================
	float mMaxShadowDistance = 350.0f;

	//============================================================
	// Cascade境界のブレンド率
	// 各Cascade範囲の終端10%をブレンド領域にする
	//============================================================
	float mCascadeBlendRatio = 0.10f;

	//============================================================
	// Cascadeごとのブレンド開始距離
	//============================================================
	float mCascadeBlendStartDepths[SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// 各Cascadeの終了距離
	// PixelShaderでCascadeを選択するときにも使用する
	//============================================================
	float mCascadeSplitDepths[SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// ShadowMap側のフレーム番号
	// ゲーム全体のフレーム番号とは別に管理する
	//============================================================
	UINT64 mShadowFrameIndex = 0;

	//============================================================
	// Cascadeごとの更新間隔
	// Initialize()で
	//   Cascade 0 = 1
	//   Cascade 1 = 1
	//   Cascade 2 = 2
	// を設定する
	//============================================================
	UINT mCascadeUpdateIntervals[SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// 現在フレームで更新するか
	//============================================================
	bool mCascadeShouldUpdate[SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// 現在フレームで実際に更新されたか
	//============================================================
	bool mLayerCascadeUpdatedThisFrame[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// 次フレームで全Cascadeを強制更新するか
	//============================================================
	bool mForceUpdateAllCascades = true;

	//============================================================
	// 現在フレームでTimestamp Queryを記録したCascade
	// 更新をスキップしたCascadeの古いTimestampを
	// 読まないために使用する
	//============================================================
	bool mLayerCascadeTimestampIssued[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// 現在描画中のShadowMapレイヤー
	// BeginRender()で設定する
	//============================================================
	ShadowMapLayer mCurrentLayer = ShadowMapLayer::Dynamic;

	//============================================================
	// デバック用
	//============================================================

	//============================================================
	// Layer・Cascadeごとの描画オブジェクト数
	// StaticとDynamicを別々に記録する。
	//============================================================
	UINT mLayerCascadeRenderedObjectCounts[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// Layer・CascadeごとのDraw Call数
	//============================================================
	UINT mLayerCascadeDrawCallCounts[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};

	static constexpr UINT TimestampQueryCount = SHADOW_LAYER_COUNT * SHADOW_CASCADE_COUNT * 2;

	//============================================================
	// GPU Timestamp Query Heap
	//============================================================
	ID3D12QueryHeap* mTimestampQueryHeap = nullptr;

	//============================================================
	// ResolveQueryData()の結果をCPUへ読み戻すBuffer
	//============================================================
	ID3D12Resource* mTimestampReadbackBuffer = nullptr;

	//============================================================
	// Readback BufferをMapした先頭アドレス
	// UINT64単位でTimestamp値が格納される
	//============================================================
	UINT64* mMappedTimestampData = nullptr;

	//============================================================
	// Command QueueのTimestamp周波数
	// 1秒間にTimestampカウンターが何回進むかを表す
	//============================================================
	UINT64 mTimestampFrequency = 0;

	//============================================================
	// CascadeごとのGPU処理時間
	// 単位はミリ秒
	//============================================================
	double mLayerCascadeGpuTimeMs[SHADOW_LAYER_COUNT][SHADOW_CASCADE_COUNT] = {};

	//============================================================
	// 全CascadeのGPU時間合計
	//============================================================
	double mTotalGpuTimeMs = 0.0;

	//============================================================
	// 前フレームのTimestamp結果が存在するか
	// EndRender(Cascade 2)でtrueにし、
	// 次フレーム開始時に結果を読み取る
	//============================================================
	bool mHasPendingTimestampResult = false;

	//============================================================
	//============================================================
};