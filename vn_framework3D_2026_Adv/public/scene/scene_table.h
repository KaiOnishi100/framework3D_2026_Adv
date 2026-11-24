#pragma once

//シーンの種類
enum eSceneTable
{	FieldTest,	CharacterRenderingTest,
	AtmosphericEscape,
	AtmosphericEscapeTitle,
	SceneMax,
};

//シーンクラス定義ファイル
#include "vn_scene.h"
#include "scene_field_test.h"
#include "scene_Character_Rendering_Test.h"
#include "scene_AtmosphericEscape.h"
#include "scene_AtmosphericEscape_Title.h"

//シーン切り替え関数(予約)
void switchScene(eSceneTable scene, bool forceReload = false);
//現在のシーン取得(実処理)
void switchScene();
