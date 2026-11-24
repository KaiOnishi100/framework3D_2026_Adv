#include "../../framework.h"
#include "../../framework/vn_environment.h"

bool vnScene::UpdateRenderPriority = false;
bool vnScene::sHitStop = false;
bool vnScene::mShadowMapEnabled = false;
float vnScene::sHitStopTimer = 0.0f;
ShadowMap vnScene::mShadowMap;
vnSprite* vnScene::pObject2D_Array[vnOBJECT2D_MAX];	//2Dオブジェクトを登録しておく配列
ObjectBase* vnScene::pObject3D_Array[vnOBJECT3D_MAX];	//3Dオブジェクトを登録しておく配列

bool vnScene::EnableShadowMap(ObjectBase* targetObject)
{
	//============================================================
	// ShadowMapの追従対象を保存
	// プレイヤーを渡しておけば
	// Shadow Pass時にプレイヤーのワールド座標を取得する
	//============================================================
	mShadowTargetObject = targetObject;

	//============================================================
	// 既に初期化済みならResourceを作り直さない
	//============================================================
	if (!mShadowMapInitialized)
	{
		const bool initializeResult =mShadowMap.Initialize();

		if (!initializeResult)
		{
			mShadowMapEnabled =false;
			mShadowMapInitialized =false;
			return false;
		}

		mShadowMapInitialized =true;
	}

	//============================================================
	// このSceneではShadowMapを使用する
	//============================================================
	mShadowMapEnabled =true;

	//============================================================
	// 通常描画側のモデルが、
	// このShadowMapのSRVやLightViewProjectionを
	// 取得できるようにManagerへ登録する
	//============================================================
	ShadowMapManager::SetShadowMap(&mShadowMap);

	//============================================================
	// Scene切り替え後の最初のフレームは、
	// 全Cascadeを必ず更新する。
	//============================================================
	mShadowMap.ForceUpdateAllCascades();

	return true;
}

void vnScene::DisableShadowMap()
{
	//============================================================
	// 既に無効なら何もしない
	//============================================================
	if (!mShadowMapEnabled &&!mShadowMapInitialized)
	{
		mShadowTargetObject = nullptr;

		return;
	}

	//============================================================
	// 通常描画側が解放済みShadowMapへアクセスしないように、
	// Managerの参照を先に解除する。
	//============================================================
	ShadowMapManager::ClearShadowMap(&mShadowMap);

	//============================================================
	// ShadowMapのGPU Resourceを解放
	//============================================================
	if (mShadowMapInitialized)
	{
		mShadowMap.Terminate();

		mShadowMapInitialized = false;
	}

	mShadowMapEnabled = false;

	mShadowTargetObject = nullptr;
}


//コンストラクタ
vnScene::vnScene()
{
	for (int i = 0; i < vnOBJECT3D_MAX; i++)
	{
		pObject3D_Array[i] = NULL;
	}
	for (int i = 0; i < vnOBJECT2D_MAX; i++)
	{
		pObject2D_Array[i] = NULL;
	}
	UpdateRenderPriority = false;

	//============================================================
	// ShadowMapは、派生SceneがEnableShadowMap()を
	// 呼ぶまで初期化しない。
	//============================================================
	mShadowMapEnabled = false;

	mShadowMapInitialized = false;

	mShadowTargetObject = nullptr;

	mShowCascadeShadowDebug = false;
}

//デストラクタ
vnScene::~vnScene()
{
	//============================================================
	// ShadowMapManagerから現在のShadowMapを解除し
	// GPU Resourceを解放する
	//============================================================
	DisableShadowMap();

	for (int i = 0; i < vnOBJECT3D_MAX; i++)
	{
		// 通常の削除と同じ経路を使い、登録解除を済ませてから破棄する
		// 子オブジェクトのデストラクターが他の登録を削除しても
		// 次の走査では更新済みの配列を参照できる
		deleteObject(pObject3D_Array[i]);
	}
	for (int i = 0; i < vnOBJECT2D_MAX; i++)
	{
		deleteObject(pObject2D_Array[i]);
	}
	UpdateRenderPriority = false;
}

//処理関数
void vnScene::execute()
{
	////3Dオブジェクトのexecuteを実行
	//for (int i = 0; i < vnOBJECT3D_MAX; i++)
	//{
	//	if (pObject3D_Array[i] == NULL)continue;
	//	if (pObject3D_Array[i]->isExecuteEnable() == false)continue;
	//	pObject3D_Array[i]->execute();
	//}
	//3DオブジェクトのpostExecuteを実行
	for (int i = 0; i < vnOBJECT3D_MAX; i++)
	{
		if (pObject3D_Array[i] == NULL)continue;
		if (pObject3D_Array[i]->isExecuteEnable() == false)continue;
		pObject3D_Array[i]->postExecute();
	}

	for (int i = 0; i < vnOBJECT3D_MAX; i++)
	{
		if (!pObject3D_Array[i]) continue;
		if (!pObject3D_Array[i]->isRenderEnable()) continue;

		// 各オブジェクトが必要な処理をフックで実行する
		pObject3D_Array[i]->executeEffects();
	}

	// プレイヤーのエフェクトのみ最前列に表示
	if (Player::GetInstance() && Player::GetInstance()->isRenderEnable())
	{
		Player::GetInstance()->executeEffects();
	}

	//2Dオブジェクトのexecuteを実行
	for (int i = 0; i < vnOBJECT2D_MAX; i++)
	{
		if (pObject2D_Array[i] == NULL)continue;
		if (pObject2D_Array[i]->isExecuteEnable() == false)continue;
		pObject2D_Array[i]->execute();
	}

#ifdef _DEBUG

	//========================================================
	// F8で表示・非表示を切り替える
	//========================================================
	if (vnKeyboard::trg(DIK_F12))
	{
		mShowCascadeShadowDebug = !mShowCascadeShadowDebug;
	}

	//========================================================
	// デバッグ情報表示
	//========================================================
	if (mShowCascadeShadowDebug)
	{
		ShowCascadeShadowDebug();
	}

	if (vnKeyboard::trg(DIK_P))
	{
		Direct3D::cyclePBRDebugMode();
	}

	PBRDebug();
#endif // _DEBUG

}

void vnScene::render()
{
    Player* player =
        Player::GetInstance();

    //============================================================
    // 通常3D描画
    //============================================================
    for (int i = 0; i < vnOBJECT3D_MAX; ++i)
    {
        ObjectBase* object =
            pObject3D_Array[i];

        if (object == nullptr)
        {
            continue;
        }

        if (!object->isRenderEnable())
        {
            continue;
        }

        object->render();
    }

    //============================================================
    // プレイヤー以外のエフェクト
    //============================================================
    for (int i = 0; i < vnOBJECT3D_MAX; ++i)
    {
        ObjectBase* object =
            pObject3D_Array[i];

        if (object == nullptr)
        {
            continue;
        }

        if (!object->isRenderEnable())
        {
            continue;
        }

        // プレイヤーは最後に描画するため、ここでは除外
        if (object == player)
        {
            continue;
        }

        object->renderEffects();
    }

    //============================================================
    // プレイヤーエフェクトは最後に1回だけ描画
    //============================================================
    if (player != nullptr &&
        player->isRenderEnable())
    {
        player->renderEffects();
    }
}

void vnScene::render2D()
{
	if (UpdateRenderPriority == true)	//ソートの必要がある時のみソートする
	{
		//描画優先順位に従って配列をソート
		for (int i = 0; i < vnOBJECT2D_MAX - 1; i++)
		{
			if (pObject2D_Array[i] == NULL)continue;
			for (int j = i + 1; j < vnOBJECT2D_MAX; j++)
			{
				if (pObject2D_Array[j] == NULL)continue;
				if (pObject2D_Array[i]->getRenderPriority() > pObject2D_Array[j]->getRenderPriority())
				{
					vnSprite* tmp = pObject2D_Array[i];
					pObject2D_Array[i] = pObject2D_Array[j];
					pObject2D_Array[j] = tmp;
				}
			}
		}
		UpdateRenderPriority = false;	//ソートが完了したら次にソートが必要になるまでソートしないようにする
	}

	//2Dオブジェクトのrenderを実行
	for (int i = 0; i < vnOBJECT2D_MAX; i++)
	{
		if (pObject2D_Array[i] == NULL)continue;
		if (pObject2D_Array[i]->isRenderEnable() == false)continue;
		pObject2D_Array[i]->render();
	}
}

void vnScene::renderShadow(ShadowMap& shadowMap, UINT cascadeIndex, ShadowCasterFilter filter)
{
	//============================================================
	// Cascade番号の範囲確認
	//============================================================
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	//============================================================
	// Sceneへ登録されている全3Dオブジェクトを確認
	//============================================================
	for (int objectIndex = 0; objectIndex < vnOBJECT3D_MAX; ++objectIndex)
	{
		ObjectBase* object = pObject3D_Array[objectIndex];

		//========================================================
		// オブジェクト未登録
		//========================================================
		if (object == nullptr)
		{
			continue;
		}

		//========================================================
		// 描画自体が無効なオブジェクト
		//========================================================
		if (!object->isRenderEnable())
		{
			continue;
		}

		//========================================================
		// ShadowMapへ影を描画しないオブジェクト
		//========================================================
		if (!object->isCastShadow())
		{
			continue;
		}

		//========================================================
		// Shadow Mobilityによる絞り込み
		// All: StaticとDynamicを両方通す
		// StaticOnly: Staticだけ通す
		// DynamicOnly: Dynamicだけ通す
		//========================================================
		bool matchesFilter = false;

		switch (filter)
		{
		case ShadowCasterFilter::All:
			//====================================================
			// Mobilityに関係なく描画対象
			//====================================================
			matchesFilter =
				true;
			break;

		case ShadowCasterFilter::StaticOnly:
			//====================================================
			// Static設定のオブジェクトだけ描画対象
			//====================================================
			matchesFilter = object->IsStaticShadowCaster();
			break;

		case ShadowCasterFilter::DynamicOnly:
			//====================================================
			// Dynamic設定のオブジェクトだけ描画対象
			//====================================================
			matchesFilter = object->IsDynamicShadowCaster();
			break;

		default:
			//====================================================
			// 想定外の値
			// 安全のため描画しない
			//====================================================
			assert(false);

			matchesFilter = false;
			break;
		}

		//========================================================
		// 今回のShadow Pass対象外
		//========================================================
		if (!matchesFilter)
		{
			continue;
		}

		//========================================================
		// オブジェクトのワールド空間Bounding Sphereを取得
		//========================================================
		XMVECTOR worldCenter = XMVectorZero();

		float worldRadius = 0.0f;

		const bool hasBounds =
			object->GetShadowBoundingSphere(
				worldCenter,
				worldRadius
			);

		//========================================================
		// Boundsを取得できるオブジェクトだけ
		// Cascade範囲との交差判定を行う
		// Boundsを持っていない場合は
		// 誤って影を消さないように描画する
		//========================================================
		if (
			hasBounds &&
			!shadowMap.IntersectsCascadeSphere(
				cascadeIndex,
				worldCenter,
				worldRadius
			)
			)
		{
			continue;
		}

		//========================================================
		// ShadowMap用のWorld・LightViewProjectionを設定
		//========================================================
		shadowMap.SetObjectMatrix(*object->getWorld());

		//========================================================
		// Depth描画
		//========================================================
		object->renderShadow();
	}
}

//============================================================
// Cascade Shadow Map描画
// ShadowMapを有効化しているSceneだけ実行する
//============================================================
void vnScene::renderShadowPass()
{
	//============================================================
	// このSceneでShadowMapを使用しない場合
	//============================================================
	if (!mShadowMapEnabled)
	{
		return;
	}

	//============================================================
	// ShadowMapがまだ初期化されていない場合
	//============================================================
	if (!mShadowMapInitialized)
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	assert(commandList != nullptr);

	if (mShadowMapEnabled && mShadowMapInitialized)
	{
		//============================================================
		// 前フレームのGPU Timestamp結果を取得
		//============================================================
		mShadowMap.UpdateTimestampResults();

		//============================================================
		// 現在フレームで更新するCascadeを決定
		// Cascade 2を2フレームに1回更新する場合などの
		// 更新判定が行われる
		//============================================================
		mShadowMap.PrepareCascadeUpdates();

		//============================================================
		// ShadowMapの基準座標を取得
		//============================================================
		XMVECTOR shadowTarget = XMVectorZero();

		if (mShadowTargetObject != nullptr)
		{
			//========================================================
			// 親子関係を含んだワールド座標を取得
			//========================================================
			mShadowTargetObject->getWorldPosition(&shadowTarget);
		}

		//============================================================
		// 更新対象CascadeのLightViewProjectionを更新
		// 更新をスキップしたCascadeの行列は
		// 前回値がそのまま維持される
		//============================================================
		mShadowMap.UpdateLightMatrix(shadowTarget);

		//============================================================
		// Cascade数だけShadowMapを描画する
		//============================================================
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			//============================================================
			// Cascade自体が更新対象外
			// LightVP、Static ShadowMap、Dynamic ShadowMapを
			// すべて前回状態で再利用する
			//============================================================
			if (!mShadowMap.ShouldUpdateCascade(cascadeIndex))
			{
				mShadowMap.MarkCascadeSkipped(cascadeIndex);

				continue;
			}

			//============================================================
			// Static Shadow Pass
			// 以下の場合のみ描画する:
			// ・初回
			// ・Static Caster変更
			// ・LightVP変更
			//============================================================
			if (mShadowMap.ShouldUpdateStaticCascade(cascadeIndex))
			{
				mShadowMap.BeginRender(
					cascadeIndex,
					ShadowMapLayer::Static
				);

				renderShadow(
					mShadowMap,
					cascadeIndex,
					ShadowCasterFilter::StaticOnly
				);

				mShadowMap.EndRender(
					cascadeIndex,
					ShadowMapLayer::Static
				);

				//========================================================
				// Static ShadowMapと現在のLightVPが一致したことを記録
				//
				// EndRender後に呼ぶ。
				//========================================================
				mShadowMap.CommitStaticCascadeUpdate(cascadeIndex);
			}
			else
			{
				//========================================================
				// Static ShadowMapは前回結果を再利用
				// BeginRenderを呼ばないためDepth Clearも発生しない
				//========================================================
				mShadowMap.MarkLayerCascadeSkipped(
					cascadeIndex,
					ShadowMapLayer::Static
				);
			}

			//============================================================
			// Dynamic Shadow Pass
			// Dynamic Casterは移動・アニメーションするため
			// Cascade更新フレームでは毎回再描画する
			//============================================================
			mShadowMap.BeginRender(
				cascadeIndex,
				ShadowMapLayer::Dynamic
			);

			renderShadow(
				mShadowMap,
				cascadeIndex,
				ShadowCasterFilter::DynamicOnly
			);

			mShadowMap.EndRender(
				cascadeIndex,
				ShadowMapLayer::Dynamic
			);
		}
		//============================================================
		// 1フレーム分のShadow処理完了
		//============================================================
		mShadowMap.FinishShadowFrame();

	}

	PointLightShadowManager::BeginShadowDrawPass();
	SpotLightShadowManager::BeginShadowDrawPass();

	const UINT spotShadowLightCount = SpotLightShadowManager::GetLightCount();

	for (UINT shadowIndex = 0u; shadowIndex < spotShadowLightCount; ++shadowIndex)
	{
		if (!SpotLightShadowManager::BeginRender(shadowIndex)) continue;
		vnScene::renderSpotLightShadow();
		SpotLightShadowManager::EndRender();
	}

	//============================================================
	// Point Light Shadow Pass
	// 登録されたPoint Shadow Lightごとに6面描画する
	//============================================================
	const UINT pointShadowLightCount =PointLightShadowManager::GetLightCount();
	for (UINT shadowIndex = 0u;shadowIndex <pointShadowLightCount;++shadowIndex)
	{
		for (UINT faceIndex = 0u;faceIndex <PointLightShadowManager::FaceCount;++faceIndex)
		{
			//========================================================
			// 指定Point Light・Faceへの描画開始
			//========================================================
			if (!PointLightShadowManager::BeginRender(shadowIndex,faceIndex))
			{
				continue;
			}
			//========================================================
			// Scene内のShadow Casterを描画
			//========================================================
			renderPointLightShadow();
			//========================================================
			// 描画したSliceをSRV状態へ戻す
			//========================================================
			PointLightShadowManager::EndRender();
		}
	}
	assert(PointLightShadowManager::AreAllSlicesShaderReadable());
}

void vnScene::renderMotionVector()
{
	for (int objectIndex = 0; objectIndex < vnOBJECT3D_MAX; ++objectIndex)
	{
		ObjectBase* object = pObject3D_Array[objectIndex];


		if (object == nullptr)
		{
			continue;
		}


		if (!object->isRenderEnable())
		{
			continue;
		}

		//========================================================
		// vnModelならRigid Motionを描画
		//========================================================
		object->renderMotionVector();
	}
}

void vnScene::renderSubsurfaceMask(ID3D12PipelineState* rigidPSO, ID3D12PipelineState* skinPSO)
{
	//========================================================
	// Sceneへ登録されている3D Objectを走査
	//========================================================
	for (int i = 0; i < vnOBJECT3D_MAX; ++i)
	{
		ObjectBase* object = pObject3D_Array[i];


		if (object == nullptr)
		{
			continue;
		}


		//====================================================
		// 通常描画されていないObjectはMaskにも入れない
		//====================================================
		if (!object->isRenderEnable())
		{
			continue;
		}


		//====================================================
		// Skin / RigidでPSO切り替え
		//====================================================
		ID3D12PipelineState* pipelineState = object->getRenderFlag(ObjectBase::eRenderFlag::Skin) ? skinPSO : rigidPSO;


		//====================================================
		// vnModelなら実際に描画
		//====================================================
		object->renderSubsurfaceMask(pipelineState);
	}
}

void vnScene::renderSpotLightShadow()
{
	//============================================================
	// 現在描画中のSpot Shadow Slice番号
	//============================================================
	const UINT shadowIndex = SpotLightShadowManager::GetRenderingLightIndex();

	//============================================================
	// 無効なShadow Indexなら描画しない
	//============================================================
	if (shadowIndex ==SpotLightShadowManager::InvalidShadowIndex)
	{
		return;
	}

	for (int objectIndex = 0; objectIndex < vnOBJECT3D_MAX; ++objectIndex)
	{
		ObjectBase* object = pObject3D_Array[objectIndex];

		//========================================================
		// 無効なObject
		//========================================================
		if (object == nullptr)
		{
			continue;
		}

		//========================================================
		// Shadowを落とさないObject
		//========================================================
		if (!object->isCastShadow())
		{
			continue;
		}

		if (SpotLightShadowManager::ShouldSkipCaster(shadowIndex, object))
		{
			continue;
		}

		//========================================================
		// World空間のBounding Sphereを取得
		//========================================================
		XMVECTOR worldBoundsCenter =XMVectorZero();

		float worldBoundsRadius =0.0f;

		const bool hasShadowBounds =
			object->GetShadowBoundingSphere(
				worldBoundsCenter,
				worldBoundsRadius
			);

		//========================================================
		// Boundsを取得できたObjectだけFrustum Cullingする
		// BoundsがないObjectは安全側として描画する
		//========================================================
		if (hasShadowBounds)
		{
			const bool insideShadowFrustum =
				SpotLightShadowManager::IsSphereInsideShadowFrustum(
					shadowIndex,
					worldBoundsCenter,
					worldBoundsRadius
				);

			//====================================================
			// Spot LightのShadow Frustumから完全に外れている
			//====================================================
			if (!insideShadowFrustum)
			{
				continue;
			}
		}

		//========================================================
		// Spot Shadow用Object行列を設定
		//========================================================
		SpotLightShadowManager::SetObjectMatrix(*object->getWorld());

		//========================================================
		// Shadow Mapへ描画
		//========================================================
		object->renderShadow();
	}
}

void vnScene::renderPointLightShadow()
{
	//============================================================
	// 現在描画中のPoint Shadow Light番号
	//============================================================
	const UINT shadowIndex = PointLightShadowManager::GetRenderingLightIndex();

	//============================================================
	// 現在描画中のCube Face番号
	//============================================================
	const UINT faceIndex = PointLightShadowManager::GetRenderingFaceIndex();

	//============================================================
	// 無効な描画状態では処理しない
	//============================================================
	if (shadowIndex == PointLightShadowManager::InvalidIndex ||
		faceIndex == PointLightShadowManager::InvalidIndex)
	{
		return;
	}

	UINT renderedObjectCount = 0u;
	UINT culledObjectCount = 0u;

	for (int objectIndex = 0; objectIndex < vnOBJECT3D_MAX; ++objectIndex)
	{
		ObjectBase* object = pObject3D_Array[objectIndex];

		if (object == nullptr)
		{
			continue;
		}

		//========================================================
		// Shadowを落とさないObject
		//========================================================
		if (!object->isCastShadow())
		{
			continue;
		}

		// 現在描画している PointLight が指定したモデルだけをスキップする
		if (PointLightShadowManager::ShouldSkipCaster(shadowIndex, object))
		{
			continue;
		}

		//========================================================
		// vnObjectの仮想関数からWorld空間Boundsを取得
		// 実体がvnModelの場合は
		// vnModel::GetShadowBoundingSphere()が呼ばれる
		//========================================================
		XMVECTOR worldBoundsCenter = XMVectorZero();

		float worldBoundsRadius = 0.0f;

		const bool hasShadowBounds =
			object->GetShadowBoundingSphere(
				worldBoundsCenter,
				worldBoundsRadius
			);

		//========================================================
		// Boundsを持つObjectは、Point Lightの影響範囲を先に確認する
		// 範囲外ならObject Bufferの枠を消費せずDrawも実行しない
		// BoundsがないObjectは安全側として下の共通描画処理へ進める
		//========================================================
		if (hasShadowBounds)
		{
			const bool insideLightRange =
				PointLightShadowManager::IsSphereInsideLightRange(
					shadowIndex,
					worldBoundsCenter,
					worldBoundsRadius
				);

			//========================================================
			// Point Lightの影響範囲から完全に離れているObjectだけ
			// Shadow Passから除外する
			//========================================================
			if (!insideLightRange)
			{
				++culledObjectCount;
				continue;
			}
		}

		//========================================================
		// 範囲内・BoundsなしのObjectを共通の経路で描画する
		// 現在のCube Faceにつき、行列設定とrenderShadow()は各1回
		//========================================================
		if (!PointLightShadowManager::SetObjectMatrix(*object->getWorld()))
		{
			// Object Bufferの容量不足時は、行列未設定のまま描画しない。
			continue;
		}

		//========================================================
		// 既存のShadow描画処理を使用
		//========================================================
		object->renderShadow();

		++renderedObjectCount;
	}

}

void vnScene::ShowCascadeShadowDebug()
{
	//============================================================
	// ShadowMap未使用時は表示しない
	//============================================================
	if (!IsShadowMapEnabled())
	{
		return;
	}

	//============================================================
	// F8で非表示にされている
	//============================================================
	if (!mShowCascadeShadowDebug)
	{
		return;
	}

	const float debugX =20.0f;

	const float debugY =250.0f;

	const float lineHeight =22.0f;

	UINT lineIndex =0;

	vnFont::setColor(0xffff0000);

	//============================================================
	// タイトル
	//============================================================
	vnFont::print(
		debugX,
		debugY +
		lineHeight *
		static_cast<float>(lineIndex),
		L"[Cascade Shadow Debug] F8: ON/OFF"
	);

	++lineIndex;

	//============================================================
	// 各Cascadeを表示
	//============================================================
	for (UINT cascadeIndex = 0;cascadeIndex < SHADOW_CASCADE_COUNT;++cascadeIndex)
	{
		const UINT interval =
			mShadowMap.GetCascadeUpdateInterval(
				cascadeIndex
			);

		//========================================================
		// Cascade見出し
		//========================================================
		vnFont::print(
			debugX,
			debugY +
			lineHeight *
			static_cast<float>(lineIndex),
			L"Cascade %u / Interval %u",
			cascadeIndex,
			interval
		);

		++lineIndex;

		//========================================================
		// Static Layer
		//========================================================
		{
			const bool updated =
				mShadowMap.WasCascadeUpdatedThisFrame(
					cascadeIndex,
					ShadowMapLayer::Static
				);

			const UINT objectCount =
				mShadowMap.GetLayerCascadeRenderedObjectCount(
					cascadeIndex,
					ShadowMapLayer::Static
				);

			const UINT drawCallCount =
				mShadowMap.GetLayerCascadeDrawCallCount(
					cascadeIndex,
					ShadowMapLayer::Static
				);

			const double gpuTimeMs =
				mShadowMap.GetLayerCascadeGpuTimeMs(
					cascadeIndex,
					ShadowMapLayer::Static
				);

			const wchar_t* state =
				updated? L"Updated": L"Cached ";

			vnFont::print(
				debugX + 20.0f,
				debugY +
				lineHeight *
				static_cast<float>(lineIndex),
				L"Static  : %ls / Objects %u / DrawCalls %u / GPU %.3f ms",
				state,
				objectCount,
				drawCallCount,
				gpuTimeMs
			);

			++lineIndex;
		}

		//========================================================
		// Dynamic Layer
		//========================================================
		{
			const bool updated =
				mShadowMap.WasCascadeUpdatedThisFrame(
					cascadeIndex,
					ShadowMapLayer::Dynamic
				);

			const UINT objectCount =
				mShadowMap.GetLayerCascadeRenderedObjectCount(
					cascadeIndex,
					ShadowMapLayer::Dynamic
				);

			const UINT drawCallCount =
				mShadowMap.GetLayerCascadeDrawCallCount(
					cascadeIndex,
					ShadowMapLayer::Dynamic
				);

			const double gpuTimeMs =
				mShadowMap.GetLayerCascadeGpuTimeMs(
					cascadeIndex,
					ShadowMapLayer::Dynamic
				);

			const wchar_t* state =
				updated? L"Updated": L"Cached ";

			vnFont::print(
				debugX + 20.0f,
				debugY +
				lineHeight *
				static_cast<float>(lineIndex),
				L"Dynamic : %ls / Objects %u / DrawCalls %u / GPU %.3f ms",
				state,
				objectCount,
				drawCallCount,
				gpuTimeMs
			);

			++lineIndex;
		}
	}

	//============================================================
	// 全Layer・全Cascadeの合計
	//============================================================
	vnFont::print(
		debugX,
		debugY +
		lineHeight *
		static_cast<float>(lineIndex),
		L"Total : Objects %u / DrawCalls %u / GPU %.3f ms",
		mShadowMap.GetTotalRenderedObjectCount(),
		mShadowMap.GetTotalDrawCallCount(),
		mShadowMap.GetTotalGpuTimeMs()
	);




	//============================================================
	// フォント色を元へ戻す
	//============================================================
	vnFont::setColor(0xffffffff);
}

void vnScene::renderGISurface(ID3D12PipelineState* rigidPSO,ID3D12PipelineState* skinPSO)
{
	//============================================================
	// Scene内Objectを全走査
	//============================================================
	for (ObjectBase* object : pObject3D_Array)
	{
		if (object == nullptr)
		{
			continue;
		}

		if (!object->isRenderEnable())
		{
			continue;
		}

		object->renderGISurface(rigidPSO, skinPSO);
	}
}

const wchar_t* vnScene::GetPBRDebugModeName(PBRDebugMode mode)
{
	switch (mode)
	{
	case PBRDebugMode::Final:
		return L"Final";

	case PBRDebugMode::BaseColor:
		return L"BaseColor";

	case PBRDebugMode::WorldNormal:
		return L"World Normal";

	case PBRDebugMode::Metallic:
		return L"Metallic";

	case PBRDebugMode::Roughness:
		return L"Roughness";

	case PBRDebugMode::AmbientOcclusion:
		return L"Ambient Occlusion";

	case PBRDebugMode::Emissive:
		return L"Emissive";

	case PBRDebugMode::DirectDiffuse:
		return L"Direct Diffuse";

	case PBRDebugMode::DirectSpecular:
		return L"Direct Specular";

	case PBRDebugMode::DiffuseIBL:
		return L"Diffuse IBL";

	case PBRDebugMode::SpecularIBL:
		return L"Specular IBL";

	case PBRDebugMode::Shadow:
		return L"Shadow";

	case PBRDebugMode::NdotL:
		return L"NdotL";

	case PBRDebugMode::F0:
		return L"F0";

	case PBRDebugMode::BRDFLUT:
		return L"BRDF LUT";

	case PBRDebugMode::PointLights:
		return L"PointLights";

	case PBRDebugMode::SpotLights:
		return L"SpotLights";

	case PBRDebugMode::LocalLights:
		return L"LocalLights";

	default:
		return L"Unknown";
	}
}

void vnScene::PBRDebug()
{
	vnFont::print(20.0f, 980.0f, L" PBRDebugMode %s",GetPBRDebugModeName(Direct3D::getPBRDebugMode()));
}

void vnScene::StartHitStop(float time)
{
	sHitStop = true;

	// マイナスや0が来ても壊れないように保険
	sHitStopTimer = (time > 0.0f) ? time : 0.0f;

	// 0秒指定なら即解除
	if (sHitStopTimer <= 0.0f)
	{
		sHitStop = false;
	}
}

void vnScene::UpdateHitStop(float dt)
{
	// ------------------------------
	// ヒットストップ中だけタイマーを減らす
	// ------------------------------
	if (!sHitStop) return;

	sHitStopTimer -= dt;

	// 0以下になったら終了
	if (sHitStopTimer <= 0.0f)
	{
		sHitStopTimer = 0.0f;
		sHitStop = false;
	}
}

bool vnScene::IsHitStop()
{
	return sHitStop;
}

void vnScene::SetHitStop(bool f)
{
	sHitStop = f;
	if (!sHitStop)
	{
		sHitStopTimer = 0.0f;
	}
}

//============================================================
// 全ObjectのMotion History確定
//============================================================
void vnScene::CommitMotionHistory()
{
	for (int objectIndex = 0;objectIndex < vnOBJECT3D_MAX;++objectIndex)
	{
		ObjectBase* object = pObject3D_Array[objectIndex];


		if (object == nullptr)
		{
			continue;
		}
		object-> CommitMotionHistory();
	}
}


//登録された2Dオブジェクトの描画優先順位が変更されたので、配列をソートをする必要がある
void vnScene::setUpdateRenderPriority()
{
	UpdateRenderPriority = true;
}

//オブジェクトの登録(配列の空きに入れる)
bool vnScene::registerObject(vnSprite *p)
{
	if (p == nullptr)
	{
		return false;
	}

	// 空きが前方にあっても後方に同じポインターが登録済みの
	// 可能性がある 全体の重複確認が終わるまで配列へ書き込まない
	int emptyIndex = -1;
	for (int i = 0; i < vnOBJECT2D_MAX; i++)
	{
		if (pObject2D_Array[i] == p)
		{
			// 既存の登録と所有権を維持し二重登録だけを拒否する
			return false;
		}
		if (pObject2D_Array[i] == nullptr && emptyIndex < 0)
		{
			emptyIndex = i;
		}
	}
	if (emptyIndex < 0)
	{
		return false;
	}

	pObject2D_Array[emptyIndex] = p;
	return true;
}

//オブジェクトの破棄(配列から削除/オブジェクト自体をdeleteする)
void vnScene::deleteObject(vnSprite *p)
{
	if (p == nullptr)
	{
		return;
	}

	for (int i = 0; i < vnOBJECT2D_MAX; i++)
	{
		if (pObject2D_Array[i] == p)
		{
			// デストラクターからシーンへアクセスされる前に登録を外す
			// 同じポインターはregisterObject()で一度だけ登録される
			pObject2D_Array[i] = nullptr;
			delete p;
			return;
		}
	}
}

//オブジェクトの登録(配列の空きに入れる)
bool vnScene::registerObject(ObjectBase* p)
{
	if (p == nullptr)
	{
		return false;
	}

	// 最初の空きを記録しつつ、配列全体で重複を確認する
	// 空きを見つけた時点で登録すると、後方の同一ポインターを見落とす
	int emptyIndex = -1;
	for (int i = 0; i < vnOBJECT3D_MAX; i++)
	{
		if (pObject3D_Array[i] == p)
		{
			return false;
		}
		if (pObject3D_Array[i] == nullptr && emptyIndex < 0)
		{
			emptyIndex = i;
		}
	}
	if (emptyIndex < 0)
	{
		return false;
	}

	pObject3D_Array[emptyIndex] = p;

	//========================================================
	// 実際にStatic Shadow Casterを追加した場合だけ影を更新する
	// 重複登録や配列満杯ではキャッシュを無効化しない
	//========================================================
	if (mShadowMapEnabled &&
		p->isCastShadow() &&
		p->IsStaticShadowCaster())
	{
		mShadowMap.InvalidateStaticShadowCache();
	}
	return true;
}

//オブジェクトの破棄(配列から削除/オブジェクト自体をdeleteする)
void vnScene::deleteObject(ObjectBase* p)
{
	if (p == nullptr)
	{
		return;
	}

	for (int i = 0; i < vnOBJECT3D_MAX; i++)
	{
		if (pObject3D_Array[i] == p)
		{
			//========================================================
			// 登録が見つかった場合だけ生存中のオブジェクトから
			// 削除後に必要になる情報を保存する
			// 未登録ポインターにはアクセスせず所有権も引き取らない
			//========================================================
			const bool removedStaticCaster =
				p->isCastShadow() && p->IsStaticShadowCaster();

			// デストラクター内から再度削除が呼ばれても
			// 同じオブジェクトが見つからないよう先に登録を外す
			pObject3D_Array[i] = nullptr;
			delete p;

			//========================================================
			// 古いStaticオブジェクトの影を消すため再描画
			//========================================================
			if (mShadowMapEnabled && removedStaticCaster)
			{
				mShadowMap.InvalidateStaticShadowCache();
			}

			// 後続要素の走査で解放済みのpを参照しない。
			return;
		}
	}
}

float vnScene::CalculatePointShadowSelectionScore(const PointLightCandidate& light, const DirectX::XMFLOAT3& cameraPosition) const
{	
	return false;
}

bool vnScene::WasPointShadowSelectedLastFrame(std::uint32_t stableLightID) const
{

	return false;
}

void vnScene::RegisterPointLights(std::vector<PointLightCandidate>& pointLights, const DirectX::XMFLOAT3& cameraPosition)
{
}
