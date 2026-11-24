#include "../framework.h"
#include "../framework/vn_environment.h"

#define USE_RESOURCE_LOADER (1)

//頂点要素
D3D12_INPUT_ELEMENT_DESC vnSprite::inputElementDescs[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

//静的共通データ
eSpriteRenderTarget vnSprite::renderTarget;
ID3D12PipelineState* vnSprite::pPipelineStateHDR = nullptr;
ID3D12PipelineState* vnSprite::pPipelineStateHDR_Add = nullptr;
ID3D12PipelineState* vnSprite::pPipelineStateUI = nullptr;
ID3D12PipelineState* vnSprite::pPipelineStateUI_Add = nullptr;
ID3D12Resource* vnSprite::constBuff = NULL;	//定数バッファ
float *vnSprite::scr = NULL;

//静的共通データ初期化
bool vnSprite::initializeCommon()
{
	HRESULT hr = S_OK;

	//============================================================
	// 共通PSO設定
	//============================================================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC baseDesc = {};

	baseDesc.InputLayout =
	{
		inputElementDescs,
		_countof(inputElementDescs)
	};

	baseDesc.pRootSignature = Direct3D::getRootSignature2D();
	baseDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_2D)->getCode();
	baseDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_2D)->getLength();

	//============================================================
	// Rasterizer
	//============================================================
	baseDesc.RasterizerState.MultisampleEnable = false;
	baseDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	baseDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	baseDesc.RasterizerState.DepthClipEnable = true;
	baseDesc.RasterizerState.FrontCounterClockwise = false;
	baseDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	baseDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	baseDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;

	//============================================================
	// Alpha Blend
	//============================================================
	baseDesc.BlendState.AlphaToCoverageEnable = false;
	baseDesc.BlendState.IndependentBlendEnable = false;
	baseDesc.BlendState.RenderTarget[0].BlendEnable = true;
	baseDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	baseDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	baseDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	baseDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	baseDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	baseDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	baseDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//============================================================
	// SpriteなのでDepthは使用しない
	//============================================================
	baseDesc.DepthStencilState.DepthEnable = false;
	baseDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	baseDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	baseDesc.DepthStencilState.StencilEnable = false;
	baseDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	baseDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	baseDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	baseDesc.NumRenderTargets = 1;
	baseDesc.SampleDesc.Count = 1;
	baseDesc.SampleDesc.Quality = 0;

	//============================================================
	// PSO作成補助処理
	//============================================================
	auto CreateSpritePSO = [&](
		DXGI_FORMAT renderTargetFormat,
		Shader::ePixelShader pixelShader,
		bool additive,
		ID3D12PipelineState** outputPipelineState,
		const wchar_t* debugName
		) -> bool
		{
			assert(outputPipelineState != nullptr);

			//========================================================
			// 共通設定をコピー
			//========================================================
			D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc;
			//========================================================
			// 描画先のFormat
			//========================================================
			desc.RTVFormats[0] = renderTargetFormat;
			//========================================================
			// Pixel Shader
			//========================================================
			desc.PS.pShaderBytecode = Shader::getPShader(pixelShader)->getCode();
			desc.PS.BytecodeLength = Shader::getPShader(pixelShader)->getLength();

			//========================================================
			// 通常Alpha Blend / 加算合成
			//========================================================
			desc.BlendState.RenderTarget[0].DestBlend = additive ? D3D12_BLEND_ONE : D3D12_BLEND_INV_SRC_ALPHA;

			const HRESULT createResult = Direct3D::getDevice()->CreateGraphicsPipelineState(
				&desc,
				IID_PPV_ARGS(outputPipelineState)
			);

			if (FAILED(createResult))
			{
				assert(false);
				return false;
			}

			(*outputPipelineState)->SetName(debugName);

			return true;
		};

	//============================================================
	// HDR通常合成
	//============================================================
	if (!CreateSpritePSO(
		RenderFormat::SceneHDR,
		Shader::ePixelShader::PS_2D_HDR,
		false,
		&pPipelineStateHDR,
		L"vnSprite HDR Alpha PSO"))
	{
		return false;
	}

	//============================================================
	// HDR加算合成
	//============================================================
	if (!CreateSpritePSO(
		RenderFormat::SceneHDR,
		Shader::ePixelShader::PS_2D_HDR,
		true,
		&pPipelineStateHDR_Add,
		L"vnSprite HDR Add PSO"))
	{
		return false;
	}

	//============================================================
	// UI通常合成
	//============================================================
	if (!CreateSpritePSO(
		RenderFormat::BackBuffer,
		Shader::ePixelShader::PS_2D,
		false,
		&pPipelineStateUI,
		L"vnSprite UI Alpha PSO"))
	{
		return false;
	}

	//============================================================
	// UI加算合成
	//============================================================
	if (!CreateSpritePSO(
		RenderFormat::BackBuffer,
		Shader::ePixelShader::PS_2D,
		true,
		&pPipelineStateUI_Add,
		L"vnSprite UI Add PSO"))
	{
		return false;
	}

	//定数バッファ
	D3D12_HEAP_PROPERTIES constHeapProp = {};
	constHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	constHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	constHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	constHeapProp.CreationNodeMask = 1;
	constHeapProp.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC constDesc = {};
	constDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	constDesc.Width = ((sizeof(float) * 4 + 0xff) & ~0xff);
	constDesc.Height = 1;
	constDesc.DepthOrArraySize = 1;
	constDesc.MipLevels = 1;
	constDesc.Format = DXGI_FORMAT_UNKNOWN;
	constDesc.SampleDesc.Count = 1;
	constDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	constDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	hr = Direct3D::getDevice()->CreateCommittedResource(
		&constHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&constDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(&constBuff)
	);

	assert(hr == S_OK);
	hr = constBuff->SetName(L"vnSprite::constBuff");
	assert(hr == S_OK);

	//定数バッファへ値をセット
	hr = constBuff->Map(0, NULL, (void**)&scr);
	assert(hr == S_OK);
	//スクリーン座標からビューポート座標へ変換するための係数
	float w = (float)vnMainFrame::screenWidth;
	float h = (float)vnMainFrame::screenHeight;
	scr[0] = 2.0f / w;
	scr[1] = -2.0f / h;
	scr[2] = -w * 0.5f;
	scr[3] = -h * 0.5f;
	constBuff->Unmap(0, NULL);

	renderTarget = eSpriteRenderTarget::SceneHDR;

	return true;
}

//静的共通データ削除
void vnSprite::terminateCommon()
{
	SAFE_RELEASE(constBuff);
	SAFE_RELEASE(constBuff);
	SAFE_RELEASE(pPipelineStateHDR);
	SAFE_RELEASE(pPipelineStateHDR_Add);
	SAFE_RELEASE(pPipelineStateUI);
	SAFE_RELEASE(pPipelineStateUI_Add);
}

//コンストラクタ
vnSprite::vnSprite(float x, float y, float width, float height, const WCHAR* texture_file, float left_u, float right_u, float top_v, float bottom_v)
{
	//基本情報を保存
	posX = x;
	posY = y;
	sizeX = width;
	sizeY = height;
	scaleX = 1.0f;
	scaleY = 1.0f;
	rot = 0.0f;

	HRESULT hr = S_OK;

	texbuff = NULL;
	basicDescHeap = NULL;

	//WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	hr = LoadFromWICFile(texture_file, WIC_FLAGS_NONE, &metadata, scratchImg);
	if (hr == S_OK)
	{
		const Image* img = scratchImg.GetImage(0, 0, 0);

		D3D12_HEAP_PROPERTIES texHeapProp = {};
		texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
		texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		texHeapProp.CreationNodeMask = 0;
		texHeapProp.VisibleNodeMask = 0;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Format = metadata.format;
		resDesc.Width = metadata.width;
		resDesc.Height = (UINT)metadata.height;
		resDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.MipLevels = (UINT16)metadata.mipLevels;
		resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		hr = Direct3D::getDevice()->CreateCommittedResource(
			&texHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			NULL,
			IID_PPV_ARGS(&texbuff)
		);
		texbuff->SetName(L"vnSprite::texbuff");

		hr = texbuff->WriteToSubresource(0,
			NULL,
			img->pixels,
			(UINT)img->rowPitch,
			(UINT)img->slicePitch
		);
	}

	{
		//デスクリプタヒープ
		D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
		descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descHeapDesc.NodeMask = 0;
		descHeapDesc.NumDescriptors = 1;// 2;
		descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		hr = Direct3D::getDevice()->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));

		//テクスチャビュー作成
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = metadata.format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		D3D12_CPU_DESCRIPTOR_HANDLE basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();
		//シェーダリソースビューの作成
		if (texbuff != NULL)
		{
			Direct3D::getDevice()->CreateShaderResourceView(texbuff,
				&srvDesc,
				basicHeapHandle
			);
		}
		else
		{
			Direct3D::getDevice()->CreateShaderResourceView(Direct3D::getWhiteTexture(),
				Direct3D::getWhiteTextueViewDesc(),
				basicHeapHandle
			);
		}
		/*basicHeapHandle.ptr += vnDirect3D::getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (UINT)constBuff->GetDesc().Width;
		//定数バッファビューの作成
		vnDirect3D::getDevice()->CreateConstantBufferView(&cbvDesc, basicHeapHandle);*/
	}

	//頂点バッファ
	const UINT vertexBufferSize = sizeof(vnVertex2D) * VertexNum;

	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapprop.CreationNodeMask = 1;
	heapprop.VisibleNodeMask = 1;


	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = vertexBufferSize;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	hr = Direct3D::getDevice()->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(&vertBuff)
	);

	assert(hr == S_OK);
	vertBuff->SetName(L"vnSprite::vertBuff");

	hr = vertBuff->Map(0, NULL, reinterpret_cast<void**>(&vtx));
	assert(hr == S_OK);

	vertexBufferView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(vnVertex2D);
	vertexBufferView.SizeInBytes = vertexBufferSize;

	//頂点座標を設定
	setVertexPosition();

	//頂点カラーを設定
	vtx[0].r = 1.0f;	vtx[0].g = 1.0f;	vtx[0].b = 1.0f;	vtx[0].a = 1.0f;
	vtx[1].r = 1.0f;	vtx[1].g = 1.0f;	vtx[1].b = 1.0f;	vtx[1].a = 1.0f;
	vtx[2].r = 1.0f;	vtx[2].g = 1.0f;	vtx[2].b = 1.0f;	vtx[2].a = 1.0f;
	vtx[3].r = 1.0f;	vtx[3].g = 1.0f;	vtx[3].b = 1.0f;	vtx[3].a = 1.0f;

	//uvを設定
	vtx[0].u = left_u;	vtx[0].v = top_v;
	vtx[1].u = right_u;	vtx[1].v = top_v;
	vtx[2].u = left_u;	vtx[2].v = bottom_v;
	vtx[3].u = right_u;	vtx[3].v = bottom_v;

	//実行状態の管理
	executeEnable = true;

	//描画状態の管理
	renderEnable = true;

	//描画優先順位
	renderPriority = 0;

	addition = false;
}

//デストラクタ
vnSprite::~vnSprite()
{
	SAFE_RELEASE(basicDescHeap);
	SAFE_RELEASE(texbuff);
	SAFE_RELEASE(vertBuff);
	texbuff = NULL;
}

//処理関数
void vnSprite::execute()
{
}

//描画関数
void vnSprite::render()
{
	//頂点座標の設定
	setVertexPosition();

	ID3D12PipelineState* selectedPipelineState = nullptr;

	//============================================================
	// 描画先とBlend方法からPSOを選択
	//============================================================
	if (renderTarget == eSpriteRenderTarget::SceneHDR)
	{
		selectedPipelineState =
			addition ? pPipelineStateHDR_Add : pPipelineStateHDR;
	}
	else
	{
		selectedPipelineState =
			addition ? pPipelineStateUI_Add : pPipelineStateUI;
	}

	assert(selectedPipelineState != nullptr);

	Direct3D::getCommandList()->SetPipelineState(selectedPipelineState);
	Direct3D::getCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Direct3D::getCommandList()->SetGraphicsRootConstantBufferView(0, constBuff->GetGPUVirtualAddress());
	Direct3D::getCommandList()->SetDescriptorHeaps(1, &basicDescHeap);
	Direct3D::getCommandList()->SetGraphicsRootDescriptorTable(1, basicDescHeap->GetGPUDescriptorHandleForHeapStart());
	Direct3D::getCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	Direct3D::getCommandList()->DrawInstanced(VertexNum, 1, 0, 0);
}

//頂点座標の設定
void vnSprite::setVertexPosition()
{
	//標準状態での各頂点の座標
	float v0x = (-sizeX * 0.5f) * scaleX, v0y = (-sizeY * 0.5f) * scaleY;
	float v1x = (+sizeX * 0.5f) * scaleX, v1y = (-sizeY * 0.5f) * scaleY;
	float v2x = (-sizeX * 0.5f) * scaleX, v2y = (+sizeY * 0.5f) * scaleY;
	float v3x = (+sizeX * 0.5f) * scaleX, v3y = (+sizeY * 0.5f) * scaleY;

	//回転を適用
	float v0x_r = v0x * cosf(rot) - (v0y * sinf(rot));	//頂点0の回転後のx座標
	float v0y_r = v0y * cosf(rot) + (v0x * sinf(rot));	//頂点0の回転後のy座標
	float v1x_r = v1x * cosf(rot) - (v1y * sinf(rot));	//頂点1の回転後のx座標
	float v1y_r = v1y * cosf(rot) + (v1x * sinf(rot));	//頂点1の回転後のy座標
	float v2x_r = v2x * cosf(rot) - (v2y * sinf(rot));	//頂点2の回転後のx座標
	float v2y_r = v2y * cosf(rot) + (v2x * sinf(rot));	//頂点2の回転後のy座標
	float v3x_r = v3x * cosf(rot) - (v3y * sinf(rot));	//頂点3の回転後のx座標
	float v3y_r = v3y * cosf(rot) + (v3x * sinf(rot));	//頂点3の回転後のy座標

	//各頂点に中心座標を適用し、絶対座標を設定
	vtx[0].x = v0x_r + posX;	vtx[0].y = v0y_r + posY;
	vtx[1].x = v1x_r + posX;	vtx[1].y = v1y_r + posY;
	vtx[2].x = v2x_r + posX;	vtx[2].y = v2y_r + posY;
	vtx[3].x = v3x_r + posX;	vtx[3].y = v3y_r + posY;
}

//---- setter関数 ----

//実行状態の設定
void vnSprite::setExecuteEnable(bool flag)
{
	executeEnable = flag;
}

//描画状態の設定
void vnSprite::setRenderEnable(bool flag)
{
	renderEnable = flag;
}

//描画優先順位の設定
void vnSprite::setRenderPriority(int value)
{
	if (renderPriority == value)return;
	renderPriority = value;
	vnScene::setUpdateRenderPriority();
}

//加算合成の設定
void vnSprite::setAddition(bool flag)
{
	addition = flag;
}

void vnSprite::setRenderTarget(eSpriteRenderTarget target)
{
	renderTarget = target;
}

//---- getter関数 ----

//実行状態の取得
bool vnSprite::isExecuteEnable()
{
	return executeEnable;
}

//描画状態の取得
bool vnSprite::isRenderEnable()
{
	return renderEnable;
}

//描画優先順位の取得
int vnSprite::getRenderPriority(void)
{
	return renderPriority;
}

//加算合成の設定
bool vnSprite::getAddition()
{
	return addition;
}

void vnSprite::setExternalTextureView(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc)
{
	//============================================================
	// ResourceまたはDescriptorHeapが無効なら何もしない
	//============================================================
	if (resource == nullptr)
	{
		return;
	}

	if (basicDescHeap == nullptr)
	{
		return;
	}

	//============================================================
	// Spriteが持っているDescriptorHeapの先頭を取得
	// 元々登録されていた画像用SRVを外部Resource用SRVで上書きする
	//============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	//============================================================
	// 指定されたResourceのSRVを作成
	// texbuff自体は差し替えない
	// Descriptorだけを外部Resourceへ向ける
	// これによりデストラクタでShadowMap Resourceを
	// 誤ってReleaseすることを防ぐ
	//============================================================
	Direct3D::getDevice()->CreateShaderResourceView(
		resource,
		&srvDesc,
		handle
	);
}
