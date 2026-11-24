#pragma once

#define vnOBJECT2D_MAX (512)	//2Dオブジェクトを登録できる最大数
#define vnOBJECT3D_MAX (2048)	//3Dオブジェクトを登録できる最大数

class ShadowMap;

class vnScene
{
private:
	static vnSprite* pObject2D_Array[vnOBJECT2D_MAX];	//2Dオブジェクトを登録しておく配列
	static ObjectBase* pObject3D_Array[vnOBJECT3D_MAX];	//3Dオブジェクトを登録しておく配列

	//2Dオブジェクトをソートする必要があるか
	static bool UpdateRenderPriority;

	static bool  sHitStop;
	static float sHitStopTimer;

	//============================================================
	// Scene共通ShadowMap
	//============================================================
	static ShadowMap mShadowMap;

	//============================================================
	// このSceneでShadowMapを使用するか
	//============================================================
	static bool mShadowMapEnabled;

	//============================================================
	// ShadowMap::Initialize()が完了しているか
	//
	// GPU Resourceを二重に作成しないために使用する
	//============================================================
	bool mShadowMapInitialized = false;

	//============================================================
	// ShadowMap更新時の基準オブジェクト
	//
	// 通常はプレイヤーを指定する
	// nullptrの場合はワールド原点を使用する
	//============================================================
	ObjectBase* mShadowTargetObject = nullptr;

	//============================================================
	// Cascade Shadowデバッグ表示
	//============================================================
	bool mShowCascadeShadowDebug = true;

	//============================================================
	// Point Light登録候補
	//
	// 通常Lighting情報とShadow選択情報をまとめて保持する。
	//============================================================
	struct PointLightCandidate
	{
		//========================================================
		// Scene内で重複しない固定ID
		//
		// ライトの生成・削除を行っても、同じライトには
		// 同じIDを使用する。
		//========================================================
		std::uint32_t StableID = 0u;
		XMFLOAT3 Position = { 0.0f,0.0f,0.0f };
		float Range = 1.0f;
		XMFLOAT3 Color = { 1.0f,1.0f,1.0f };
		float Intensity = 1.0f;

		//========================================================
		// このライトがShadow候補か
		//
		// falseの場合は通常Point Lightとしてのみ登録する。
		//========================================================
		bool CastShadow = false;

		//========================================================
		// Shadow選択優先度
		//
		// 1.0:
		//   通常
		//
		// 2.0:
		//   通常ライトの2倍優先
		//
		// 0.5:
		//   優先度が低い
		//========================================================
		float ShadowPriority = 1.0f;

		PointLightShadowManager::UpdateMode ShadowUpdateMode = PointLightShadowManager::UpdateMode::EveryFrame;
		PointLightShadowManager::ShadowResolution ShadowResolution = PointLightShadowManager::ShadowResolution::Medium;

		//========================================================
		// 選択処理で計算される一時データ
		//========================================================
		float SelectionScore = 0.0f;
		int AssignedShadowIndex = -1;
	};

protected:

	//============================================================
	// このSceneでShadowMapを有効にする
	//
	// targetObject:
	//   ShadowMapの追従対象。
	//   通常はプレイヤーを渡す。
	//
	// 戻り値:
	//   true  = 初期化成功
	//   false = 初期化失敗
	//============================================================
	bool EnableShadowMap(ObjectBase* targetObject = nullptr);

	//============================================================
	// このSceneのShadowMapを無効化する
	//
	// 通常はvnSceneのデストラクタから自動的に呼ばれる。
	//============================================================
	void DisableShadowMap();

	//============================================================
	// ShadowMapの追従対象を変更する
	//============================================================
	void SetShadowTarget(ObjectBase* targetObject)
	{
		mShadowTargetObject =targetObject;
	}

	//============================================================
	// ShadowMap本体を取得
	//
	// 派生Scene固有の設定やデバッグSprite設定に使用する。
	// 未初期化の場合はnullptr。
	//============================================================
	ShadowMap* GetShadowMap()
	{
		if (!mShadowMapInitialized)
		{
			return nullptr;
		}

		return &mShadowMap;
	}

	const ShadowMap* GetShadowMap() const
	{
		if (!mShadowMapInitialized)
		{
			return nullptr;
		}

		return &mShadowMap;
	}
public:
	vnScene();
	virtual ~vnScene();

	virtual bool initialize() = 0;
	virtual void terminate() = 0;

	virtual void execute();

	//通常描画
	virtual void render();
	virtual void render2D();

	//============================================================
	// ShadowMap描画
	//============================================================
	virtual void renderShadow(ShadowMap& shadowMap, UINT cascadeIndex,ShadowCasterFilter filter = ShadowCasterFilter::All);
	virtual void renderShadowPass();
	//============================================================
	// Object Motion Vector描画
	//============================================================
	static void renderMotionVector();
	//============================================================
	// Screen Space SSS Mask描画
	//============================================================
	static void renderSubsurfaceMask(ID3D12PipelineState* rigidPSO, ID3D12PipelineState* skinPSO);
	//============================================================
	// Spot Light ShadowMap描画
	//============================================================
	virtual void renderSpotLightShadow();
	//============================================================
	// Point Light Shadow MapへObjectを描画
	//============================================================
	virtual void renderPointLightShadow();

	static void renderGISurface(ID3D12PipelineState* rigidPSO,ID3D12PipelineState* skinPSO);
	//============================================================
	// ShadowMapが有効か
	//============================================================
	bool IsShadowMapEnabled() const
	{
		return mShadowMapEnabled && mShadowMapInitialized;
	}

	//============================================================
	// Cascade Shadowデバッグ表示
	//============================================================
	void ShowCascadeShadowDebug();

	const wchar_t* GetPBRDebugModeName(PBRDebugMode mode);

	void PBRDebug();

	void SetCascadeShadowDebugVisible(bool visible)
	{
		mShowCascadeShadowDebug =visible;
	}

	bool IsCascadeShadowDebugVisible() const
	{
		return mShowCascadeShadowDebug;
	}

	static void StartHitStop(float time);
	static void UpdateHitStop(float dt);
	static bool IsHitStop();
	static void SetHitStop(bool f);

	// 全3D ObjectのMotion Historyを確定
	static void CommitMotionHistory();


	//登録された2Dオブジェクトの描画優先順位が変更されたので、配列をソートをする必要がある
	static void setUpdateRenderPriority();

	// オブジェクトの登録。nullptr・同一ポインターの重複・満杯はfalse。
	// 重複時は既存の登録を維持するため、呼び出し側で破棄しないこと。
	static bool registerObject(vnSprite* p);

	// 登録を解除してから一度だけdeleteする。nullptr・未登録なら何もしない。
	// 呼び出し側が保持するポインターは自動でnullptrにならない。
	static void deleteObject(vnSprite* p);

	// オブジェクトの登録。nullptr・同一ポインターの重複・満杯はfalse。
	// 重複時は既存の登録を維持するため、呼び出し側で破棄しないこと。
	static bool registerObject(ObjectBase* p);

	// 登録を解除してから一度だけdeleteする。nullptr・未登録なら何もしない。
	// 呼び出し側が保持するポインターは自動でnullptrにならない。
	static void deleteObject(ObjectBase* p);

	//============================================================
	// Point Shadow候補の優先スコアを計算
	//============================================================
	float CalculatePointShadowSelectionScore(const PointLightCandidate& light,const DirectX::XMFLOAT3& cameraPosition) const;

	//============================================================
	// 前フレームもShadow対象だったか
	//============================================================
	bool WasPointShadowSelectedLastFrame(std::uint32_t stableLightID) const;

	//============================================================
	// Point Lightを選択・登録
	//============================================================
	void RegisterPointLights(std::vector<PointLightCandidate>& pointLights,const DirectX::XMFLOAT3& cameraPosition);
};
