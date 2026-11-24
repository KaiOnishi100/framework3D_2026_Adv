#pragma once

//============================================================
// Spriteの描画先
//============================================================
enum class eSpriteRenderTarget
{
	//========================================================
	// HDR Sceneへ描画
	// BloomやTone Mappingの影響を受ける
	//========================================================

	SceneHDR,

	//========================================================
	// Tone Mapping後のBackBufferへ描画
	// 主にUI用
	//========================================================
	BackBuffer
};
//ポリゴン頂点構造体
struct vnVertex2D
{
	float x, y;			//スクリーン座標
	float r, g, b, a;	//頂点カラー
	float u, v;			//テクスチャ座標
};

//スプライトクラス
class vnSprite
{
public:
	//座標
	float posX;
	float posY;
	//サイズ
	float sizeX;
	float sizeY;
	//スケール
	float scaleX;
	float scaleY;
	//回転
	float rot;

	//頂点数
	static const int VertexNum = 4;
	//頂点データ
	vnVertex2D	*vtx;

protected:
	//頂点データへの各種情報の設定
	void setVertexPosition();

private:

	//実行状態の管理
	bool executeEnable;

	//描画状態の管理
	bool renderEnable;

	//描画優先順位(大きいほど手前に描画される)
	int renderPriority;

	//加算合成
	bool addition;

	//頂点バッファ
	ID3D12Resource* vertBuff;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	ID3D12Resource* texbuff;
	ID3D12DescriptorHeap* basicDescHeap;

	//静的共通データ
	static D3D12_INPUT_ELEMENT_DESC inputElementDescs[];	//頂点要素
	//============================================================
	// HDR Scene用
	//
	// 出力先:
	//   R16G16B16A16_FLOAT
	//
	// 用途:
	//   画面効果
	//   発光スプライト
	//   Bloom対象にしたい画像
	//============================================================
	static ID3D12PipelineState* pPipelineStateHDR;
	static ID3D12PipelineState* pPipelineStateHDR_Add;

	//============================================================
	// SDR BackBuffer用
	//
	// 出力先:
	//   R8G8B8A8_UNORM
	//
	// 用途:
	//   HUD
	//   メニュー
	//   HPゲージ
	//   文字など
	//============================================================
	static ID3D12PipelineState* pPipelineStateUI;
	static ID3D12PipelineState* pPipelineStateUI_Add;

	//============================================================
	// Spriteの描画先
	//============================================================
	static eSpriteRenderTarget renderTarget;

	static ID3D12Resource* constBuff;	//定数バッファ
	static float *scr;	//スプライトをスクリーン座標で扱えるようにするためのシェーダー定数

public:
	//静的共通データ初期化
	static bool initializeCommon();

	//静的共通データ削除
	static void terminateCommon();

	//コンストラクタ
	vnSprite() {}
	vnSprite(float x, float y, float width, float height, const WCHAR* texture_file, float left_u = 0.0f, float right_u = 1.0f, float top_v = 0.0f, float bottom_v = 1.0f);

	//デストラクタ
	virtual ~vnSprite();

	//処理
	virtual void execute();

	//描画
	virtual void render();

	//---- setter関数 ----

	//実行状態の設定
	void setExecuteEnable(bool flag);

	//描画状態の設定
	void setRenderEnable(bool flag);

	//描画優先順位の設定
	void setRenderPriority(int value);

	//加算合成の設定
	void setAddition(bool flag);

	//============================================================
	// Spriteの描画先を設定
	//============================================================
	static void setRenderTarget(eSpriteRenderTarget target);

	//---- getter関数 ----

	//実行状態の取得
	bool isExecuteEnable();

	//描画状態の取得
	bool isRenderEnable();

	//描画優先順位の取得
	int getRenderPriority();

	//加算合成の設定
	bool getAddition();

	//============================================================
    // 外部ResourceをSpriteのテクスチャとして設定
    //
    // 主な用途:
    //   ・ShadowMapのデバッグ表示
    //   ・RenderTargetのデバッグ表示
    //   ・ポストプロセス途中結果の表示
    //
    // 注意:
    //   resourceの所有権はvnSpriteへ移動しない。
    //   この関数ではresourceをReleaseしない。
    //   resourceはvnSpriteより長く生存している必要がある。
    //============================================================
	void setExternalTextureView(
		ID3D12Resource* resource,
		const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
	);
};