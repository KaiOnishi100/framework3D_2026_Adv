#include "AssetScaleProcessor.h"

#include <cmath>
#include <iostream>


//============================================================
// Apply
//============================================================
bool AssetScaleProcessor::Apply(ImportedModel& model, float scale)
{
    //========================================================
    // 不正値防止
    // ※0倍にすると全Vertexが原点へ潰れるため禁止
    // ※NaN / Infも禁止
    //========================================================
    if (!std::isfinite(scale) || scale <= 0.0f)
    {
        std::cerr
            << "[AssetScaleProcessor] "
            << "Invalid scale : "
            << scale
            << "\n";

        return false;
    }

    //========================================================
    // 1.0なら何もしない
    //========================================================
    if (std::abs(scale - 1.0f) < 0.000001f)
    {
        std::cout
            << "[AssetScaleProcessor] "
            << "Scale = 1.0, skip.\n";

        return true;
    }

    //========================================================
    // Mesh Vertex Position
    // Normal / Tangentは方向VectorなのでScaleしない
    // UV / Color / BoneWeightも変更しない
    //========================================================
    for (ImportedMesh& mesh : model.Meshes)
    {
        for (ImportedVertex& vertex : mesh.Vertices)
        {
            vertex.Position.x *= scale;
            vertex.Position.y *= scale;
            vertex.Position.z *= scale;
        }
    }


    //========================================================
    // Skeleton Local Translation
    // Rotation: 単位とは無関係なので変更しない
    // Scale: Transform倍率なので変更しない
    // PositionだけSource Unit → Meterへ変換する
    //========================================================
    for (ImportedBone& bone : model.Bones)
    {
        bone.Position.x *= scale;
        bone.Position.y *= scale;
        bone.Position.z *= scale;
    }


    //========================================================
    // Inverse Bind Matrix
    // GeometryとSkeletonを同じScaleへ変換するため
    // InverseBind側のTranslationも同じScaleへ変換する
    //========================================================
    for (ImportedSkinBone& skinBone : model.SkinBones)
    {
        skinBone.InverseBind._41 *= scale;
        skinBone.InverseBind._42 *= scale;
        skinBone.InverseBind._43 *= scale;
    }

    std::size_t scaledAnimationPositionKeyCount = 0;

    for (ImportedAnimation& animation : model.Animations)
    {
        for (ImportedAnimationChannel& channel : animation.Channels)
        {
            for (ImportedVectorKey& key : channel.PositionKeys)
            {
                key.Value.x *= scale;
                key.Value.y *= scale;
                key.Value.z *= scale;

                ++scaledAnimationPositionKeyCount;
            }
        }
    }

    //========================================================
    // Debug Log
    //========================================================
    std::cout
        << "\n"
        << "========================================\n"
        << " Asset Scale Processing\n"
        << "========================================\n"
        << "Scale                   : "
        << scale
        << "\n"
        << "Meshes                  : "
        << model.Meshes.size()
        << "\n"
        << "Skeleton Nodes          : "
        << model.Bones.size()
        << "\n"
        << "Skin Bones              : "
        << model.SkinBones.size()
        << "\n"
        << "Animations              : "
        << model.Animations.size()
        << "\n"
        << "Animation Position Keys : "
        << scaledAnimationPositionKeyCount
        << "\n"
        << "========================================\n";


    return true;
}