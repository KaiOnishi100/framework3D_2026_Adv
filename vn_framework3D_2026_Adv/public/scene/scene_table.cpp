#include "../../framework.h"
#include "../../framework/vn_environment.h"

//起動時のシーン
#if _DEBUG
eSceneTable initialScene = eSceneTable::AtmosphericEscapeTitle;
#else
eSceneTable initialScene = eSceneTable::AtmosphericEscapeTitle;
#endif
//現在のシーン
eSceneTable currentScene = (eSceneTable)-1;
//切り替え予約のシーン
eSceneTable reserveScene = initialScene;
// 同じSceneでも再生成するか
bool reserveSceneForceReload = false;
//各シーンの名前
WCHAR SceneName[(int)eSceneTable::SceneMax][32] =
{
    L"Field Test",
    L"CharacterRenderingTest",
    L"AtmosphericEscape",
    L"AtmosphericEscapeTitle"
};

//ステージ切り替え関数(予約)
void switchScene(eSceneTable scene, bool forceReload)
{
	reserveScene = scene;
	reserveSceneForceReload = forceReload;
}

//現在のステージ取得(実処理)
void switchScene()
{
    //========================================================
    // 今回処理するSceneを確定
    //========================================================
    const eSceneTable nextScene = reserveScene;

    const bool forceReload = reserveSceneForceReload;

    //========================================================
    // Force Reloadフラグを消費
    //========================================================
    reserveSceneForceReload = false;


    //========================================================
    // 通常のScene切り替え
    // 同じSceneであり
    // かつForce Reloadでない場合は何もしない
    //========================================================
    if (currentScene == nextScene && !forceReload)
    {
        return;
    }

    vnMainFrame::terminateScene();

    //========================================================
    // 新しいSceneを生成
    //========================================================
    vnScene* pScene = nullptr;

    switch (nextScene)
    {
    case FieldTest:
    {
        pScene =new SceneFieldTest();
        break;
    }
    case CharacterRenderingTest:
    {
        pScene = new SceneCharacterRenderingTest();
        break;
    }
    case AtmosphericEscape:
    {
        pScene = new SceneAtmosphericEscape();
        break;
    }
    case AtmosphericEscapeTitle:
    {
        pScene = new SceneAtmosphericEscapeTitle();
        break;
    }

    default:
    {
        //====================================================
        // 不正なScene
        //====================================================
        assert(false);
        return;
    }
    }

    //========================================================
    // Scene初期化
    //========================================================
    vnMainFrame::initializeScene(pScene);

    //========================================================
    // Current Scene更新
    //========================================================
    currentScene = nextScene;
}
