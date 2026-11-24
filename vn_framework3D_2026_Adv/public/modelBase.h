#pragma once

#include "objectBase.h"

class Character;

// VNMのファイルバイナリを共有管理する構造体
struct SharedModelBinary;
struct SharedGeometryResource;
struct SharedTextureResource;

class ModelBase : public ObjectBase
{
private:
	Character* mOwnerCharacter = nullptr;
protected:

	//モデルデータ
	vnModelData* pModelData;

	//マテリアル
	stMaterial* pMaterials;

	//============================================================
	// Skinned Motion Vector用 Previous Bone
	// pSkinMatrix: 今フレームのSkin Matrix
	// mPreviousSkinMatrix: 前フレームで描画したSkin Matrix
	//============================================================
	XMMATRIX* pSkinMatrix = nullptr;
	XMMATRIX* mPreviousSkinMatrix = nullptr;

	//============================================================
	// Skinned Model用Shadow Bounds Transform
	// Skinned Modelでは実際のWorld Transformが
	// Bone Matrix側へ含まれているため、
	// vnModel::Worldとは別にBounds用Worldを保持する
	//============================================================
	XMMATRIX mShadowBoundsWorld = XMMatrixIdentity();

	//頂点バッファ
	ID3D12Resource* pVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	//インデックスバッファ
	ID3D12Resource* pIndexBuffer;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;


	//コンスタントバッファ構造体
	ID3D12Resource* constBuff;
	//ID3D12DescriptorHeap* constDescHeap;
	stConstantBuffer* pConstBuffer;

	ID3D12Resource* bonesBuff;
	ID3D12Resource* mPreviousBonesBuff = nullptr;

	XMFLOAT4X4* bones;
	XMFLOAT4X4* mPreviousBones = nullptr;

	// Bone履歴が1フレーム以上存在するか
	bool mSkinMotionHistoryValid = false;

	bool mVisibleInMainCamera = true;

	//============================================================
	// Shadow Bounds専用Worldを使用するか
	//============================================================
	bool mUseShadowBoundsWorld = false;

	//============================================================
	// Shadow Casterカリング用Bounding Sphere
	// モデルローカル空間で保持する
	// 描画時にWorld行列でワールド空間へ変換する
	//============================================================
	XMFLOAT3 mShadowBoundsCenterLocal =
	{
		0.0f,
		0.0f,
		0.0f
	};

	float mShadowBoundsRadiusLocal = 0.0f;
	bool mHasShadowBounds = false;
	// SkinningなどによるBoundsのはみ出しを防ぐ倍率
	float mShadowBoundsPadding = 1.15f;

	//========================================================
	// STEP50-D -VNM Binary Shared Ownership
	// 同じVNMを使用するvnModel同士で読み込み済みバイナリを共有する	
	//========================================================
	std::shared_ptr<SharedModelBinary> mSharedModelBinary;

	//============================================================
	// Shared Geometry
	// 同じVNMファイルから生成された複数のvnModelが
	// 共通のVertex / Index Bufferを参照する
	//============================================================
	std::shared_ptr<SharedGeometryResource> mSharedGeometry;

	//============================================================
	// 共有BaseColor Texture
	// Materialごとに共有Textureへの参照を保持する
	// 同じDDSを使用する複数のvnModelで、
	// GPU Texture Resourceを共有する
	//============================================================
	std::vector<std::shared_ptr<SharedTextureResource>> mSharedBaseColorTextures;

	//============================================================
	// Shared Normal Map
	// MaterialごとにNormal Mapの共有Resourceを保持する
	// 同じ画像を使用するvnModel同士でGPU Textureを共有する
	//============================================================
	std::vector<std::shared_ptr<SharedTextureResource>> mSharedNormalTextures;

	//============================================================
	// Shared ORM Map
	// Occlusion / Roughness / Metallicを格納した
	// GPU Textureをモデル間で共有する
	//============================================================
	std::vector<std::shared_ptr<SharedTextureResource>> mSharedORMTextures;

	//============================================================
	// Shared Emissive Texture
	// 自発光テクスチャをモデル間で共有する
	//============================================================
	std::vector<std::shared_ptr<SharedTextureResource>> mSharedEmissiveTextures;

	//============================================================
	// Shared Thickness Texture
	// SSS用Thickness Mapを共有する
	//============================================================
	std::vector<std::shared_ptr<SharedTextureResource>> mSharedThicknessTextures;

	//============================================================
	// Shared Skin Mask Texture
	// SSSを適用する範囲を示すMaskを共有する
	//============================================================
	std::vector<std::shared_ptr<SharedTextureResource>> mSharedSkinMaskTextures;
public:
	//コンストラクタ
	ModelBase(const WCHAR* folder, const WCHAR* file);
	//デストラクタ
	~ModelBase();

	//システム関数
	virtual void execute();
	virtual void render();
	virtual void renderShadow() override;
	virtual void renderMotionVector() override;

	virtual void CommitMotionHistory() override;
	virtual void ResetMotionHistory() override;


	virtual void renderSubsurfaceMask(ID3D12PipelineState* pipelineState) override;
	virtual void renderGISurface(ID3D12PipelineState* rigidPSO, ID3D12PipelineState* skinPSO) override;

	//スキンマトリクスの設定
	virtual void setSkinMatrix(XMMATRIX* m);

	//モデルデータ(vnmファイル)の情報の取得
	int getVertexNum();
	int getIndexNum();
	int getFaceNum();
	int getMaterialNum();
	int getMeshNum();
	int getBoneNum();
	const WCHAR* getTextureName(int id);

	vnModel_MeshData* getMesh();	//メッシュデータ(先頭アドレス)の取得
	vnVertex3DFile* getFileVertex();// VNMファイル内の頂点データを取得
	LPWORD getIndex();				//インデックスデータ(先頭アドレス)の取得

	//拡散色の設定
	void setDiffuse(float r, float g, float b, float a, int id = -1);
	void setDiffuse(const XMVECTOR* v, int id = -1);
	void setDiffuseR(float value, int id = -1);
	void setDiffuseG(float value, int id = -1);
	void setDiffuseB(float value, int id = -1);
	void setDiffuseA(float value, int id = -1);
	void addDiffuseR(float value, int id = -1);
	void addDiffuseG(float value, int id = -1);
	void addDiffuseB(float value, int id = -1);
	void addDiffuseA(float value, int id = -1);

	//拡散色の取得
	XMVECTOR* getDiffuse(int id = 0);
	float getDiffuseR(int id = 0);
	float getDiffuseG(int id = 0);
	float getDiffuseB(int id = 0);
	float getDiffuseA(int id = 0);

	//環境色の設定
	void setAmbient(float r, float g, float b, float a, int id = -1);
	void setAmbient(const XMVECTOR* v, int id = -1);
	void setAmbientR(float value, int id = -1);
	void setAmbientG(float value, int id = -1);
	void setAmbientB(float value, int id = -1);
	void setAmbientA(float value, int id = -1);
	void addAmbientR(float value, int id = -1);
	void addAmbientG(float value, int id = -1);
	void addAmbientB(float value, int id = -1);
	void addAmbientA(float value, int id = -1);

	//環境色の取得
	XMVECTOR* getAmbient(int id = 0);
	float getAmbientR(int id = 0);
	float getAmbientG(int id = 0);
	float getAmbientB(int id = 0);
	float getAmbientA(int id = 0);

	//鏡面反射色の設定
	void setSpecular(float r, float g, float b, float p, int id = -1);
	void setSpecular(const XMVECTOR* v, int id = -1);
	void setSpecularR(float value, int id = -1);
	void setSpecularG(float value, int id = -1);
	void setSpecularB(float value, int id = -1);
	void setSpecularPower(float value, int id = -1);
	void addSpecularR(float value, int id = -1);
	void addSpecularG(float value, int id = -1);
	void addSpecularB(float value, int id = -1);
	void addSpecularPower(float value, int id = -1);

	//鏡面反射色の取得
	XMVECTOR* getSpecular(int id = 0);
	float getSpecularR(int id = 0);
	float getSpecularG(int id = 0);
	float getSpecularB(int id = 0);
	float getSpecularPower(int id = 0);

	//============================================================
	// Shadow Bounds用World Matrix設定
	//============================================================
	void SetShadowBoundsWorld(const XMMATRIX& world)
	{
		mShadowBoundsWorld = world;
		mUseShadowBoundsWorld = true;
	}

	//============================================================
	// Override解除
	//============================================================
	void ClearShadowBoundsWorld()
	{
		mUseShadowBoundsWorld = false;
		mShadowBoundsWorld = XMMatrixIdentity();
	}

	//============================================================
	// メインカメラの描画パスだけに適用する
	//============================================================
	void SetVisibleInMainCamera(bool visible)
	{
		mVisibleInMainCamera = visible;
	}

	//============================================================
	// PBR有効・無効
	//============================================================
	void SetPBREnabled(bool enabled, int materialId = 0);
	//============================================================
	// Metallic
	//============================================================
	void SetMetallic(float metallic, int materialId = 0);
	//============================================================
	// Roughness
	//============================================================
	void SetRoughness(float roughness, int materialId = 0);
	//============================================================
	// Ambient Occlusion
	//============================================================
	void SetMaterialAO(float ao, int materialId = 0);

	//============================================================
	// Emissive
	//============================================================
	void SetEmissive(
		float r,
		float g,
		float b,
		float intensity,
		int materialId = 0
	);

	//タイリングとオフセットの取得
	const XMVECTOR* getTilingOffset(int id = 0);

	void SetCharacter(Character* c) { mOwnerCharacter = c; }
	Character* GetCharacter() const { return mOwnerCharacter; }

	//============================================================
	// GPU Texture Upload Batch
	//============================================================
	static bool BeginTextureUploadBatch();
	static bool EndTextureUploadBatch();

	//============================================================
	// Parallel CPU Asset Preload
	// vnModelを生成する前に使用予定のVNMを複数スレッドで読み込んでおく
	// modelPaths: VNMのフルパス、または作業ディレクトリ基準の相対パス
	// workerCount:
	//   0 = 自動（最大4スレッド）
	//   1 = シングルスレッド
	//   2～ = 指定したスレッド数
	//============================================================
	static bool PreloadVnmFilesParallel(
		const std::vector<std::wstring>& modelPaths,
		unsigned int workerCount = 0
	);

	//============================================================
	// プリロード用の一時的な強参照を解除する
	// 既に生成されたvnModelはshared_ptrを持っているため
	// モデルが使用中のVNMバイナリは解放されない
	//============================================================
	static void ReleaseVnmPreloadCache();

	//============================================================
	// DDS CPU Parallel Preload
	// VNMファイル内のMaterial情報からDDSを収集し
	// 複数のWorker ThreadでCPUメモリに読み込む
	// 対応VNM:
	//  V1 : BaseColor
	//  V2 : BaseColor / Normal / ORM / Thickness / Emissive
	//  V3 : V2 + SkinMask
	// workerCount:
	//  0 = 自動決定（最大4スレッド）
	//  1 = シングルスレッド
	//  2以上 = 指定したスレッド数
	//============================================================
	static bool PreloadDdsFromVnmFilesParallel(
		const std::vector<std::wstring>& modelPaths,
		unsigned int workerCount = 0
	);

	//============================================================
	// DDSプリロードキャッシュ解放
	//
	// プリロード済みScratchImageの強参照を解除する。
	// GPU転送に必要なCPUデータを使い終わってから呼ぶ。
	//============================================================
	static void ReleaseDdsPreloadCache();

	//============================================================
	// モデル頂点からShadow用Bounding Sphereを作る
	//============================================================
	void BuildShadowBoundingSphere();

	//============================================================
	// ワールド空間のShadow用Bounding Sphereを取得
	//============================================================
	virtual bool GetShadowBoundingSphere(
		XMVECTOR& outWorldCenter,
		float& outWorldRadius
	) const override;

	//============================================================
	// Normal MapをMaterialへ設定
	// folder: Texture Folder
	// file: Texture File Name
	// materialId: 対象Material番号
	//============================================================
	bool LoadNormalMap(
		const WCHAR* folder,
		const WCHAR* file,
		int materialId = 0
	);

	//============================================================
	// Normal Map強度設定
	//============================================================
	void SetNormalMapStrength(
		float strength,
		int materialId = 0
	);

	//============================================================
	// Normal MapのY反転
	//
	// DirectX用Normal MapとOpenGL用Normal Mapで
	// 緑チャンネルの向きが異なる場合に使用する
	//============================================================
	void SetNormalMapFlipY(
		bool flipY,
		int materialId = 0
	);

	//============================================================
	// Packed ORM Textureを読み込む
	//
	// Texture Channel:
	//   R = Ambient Occlusion
	//   G = Roughness
	//   B = Metallic
	//============================================================
	bool LoadORMMap(
		const WCHAR* folder,
		const WCHAR* file,
		int materialId = 0
	);

	//============================================================
	// ORM Textureの使用状態
	//============================================================
	void SetORMMapEnabled(
		bool enabled,
		int materialId = 0
	);

	//============================================================
	// ORM各Channelの倍率
	//============================================================
	void SetORMScale(
		float aoScale,
		float roughnessScale,
		float metallicScale,
		int materialId = 0
	);

	//============================================================
	// Environment CubeMap Descriptorを更新
	//
	// materialId:
	//   -1 = 全マテリアル
	//    0以上 = 指定マテリアルのみ
	//============================================================
	void RefreshEnvironmentMapDescriptor(
		int materialId = -1
	);

	//============================================================
	// Environment Reflection設定
	//============================================================
	void SetEnvironmentReflection(
		bool enabled,
		float diffuseIntensity = 1.0f,
		float specularIntensity = 1.0f,
		int materialId = 0
	);

	//============================================================
	// Subsurface Scattering
	//============================================================

	//============================================================
	// SSS Thickness Map読み込み
	// TextureのR Channelを使用する
	// Engine Convention:
	//   0 = Thick
	//   1 = Thin
	//============================================================
	bool LoadSubsurfaceThicknessMap(
		const WCHAR* folder,
		const WCHAR* file,
		int materialId = 0
	);


	//============================================================
	// Thickness Map ON / OFF
	//============================================================
	void SetSubsurfaceThicknessMapEnabled(
		bool enabled,
		int materialId = 0
	);


	//============================================================
	// Thickness Map倍率
	// 1.0 = Texture値そのまま
	//============================================================
	void SetSubsurfaceThicknessMapScale(
		float scale,
		int materialId = 0
	);


	//============================================================
	// Thickness Map反転
	// 外部Textureの規約が逆の場合に使用する
	//============================================================
	void SetSubsurfaceThicknessMapInvert(
		bool invert,
		int materialId = 0
	);

	// SSS ON / OFF
	void SetSubsurfaceEnabled(
		bool enabled,
		int materialId = 0
	);

	// SSS全体強度
	void SetSubsurfaceStrength(
		float strength,
		int materialId = 0
	);

	// Thickness Mask
	void SetSubsurfaceThickness(
		float thickness,
		int materialId = 0
	);

	// Transmission Direction Distortion
	void SetSubsurfaceDistortion(
		float distortion,
		int materialId = 0
	);

	// Scatter Color
	void SetSubsurfaceColor(
		float r,
		float g,
		float b,
		int materialId = 0
	);

	// Transmission Power
	void SetSubsurfacePower(
		float power,
		int materialId = 0
	);

	//============================================================
	// MaterialごとのSSS Scattering Profile
	//============================================================
	void SetSubsurfaceScatteringProfile(
		float red,
		float green,
		float blue,
		int materialId = 0
	);


	//============================================================
	// MaterialごとのWorld-Space SSS Radius
	// 0: Global Radiusを使用
	//============================================================
	void SetSubsurfaceWorldSpaceRadius(
		float radius,
		int materialId = 0
	);

	//============================================================
	// SSS Skin Mask
	// R Channel:
	// 0 = Skinではない
	// 1 = Skin
	//============================================================
	bool LoadSubsurfaceSkinMask(
		const WCHAR* folder,
		const WCHAR* file,
		int materialId = 0
	);


	//============================================================
	// Skin Mask ON / OFF
	//============================================================
	void SetSubsurfaceSkinMaskEnabled(
		bool enabled,
		int materialId = 0
	);


	//============================================================
	// Material固有設定を解除してGlobalへ戻す
	//============================================================
	void ResetSubsurfaceDiffusionProfile(
		int materialId = 0
	);

	//============================================================
	// Emissive Map Load
	// VNM V2に保存されたEmissive DDSを
	// Runtime Materialへロードする
	//============================================================
	bool LoadEmissiveMap(
		const WCHAR* folder,
		const WCHAR* file,
		int materialId
	);

private:
	//============================================================
	// Tangent計算後のGPU頂点データ
	// VNMファイル内の頂点はTangentを持たないため
	// 読み込み時にこの配列へ展開してからGPUへ送る
	//============================================================
	std::vector<vnVertex3D> mGpuVertices;
	std::vector<vnVertex3DSkin> mGpuSkinVertices;
	//============================================================
	// VNM頂点からGPU頂点を作成し、Tangentを計算する
	//============================================================
	bool BuildGpuVerticesWithTangents();
	bool BuildGpuSkinnedVerticesWithTangents();

private:

	//============================================================
	// Main Camera Frustum Culling
	// 現在のカメラの表示範囲に
	// このモデルのBounding Sphereが入っているか確認する
	// true  : 描画する
	// false : 描画しない
	// Shadow Pass用のカリング処理は変更しない
	//============================================================
	bool IsInsideMainCameraFrustum() const;
};
