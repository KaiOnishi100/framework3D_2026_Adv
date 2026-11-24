#include "../framework.h"
#include "vn_environment.h"
#include "shadowMapManager.h"

//============================================================
// 静的メンバ実体
//============================================================
ShadowMap* ShadowMapManager::sShadowMap = nullptr;

//============================================================
// ShadowMap登録
//============================================================
void ShadowMapManager::SetShadowMap(ShadowMap* shadowMap)
{
	sShadowMap = shadowMap;
}

//============================================================
// ShadowMap登録解除
//============================================================
void ShadowMapManager::Clear()
{
	sShadowMap = nullptr;
}

void ShadowMapManager::ClearShadowMap(const ShadowMap* shadowMap)
{
	if (sShadowMap == shadowMap)
	{
		sShadowMap = nullptr;
	}
}

//============================================================
// 有効確認
//============================================================
bool ShadowMapManager::IsValid()
{
	return sShadowMap != nullptr;
}

//============================================================
// LightViewProjection取得
//============================================================
const XMMATRIX& ShadowMapManager::GetLightViewProj(UINT cascadeIndex)
{
	assert(sShadowMap != nullptr);
	assert(cascadeIndex <sShadowMap->GetCascadeCount());
	return sShadowMap->GetLightViewProj(cascadeIndex);
}

//============================================================
// SRV Heap取得
//============================================================
ID3D12DescriptorHeap* ShadowMapManager::GetSRVHeap()
{
	if (sShadowMap == nullptr)
	{
		return nullptr;
	}

	return sShadowMap->GetSRVHeap();
}

//============================================================
// SRV GPUハンドル取得
//============================================================
D3D12_GPU_DESCRIPTOR_HANDLE ShadowMapManager::GetSRVGPUHandle(UINT cascadeIndex, ShadowMapLayer layer)
{
	if (sShadowMap == nullptr)
	{
		return {};
	}

	assert(cascadeIndex <sShadowMap->GetCascadeCount());

	return sShadowMap->GetSRVGPUHandle(cascadeIndex,layer);
}

//============================================================
// ShadowMapサイズ取得
//============================================================
UINT ShadowMapManager::GetShadowMapSize()
{
	if (sShadowMap == nullptr)
	{
		return 0;
	}

	return sShadowMap->GetSize();
}

D3D12_CPU_DESCRIPTOR_HANDLE ShadowMapManager::GetSRVCopyCPUHandle(UINT cascadeIndex, ShadowMapLayer layer)
{
	if (sShadowMap == nullptr)
	{
		return {};
	}

	assert(cascadeIndex < sShadowMap->GetCascadeCount());

	return sShadowMap->GetSRVCopyCPUHandle(cascadeIndex,layer);
}

ID3D12PipelineState* ShadowMapManager::GetPipelineState(bool isSkin)
{
	if (sShadowMap == nullptr)
	{
		return nullptr;
	}

	if (isSkin)
	{
		return sShadowMap->GetSkinPipelineState();
	}

	return sShadowMap->GetStaticPipelineState();
}

ID3D12RootSignature* ShadowMapManager::GetRootSignature()
{
	if (sShadowMap == nullptr)
	{
		return nullptr;
	}

	return sShadowMap->GetRootSignature();
}

UINT ShadowMapManager::GetCascadeCount()
{
	if (sShadowMap == nullptr)
	{
		return 0;
	}

	return
		sShadowMap->GetCascadeCount();
}

float ShadowMapManager::GetCascadeSplitDepth(UINT cascadeIndex)
{
	if (sShadowMap == nullptr)
	{
		return 0.0f;
	}

	assert(cascadeIndex < sShadowMap->GetCascadeCount());

	return sShadowMap->GetCascadeSplitDepth(cascadeIndex);
}

float ShadowMapManager::GetCascadeBlendStartDepth(UINT cascadeIndex)
{

	if (sShadowMap == nullptr)
	{
		return 0.0f;
	}

	assert(cascadeIndex < sShadowMap->GetCascadeCount());

	return sShadowMap->GetCascadeBlendStartDepth(cascadeIndex);
}

UINT ShadowMapManager::GetCascadeRenderedObjectCount(UINT cascadeIndex)
{
	assert(cascadeIndex < SHADOW_CASCADE_COUNT);

	if (sShadowMap == nullptr)
	{
		return 0;
	}

	return sShadowMap->GetCascadeRenderedObjectCount(cascadeIndex);
}

UINT ShadowMapManager::GetTotalRenderedObjectCount()
{
	if (sShadowMap == nullptr)
	{
		return 0;
	}

	return sShadowMap->GetTotalRenderedObjectCount();
}

//============================================================
// 現在描画中のCascadeへDraw Call数を加算
//============================================================
void ShadowMapManager::AddShadowDrawCall(
	UINT count
)
{
	if (sShadowMap == nullptr)
	{
		return;
	}

	sShadowMap->AddShadowDrawCall(count);
}

//============================================================
// 指定CascadeのDraw Call数を取得
//============================================================
UINT ShadowMapManager::GetCascadeDrawCallCount(UINT cascadeIndex)
{
	if (sShadowMap == nullptr)
	{
		return 0;
	}

	assert(cascadeIndex < sShadowMap->GetCascadeCount());

	return sShadowMap->GetCascadeDrawCallCount(cascadeIndex);
}

//============================================================
// 全CascadeのDraw Call数合計を取得
//============================================================
UINT ShadowMapManager::GetTotalDrawCallCount()
{
	if (sShadowMap == nullptr)
	{
		return 0;
	}

	return sShadowMap->GetTotalDrawCallCount();
}

//============================================================
// Timestamp結果を更新
//============================================================
void ShadowMapManager::UpdateTimestampResults()
{
	if (sShadowMap == nullptr)
	{
		return;
	}

	sShadowMap->UpdateTimestampResults();
}

//============================================================
// 指定CascadeのGPU時間を取得
//============================================================
double ShadowMapManager::GetCascadeGpuTimeMs(UINT cascadeIndex)
{
	if (sShadowMap == nullptr)
	{
		return 0.0;
	}

	assert(cascadeIndex <sShadowMap->GetCascadeCount());

	return sShadowMap->GetCascadeGpuTimeMs(cascadeIndex);
}

//============================================================
// 全CascadeのGPU時間を取得
//============================================================
double ShadowMapManager::GetTotalGpuTimeMs()
{
	if (sShadowMap == nullptr)
	{
		return 0.0;
	}

	return sShadowMap->GetTotalGpuTimeMs();
}

bool ShadowMapManager::WasCascadeUpdatedThisFrame(UINT cascadeIndex, ShadowMapLayer layer)
{
	if (sShadowMap == nullptr)
	{
		return false;
	}

	assert(cascadeIndex < sShadowMap->GetCascadeCount());

	return sShadowMap->WasCascadeUpdatedThisFrame(cascadeIndex, layer);
}

UINT ShadowMapManager::GetLayerCascadeRenderedObjectCount(UINT cascadeIndex, ShadowMapLayer layer)
{
	if (sShadowMap == nullptr)
	{
		return 0;
	}

	assert(cascadeIndex < sShadowMap->GetCascadeCount());

	return sShadowMap->GetLayerCascadeRenderedObjectCount(cascadeIndex, layer);
}

UINT ShadowMapManager::GetLayerCascadeDrawCallCount(UINT cascadeIndex, ShadowMapLayer layer)
{
	if (sShadowMap == nullptr)
	{
		return 0;
	}

	assert(cascadeIndex <sShadowMap->GetCascadeCount());

	return sShadowMap->GetLayerCascadeDrawCallCount(cascadeIndex, layer);
}

double ShadowMapManager::GetLayerCascadeGpuTimeMs(UINT cascadeIndex, ShadowMapLayer layer)
{
	if (sShadowMap == nullptr)
	{
		return 0.0;
	}

	assert(cascadeIndex <sShadowMap->GetCascadeCount());

	return sShadowMap->GetLayerCascadeGpuTimeMs(cascadeIndex, layer);
}

void ShadowMapManager::InvalidateStaticShadowCache()
{
	if (sShadowMap == nullptr)
	{
		return;
	}

	sShadowMap->InvalidateStaticShadowCache();
}
