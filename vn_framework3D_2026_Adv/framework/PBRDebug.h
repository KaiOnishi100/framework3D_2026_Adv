#pragma once

//============================================================
// PBRデバッグ表示モード
// シーン全体のPBR計算結果を、
// 指定した要素だけに切り替えて表示する
//============================================================
enum class PBRDebugMode : int
{
	// 通常の最終描画
	Final = 0,

	// マテリアル入力
	BaseColor,
	WorldNormal,
	Metallic,
	Roughness,
	AmbientOcclusion,
	Emissive,

	// 直接光
	DirectDiffuse,
	DirectSpecular,

	// IBL
	DiffuseIBL,
	SpecularIBL,

	// その他
	Shadow,
	NdotL,
	F0,
	BRDFLUT,

	PointLights,
	SpotLights,
	LocalLights,
	// モード総数
	Count
};