#pragma once

#include "../framework/vn_modelData.h"
#include "../framework/DDS_Loader.h"

//============================================================
// 通常3Dマテリアル用Descriptor Heapの配置
// ※RootSignatureのDescriptor Tableと完全に一致させる必要がある
//============================================================
namespace MaterialDescriptor
{
	//========================================================
	// 通常テクスチャ
	//========================================================
	static constexpr UINT Diffuse = 0;
	//========================================================
	// Static ShadowMap
	//========================================================
	static constexpr UINT StaticShadow0 = 1;
	static constexpr UINT StaticShadow1 = 2;
	static constexpr UINT StaticShadow2 = 3;
	//========================================================
	// マテリアル定数バッファ
	//========================================================
	static constexpr UINT Material = 4;
	//========================================================
	// Dynamic ShadowMap
	//========================================================
	static constexpr UINT DynamicShadow0 = 5;
	static constexpr UINT DynamicShadow1 = 6;
	static constexpr UINT DynamicShadow2 = 7;
	//========================================================
	// Normal Map
	//========================================================
	static constexpr UINT Normal = 8;
	//========================================================
	// ORM Texture
	//========================================================
	static constexpr UINT ORM = 9;
	//========================================================
	// t9 : Environment CubeMap
	// Skyboxと同じCubeMapを、
	// PBRの環境鏡面反射にも使用する
	//========================================================
	static constexpr UINT Environment = 10;
	//========================================================
	// t10 : GGX Prefiltered Environment Map
	//========================================================
	static constexpr UINT PrefilteredEnvironment = 11;
	//========================================================
	// t11 : Diffuse Irradiance CubeMap
	//========================================================
	static constexpr UINT IrradianceEnvironment = 12;
	//========================================================
	// t12 : BRDF LUT
	//========================================================
	static constexpr UINT BRDFLUT = 13;
	//========================================================
	// t13 : Local Light Buffer
	//========================================================
	static constexpr UINT LocalLightBuffer = 14;
	//========================================================
	// t14 : Spot Light Shadow Map
	//========================================================
	static constexpr UINT SpotLightShadow = 15;
	//====================================================
	// t15 : Point Light Shadow Texture2DArray
	//====================================================
	static constexpr UINT PointLightShadow = 16;
	//========================================================
	// t16 : SSS Thickness Map
	//========================================================
	static constexpr UINT Thickness = 17;
	//========================================================
	// t17 : Emissive Map
	//========================================================
	static constexpr UINT Emissive = 18;
	//========================================================
	// t18 : SSS Skin Mask
	//========================================================
	static constexpr UINT SkinMask = 19;
	//========================================================
	// Descriptor総数
	//========================================================
	static constexpr UINT Count = 20;
}

//============================================================
// Shadow Casterの描画対象フィルター
// ShadowMobility: オブジェクト自身がStaticかDynamicかを表す
// ShadowCasterFilter: 今回のShadow Passで何を描画するかを表す
//============================================================
enum class ShadowCasterFilter
{
	//========================================================
	// StaticとDynamicの両方を描画する
	//
	// 現在の通常Shadow Passや、
	// 従来と同じ動作を維持したい場合に使用する
	//========================================================
	All,

	//========================================================
	// Static Shadow Casterだけ描画する
	//========================================================
	StaticOnly,

	//========================================================
	// Dynamic Shadow Casterだけ描画する
	//========================================================
	DynamicOnly
};

//============================================================
// Shadow Casterの更新種別
//============================================================
enum class ShadowMobility
{
	//========================================================
	// 毎回描画する動的オブジェクト
	// プレイヤー、敵、動くギミックなど
	//========================================================
	Dynamic,

	//========================================================
	// 基本的に移動しない静的オブジェクト
	// 建物、岩、フェンスなど
	//========================================================
	Static
};


class ObjectBase
{
public:
	//レンダーフラグ
	enum eRenderFlag
	{
		Transparent = 0x00000001,	//半透明の描画
		Lighting = 0x00000002,	//ライティング
		DoubleSided = 0x00000004,	//両面描画
		Addition = 0x00000008,	//加算合成
		ZWrite = 0x00000010,	//深度書き込み無効
		Skin = 0x00000020,	//スキニング頂点変換
		Last = 0x00000040,
		MaxNum = Last,
	};

protected:
	//移動値
	XMVECTOR	Position;

	//回転値
	XMVECTOR	Rotation;

	//拡大値
	XMVECTOR	Scale;

	//クオータニオン
	XMVECTOR	Quaternion;

	//変換行列
	XMMATRIX	Local;
	XMMATRIX	World;

	//============================================================
	// 前フレームのWorld Matrix
	// Motion Vector / TAAなどの時間方向処理で使用する
	// Current World: 今フレームのオブジェクト位置
	// Previous World: 前フレームで描画したオブジェクト位置
	//============================================================
	XMMATRIX PreviousWorld;

	//親子関係
	ObjectBase* pParent;

	//行列の計算
	void calculateLocalMatrix();
	void calculateWorldMatrix();

	//実行状態の管理
	bool executeEnable;

	//描画状態の管理
	bool renderEnable;

	//回転にクオータニオンを使用
	bool rotateQuaternion;

	//ShadowMapへ描画するか
	bool castShadow = false;

	// このオブジェクトが他オブジェクトの影を受けるか
	bool receiveShadow = false;

	//============================================================
	// Motion Historyが有効か
	//============================================================
	bool mMotionHistoryValid = false;

	//============================================================
	// Shadow Casterの更新種別
	//============================================================
	ShadowMobility mShadowMobility = ShadowMobility::Dynamic;

	UINT32 renderFlag;

	//静的共通データ
	static D3D12_INPUT_ELEMENT_DESC inputElementDescs[];	//頂点要素
	static D3D12_INPUT_ELEMENT_DESC inputElementDescs_Skin[];	//頂点要素
	static ID3D12PipelineState* pPipelineState[eRenderFlag::MaxNum];		//パイプラインステート

	//コンスタントバッファ構造体
	struct stConstantBuffer
	{
		XMFLOAT4X4	WVP;		//World*View*Proj
		XMFLOAT4X4	World;		//World
		XMFLOAT4X4  WorldView;  //World*View
		XMFLOAT4X4  WorldInvTranspose;// 法線変換用逆転置行列
		XMFLOAT4X4  PreviousWVP;// 前フレームのPreviousWorld * PreviousViewProjection
		XMFLOAT4X4  LightWVP[SHADOW_CASCADE_COUNT];   //各頂点がライトから見てどこにあるかを求める
		XMFLOAT4X4	Bones[256];
	};

	struct stMaterialBuffer
	{
		XMFLOAT4	Diffuse;		//拡散色
		XMFLOAT4	Ambient;		//環境色
		XMFLOAT4	Specular;		//鏡面反射色
		XMFLOAT4	TilingOffset;	//タイリングとオフセット
		XMFLOAT4	ShadowParam;	//影用パラメーター

		//========================================================
		// Normal Map設定
		//
		// x : 使用するか
		//     0.0 = 無効
		//     1.0 = 有効
		//
		// y : Normal強度
		//     0.0 = 凹凸なし
		//     1.0 = 通常
		//     2.0 = 強調
		//
		// z : Y方向反転
		//     1.0  = そのまま
		//    -1.0  = 緑チャンネル反転
		//
		// w : 未使用
		//========================================================
		XMFLOAT4	NormalParam;

		//========================================================
		// PBRパラメータ
		//
		// x : Metallic
		//     0.0 = 非金属
		//     1.0 = 金属
		//
		// y : Roughness
		//     0.0 = 滑らか
		//     1.0 = 粗い
		//
		// z : Ambient Occlusion
		//     0.0 = 完全遮蔽
		//     1.0 = 遮蔽なし
		//
		// w : PBR使用フラグ
		//     0.0 = Lambert + Blinn-Phong
		//     1.0 = Cook-Torrance PBR
		//========================================================
		XMFLOAT4	PBRParam;

		//========================================================
		// Emissive
		//
		// xyz : 発光色
		// w   : 発光強度
		//========================================================
		XMFLOAT4	EmissiveParam;

		//============================================================
		// ORM Texture設定
		//
		// x : ORM Texture使用フラグ
		// y : AO倍率
		// z : Roughness倍率
		// w : Metallic倍率
		//============================================================
		XMFLOAT4	ORMParam;

		//============================================================
		// Environment Map Parameter
		//
		// x : Environment Reflection使用フラグ
		// y : Environment Reflection強度
		// z : CubeMapの最大Mip Level
		// w : 未使用
		//============================================================
		XMFLOAT4	EnvironmentParam;

		//============================================================
		// Subsurface Scattering
		//
		// x : SSS使用フラグ
		//     0.0 = 無効
		//     1.0 = 有効
		//
		// y : SSS Strength
		//     透過散乱光全体の強さ
		//
		// z : Thickness Mask
		//     0.0 = 光をほぼ通さない
		//     1.0 = 光を強く通す
		//
		// w : Distortion
		//     透過光の方向を法線方向へどれくらい曲げるか
		//     0.0～1.0程度
		//============================================================
		XMFLOAT4	SSSParam;

		//============================================================
		// Subsurface Color
		// xyz :
		//     散乱光の色
		//     Skinなら赤～オレンジ系
		// w :
		//     Transmission Power
		//     大きい: 透過光が狭く鋭い
		//     小さい: 広く柔らかい
		//============================================================
		XMFLOAT4	SSSColor;

		//============================================================
		// Thickness Map
		//
		// x : Use Thickness Map
		// y : Map Scale
		// z : Invert
		// w : unused
		//============================================================
		XMFLOAT4	SSSMapParam;

		//============================================================
		// Screen-Space SSS Diffusion
		// xyz: Material Scattering Profile
		// w: Material World-Space Radius
		// 0: Global値へFallback
		//============================================================
		XMFLOAT4	SSSDiffusionParam;
	};
	struct stMaterial
	{
		// カラー
		XMVECTOR	Diffuse;		//拡散色
		XMVECTOR	Ambient;		//環境色
		XMVECTOR	Specular;		//鏡面反射色
		XMVECTOR	TilingOffset;	//タイリングとオフセット
		XMVECTOR	NormalParam;	//MormalMap設定
		XMVECTOR	PBRParam; 
		XMVECTOR	EmissiveParam;
		XMVECTOR	ORMParam; // ORMテクスチャ設定
		XMVECTOR	EnvironmentParam;// Environment Reflection設定

		// Subsurface Scattering
		XMVECTOR	SSSParam;
		XMVECTOR	SSSColor;
		// SSS Thickness Map Parameter
		XMVECTOR	SSSMapParam;
		// MaterialごとのScreen-Space SSS設定
		XMVECTOR	SSSDiffusionParam;

		// テクスチャ
		ID3D12Resource* texbuff;
		ID3D12Resource* normalTexBuff;// Normal Map Texture
		ID3D12Resource* ormTexBuff;// ORM Texture
		ComPtr<ID3D12Resource> thicknessTexBuff;// SSS Thickness Texture
		ComPtr<ID3D12Resource> emissiveTexBuff;// Emissive Texture
		ComPtr<ID3D12Resource> skinMaskTexBuff;// sukinMask Texture

		// 定数バッファ
		ID3D12Resource* materialBuff;
		ID3D12DescriptorHeap* basicDescHeap;
		stMaterialBuffer* pMaterialBuffer;
	};



public:
	//静的共通データ初期化
	static bool initializeCommon();

	//静的共通データ削除
	static void terminateCommon();

	//コンストラクタ
	ObjectBase();

	//デストラクタ
	virtual ~ObjectBase();

	//システム関数
	virtual void execute(float dt);
	virtual void postExecute();
	virtual void render();
	virtual void renderShadow() {};
	// execute フェーズでオブジェクト固有の追加処理
	virtual void executeEffects() {};
	// render フェーズでオブジェクト固有のエフェクト描画
	virtual void renderEffects() {};
	// Motion Vector描画
	virtual void renderMotionVector() {};
	// Screen Space SSS Mask描画
	virtual void renderSubsurfaceMask(ID3D12PipelineState* pipelineState) {};
	// GI Surface Buffer描画
	virtual void renderGISurface(ID3D12PipelineState* rigidPSO, ID3D12PipelineState* skinPSO) {};

	//============================================================
	// 影を落とす設定
	//============================================================
	virtual void setCastShadow(bool value) { castShadow = value; }

	bool isCastShadow() const { return castShadow; }

	//============================================================
	// 影を受ける設定
	//============================================================
	virtual void setReceiveShadow(bool value) { receiveShadow = value; }

	bool isReceiveShadow() const { return receiveShadow; }

	//============================================================
	// Shadow Mobility設定
	//============================================================
	void SetShadowMobility(ShadowMobility mobility) { mShadowMobility = mobility; }

	//============================================================
	// Shadow Mobility取得
	//============================================================
	ShadowMobility GetShadowMobility() const { return mShadowMobility; }

	//============================================================
	// 静的Shadow Casterか
	//============================================================
	bool IsStaticShadowCaster() const { return mShadowMobility == ShadowMobility::Static; }

	//============================================================
	// 動的Shadow Casterか
	//============================================================
	bool IsDynamicShadowCaster() const { return mShadowMobility == ShadowMobility::Dynamic; }



	bool HasRigidMotion(float epsilon = 0.00001f) const;

	//============================================================
	// Shadow描画用Bounding Sphere取得
	// 基底クラスではBoundsを持っていないためfalse
	// Boundsを取得できないオブジェクトは、
	// 安全のためCascadeカリングせず描画する
	//============================================================
	virtual bool GetShadowBoundingSphere(XMVECTOR& outWorldCenter, float& outWorldRadius) const
	{
		outWorldCenter = XMVectorZero();
		outWorldRadius = 0.0f;
		return false;
	}


	//オブジェクトが向いている方向の取得
	XMVECTOR getForwardVector(XMVECTOR qRot) const;
	XMVECTOR getForward()const;
	XMVECTOR getRight()const;

	//============================================================
	// 前フレームWorld Matrix取得
	//============================================================
	const XMMATRIX& GetPreviousWorld() const { return PreviousWorld; }

	// Motion Historyが有効か
	bool HasMotionHistory() const { return mMotionHistoryValid; }

	// Motion Historyを現在位置へReset
	virtual void ResetMotionHistory();
	// 今フレームのTransformをPreviousへ確定する
	virtual void CommitMotionHistory();

	void Translate(const XMVECTOR& delta);

	//移動値の設定
	void setPosition(float x, float y, float z);
	void setPosition(const XMVECTOR* v);
	void setPositionX(float value);
	void setPositionY(float value);
	void setPositionZ(float value);
	void addPosition(float x, float y, float z);
	void addPosition(const XMVECTOR* v);
	void addPositionX(float value);
	void addPositionY(float value);
	void addPositionZ(float value);

	//移動値の取得
	XMVECTOR* getPosition();
	const XMVECTOR* getPosition() const;
	float getPositionX(void);
	float getPositionY(void);
	float getPositionZ(void);


	//回転値の設定
	void setRotation(float x, float y, float z);
	void setRotation(const XMVECTOR* v);
	void setRotationX(float radian);
	void setRotationY(float radian);
	void setRotationZ(float radian);
	void addRotation(float x, float y, float z);
	void addRotation(const XMVECTOR* v);
	void addRotationX(float radian);
	void addRotationY(float radian);
	void addRotationZ(float radian);

	//回転値の取得
	XMVECTOR* getRotation();
	float getRotationX(void);
	float getRotationY(void);
	float getRotationZ(void);

	//拡大値の設定
	void setScale(float x, float y, float z);
	void setScale(const XMVECTOR* v);
	void setScaleX(float value);
	void setScaleY(float value);
	void setScaleZ(float value);
	void addScale(float x, float y, float z);
	void addScale(const XMVECTOR* v);
	void addScaleX(float value);
	void addScaleY(float value);
	void addScaleZ(float value);

	//拡大値の取得
	XMVECTOR* getScale();
	float getScaleX(void);
	float getScaleY(void);
	float getScaleZ(void);

	//クオータニオンの設定
	void setQuaternion(const XMVECTOR* q);
	void setQuaternionX(float value);
	void setQuaternionY(float value);
	void setQuaternionZ(float value);
	void setQuaternionW(float value);

	//旧プロジェクトで作っていた関数(あとで統合予定)
	void setRotationQ(const XMVECTOR* q);
	void setRotationQ(float pitch, float yaw, float roll);
	void setRotationQX(float radian);
	void setRotationQY(float radian);
	void setRotationQZ(float radian);

	void addRotationQ(float x, float y, float z);
	void addRotationQ(const XMVECTOR* q);
	void addRotationQX(float radian);
	void addRotationQY(float radian);
	void addRotationQZ(float radian);

	void RotateAxis(const XMVECTOR& axis, float radian);
	void RotateX(float radian);
	void RotateY(float radian);
	void RotateZ(float radian);

	void SetRotationFromEuler(float pitch, float yaw, float roll);

	float getRotationQX(void);
	float getRotationQY(void);
	float getRotationQZ(void);

	//クオータニオンの取得
	XMVECTOR* getQuaternion();


	//ワールド行列の取得
	XMMATRIX* getWorld();

	//ワールド座標の取得
	void getWorldPosition(XMVECTOR* v);
	float getWorldPositionX();
	float getWorldPositionY();
	float getWorldPositionZ();

	//ワールド行列の設定
	void setWorld(XMMATRIX* m);


	//実行状態の設定
	void setExecuteEnable(bool flag);

	//描画状態の設定
	void setRenderEnable(bool flag);

	//実行状態の取得
	bool isExecuteEnable();

	//描画状態の取得
	bool isRenderEnable();

	//親子関係の設定
	void setParent(ObjectBase* p);

	//親子関係の取得
	ObjectBase* getParent();

	void setRenderFlag(eRenderFlag flag, bool value);

	bool getRenderFlag(eRenderFlag flag);

	//回転にクオータニオンを使用
	void useQuaternion(bool flag);

	//スキンマトリクスの設定
	virtual void setSkinMatrix(XMMATRIX* m) {}

};
