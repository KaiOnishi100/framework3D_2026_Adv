
#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "mansionArt.h"

//============================================================
// AddStaticModel
// 共有リソースキャッシュに対応したvnModelを生成する
//============================================================
bool MansionArt::AddStaticModel(vnScene* scene,const MansionArtPlacement& placement, ModelBase** createdModel)
{
    //============================================================
    // 必須データ確認
    //============================================================
    if (scene == nullptr ||
        placement.Id.empty() ||
        placement.Folder.empty() ||
        placement.File.empty())
    {
        return false;
    }

    //============================================================
    // 重複IDを禁止
    // Art ObjectはCSV等からIDで管理するので
    // 同じIDの登録は不正とする
    //============================================================
    if (mModels.find(placement.Id) != mModels.end())
    {
        return false;
    }

    //============================================================
    // Model生成
    // vnModel内部でGeometry CacheやTexture Cacheが使用される
    //============================================================
    ModelBase* model = new ModelBase(
        placement.Folder.c_str(),
        placement.File.c_str()
    );

    if (createdModel != nullptr)
    {
        *createdModel = model;
    }

    if (model == nullptr)
    {
        return false;
    }

    //============================================================
    // World Transform
    //============================================================
    model->setPosition(
        placement.Position.x,
        placement.Position.y,
        placement.Position.z
    );

    model->setRotation(
        placement.Rotation.x,
        placement.Rotation.y,
        placement.Rotation.z
    );

    model->setScale(
        placement.Scale.x,
        placement.Scale.y,
        placement.Scale.z
    );

    //============================================================
    // Rendering
    //============================================================
    model->SetPBREnabled(true);

    //============================================================
    // Shadow
    //============================================================
    model->setCastShadow(placement.CastShadow);
    model->setReceiveShadow(true);
    model->SetShadowMobility(ShadowMobility::Static);

    //============================================================
    // Scene登録
    // 登録失敗時には、MansionArt側でModelを破棄する
    //============================================================
    if (!scene->registerObject(model))
    {
        delete model;
        return false;
    }

    //============================================================
    // ID登録
    //============================================================
    mModels.emplace(placement.Id, model);

    return true;
}

//============================================================
// Terminate
// Sceneから全Art Modelを削除する
//============================================================
void MansionArt::Terminate(vnScene* scene)
{
    if (scene == nullptr)
    {
        return;
    }

    for (auto& pair : mModels)
    {
        ModelBase* model = pair.second;

        if (model != nullptr)
        {
            scene->deleteObject(model);
        }
    }

    mModels.clear();
}
