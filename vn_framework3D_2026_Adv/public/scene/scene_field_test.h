#pragma once

#define FENCE_NUM	(32)		//フェンスモデルの数

#define FENCE_RADIUS (37.5f)	//フェンスを配置する円周の半径
#define FIELD_RADIUS (36.5f)	//プレイヤーが移動できる円周の半径

class SceneFieldTest : public vnScene
{
private:
	ModelBase* pPlayer;

	ModelBase* pTestModel;
	SkeletalModel* pSKTestModel;
	SkeletalModel* pSKTestPEEPERModel;
	SkeletalModel* pSKAnimTestModel;
	SkeletalModel* pSKRealHumanModel;
	SkeletalAnimationController* pSKAnimController = nullptr;

	ModelBase* pCube;
	ModelBase* pIronCube;

	ModelBase* pFence[FENCE_NUM];

	ModelBase* pGround;

	ModelBase* pSphere;

	Boss* pNPCBoss;
	vnCharacter* pNPC;
	ModelBase* pDebugFan;

	vnBillboard* pBillboard;

	ModelBase* pCrown;

	Emitter* pEmitter;
	Emitter* pEmitterNPC;

	vnMotionData* pMotionIdle;
	vnMotionData* pMotionWalk;

	//vnSprite* mDebugSprite = nullptr;

	EffectManager mEffectManager;

	//フレーム依存をなくすためのタイマー(ゲーム時間計測用)
	std::chrono::steady_clock::time_point prevTime;

	//極座標の情報
	float radius;	//半径
	float theta;	//角度(平面角 / 経度)
	float phi;		//角度(仰角 / 緯度)


public:
	//初期化
	bool initialize();
	//終了
	void terminate();

	//処理
	void execute();
	//描画
	void render();

	void DrawCascadeFrustum(const ShadowMap& shadowMap, UINT cascadeIndex);
};
