#include "ModelImporter.h"

//============================================================
// Assimp
//============================================================
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/material.h>


//============================================================
// Texture Resolver
//============================================================
#include "TextureResolver.h"


//============================================================
// Standard Library
//============================================================
#include <iostream>


void ModelImporter::DebugPrintTextureInfo(const aiScene* scene)
{
    if (scene == nullptr)
    {
        return;
    }


    std::cout
        << "\n========================================\n"
        << " Texture Debug\n"
        << "========================================\n";


    //========================================================
    // FBX内部に埋め込まれているTexture一覧
    //========================================================
    std::cout
        << "Embedded Textures : "
        << scene->mNumTextures
        << "\n";


    for (unsigned int textureIndex = 0; textureIndex < scene->mNumTextures; ++textureIndex)
    {
        const aiTexture* texture = scene->mTextures[textureIndex];

        if (texture == nullptr)
        {
            continue;
        }


        std::cout
            << "\n[Embedded Texture "
            << textureIndex
            << "]\n";


        //====================================================
        // mFilename
        //====================================================
        std::cout
            << "  Filename   : "
            << texture->mFilename.C_Str()
            << "\n";


        //====================================================
        // Format Hint
        //====================================================
        std::cout
            << "  FormatHint : "
            << texture->achFormatHint
            << "\n";


        //====================================================
        // mHeight == 0
        // PNG/JPEG/DDSなどの圧縮済みファイルが
        // そのままFBX内部へ格納されているmWidthはByte数になる
        //====================================================
        if (texture->mHeight == 0)
        {
            std::cout
                << "  Type       : Compressed\n"
                << "  DataSize   : "
                << texture->mWidth
                << " bytes\n";
        }
        else
        {
            //================================================
            // mHeight != 0
            // Assimpが展開済みのRGBA Pixelとして保持
            //================================================
            std::cout
                << "  Type       : Raw RGBA\n"
                << "  Width      : "
                << texture->mWidth
                << "\n"
                << "  Height     : "
                << texture->mHeight
                << "\n";
        }
    }


    //========================================================
    // MaterialごとのTexture Slot
    //========================================================
    for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
    {
        const aiMaterial* material = scene->mMaterials[materialIndex];

        if (material == nullptr)
        {
            continue;
        }


        aiString materialName;

        material->Get(AI_MATKEY_NAME, materialName);

        std::cout
            << "\n----------------------------------------\n"
            << "[Material "
            << materialIndex
            << "] "
            << materialName.C_Str()
            << "\n";


        //====================================================
        // 調査対象Texture Type
        // FBXによってBaseColorがDIFFUSEへ入ることもあれば
        // BASE_COLORへ入ることもあるので両方見る
        //====================================================
        struct TextureTypeDebug
        {
            aiTextureType Type;
            const char* Name;
        };


        const TextureTypeDebug textureTypes[] =
        {
            {
                aiTextureType_BASE_COLOR,
                "BASE_COLOR"
            },

            {
                aiTextureType_DIFFUSE,
                "DIFFUSE"
            },

            {
                aiTextureType_NORMALS,
                "NORMALS"
            },

            {
                aiTextureType_NORMAL_CAMERA,
                "NORMAL_CAMERA"
            },

            {
                aiTextureType_METALNESS,
                "METALNESS"
            },

            {
                aiTextureType_DIFFUSE_ROUGHNESS,
                "ROUGHNESS"
            },

            {
                aiTextureType_AMBIENT_OCCLUSION,
                "AMBIENT_OCCLUSION"
            },

            {
                aiTextureType_EMISSIVE,
                "EMISSIVE"
            }
        };


        for (const TextureTypeDebug& typeInfo : textureTypes)
        {
            const unsigned int count = material->GetTextureCount(typeInfo.Type);


            if (count == 0)
            {
                continue;
            }

            std::cout
                << "  "
                << typeInfo.Name
                << " : "
                << count
                << "\n";

            for (unsigned int textureSlot = 0; textureSlot < count; ++textureSlot)
            {
                aiString texturePath;


                const aiReturn result =
                    material->GetTexture(
                        typeInfo.Type,
                        textureSlot,
                        &texturePath
                    );


                if (result != AI_SUCCESS)
                {
                    continue;
                }


                std::cout
                    << "      ["
                    << textureSlot
                    << "] "
                    << texturePath.C_Str()
                    << "\n";


                //================================================
                // Embedded Textureか確認
                //================================================
                const aiTexture* embeddedTexture =
                    scene->GetEmbeddedTexture(texturePath.C_Str());

                std::cout
                    << "          Embedded : "
                    << (embeddedTexture != nullptr ? "Yes" : "No")
                    << "\n";
            }
        }
    }


    std::cout << "========================================\n";
}

//============================================================
// Model Import
//============================================================
bool ModelImporter::Import(
    const std::filesystem::path& filePath,
    ImportedModel& output
)
{
    //========================================================
    // 前回のImport結果を一度初期化する
    // ※同じvnImportedModelを再利用しても
    // ※前ModelのMeshやMaterialが残らないようにする
    //========================================================
    output = ImportedModel{};

    //========================================================
    // Assimp Importer
    // aiSceneはImporterが生存している間だけ有効
    //========================================================
    Assimp::Importer importer;

    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

    //========================================================
    // Import Flags
    //========================================================
    const unsigned int importFlags =
        //====================================================
        // Polygon → Triangle
        //====================================================
        aiProcess_Triangulate |

        //====================================================
        // 重複Vertex統合
        //====================================================
        aiProcess_JoinIdenticalVertices |

        //====================================================
        // Normal Mapping用Tangent生成
        //====================================================
        aiProcess_CalcTangentSpace |

        //====================================================
        // Vertex Cache最適化
        //====================================================
        aiProcess_ImproveCacheLocality |

        //====================================================
        // Primitive Type整理
        //====================================================
        aiProcess_SortByPType;


    //========================================================
    // File Load
    //========================================================
    const aiScene* scene = importer.ReadFile(filePath.string(), importFlags);


    //========================================================
    // Import Error
    //========================================================
    if (scene == nullptr ||scene->mRootNode == nullptr)
    {
        std::cerr
            << "Model Import Failed.\n"
            << "File : "
            << filePath.string()
            << "\n"
            << "Assimp Error : "
            << importer.GetErrorString()
            << "\n";

        return false;
    }

    DebugPrintTextureInfo(scene);

    //============================================================
    // FBX / glTF内部Texture
    //============================================================
    ImportEmbeddedTextures(scene, output);

    //============================================================
    // Material
    //============================================================
    ImportMaterials(scene, filePath, output);


    //============================================================
    // Full Skeleton
    //============================================================
    output.Bones.clear();

    output.SkinBones.clear();

    output.HasSkinning = false;


    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh* mesh = scene->mMeshes[meshIndex];

        if (mesh != nullptr && mesh->HasBones())
        {
            output.HasSkinning = true;

            break;
        }
    }

    ImportSkeleton(scene->mRootNode, -1, 0, output);

    //============================================================
    // Mesh
    //============================================================
    ImportMeshes(scene, output);

    //============================================================
    // Animation
    //============================================================
    ImportAnimations(scene, output);

    return true;
}

void ModelImporter::ImportMeshes(const aiScene* scene, ImportedModel& output)
{
    //========================================================
    // Safety Check
    //========================================================
    if (scene == nullptr || scene->mRootNode == nullptr)
    {
        return;
    }


    //========================================================
    // Node Treeを辿りながらpush_backするので一度空にしておく
    //========================================================
    output.Meshes.clear();

    //========================================================
    // Rootの親TransformはIdentity
    //========================================================
    aiMatrix4x4 identity;

    //========================================================
    // Root Nodeから再帰開始
    //========================================================
    ImportNodeMeshes(scene, scene->mRootNode, identity, output);
}

void ModelImporter::ImportNodeMeshes(const aiScene* scene, const aiNode* node, const aiMatrix4x4& parentTransform, ImportedModel& output)
{
    //========================================================
    // Safety Check
    //========================================================
    if (scene == nullptr || node == nullptr)
    {
        return;
    }

    //========================================================
    // Global Transform
    // Root
    //  ↓
    // Parent
    //  ↓
    // Current NodeのTransformを積み上げる
    //========================================================
    const aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;


    //========================================================
    // Normal Transform
    // ※Positionは4x4 Matrixで変換できるが
    // ※Normalへ同じMatrixをそのまま掛けると
    // ※Non-Uniform Scaleが存在する場合に方向が壊れる
    // ※Normalにはinverse(transpose(M))を使用する
    //========================================================
    aiMatrix3x3 normalMatrix(globalTransform);

    normalMatrix.Inverse();

    normalMatrix.Transpose();


    //========================================================
    // Tangent / Bitangent用
    // Tangentは方向VectorなのでTranslationは不要
    // Global Transformの3x3部分だけ使う
    //========================================================
    const aiMatrix3x3 directionMatrix(globalTransform);

    //========================================================
    // このNodeが参照しているMeshをImport
    //========================================================
    for (unsigned int nodeMeshIndex = 0; nodeMeshIndex < node->mNumMeshes; ++nodeMeshIndex)
    {
        //====================================================
        // node->mMeshes[]には、
        // scene->mMeshes[]へのIndexが入っている
        //====================================================
        const unsigned int sceneMeshIndex = node->mMeshes[nodeMeshIndex];

        if (sceneMeshIndex >= scene->mNumMeshes)
        {
            continue;
        }

        const aiMesh* sourceMesh = scene->mMeshes[sceneMeshIndex];

        if (sourceMesh == nullptr)
        {
            continue;
        }

        const bool isSkinned = sourceMesh->HasBones();

        std::cout
            << "[Mesh Import] "
            << sourceMesh->mName.C_Str()
            << "  Vertices="
            << sourceMesh->mNumVertices
            << "  Bones="
            << sourceMesh->mNumBones
            << "  Skinned="
            << (isSkinned ? "Yes" : "No")
            << "\n";

        //====================================================
        // 新しいImportedMeshを作る
        // ※aiMesh単位ではなくNode参照単位にすることで
        // ※同じMeshを複数Nodeで使用している場合も
        // ※それぞれ正しいTransformでImportできる
        //====================================================
        ImportedMesh destinationMesh;

        //====================================================
        // Mesh Name
        //====================================================
        destinationMesh.Name = sourceMesh->mName.C_Str();

        //====================================================
        // Material
        //====================================================
        destinationMesh.MaterialIndex = sourceMesh->mMaterialIndex;

        //====================================================
        // Vertex数確保
        //====================================================
        destinationMesh.Vertices.resize(sourceMesh->mNumVertices);

        //====================================================
        // Vertex Import
        //====================================================
        for (unsigned int vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex)
        {
            ImportedVertex& destination = destinationMesh.Vertices[vertexIndex];

            //============================================================
            // Position
            //============================================================
            if (sourceMesh->HasPositions())
            {
                aiVector3D position = sourceMesh->mVertices[vertexIndex];

                position = globalTransform * position;

                destination.Position =
                {
                    position.x,
                    position.y,
                    position.z
                };
            }


            //================================================
            // Normal
            //================================================
            if (sourceMesh->HasNormals())
            {
                aiVector3D normal = sourceMesh->mNormals[vertexIndex];

                //============================================
                // inverse transposeで変換
                //============================================
                    normal = normalMatrix * normal;

                //============================================
                // Matrix変換後は再正規化
                //============================================
                normal.Normalize();

                destination.Normal =
                {
                    normal.x,
                    normal.y,
                    normal.z
                };
            }


            //================================================
            // UV0
            // UVにはNode Transformを適用しない
            //================================================
            if (sourceMesh->HasTextureCoords(0))
            {
                const aiVector3D& uv = sourceMesh->mTextureCoords[0][vertexIndex];

                destination.TexCoord =
                {
                    uv.x,
                    uv.y
                };
            }


            //================================================
            // Vertex Color
            //================================================
            if (output.ImportVertexColors && sourceMesh->HasVertexColors(0))
            {
                const aiColor4D& color = sourceMesh->mColors[0][vertexIndex];

                destination.Color =
                {
                    color.r,
                    color.g,
                    color.b,
                    color.a
                };
            }
            else
            {
                destination.Color =
                {
                    1.0f,
                    1.0f,
                    1.0f,
                    1.0f
                };
            }


            //================================================
            // Tangent
            // Normal Map対応時に使用
            //================================================
            if (sourceMesh->HasTangentsAndBitangents())
            {
                aiVector3D tangent = sourceMesh->mTangents[vertexIndex];

                aiVector3D bitangent = sourceMesh->mBitangents[vertexIndex];


                //============================================
                // Translationを除いた3x3 Matrixで方向変換
                //============================================
                tangent = directionMatrix * tangent;

                bitangent = directionMatrix * bitangent;

                tangent.Normalize();

                bitangent.Normalize();


                //============================================
                // 変換後Normalを取得
                //============================================
                aiVector3D normal(
                    destination.Normal.x,
                    destination.Normal.y,
                    destination.Normal.z
                );


                normal.Normalize();


                //============================================
                // TangentをNormalに対して再直交化
                // T = normalize( T - N * dot(N,T))
                // Scaleなどがある場合の誤差を抑える
                //============================================
                tangent = tangent - normal * (normal * tangent);

                tangent.Normalize();

                //============================================
                // Tangent Space Handedness
                // B ≒ cross(N,T) * w
                // wを+1/-1として保存する
                //============================================
                const aiVector3D calculatedBitangent = normal ^ tangent;

                const float handedness =
                    (calculatedBitangent * bitangent) < 0.0f ? -1.0f : 1.0f;


                destination.Tangent =
                {
                    tangent.x,
                    tangent.y,
                    tangent.z,
                    handedness
                };
            }
        }


        //====================================================
        // Index
        // ※Local Indexのまま保持
        // ※VNM Writer側でlocalIndex + vertexBaseとしてGlobal Index化する
        //====================================================
        destinationMesh.Indices.reserve(sourceMesh->mNumFaces * 3u);

        for (unsigned int faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex)
        {
            const aiFace& face = sourceMesh->mFaces[faceIndex];

            //================================================
            // Import時にaiProcess_Triangulateしているので基本3になる
            //================================================
            if (face.mNumIndices != 3u)
            {
                continue;
            }

            destinationMesh.Indices.push_back(face.mIndices[0]);
            destinationMesh.Indices.push_back(face.mIndices[1]);
            destinationMesh.Indices.push_back(face.mIndices[2]);
        }


        //====================================================
        // Skinning有無
        // ※Bone対応時にはTransform Bakeの扱いを
        // ※別途変更する必要がある
        // Static Mesh確認用
        //====================================================
        if (isSkinned)
        {
            //========================================================
            // 通常のSkinned Mesh
            // aiBone Weightをそのまま取り込む
            //========================================================
            const bool skinningResult =
                ImportMeshSkinning(
                    sourceMesh,
                    globalTransform,
                    destinationMesh,
                    output
                );


            if (!skinningResult)
            {
                std::cerr
                    << "[ModelImporter] "
                    << "Skinning Import Failed.\n"
                    << "Mesh : "
                    << destinationMesh.Name
                    << "\n";

                continue;
            }
        }
        else if(output.HasSkinning)
        {
            const bool rigidResult =
                ImportRigidMeshSkinning(
                    node,
                    globalTransform,
                    destinationMesh,
                    output
                );


            if (!rigidResult)
            {
                std::cerr
                    << "[ModelImporter] "
                    << "Rigid Skinning Import Failed.\n"
                    << "Mesh : "
                    << destinationMesh.Name
                    << "\n";

                continue;
            }
        }

        //====================================================
        // 完成したMeshを追加
        //====================================================
        output.Meshes.push_back(std::move(destinationMesh));
    }

    //========================================================
    // Child Nodeへ再帰
    //========================================================
    for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
    {
        ImportNodeMeshes(
            scene,
            node->mChildren[childIndex],
            globalTransform,
            output
        );
    }
}


void ModelImporter::ImportMaterials(
    const aiScene* scene,
    const std::filesystem::path& modelPath,
    ImportedModel& output
)
{
    if (scene == nullptr)
    {
        return;
    }

    //============================================================
    // Material数確保
    //============================================================
    output.Materials.resize(scene->mNumMaterials);

    //============================================================
    // Assimp Material → ImportedMaterial
    //============================================================
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* source = scene->mMaterials[i];


        if (source == nullptr)
        {
            continue;
        }

        ImportedMaterial& destination = output.Materials[i];

        //========================================================
        // Material Name
        //========================================================
        aiString materialName;

        source->Get(AI_MATKEY_NAME, materialName);

        destination.Name = materialName.C_Str();

        //========================================================
        // Base Color
        //========================================================
        aiColor4D baseColor;


        if (aiGetMaterialColor(source, AI_MATKEY_BASE_COLOR, &baseColor) == AI_SUCCESS)
        {
            destination.BaseColor =
            {
                baseColor.r,
                baseColor.g,
                baseColor.b,
                baseColor.a
            };
        }
        else if (aiGetMaterialColor(source, AI_MATKEY_COLOR_DIFFUSE, &baseColor) == AI_SUCCESS)
        {
            destination.BaseColor =
            {
                baseColor.r,
                baseColor.g,
                baseColor.b,
                baseColor.a
            };
        }

        //========================================================
        // Metallic
        //========================================================
        float metallic = 0.0f;

        if (source->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
        {
            destination.Metallic = metallic;
        }

        //========================================================
        // Roughness
        //========================================================
        float roughness = 1.0f;

        if (source->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
        {
            destination.Roughness = roughness;
        }


        //========================================================
        // Texture群を自動解決
        //========================================================
        const bool allowLooseTextureFallback = scene->mNumMaterials == 1;

        destination.SourceTextures =
            TextureResolver::Resolve(
                source,
                modelPath,
                allowLooseTextureFallback
            );
    }
}

void ModelImporter::ImportSkeleton(const aiNode* node, int parentIndex, int generation, ImportedModel& output)
{
    if (node == nullptr)
    {
        return;
    }

    ImportedBone bone;

    //========================================================
    // Name
    //========================================================
    bone.Name = node->mName.C_Str();

    //========================================================
    // Hierarchy
    //========================================================
    bone.ParentIndex = parentIndex;
    bone.Generation = generation;


    //========================================================
    // Local Matrixを分解
    // Assimp:aiNode::mTransformationはParentから見たLocal Transform
    //========================================================
    aiVector3D scale;

    aiQuaternion rotation;

    aiVector3D position;


    node->mTransformation.Decompose(
        scale,
        rotation,
        position
    );


    //========================================================
    // Translation
    //========================================================
    bone.Position =
    {
        position.x,
        position.y,
        position.z
    };


    //========================================================
    // Quaternion
    // aiQuaternion: w, x, y, z
    // XMFLOAT4: x, y, z, w
    //========================================================
    bone.Rotation =
    {
        rotation.x,
        rotation.y,
        rotation.z,
        rotation.w
    };


    //========================================================
    // Scale
    //========================================================
    bone.Scale =
    {
        scale.x,
        scale.y,
        scale.z
    };

    //========================================================
    // .bone用Euler Radian
    //========================================================
    bone.EulerRotation = QuaternionToEuler(bone.Rotation);

    //========================================================
    // BoneのIndexを確定してからChildへ進む
    //========================================================
    const int currentIndex = static_cast<int>(output.Bones.size());

    {
        using namespace DirectX;


        //========================================================
        // Assimp Original
        // ToXMFLOAT4X4()は現在Transpose版なので
        // DirectX row-vector形式へ変換済みの正解Matrix
        //========================================================
        const XMFLOAT4X4 exactLocal = ToXMFLOAT4X4(node->mTransformation);


        //========================================================
        // Imported Quaternion
        // x y z w
        //========================================================
        XMVECTOR rawQuaternion =
            XMVectorSet(
                bone.Rotation.x,
                bone.Rotation.y,
                bone.Rotation.z,
                bone.Rotation.w
            );


        rawQuaternion = XMQuaternionNormalize(rawQuaternion);


        //========================================================
        // Raw Quaternionから再構築
        // vnObject::calculateLocalMatrix()と同じ順番:
        // Scale * Rotation * Translation
        //========================================================
        const XMMATRIX rawMatrix =
            XMMatrixScaling(bone.Scale.x, bone.Scale.y, bone.Scale.z) *
            XMMatrixRotationQuaternion(rawQuaternion) *
            XMMatrixTranslation(bone.Position.x, bone.Position.y, bone.Position.z);

        XMFLOAT4X4 rawLocal = {};

        XMStoreFloat4x4(&rawLocal, rawMatrix);


        //========================================================
        // ※QuaternionをConjugateした場合も確認
        // ※Assimp → DirectXでQuaternionの向きが逆なら
        // ※こちらの方がOriginalへ近くなる
        //========================================================
        const XMVECTOR conjugatedQuaternion = XMQuaternionConjugate(rawQuaternion);


        const XMMATRIX conjugatedMatrix =
            XMMatrixScaling(bone.Scale.x, bone.Scale.y, bone.Scale.z) *
            XMMatrixRotationQuaternion(conjugatedQuaternion) *
            XMMatrixTranslation(bone.Position.x, bone.Position.y, bone.Position.z);

        XMFLOAT4X4 conjugatedLocal = {};

        XMStoreFloat4x4(&conjugatedLocal, conjugatedMatrix);


        //========================================================
        // 誤差
        //========================================================
        const float rawError = CalculateMatrixMaxError(exactLocal, rawLocal);

        const float conjugatedError = CalculateMatrixMaxError(exactLocal, conjugatedLocal);

        //========================================================
        // 誤差が大きいBoneだけ出力
        // 例:
        // Face
        // ShoulderPad.L
        // ShoulderPad.R
        // Warrior_Sword
        //========================================================
        if (rawError > 0.001f || conjugatedError > 0.001f)
        {
            std::cout
                << "[Bone Matrix Check] "
                << bone.Name
                << "\n"
                << "    Raw Error       = "
                << rawError
                << "\n"
                << "    Conjugate Error = "
                << conjugatedError
                << "\n";
        }
    }

    output.Bones.push_back(bone);

    //========================================================
    // Children
    //========================================================
    for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
    {
        ImportSkeleton(
            node->mChildren[childIndex],
            currentIndex,
            generation + 1,
            output
        );
    }
}

bool ModelImporter::ImportMeshSkinning(const aiMesh* mesh,
    const aiMatrix4x4& meshGlobalTransform,
    ImportedMesh& importedMesh,
    ImportedModel& model)
{
    if (mesh == nullptr || !mesh->HasBones())
    {
        return true;
    }

    //========================================================
    // VertexごとのInfluence一覧
    // AssimpはBone側にWeightが入っているので
    // まずVertex側へ反転して集める
    //========================================================
    std::vector<std::vector<VertexInfluence>>influences(mesh->mNumVertices);

    //========================================================
    // 全aiBone
    //========================================================
    for (unsigned int assimpBoneIndex = 0; assimpBoneIndex < mesh->mNumBones; ++assimpBoneIndex)
    {
        const aiBone* assimpBone = mesh->mBones[assimpBoneIndex];

        if (assimpBone == nullptr)
        {
            continue;
        }

        const std::string boneName = assimpBone->mName.C_Str();

        //====================================================
        // Full Skeleton側のIndexを検索
        //====================================================
        const int skeletonIndex = FindSkeletonBoneIndex(model, boneName);


        if (skeletonIndex < 0)
        {
            std::cerr
                << "[Skin] Skeleton bone not found : "
                << boneName
                << "\n";

            return false;
        }


        //====================================================
        // Global Skin Boneを検索
        // ※複数Meshで同じBoneが使用される場合に重複登録しない
        //====================================================
        int skinBoneIndex = -1;


        for (std::size_t i = 0; i < model.SkinBones.size(); ++i)
        {
            if (model.SkinBones[i].Name == boneName)
            {
                skinBoneIndex = static_cast<int>(i);

                break;
            }
        }


        //====================================================
        // 未登録なら追加
        //====================================================
        if (skinBoneIndex < 0)
        {
            if (model.SkinBones.size() >= 65535)
            {
                std::cerr << "[Skin] Too many skin bones.\n";

                return false;
            }


            ImportedSkinBone skinBone;

            skinBone.Name = boneName;
            skinBone.SkeletonIndex = static_cast<std::uint16_t>(skeletonIndex);


            //============================================================
            // Mesh Global Transformの逆行列
            //============================================================
            aiMatrix4x4 inverseMeshGlobal = meshGlobalTransform;

            inverseMeshGlobal.Inverse();

            const aiMatrix4x4 adjustedOffset = assimpBone->mOffsetMatrix * inverseMeshGlobal;

            skinBone.InverseBind = ToXMFLOAT4X4(adjustedOffset);

            skinBoneIndex = static_cast<int>(model.SkinBones.size());

            model.SkinBones.push_back(skinBone);
        }


        //====================================================
        // このBoneが影響するVertexへWeightを登録
        //====================================================
        for (unsigned int weightIndex = 0; weightIndex < assimpBone->mNumWeights; ++weightIndex)
        {
            const aiVertexWeight& weight = assimpBone->mWeights[weightIndex];

            if (weight.mVertexId >= mesh->mNumVertices)
            {
                continue;
            }


            if (weight.mWeight <= 0.0f)
            {
                continue;
            }


            influences[weight.mVertexId].push_back(VertexInfluence
                {
                    static_cast<std::uint16_t>(skinBoneIndex),
                    weight.mWeight
                }
            );
        }
    }

    //============================================================
    // 各Vertex
    // InfluenceをWeight順に並べ大きい4つだけ残す
    //============================================================
    for (std::size_t vertexIndex = 0; vertexIndex < importedMesh.Vertices.size(); ++vertexIndex)
    {
        std::vector<VertexInfluence>& vertexInfluences = influences[vertexIndex];

        //========================================================
        // Weightの大きい順
        //========================================================
        std::sort(vertexInfluences.begin(), vertexInfluences.end(),
            [](const VertexInfluence& a, const VertexInfluence& b)
            {
                return  a.Weight > b.Weight;
            }
        );


        //========================================================
        // 最大4つ
        //========================================================
        if (vertexInfluences.size() > 4)
        {
            vertexInfluences.resize(4);
        }

        ImportedVertex& vertex = importedMesh.Vertices[vertexIndex];

        //========================================================
        // 初期化
        //========================================================
        for (int influenceIndex = 0; influenceIndex < 4; ++influenceIndex)
        {
            vertex.BoneIndices[influenceIndex] = 0;
            vertex.BoneWeights[influenceIndex] = 0.0f;
        }


        //========================================================
        // Weight合計
        //========================================================
        float totalWeight = 0.0f;


        for (const VertexInfluence& influence : vertexInfluences)
        {
            totalWeight += influence.Weight;
        }


        //========================================================
        // Weightが無いVertex
        // ※Skinned Meshでは基本的には発生しないが
        // ※保険としてBone0へWeight1を設定する
        //========================================================
        if (totalWeight <= 0.000001f)
        {
            vertex.BoneIndices[0] = 0;
            vertex.BoneWeights[0] = 1.0f;


            continue;
        }


        //========================================================
        // Top4を正規化して格納
        // ※4本へ削った場合:Weight合計が1.0未満になるため再正規化する
        //========================================================
        for (std::size_t influenceIndex = 0; influenceIndex < vertexInfluences.size(); ++influenceIndex)
        {
            vertex.BoneIndices[influenceIndex] = vertexInfluences[influenceIndex].BoneIndex;
            vertex.BoneWeights[influenceIndex] = vertexInfluences[influenceIndex].Weight / totalWeight;
        }
    }

    DirectX::XMFLOAT3 minPosition
    {
        FLT_MAX,
        FLT_MAX,
        FLT_MAX
    };

    DirectX::XMFLOAT3 maxPosition
    {
        -FLT_MAX,
        -FLT_MAX,
        -FLT_MAX
    };


    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const ImportedVertex& vertex : mesh.Vertices)
        {
            minPosition.x = (std::min)(minPosition.x, vertex.Position.x);
            minPosition.y = (std::min)(minPosition.y, vertex.Position.y);
            minPosition.z = (std::min)(minPosition.z, vertex.Position.z);

            maxPosition.x = (std::max)(maxPosition.x, vertex.Position.x);
            maxPosition.y = (std::max)(maxPosition.y, vertex.Position.y);
            maxPosition.z = (std::max)(maxPosition.z, vertex.Position.z);
        }
    }


    std::cout
        << "\n========================================\n"
        << " Model Bounds\n"
        << "========================================\n"
        << "Min : "
        << minPosition.x << ", "
        << minPosition.y << ", "
        << minPosition.z << "\n"
        << "Max : "
        << maxPosition.x << ", "
        << maxPosition.y << ", "
        << maxPosition.z << "\n"
        << "Size: "
        << maxPosition.x - minPosition.x << ", "
        << maxPosition.y - minPosition.y << ", "
        << maxPosition.z - minPosition.z << "\n"
        << "========================================\n";

    return true;
}

void ModelImporter::ImportAnimations(const aiScene* scene, ImportedModel& output)
{
    //========================================================
    // 前回結果を破棄
    //========================================================
    output.Animations.clear();

    //========================================================
    // Safety Check
    //========================================================
    if (scene == nullptr)
    {
        return;
    }


    //========================================================
    // Animationなし
    // ※Static ModelやAnimationなしFBXでは正常な状態
    //========================================================
    if (!scene->HasAnimations() || scene->mNumAnimations == 0)
    {
        std::cout
            << "\n========================================\n"
            << " Animation Import\n"
            << "========================================\n"
            << "Animations : 0\n"
            << "========================================\n";

        return;
    }

    //========================================================
    // Animation数を先に確保
    //========================================================
    output.Animations.reserve(scene->mNumAnimations);

    std::cout
        << "\n========================================\n"
        << " Animation Import\n"
        << "========================================\n"
        << "Animations : "
        << scene->mNumAnimations
        << "\n";


    //========================================================
    // 全Animation Clip
    //========================================================
    for (unsigned int animationIndex = 0;animationIndex < scene->mNumAnimations;++animationIndex)
    {
        const aiAnimation* sourceAnimation = scene->mAnimations[animationIndex];

        if (sourceAnimation == nullptr)
        {
            continue;
        }

        ImportedAnimation destinationAnimation;

        //====================================================
        // Animation Name
        //====================================================
        destinationAnimation.Name = sourceAnimation->mName.C_Str();

        //====================================================
        // FBXによってはAnimation名が空の場合があるため
        // DebugしやすいFallback名を付ける
        //====================================================
        if (destinationAnimation.Name.empty())
        {
            destinationAnimation.Name = "Animation_" + std::to_string(animationIndex);
        }


        //====================================================
        // Ticks Per Second
        // Assimp Animation Time: key.mTime / ticksPerSecond
        // で秒へ変換できる
        // ※mTicksPerSecond == 0 のAssetも存在するため
        // ※その場合のみFallback値を使用する
        //====================================================
        double ticksPerSecond = sourceAnimation->mTicksPerSecond;

        if (ticksPerSecond <= 0.000001)
        {
            //================================================
            // Assimp系Importerでよく使用されるFallback
            // 本来Asset側にFPS情報があることが望ましいため
            // Warningも表示する
            //================================================
            ticksPerSecond = 25.0;


            std::cout
                << "[Animation Warning] "
                << destinationAnimation.Name
                << " has no TicksPerSecond. "
                << "Fallback = 25.0\n";
        }

        destinationAnimation.TicksPerSecond = ticksPerSecond;

        //====================================================
        // Animation Duration
        // Assimp: Tick
        // Converter: Seconds
        //====================================================
        destinationAnimation.DurationSeconds = sourceAnimation->mDuration / ticksPerSecond;

        //====================================================
        // Bone Animation Channel数
        //====================================================
        destinationAnimation.Channels.reserve( sourceAnimation->mNumChannels);

        //====================================================
        // 実際のKeyから得た最大Time
        // FBXによってmDurationと最後のKeyが
        // 微妙に一致しない場合への保険
        //====================================================
        double maximumKeyTime = 0.0;

        //====================================================
        // 全Node Animation
        //====================================================
        for (unsigned int channelIndex = 0;channelIndex < sourceAnimation->mNumChannels;++channelIndex)
        {
            const aiNodeAnim* sourceChannel = sourceAnimation->mChannels[channelIndex];

            if (sourceChannel == nullptr)
            {
                continue;
            }

            ImportedAnimationChannel destinationChannel;

            //================================================
            // Animation対象Node名
            //================================================
            destinationChannel.NodeName = sourceChannel->mNodeName.C_Str();


            //================================================
            // Full Skeletonとの対応を取得
            //================================================
            destinationChannel.SkeletonIndex =
                FindSkeletonBoneIndex(output, destinationChannel.NodeName);


            //================================================
            // Full SkeletonはaiNode Tree全体を保存しているため
            // 通常ここで失敗するのは不自然
            // ※ただしImport自体を全停止させず
            // ※ChannelだけSkipして原因をLogへ出す
            //================================================
            if (destinationChannel.SkeletonIndex < 0)
            {
                std::cout
                    << "[Animation Warning] "
                    << "Skeleton node not found.\n"
                    << "    Animation : "
                    << destinationAnimation.Name
                    << "\n"
                    << "    Node      : "
                    << destinationChannel.NodeName
                    << "\n";


                continue;
            }

            //================================================
            // Position Keys
            //================================================
            destinationChannel.PositionKeys.reserve(sourceChannel->mNumPositionKeys);

            for (unsigned int keyIndex = 0; keyIndex < sourceChannel->mNumPositionKeys; ++keyIndex)
            {
                const aiVectorKey& sourceKey = sourceChannel->mPositionKeys[keyIndex];

                ImportedVectorKey destinationKey;

                //============================================
                // Tick → Seconds
                //============================================
                destinationKey.Time = sourceKey.mTime / ticksPerSecond;

                destinationKey.Value =
                {
                    sourceKey.mValue.x,
                    sourceKey.mValue.y,
                    sourceKey.mValue.z
                };

                maximumKeyTime = (std::max)(maximumKeyTime, destinationKey.Time);

                destinationChannel.PositionKeys.push_back(destinationKey);
            }

            //================================================
            // Rotation Keys
            // Assimp Quaternion: w, x, y, z
            // DirectX XMFLOAT4: x, y, z, w
            //================================================
            destinationChannel.RotationKeys.reserve(sourceChannel->mNumRotationKeys);

            for (unsigned int keyIndex = 0; keyIndex < sourceChannel->mNumRotationKeys; ++keyIndex)
            {
                const aiQuatKey& sourceKey = sourceChannel->mRotationKeys[keyIndex];

                ImportedQuaternionKey destinationKey;

                destinationKey.Time = sourceKey.mTime / ticksPerSecond;

                //============================================
                // Assimp Quaternion
                //        ↓
                // DirectX Quaternion
                //============================================
                DirectX::XMVECTOR quaternion =
                    DirectX::XMVectorSet(
                        sourceKey.mValue.x,
                        sourceKey.mValue.y,
                        sourceKey.mValue.z,
                        sourceKey.mValue.w
                    );


                //============================================
                // ※念のためNormalize
                //============================================
                quaternion = DirectX::XMQuaternionNormalize(quaternion);

                DirectX::XMStoreFloat4(&destinationKey.Value, quaternion);

                //============================================
                // Quaternion Sign Continuity
                // 前KeyとのDotが負なら
                // 今Keyを反転して同じ半球へ揃える
                //============================================
                if (!destinationChannel.RotationKeys.empty())
                {
                    const DirectX::XMFLOAT4& previous = destinationChannel.RotationKeys.back().Value;


                    const float dot =
                        previous.x * destinationKey.Value.x +
                        previous.y * destinationKey.Value.y +
                        previous.z * destinationKey.Value.z +
                        previous.w * destinationKey.Value.w;


                    if (dot < 0.0f)
                    {
                        destinationKey.Value.x = -destinationKey.Value.x;
                        destinationKey.Value.y = -destinationKey.Value.y;
                        destinationKey.Value.z = -destinationKey.Value.z;
                        destinationKey.Value.w = -destinationKey.Value.w;
                    }
                }

                maximumKeyTime = (std::max)(maximumKeyTime, destinationKey.Time);

                destinationChannel.RotationKeys.push_back(destinationKey);
            }

            //================================================
            // Scale Keys
            //================================================
            destinationChannel.ScaleKeys.reserve(sourceChannel->mNumScalingKeys);

            for (unsigned int keyIndex = 0; keyIndex < sourceChannel->mNumScalingKeys; ++keyIndex)
            {
                const aiVectorKey& sourceKey = sourceChannel->mScalingKeys[keyIndex];

                ImportedVectorKey destinationKey;

                destinationKey.Time = sourceKey.mTime / ticksPerSecond;

                destinationKey.Value =
                {
                    sourceKey.mValue.x,
                    sourceKey.mValue.y,
                    sourceKey.mValue.z
                };

                maximumKeyTime = (std::max)(maximumKeyTime, destinationKey.Time);

                destinationChannel.ScaleKeys.push_back(destinationKey);
            }


            //================================================
            // 完成したNode Channel
            //================================================
            destinationAnimation.Channels.push_back(std::move(destinationChannel));
        }


        //====================================================
        // mDurationより実Keyの方が長かった場合の保険
        //====================================================
        destinationAnimation.DurationSeconds =
            (std::max)(destinationAnimation.DurationSeconds, maximumKeyTime);


        //====================================================
        // Debug
        //====================================================
        std::cout
            << "\n[Animation "
            << animationIndex
            << "]\n"
            << "    Name           : "
            << destinationAnimation.Name
            << "\n"
            << "    Duration       : "
            << destinationAnimation.DurationSeconds
            << " sec\n"
            << "    TicksPerSecond : "
            << destinationAnimation.TicksPerSecond
            << "\n"
            << "    Channels       : "
            << destinationAnimation.Channels.size()
            << "\n";


        //====================================================
        // Clip登録
        //====================================================
        output.Animations.push_back(std::move(destinationAnimation));
    }


    std::cout
        << "\n========================================\n"
        << " Animation Import Complete\n"
        << "========================================\n"
        << "Imported Animations : "
        << output.Animations.size()
        << "\n"
        << "========================================\n";
}

void ModelImporter::ImportEmbeddedTextures(const aiScene* scene, ImportedModel& output)
{
    //========================================================
    // Sceneが無い場合は処理できない
    //========================================================
    if (scene == nullptr)
    {
        return;
    }

    //========================================================
    // 前回ImportしたTextureを破棄
    //========================================================
    output.EmbeddedTextures.clear();

    //========================================================
    // 必要数が分かっているので先に確保しておく
    //========================================================
    output.EmbeddedTextures.reserve(scene->mNumTextures);

    //========================================================
    // Scene内の全Embedded Textureを取得
    //========================================================
    for (unsigned int textureIndex = 0; textureIndex < scene->mNumTextures; ++textureIndex)
    {
        const aiTexture* source = scene->mTextures[textureIndex];

        if (source == nullptr)
        {
            continue;
        }

        ImportedEmbeddedTexture destination;

        //====================================================
        // Assimp Embedded Texture Index
        //====================================================
        destination.Index = static_cast<int>(textureIndex);

        //====================================================
        // 元Texture名
        //====================================================
        destination.Name = source->mFilename.C_Str();

        //====================================================
        // ※名前が空の場合はDebugしやすいようAssimp参照名を入れておく
        //====================================================
        if (destination.Name.empty())
        {
            destination.Name =
                "*" + std::to_string(textureIndex);
        }

        //====================================================
        // Format Hint
        //====================================================
        destination.Format = source->achFormatHint;

        //====================================================
        // 圧縮済みFile Data
        //====================================================
        if (source->mHeight == 0)
        {
            destination.IsCompressed = true;

            //================================================
            // 圧縮済みの場合は
            // Width / Heightは画像Decodeしていないので0
            //================================================
            destination.Width = 0;
            destination.Height = 0;

            //================================================
            // mWidth = Byte数
            //================================================
            const std::size_t dataSize = static_cast<std::size_t>(source->mWidth);

            destination.Data.resize(dataSize);

            //================================================
            // 圧縮File Dataをそのままコピー
            //================================================
            if (dataSize > 0 && source->pcData != nullptr)
            {
                std::memcpy(
                    destination.Data.data(),
                    reinterpret_cast<const void*>(source->pcData),
                    dataSize
                );
            }

            //================================================
            // Debug
            //================================================
            std::cout
                << "[EmbeddedTexture] "
                << "Index="
                << destination.Index
                << " Name="
                << destination.Name
                << " Compressed"
                << " Size="
                << dataSize
                << " bytes"
                << " Format="
                << destination.Format
                << "\n";
        }


        //====================================================
        // mHeight > 0
        // Assimpが展開済みのaiTexel配列として保持している
        //====================================================
        else
        {
            destination.IsCompressed = false;

            destination.Width = source->mWidth;
            destination.Height = source->mHeight;

            //================================================
            // Pixel数
            //================================================
            const std::size_t texelCount =
                static_cast<std::size_t>(source->mWidth) *
                static_cast<std::size_t>(source->mHeight);


            //================================================
            // aiTexel配列全体のByte数
            //================================================
            const std::size_t dataSize = texelCount * sizeof(aiTexel);

            destination.Data.resize(dataSize);

            if (dataSize > 0 && source->pcData != nullptr)
            {
                std::memcpy(
                    destination.Data.data(),
                    source->pcData,
                    dataSize
                );
            }


            //================================================
            // Debug
            //================================================
            std::cout
                << "[EmbeddedTexture] "
                << "Index="
                << destination.Index
                << " Name="
                << destination.Name
                << " Raw "
                << destination.Width
                << "x"
                << destination.Height
                << "\n";
        }


        //====================================================
        // ImportedModelへ登録
        //====================================================
        output.EmbeddedTextures.push_back(std::move(destination));
    }


    //========================================================
    // Import結果
    //========================================================
    std::cout
        << "[EmbeddedTexture Import] "
        << output.EmbeddedTextures.size()
        << " textures\n";
}

bool ModelImporter::ImportRigidMeshSkinning(const aiNode* node, const aiMatrix4x4& globalTransform, ImportedMesh& importedMesh, ImportedModel& model)
{
    //========================================================
   // Safety Check
   //========================================================
    if (node == nullptr)
    {
        return false;
    }


    if (importedMesh.Vertices.empty())
    {
        return true;
    }

    //========================================================
    // 今回このStatic Meshを追従させるNode
    //========================================================
    const std::string boneName = node->mName.C_Str();

    const int skeletonIndex = FindSkeletonBoneIndex(model, boneName);

    if (skeletonIndex < 0)
    {
        std::cerr
            << "[Rigid Skin] "
            << "Skeleton Node not found.\n"
            << "Node : "
            << boneName
            << "\n";

        return false;
    }


    //========================================================
    // BoneTableはuint16_tなのでRange確認
    //========================================================
    if (skeletonIndex > static_cast<int>((std::numeric_limits<std::uint16_t>::max)()))
    {
        std::cerr
            << "[Rigid Skin] "
            << "Skeleton Index overflow.\n";

        return false;
    }

    //========================================================
    // すでに同名SkinBoneが登録されていないか確認
    //========================================================
    int skinBoneIndex = -1;

    for (std::size_t i = 0; i < model.SkinBones.size(); ++i)
    {
        if (model.SkinBones[i].Name == boneName)
        {
            skinBoneIndex = static_cast<int>(i);

            break;
        }
    }

    //========================================================
    // SkinBone未登録なら追加
    //========================================================
    if (skinBoneIndex < 0)
    {
        if (model.SkinBones.size() >= (std::numeric_limits<std::uint16_t>::max)())
        {
            std::cerr << "[Rigid Skin] Too many Skin Bones.\n";

            return false;
        }


        ImportedSkinBone skinBone;

        skinBone.Name = boneName;

        skinBone.SkeletonIndex = static_cast<std::uint16_t>(skeletonIndex);

        //====================================================
        // Rigid Mesh用Inverse Bind
        // このMeshのVertexは既存処理によって
        // globalTransformでModel SpaceへBakeされている
        // Bind Pose:
        // VertexModel * inverse(NodeBindGlobal) * NodeBindGlobal
        // = VertexModel
        //====================================================
        aiMatrix4x4 inverseBind = globalTransform;

        inverseBind.Inverse();

        skinBone.InverseBind = ToXMFLOAT4X4(inverseBind);

        skinBoneIndex = static_cast<int>(model.SkinBones.size());

        model.SkinBones.push_back(skinBone);
    }


    //========================================================
    // このMeshの全Vertexを1Boneへ100%追従させる
    // これをRigid Skinningとする
    // ※Bone0を適当に使うのではなく
    // ※このMeshを保持しているNodeへ追従させる
    //========================================================
    for (ImportedVertex& vertex : importedMesh.Vertices)
    {
        //====================================================
        // 全Influence初期化
        //====================================================
        for (int influence = 0; influence < 4; ++influence)
        {
            vertex.BoneIndices[influence] = 0;
            vertex.BoneWeights[influence] = 0.0f;
        }

        //====================================================
        // Node Boneへ100%
        //====================================================
        vertex.BoneIndices[0] = static_cast<std::uint16_t>(skinBoneIndex);

        vertex.BoneWeights[0] = 1.0f;
    }


    std::cout
        << "[Rigid Skin] "
        << importedMesh.Name
        << " -> "
        << boneName
        << "  SkinBone="
        << skinBoneIndex
        << "  SkeletonBone="
        << skeletonIndex
        << "\n";

    return true;
}
