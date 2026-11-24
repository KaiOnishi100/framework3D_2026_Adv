#pragma once

//============================================================
// Standard Library
//============================================================
#include <filesystem>
#include <string>


//============================================================
// Assimp
//============================================================
#include <assimp/scene.h>


//============================================================
// 自作Importer中間データ
//============================================================
#include "ImportedModel.h"

struct VertexInfluence
{
    std::uint16_t BoneIndex = 0;

    float Weight = 0.0f;
};

//============================================================
// ModelImporter
// FBX / glTF / OBJなどをAssimpで読み込み
// ImportedModelという自作エンジン共通の中間形式へ変換するクラス
// 外部Asset
//     ↓
// Assimp
//     ↓
// ModelImporter
//     ↓
// ImportedModel
//============================================================
class ModelImporter
{
public:

    //========================================================
    // Helper
    //========================================================

    //============================================================
    // Quaternion → Euler Radian
    //
    // DirectXMathを使用してEulerへ変換する
    // .bone保存用
    //============================================================
    static DirectX::XMFLOAT3 QuaternionToEuler(const DirectX::XMFLOAT4& q)
    {
        using namespace DirectX;


        //========================================================
        // Quaternion成分
        //========================================================
        const float x = q.x;
        const float y = q.y;
        const float z = q.z;
        const float w = q.w;


        //========================================================
        // X Rotation
        //========================================================
        const float sinX =
            2.0f * (w * x + y * z);

        const float cosX =
            1.0f - 2.0f * (x * x + y * y);

        const float rotX =
            std::atan2(sinX, cosX);


        //========================================================
        // Y Rotation
        //========================================================
        const float sinY =
            2.0f * (w * y - z * x);

        float rotY;

        if (std::abs(sinY) >= 1.0f)
        {
            rotY = std::copysign(DirectX::XM_PIDIV2, sinY);
        }
        else
        {
            rotY = std::asin(sinY);
        }


        //========================================================
        // Z Rotation
        //========================================================
        const float sinZ =
            2.0f * (w * z + x * y);

        const float cosZ =
            1.0f - 2.0f * (y * y + z * z);

        const float rotZ =
            std::atan2(sinZ, cosZ);

        return { rotX, rotY,  rotZ };
    }

    static int FindSkeletonBoneIndex(const ImportedModel& model, const std::string& name)
    {
        for (std::size_t i = 0; i < model.Bones.size(); ++i)
        {
            if (model.Bones[i].Name == name)
            {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    //============================================================
    // 4x4 Matrixの最大誤差を計算
    // 16要素を比較し、 max(abs(A - B))を返す
    // 0に近いほど同じMatrix
    //============================================================
    static float CalculateMatrixMaxError(const DirectX::XMFLOAT4X4& a, const DirectX::XMFLOAT4X4& b)
    {
        const float* valuesA =
            reinterpret_cast<const float*>(&a);

        const float* valuesB =
            reinterpret_cast<const float*>(&b);

        float maxError = 0.0f;


        for (int element = 0; element < 16; ++element)
        {
            const float error = std::abs(valuesA[element] - valuesB[element]);

            maxError = (std::max)(maxError, error);
        }

        return maxError;
    }

    //============================================================
    // Assimp Matrix
    //      ↓
    // DirectX XMFLOAT4X4
    //============================================================
    static DirectX::XMFLOAT4X4 ToXMFLOAT4X4(const aiMatrix4x4& m)
    {
        return DirectX::XMFLOAT4X4
        (
            //====================================================
            // Row 1
            //====================================================
            m.a1,
            m.b1,
            m.c1,
            m.d1,

            //====================================================
            // Row 2
            //====================================================
            m.a2,
            m.b2,
            m.c2,
            m.d2,

            //====================================================
            // Row 3
            //====================================================
            m.a3,
            m.b3,
            m.c3,
            m.d3,

            //====================================================
            // Row 4
            //====================================================
            m.a4,
            m.b4,
            m.c4,
            m.d4
        );
    }

    //============================================================
    // Embedded Texture検索
    //============================================================
    static const ImportedEmbeddedTexture* FindEmbeddedTexture(const ImportedModel& model, const std::string& name)
    {
        for (const ImportedEmbeddedTexture& texture : model.EmbeddedTextures)
        {
            if (texture.Name == name)
            {
                return &texture;
            }
        }
        return nullptr;
    }

    //============================================================
    // AssimpがFBXから認識しているTexture情報を確認する
    //============================================================
    static void DebugPrintTextureInfo(const aiScene* scene);


public:

    //============================================================
    // Model Import
    // filePath: FBX / glTF / OBJなどの入力Model Path
    // output: Import結果を保存する中間Model
    // return:
    //     true  = 成功
    //     false = 失敗
    //============================================================
    static bool Import(
        const std::filesystem::path& filePath,
        ImportedModel& output
    );


private:

    //============================================================
    // Mesh Import
    // AssimpのaiMeshから
    // Position
    // Normal
    // UV
    // Tangent
    // VertexColor
    // Index
    // などをImportedMeshへ変換する
    //============================================================
    static void ImportMeshes(
        const aiScene* scene,
        ImportedModel& output
    );

    //============================================================
    // Node TreeからMeshを再帰的にImport
    // FBX Import時の
    // ・Rotation
    // ・Scale
    // ・Translation
    // ・座標系補正用Root Transform
    // などを失わないため、Node Treeを辿って
    // Global TransformをVertexへBakeする
    //============================================================
    static void ImportNodeMeshes(
        const aiScene* scene,
        const aiNode* node,
        const aiMatrix4x4& parentTransform,
        ImportedModel& output
    );


    //============================================================
    // Material Import
    // Assimp Materialから
    // BaseColor
    // Metallic
    // Roughness
    // Emissive
    // を読み込む
    // TextureについてはvnTextureResolverを使用して
    // BaseColor
    // Normal
    // ORM
    // AO
    // Roughness
    // Metallic
    // Thickness
    // Emissive
    // を自動検出する
    // modelPath:
    //     Textureの相対Path解決や、
    //     Modelと同じDirectory内のTexture検索に使用する
    //============================================================
    static void ImportMaterials(
        const aiScene* scene,
        const std::filesystem::path& modelPath,
        ImportedModel& output
    );

    //============================================================
    // Assimp Node Hierarchy
    //        ↓
    // ImportedModel::Bones
    //============================================================
    static void ImportSkeleton(
        const aiNode* node,
        int parentIndex,
        int generation,
        ImportedModel& output
    );
    
    //============================================================
    // Assimp Mesh Skinning
    // aiMesh::mBones
    //       ↓
    // ImportedModel::SkinBones
    //       ↓
    // ImportedVertex::BoneIndices[4]
    // ImportedVertex::BoneWeights[4]
    //============================================================
    static bool ImportMeshSkinning(
        const aiMesh* mesh,
        const aiMatrix4x4& meshGlobalTransform,
        ImportedMesh& importedMesh,
        ImportedModel& model
    );

    //============================================================
    // Animation Import
    // aiAnimation / aiNodeAnimから
    // Position Key
    // Rotation Key
    // Scale Key
    //============================================================
    static void ImportAnimations(
        const aiScene* scene,
        ImportedModel& output
    );

    //============================================================
    // FBX / glTF Embedded Texture
    //============================================================
    static void ImportEmbeddedTextures(
        const aiScene* scene,
        ImportedModel& output
    );

    //============================================================
    // Boneを持たないMeshをRigid Skin Meshへ変換
    // ※Scene全体がSkinned Modelの場合
    // ※Boneを持たないMeshにもSkinWeightが必要
    // ※そのMeshを保持しているaiNodeを1Boneとして登録する
    // BoneIndex = Node
    // Weight    = 1.0
    //============================================================
    static bool ImportRigidMeshSkinning(
        const aiNode* node,
        const aiMatrix4x4& globalTransform,
        ImportedMesh& importedMesh,
        ImportedModel& model
    );
};