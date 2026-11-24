#include "../framework.h"
#include "../framework/vn_environment.h"
#include "objectBase.h"

//静的共通データ
//頂点要素
D3D12_INPUT_ELEMENT_DESC ObjectBase::inputElementDescs[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};
D3D12_INPUT_ELEMENT_DESC ObjectBase::inputElementDescs_Skin[]
{
	{ "POSITION",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT",	 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "SKIN_INDEX",  0, DXGI_FORMAT_R16G16B16A16_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "SKIN_WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};
//パイプラインステート
ID3D12PipelineState* ObjectBase::pPipelineState[ObjectBase::eRenderFlag::MaxNum];


//静的共通データ初期化
bool ObjectBase::initializeCommon()
{
	HRESULT hr;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = Direct3D::getRootSignature();

	psoDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_3D)->getCode();
	psoDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_3D)->getLength();
	psoDesc.PS.pShaderBytecode = Shader::getPShader(Shader::ePixelShader::PS_3D)->getCode();
	psoDesc.PS.BytecodeLength = Shader::getPShader(Shader::ePixelShader::PS_3D)->getLength();

	psoDesc.RasterizerState.MultisampleEnable = false;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.DepthClipEnable = true;
	psoDesc.RasterizerState.FrontCounterClockwise = false;
	psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	psoDesc.RasterizerState.AntialiasedLineEnable = false;
	psoDesc.RasterizerState.ForcedSampleCount = 0;
	psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	psoDesc.BlendState.AlphaToCoverageEnable = false;
	psoDesc.BlendState.IndependentBlendEnable = false;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = false;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_DEST_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].LogicOpEnable = false;
	psoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.DepthStencilState.DepthEnable = true;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psoDesc.DepthStencilState.StencilEnable = false;
	psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	psoDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = RenderFormat::SceneHDR;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	for (int i = 0; i < eRenderFlag::MaxNum; i++)
	{
		//半透明(ブレンド)有効
		if (i & eRenderFlag::Transparent)
		{
			psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
		}
		else
		{
			psoDesc.BlendState.RenderTarget[0].BlendEnable = false;
		}
		//ライティング無効
		if (i & eRenderFlag::Lighting)
		{
			psoDesc.PS.pShaderBytecode = Shader::getPShader(Shader::ePixelShader::PS_3D)->getCode();
			psoDesc.PS.BytecodeLength = Shader::getPShader(Shader::ePixelShader::PS_3D)->getLength();
		}
		else
		{
			psoDesc.PS.pShaderBytecode = Shader::getPShader(Shader::ePixelShader::PS_3D_NoLight)->getCode();
			psoDesc.PS.BytecodeLength = Shader::getPShader(Shader::ePixelShader::PS_3D_NoLight)->getLength();
		}
		if (i & eRenderFlag::ZWrite)
		{	//深度書き込みあり
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		}
		else
		{	//深度書き込みなし
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		}
		if (i & eRenderFlag::Addition)
		{	//加算合成あり
			psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		}
		else
		{	//加算合成なし
			psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		}
		if (i & eRenderFlag::DoubleSided)
		{	//両面描画
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		}
		else
		{	//表面描画
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		}
		//スキニング頂点変換
		if (i & eRenderFlag::Skin)
		{
			psoDesc.InputLayout = { inputElementDescs_Skin, _countof(inputElementDescs_Skin) };
			psoDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_3D_Skin)->getCode();
			psoDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_3D_Skin)->getLength();
		}
		else
		{
			psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
			psoDesc.VS.pShaderBytecode = Shader::getVShader(Shader::eVertexShader::VS_3D)->getCode();
			psoDesc.VS.BytecodeLength = Shader::getVShader(Shader::eVertexShader::VS_3D)->getLength();
		}

		hr = Direct3D::getDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pPipelineState[i]));


		//============================================================
		// PSO作成失敗を、SetNameより前に検出する
		//============================================================
		if (FAILED(hr))
		{
			wchar_t message[512] = {};

			swprintf_s(
				message,
				L"[PSO Create Failed]\n"
				L"index      : %d\n"
				L"renderFlag : 0x%08X\n"
				L"Skin       : %s\n"
				L"Lighting   : %s\n"
				L"HRESULT    : 0x%08X\n",
				i,
				i,
				(i & eRenderFlag::Skin) ? L"true" : L"false",
				(i & eRenderFlag::Lighting) ? L"true" : L"false",
				static_cast<unsigned int>(hr)
			);

			OutputDebugStringW(message);

			//============================================================
			// ここで停止させる
			// SetNameは呼ばない
			//============================================================
			assert(false);
			continue;
		}

		//============================================================
		// PIXでrenderFlagごとのPSOを特定しやすくする
		//============================================================
		wchar_t pipelineStateName[128] = {};

		swprintf_s(
			pipelineStateName,
			L"vnObject HDR PSO [%d] Flags=0x%08X",
			i,
			i
		);

		pPipelineState[i]->SetName(
			pipelineStateName
		);
	}
	return true;
}

//静的共通データ削除
void ObjectBase::terminateCommon()
{
	for (int i = 0; i < eRenderFlag::MaxNum; i++)
	{
		SAFE_RELEASE(pPipelineState[i]);
	}
}


ObjectBase::ObjectBase()
{
	Position = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

	Rotation = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	
	Scale = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);

	Quaternion = XMQuaternionIdentity();

	Local = XMMatrixIdentity();
	World = XMMatrixIdentity();

	PreviousWorld = XMMatrixIdentity();

	mMotionHistoryValid = false;

	pParent = NULL;

	executeEnable = true;
	renderEnable = true;

	//レンダーフラグの初期値
	renderFlag = (eRenderFlag::Lighting | eRenderFlag::ZWrite);

	rotateQuaternion = false;
}

ObjectBase::~ObjectBase()
{
}

void ObjectBase::execute(float dt)
{
}

void ObjectBase::postExecute()
{
	calculateLocalMatrix();
	calculateWorldMatrix();
}

void ObjectBase::render()
{
}

bool ObjectBase::HasRigidMotion(float epsilon) const
{
	if (!mMotionHistoryValid)
	{
		return false;
	}


	XMFLOAT4X4 currentWorld = {};
	XMFLOAT4X4 previousWorld = {};


	XMStoreFloat4x4(
		&currentWorld,
		World
	);


	XMStoreFloat4x4(
		&previousWorld,
		PreviousWorld
	);


	//============================================================
	// 4x4 Matrixの16要素を比較
	// Translation / Rotation / Scale / Parent移動の
	// どれが変化しても検出できる
	//============================================================
	const float* current = reinterpret_cast<const float*>(&currentWorld);
	const float* previous = reinterpret_cast<const float*>(&previousWorld);


	for (int elementIndex = 0; elementIndex < 16; ++elementIndex)
	{
		if (fabsf(current[elementIndex] - previous[elementIndex]) > epsilon)
		{
			return true;
		}
	}

	return false;
}

XMVECTOR ObjectBase::getForwardVector(XMVECTOR qRot) const
{
	XMVECTOR forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	return XMVector3Normalize(XMVector3Rotate(forward, qRot));
}

XMVECTOR ObjectBase::getForward() const
{
	return XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 0, 1, 0), Quaternion));
}

XMVECTOR ObjectBase::getRight() const
{
	return XMVector3Rotate(XMVectorSet(1, 0, 0, 0), Quaternion);
}

void ObjectBase::ResetMotionHistory()
{
	PreviousWorld = World;
	mMotionHistoryValid = true;
}

void ObjectBase::CommitMotionHistory()
{
	PreviousWorld = World;
	// 一度でも描画フレームを通過したらMotion Historyを有効にする
	mMotionHistoryValid = true;
}

void ObjectBase::Translate(const XMVECTOR& delta)
{
	Position = XMVectorAdd(Position, delta);
}

void ObjectBase::calculateLocalMatrix()
{
	XMMATRIX trans = XMMatrixTranslation(getPositionX(), getPositionY(), getPositionZ());
	XMMATRIX rotate =
		rotateQuaternion ?
		XMMatrixRotationQuaternion(Quaternion) :XMMatrixRotationRollPitchYaw(getRotationX(), getRotationY(), getRotationZ());
	XMMATRIX scale = XMMatrixScaling(getScaleX(), getScaleY(), getScaleZ());
	Local = scale * rotate * trans;
}

void ObjectBase::calculateWorldMatrix()
{
	if (pParent == NULL)
	{
		World = Local;
	}
	else
	{
		World = Local * (*pParent->getWorld());
	}
}

//移動値の設定
void ObjectBase::setPosition(float x, float y, float z)
{
	Position = XMVectorSet(x, y, z, 0.0f);
}

void ObjectBase::setPosition(const XMVECTOR *v)
{
	Position = *v;
}

void ObjectBase::setPositionX(float value)
{
	Position = XMVectorSetX(Position, value);
}

void ObjectBase::setPositionY(float value)
{
	Position = XMVectorSetY(Position, value);
}

void ObjectBase::setPositionZ(float value)
{
	Position = XMVectorSetZ(Position, value);
}

void ObjectBase::addPosition(float x, float y, float z)
{
	Position = XMVectorAdd(Position, XMVectorSet(x, y, z, 0.0f));
}

void ObjectBase::addPosition(const XMVECTOR *v)
{
	Position = XMVectorAdd(Position, *v);
}

void ObjectBase::addPositionX(float value)
{
	Position = XMVectorAdd(Position, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
}

void ObjectBase::addPositionY(float value)
{
	Position = XMVectorAdd(Position, XMVectorSet(0.0f, value, 0.0f, 0.0f));
}

void ObjectBase::addPositionZ(float value)
{
	Position = XMVectorAdd(Position, XMVectorSet(0.0f, 0.0f, value, 0.0f));
}

//移動値の取得
XMVECTOR *ObjectBase::getPosition()
{
	return &Position;
}

const XMVECTOR* ObjectBase::getPosition() const
{
	return &Position;
}

float ObjectBase::getPositionX(void)
{
	return XMVectorGetX(Position);
}

float ObjectBase::getPositionY(void)
{
	return XMVectorGetY(Position);
}

float ObjectBase::getPositionZ(void)
{
	return XMVectorGetZ(Position);
}

//回転値の設定
void ObjectBase::setRotation(float x, float y, float z)
{
	Rotation = XMVectorSet(x, y, z, 0.0f);
}

void ObjectBase::setRotation(const XMVECTOR *v)
{
	Rotation = *v;
}

void ObjectBase::setRotationX(float radian)
{
	Rotation = XMVectorSetX(Rotation, radian);
}

void ObjectBase::setRotationY(float radian)
{
	Rotation = XMVectorSetY(Rotation, radian);
}

void ObjectBase::setRotationZ(float radian)
{
	Rotation = XMVectorSetZ(Rotation, radian);
}

void ObjectBase::addRotation(float x, float y, float z)
{
	Rotation = XMVectorAdd(Rotation, XMVectorSet(x, y, z, 0.0f));
}

void ObjectBase::addRotation(const XMVECTOR *v)
{
	Rotation = XMVectorAdd(Rotation, *v);
}

void ObjectBase::addRotationX(float radian)
{
	Rotation = XMVectorAdd(Rotation, XMVectorSet(radian, 0.0f, 0.0f, 0.0f));
}

void ObjectBase::addRotationY(float radian)
{
	Rotation = XMVectorAdd(Rotation, XMVectorSet(0.0f, radian, 0.0f, 0.0f));
}

void ObjectBase::addRotationZ(float radian)
{
	Rotation = XMVectorAdd(Rotation, XMVectorSet(0.0f, 0.0f, radian, 0.0f));
}

//回転値の取得
XMVECTOR *ObjectBase::getRotation()
{
	return &Rotation;
}

float ObjectBase::getRotationX(void)
{
	return XMVectorGetX(Rotation);
}

float ObjectBase::getRotationY(void)
{
	return XMVectorGetY(Rotation);
}

float ObjectBase::getRotationZ(void)
{
	return XMVectorGetZ(Rotation);
}

//拡大値の設定
void ObjectBase::setScale(float x, float y, float z)
{
	Scale = XMVectorSet(x, y, z, 0.0f);
}

void ObjectBase::setScale(const XMVECTOR *v)
{
	Scale = *v;
}

void ObjectBase::setScaleX(float value)
{
	Scale = XMVectorSetX(Scale, value);
}

void ObjectBase::setScaleY(float value)
{
	Scale = XMVectorSetY(Scale, value);
}

void ObjectBase::setScaleZ(float value)
{
	Scale = XMVectorSetZ(Scale, value);
}

void ObjectBase::addScale(float x, float y, float z)
{
	Scale = XMVectorAdd(Scale, XMVectorSet(x, y, z, 0.0f));
}

void ObjectBase::addScale(const XMVECTOR *v)
{
	Scale = XMVectorAdd(Scale, *v);
}

void ObjectBase::addScaleX(float value)
{
	Scale = XMVectorAdd(Scale, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
}

void ObjectBase::addScaleY(float value)
{
	Scale = XMVectorAdd(Scale, XMVectorSet(0.0f, value, 0.0f, 0.0f));
}

void ObjectBase::addScaleZ(float value)
{
	Scale = XMVectorAdd(Scale, XMVectorSet(0.0f, 0.0f, value, 0.0f));
}

//拡大値の取得
XMVECTOR *ObjectBase::getScale()
{
	return &Scale;
}

float ObjectBase::getScaleX(void)
{
	return XMVectorGetX(Scale);
}

float ObjectBase::getScaleY(void)
{
	return XMVectorGetY(Scale);
}

float ObjectBase::getScaleZ(void)
{
	return XMVectorGetZ(Scale);
}

//クオータニオンの設定
void ObjectBase::setQuaternion(const XMVECTOR* q)
{
	Quaternion = *q;
}

void ObjectBase::setQuaternionX(float value)
{
	Quaternion = XMVectorSetX(Quaternion, value);
}

void ObjectBase::setQuaternionY(float value)
{
	Quaternion = XMVectorSetY(Quaternion, value);
}

void ObjectBase::setQuaternionZ(float value)
{
	Quaternion = XMVectorSetZ(Quaternion, value);
}

void ObjectBase::setQuaternionW(float value)
{
	Quaternion = XMVectorSetW(Quaternion, value);
}

void ObjectBase::setRotationQ(const XMVECTOR* q)
{
	Quaternion = XMQuaternionNormalize(*q);
}

void ObjectBase::setRotationQ(float pitch, float yaw, float roll)
{
	Quaternion = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
	Quaternion = XMQuaternionNormalize(Quaternion);
}

void ObjectBase::setRotationQX(float radian)
{
	Quaternion = XMQuaternionRotationAxis(
		XMVectorSet(1, 0, 0, 0),
		radian
	);
}

void ObjectBase::setRotationQY(float radian)
{
	Quaternion = XMQuaternionRotationAxis(
		XMVectorSet(0, 1, 0, 0),
		radian
	);
}

void ObjectBase::setRotationQZ(float radian)
{
	Quaternion = XMQuaternionRotationAxis(
		XMVectorSet(0, 0, 1, 0),
		radian
	);
}

void ObjectBase::addRotationQ(float x, float y, float z)
{
	Quaternion = XMVectorAdd(Quaternion, XMVectorSet(x, y, z, 0.0f));
}

void ObjectBase::addRotationQ(const XMVECTOR* q)
{
	Quaternion = XMQuaternionMultiply(*q, Quaternion);
	Quaternion = XMQuaternionNormalize(Quaternion);
}

void ObjectBase::addRotationQX(float radian)
{
	RotateX(radian);
}

void ObjectBase::addRotationQY(float radian)
{
	RotateY(radian);
}

void ObjectBase::addRotationQZ(float radian)
{
	RotateZ(radian);
}

void ObjectBase::RotateAxis(const XMVECTOR& axis, float radian)
{
	XMVECTOR q = XMQuaternionRotationAxis(axis, radian);

	Quaternion = XMQuaternionMultiply(q, Quaternion);
	Quaternion = XMQuaternionNormalize(Quaternion);
}

void ObjectBase::RotateX(float radian)
{
	RotateAxis(XMVectorSet(1, 0, 0, 0), radian);
}

void ObjectBase::RotateY(float radian)
{
	RotateAxis(XMVectorSet(0, 1, 0, 0), radian);
}
void ObjectBase::RotateZ(float radian)
{
	RotateAxis(XMVectorSet(0, 0, 1, 0), radian);
}

void ObjectBase::SetRotationFromEuler(float pitch, float yaw, float roll)
{
	Quaternion = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
	Quaternion = XMQuaternionNormalize(Quaternion);
}

float ObjectBase::getRotationQX(void)
{
	return XMVectorGetX(Quaternion);
}

float ObjectBase::getRotationQY(void)
{
	return XMVectorGetY(Quaternion);
}

float ObjectBase::getRotationQZ(void)
{
	return XMVectorGetZ(Quaternion);
}

//クオータニオンの取得
XMVECTOR* ObjectBase::getQuaternion()
{
	return &Quaternion;
}

//ワールド行列の取得
XMMATRIX *ObjectBase::getWorld()
{
	return &World;
}

//ワールド座標の取得
void ObjectBase::getWorldPosition(XMVECTOR *v)
{
	*v = World.r[3];
}

float ObjectBase::getWorldPositionX()
{
	return XMVectorGetX(World.r[3]);
}

float ObjectBase::getWorldPositionY()
{
	return XMVectorGetY(World.r[3]);
}

float ObjectBase::getWorldPositionZ()
{
	return XMVectorGetZ(World.r[3]);
}

//ワールド行列の設定
void ObjectBase::setWorld(XMMATRIX *m)
{
	if (m)
	{
		World = *m;
	}
}

//実行状態の設定
void ObjectBase::setExecuteEnable(bool flag)
{
	executeEnable =  flag;
}

//描画状態の設定
void ObjectBase::setRenderEnable(bool flag)
{
	renderEnable = flag;
}

//実行状態の取得
bool ObjectBase::isExecuteEnable()
{
	return executeEnable;
}

//描画状態の取得
bool ObjectBase::isRenderEnable()
{
	return renderEnable;
}

//親子関係の設定
void ObjectBase::setParent(ObjectBase* p)
{
	pParent = p;
}

//親子関係の取得
ObjectBase* ObjectBase::getParent()
{
	return pParent;
}

void ObjectBase::setRenderFlag(eRenderFlag flag, bool value)
{
	if (value)
	{
		renderFlag |= flag;
	}
	else
	{
		renderFlag &= ~(flag);
	}
}

bool ObjectBase::getRenderFlag(eRenderFlag flag)
{
	return (renderFlag & flag) != 0;
}


//回転にクオータニオンを使用
void ObjectBase::useQuaternion(bool flag)
{
	rotateQuaternion = flag;
}
