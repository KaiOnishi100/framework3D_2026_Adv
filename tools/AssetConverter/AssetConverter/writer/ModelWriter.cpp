#include "ModelWriter.h"
#include "../texture/TextureProcessor.h"
//============================================================
// Standard Library
//============================================================
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>

bool ModelWriter::WriteStaticModel(const ImportedModel& model, const std::filesystem::path& outputFile)
{
    //========================================================
    // 基本チェック
    //========================================================
    if (model.Meshes.empty())
    {
        std::cerr << "[VNM Writer] Mesh is empty.\n";

        return false;
    }


    if (model.Materials.empty())
    {
        std::cerr << "[VNM Writer] Material is empty.\n";

        return false;
    }


    //========================================================
    // 今回はStatic Modelのみ
    //========================================================
    if (model.HasSkinning)
    {
        std::cerr << "[VNM Writer] " << "Skinned Model is not supported yet.\n";

        return false;
    }


    //========================================================
    // Output Directory
    //========================================================
    const std::filesystem::path outputDirectory = outputFile.parent_path();


    if (!outputDirectory.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(outputDirectory, error);

        if (error)
        {
            std::cerr << "[VNM Writer] " << "Could not create output directory.\n";
            return false;
        }
    }

    //========================================================
    // Model名
    //========================================================
    const std::wstring outputStem = outputFile.stem().wstring();

    //========================================================
    // Total Vertex / Index Count
    //========================================================
    std::uint64_t totalVertexCount = 0u;

    std::uint64_t totalIndexCount = 0u;

    for (const ImportedMesh& mesh : model.Meshes)
    {
        totalVertexCount += mesh.Vertices.size();

        totalIndexCount += mesh.Indices.size();
    }


    //========================================================
    // VNM HeaderではUINTなのでRange確認
    //========================================================
    if (totalVertexCount > (std::numeric_limits<UINT>::max)() ||
        totalIndexCount > (std::numeric_limits<UINT>::max)())
    {
        std::cerr << "[VNM Writer] Model is too large.\n";
        return false;
    }


    //========================================================
    // 現在のRuntime規約
    //========================================================
    const bool use16BitIndices = totalVertexCount <= 0xffffu;

    const std::uint32_t indexStride =
        use16BitIndices ? sizeof(std::uint16_t) : sizeof(std::uint32_t);

    //========================================================
    // Material V2生成
    //========================================================
    std::vector<vnModel_MaterialData> outputMaterials;

    outputMaterials.resize(model.Materials.size());

    for (std::size_t materialIndex = 0; materialIndex < model.Materials.size(); ++materialIndex)
    {
        const ImportedMaterial& source = model.Materials[materialIndex];

        vnModel_MaterialData& destination = outputMaterials[materialIndex];

        destination = {};

        //========================================================
        // Unity Unicode版Materialは144 bytes
        //========================================================
        destination.StructSize = sizeof(vnModel_MaterialData);

        //========================================================
        // Unity Exporterでは基本1Texture Slot
        //========================================================
        destination.TextureNum = 1u;

        destination.FloatNum = 16u;

        destination.Flag = 0u;


        //========================================================
        // Diffuse
        //========================================================
        destination.Diffuse[0] = source.BaseColor.x;
        destination.Diffuse[1] = source.BaseColor.y;
        destination.Diffuse[2] = source.BaseColor.z;
        destination.Diffuse[3] = source.BaseColor.w;


        //========================================================
        // Ambient
        //========================================================
        destination.Ambient[0] = 0.0f;
        destination.Ambient[1] = 0.0f;
        destination.Ambient[2] = 0.0f;
        destination.Ambient[3] = 0.0f;


        //========================================================
        // Specular
        //========================================================
        destination.Specular[0] = 0.0f;
        destination.Specular[1] = 0.0f;
        destination.Specular[2] = 0.0f;
        destination.Specular[3] = 50.0f;


        //========================================================
        // UV Tiling / Offset
        //========================================================
        destination.TilingOffset[0] = 1.0f;
        destination.TilingOffset[1] = 1.0f;
        destination.TilingOffset[2] = 0.0f;
        destination.TilingOffset[3] = 0.0f;


        //========================================================
        // BaseColor TextureだけV1へ保存
        //========================================================
        const std::wstring baseColorTexture = GetTextureFileName(source.Textures.BaseColor);

        CopyWideString(
            destination.Texture,
            baseColorTexture
        );
    }

    //============================================================
    // Mesh Data生成
    //============================================================
    std::vector<vnModel_MeshData> outputMeshes;

    outputMeshes.resize(model.Meshes.size());

    //============================================================
    // 現在のIndex Buffer上の開始位置
    // Mesh0 StartIndex = 0
    // Mesh1 StartIndex = Mesh0.IndexNum
    // Mesh2 StartIndex = Mesh0.IndexNum + Mesh1.IndexNum
    //============================================================
    UINT currentIndex = 0u;


    for (std::size_t meshIndex = 0; meshIndex < model.Meshes.size(); ++meshIndex)
    {
        const ImportedMesh& source = model.Meshes[meshIndex];


        //========================================================
        // Material Index確認
        //========================================================
        if (source.MaterialIndex >= model.Materials.size())
        {
            std::cerr << "[VNM Writer] Invalid Material Index.\n";

            return false;
        }

        vnModel_MeshData& destination = outputMeshes[meshIndex];

        //========================================================
        // 使用Material
        //========================================================
        destination.MaterialID = static_cast<UINT>(source.MaterialIndex);

        //========================================================
        // このMeshが使用するIndex数
        //========================================================
        destination.IndexNum = static_cast<UINT>(source.Indices.size());

        //========================================================
        // Index Buffer内の開始位置
        //========================================================
        destination.StartIndex = currentIndex;

        //========================================================
        // Unity版VNM互換
        // IndexそのものへVertex Base Offsetを加算するため
        // BaseVertexLocationとして使用するStartVertexは0
        //========================================================
        destination.StartVertex = 0u;

        //========================================================
        // 次MeshのStartIndexへ進める
        //========================================================
        currentIndex += static_cast<UINT>(source.Indices.size());
    }


    //========================================================
    // Vertex Data生成
    // 今回は既存VNM互換の48-byte Vertexを使用
    // Assimp Tangentは中間データには保持しているが
    // ※Runtime側のBuildGpuVerticesWithTangents()で
    // ※再生成するため今回は保存しない
    //========================================================
    std::vector<vnVertex3DFile> outputVertices;

    //========================================================
    // Index Data生成
    // Mesh内IndexはLocal Indexのまま保存する
    // DrawIndexedInstanced:
    // StartIndexLocation
    // BaseVertexLocation
    // にMeshのStartIndex / StartVertexを渡すため
    //========================================================
    std::vector<std::uint16_t> indices16;

    std::vector<std::uint32_t> indices32;

    outputVertices.reserve(static_cast<std::size_t>(totalVertexCount));

    //============================================================
    // Vertex Data生成
    //============================================================
    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const ImportedVertex& source : mesh.Vertices)
        {
            vnVertex3DFile destination = {};


            //====================================================
            // Position
            //====================================================
            destination.x = source.Position.x;
            destination.y = source.Position.y;
            destination.z = source.Position.z;

            //====================================================
            // Normal
            //====================================================
            destination.nx = source.Normal.x;
            destination.ny = source.Normal.y;
            destination.nz = source.Normal.z;


            //====================================================
            // Vertex Color
            //====================================================
            destination.r = source.Color.x;
            destination.g = source.Color.y;
            destination.b = source.Color.z;
            destination.a = source.Color.w;


            //====================================================
            // UV
            //====================================================
            destination.u = source.TexCoord.x;
            destination.v = 1.0f - source.TexCoord.y;


            //====================================================
            // VNM Vertex配列へ追加
            //====================================================
            outputVertices.push_back(destination);
        }
    }


    //============================================================
    // 念のためVertex数を確認
    //============================================================
    if (outputVertices.size() != totalVertexCount)
    {
        std::cerr
            << "[VNM Writer] "
            << "Vertex count mismatch.\n";

        return false;
    }

    //============================================================
    // 現在のMeshがVertex Buffer内のどこから始まるか
    //============================================================
    std::uint32_t vertexBase = 0u;

    for (const ImportedMesh& mesh : model.Meshes)
    {
        //========================================================
        // Mesh内IndexをGlobal Indexへ変換
        //========================================================
        for (const std::uint32_t localIndex : mesh.Indices)
        {
            //====================================================
            // Local IndexがMesh Vertex範囲内か確認
            //====================================================
            if (localIndex >= mesh.Vertices.size())
            {
                std::cerr << "[VNM Writer] Invalid Index.\n";

                return false;
            }

            //====================================================
            // VNM全Vertex Buffer内の番号へ変換する
            //====================================================
            const std::uint32_t globalIndex = vertexBase + localIndex;

            //====================================================
            // 16-bit Index
            //====================================================
            if (use16BitIndices)
            {
                //================================================
                // 念のためRange Check
                //================================================
                if (globalIndex > (std::numeric_limits<std::uint16_t>::max)())
                {
                    std::cerr
                        << "[VNM Writer] "
                        << "16-bit index overflow.\n";

                    return false;
                }

                indices16.push_back(static_cast<std::uint16_t>(globalIndex));
            }

            //====================================================
            // 32-bit Index
            //====================================================
            else
            {
                indices32.push_back(globalIndex);
            }
        }


        //========================================================
        // 次MeshのVertex開始位置へ進める
        //========================================================
        vertexBase += static_cast<std::uint32_t>(mesh.Vertices.size());
    }

    //========================================================
    // File Layout計算
    //========================================================
    const std::uint64_t headerOffset = 0u;

    //============================================================
    // Header直後
    //============================================================
    const std::uint64_t materialOffset = sizeof(vnModelData);


    //============================================================
    // Material直後
    //============================================================
    const std::uint64_t meshOffset =
        materialOffset +
        sizeof(vnModel_MaterialData) *
        outputMaterials.size();


    //============================================================
    // Mesh直後
    //============================================================
    const std::uint64_t vertexOffset =
        meshOffset +
        sizeof(vnModel_MeshData) *
        outputMeshes.size();


    //============================================================
    // Vertex直後
    //============================================================
    const std::uint64_t indexOffset =
        vertexOffset +
        sizeof(vnVertex3DFile) *
        outputVertices.size();


    //============================================================
    // File End
    //============================================================
    const std::uint64_t fileEnd =
        indexOffset + static_cast<std::uint64_t>(indexStride) * totalIndexCount;

    //========================================================
    // Access OffsetはUINTなので確認
    //========================================================
    if (fileEnd > (std::numeric_limits<UINT>::max)())
    {
        std::cerr << "[VNM Writer] VNM file is too large.\n";

        return false;
    }

    //========================================================
    // Header
    //========================================================
    vnModelData header = {};

    //========================================================
    // Magic
    //========================================================
    header.Magic = VN_MODEL_MAGIC;

    //========================================================
    // Date / Version
    //========================================================
    header.Date = CreateVNMDate();

    header.Version = 0x20260101u;

    header.HeaderSize = sizeof(vnModelData);

    header.MaterialNum = static_cast<UINT>(outputMaterials.size());

    header.MeshNum = static_cast<UINT>(outputMeshes.size());

    header.VertexNum = static_cast<UINT>(outputVertices.size());

    header.IndexNum = static_cast<UINT>(totalIndexCount);

    header.MaterialAccess = static_cast<UINT>(materialOffset);

    header.MeshAccess = static_cast<UINT>(meshOffset);

    header.VertexAccess = static_cast<UINT>(vertexOffset);

    header.IndexAccess = static_cast<UINT>(indexOffset);


    //========================================================
    // Static Model
    //========================================================
    header.BoneNum = 0u;

    header.BoneTableAccess = 0u;

    header.iBindAccess = 0u;


    //========================================================
    // VNM内は48-byte Legacy Vertex
    //========================================================
    header.VertexStride = sizeof(vnVertex3DFile);


    //========================================================
    // File書き込み
    //========================================================
    std::ofstream stream(outputFile, std::ios::binary | std::ios::trunc);


    if (!stream)
    {
        std::cerr
            << "[VNM Writer] "
            << "Could not open output file.\n";

        return false;
    }


    //========================================================
    // ファイルを必要サイズまで0で確保する
    // Section間のAlignment Paddingも自動で0になる
    //========================================================
    std::vector<char> zeroData(
        static_cast<std::size_t>(fileEnd), 0);


    stream.write(
        zeroData.data(),
        static_cast<std::streamsize>(zeroData.size())
    );


    //========================================================
    // Header
    //========================================================
    stream.seekp(headerOffset);

    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));


    //========================================================
    // Materials
    //========================================================
    stream.seekp(materialOffset);


    stream.write(
        reinterpret_cast<const char*>(outputMaterials.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MaterialData) * outputMaterials.size())
    );


    //========================================================
    // Meshes
    //========================================================
    stream.seekp(meshOffset);


    stream.write(
        reinterpret_cast<const char*>(outputMeshes.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MeshData) * outputMeshes.size())
    );


    //========================================================
    // Vertices
    //========================================================
    stream.seekp(vertexOffset);


    stream.write(
        reinterpret_cast<const char*>(outputVertices.data()),
        static_cast<std::streamsize>(sizeof(vnVertex3DFile) * outputVertices.size())
    );


    //========================================================
    // Indices
    //========================================================
    stream.seekp(indexOffset);


    if (use16BitIndices)
    {
        stream.write(
            reinterpret_cast<const char*>(indices16.data()),
            static_cast<std::streamsize>(sizeof(std::uint16_t) * indices16.size())
        );
    }
    else
    {
        stream.write(
            reinterpret_cast<const char*>(indices32.data()),
            static_cast<std::streamsize>(sizeof(std::uint32_t) * indices32.size())
        );
    }


    //========================================================
    // 書き込み確認
    //========================================================
    if (!stream.good())
    {
        std::cerr
            << "[VNM Writer] "
            << "File write failed.\n";

        return false;
    }


    stream.close();


    //========================================================
    // Result
    //========================================================
    std::wcout
        << L"\n"
        << L"========================================\n"
        << L" VNM Export Complete\n"
        << L"========================================\n"
        << L"File      : "
        << outputFile.wstring()
        << L"\n"
        << L"Materials : "
        << outputMaterials.size()
        << L"\n"
        << L"Meshes    : "
        << outputMeshes.size()
        << L"\n"
        << L"Vertices  : "
        << outputVertices.size()
        << L"\n"
        << L"Indices   : "
        << totalIndexCount
        << L"\n"
        << L"IndexBits : "
        << (use16BitIndices ? 16 : 32)
        << L"\n"
        << L"========================================\n";


    return true;
}

bool ModelWriter::IsOpenGLNormalMap(const std::string& path)
{

    if (path.empty())
    {
        return false;
    }

    std::string lower = path;

    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        }
    );

    return
        lower.find("_nor_gl") !=
        std::string::npos ||
        lower.find("_normal_gl") !=
        std::string::npos ||
        lower.find("opengl") !=
        std::string::npos;

}

std::wstring ModelWriter::CopyTexture(const std::string& sourceTexture, const std::filesystem::path& outputDirectory, const std::wstring& outputStem, std::size_t materialIndex, const std::wstring& typeName)
{
    //========================================================
    // Textureなし
    //========================================================
    if (sourceTexture.empty())
    {
        return {};
    }

    const std::filesystem::path sourcePath = sourceTexture;

    //========================================================
    // Sourceが存在しない場合
    //========================================================
    if (!std::filesystem::exists(sourcePath))
    {
        std::wcerr
            << L"[Texture Missing] "
            << sourcePath.wstring()
            << L"\n";

        return {};
    }

    //========================================================
    // 元TextureのExtensionを維持する
    //
    // .png
    // .dds
    // .tga
    // ...
    //========================================================
    const std::wstring extension = sourcePath.extension().wstring();

    //========================================================
    // 出力名
    // Example:
    // Test_Mat00_Normal.png
    //========================================================
    wchar_t fileName[256] = {};


    swprintf_s(
        fileName,
        L"%s_Mat%02zu_%s%s",
        outputStem.c_str(),
        materialIndex,
        typeName.c_str(),
        extension.c_str()
    );


    const std::filesystem::path destinationPath = outputDirectory / fileName;

    //========================================================
    // SourceとDestinationが同じ場合はCopy不要
    //========================================================
    std::error_code error;


    const bool sameFile =
        std::filesystem::equivalent(
            sourcePath,
            destinationPath,
            error
        );


    if (!sameFile)
    {
        error.clear();


        //====================================================
        // 同名Fileが存在すれば上書き
        // Converterを再実行した時にも最新Textureへ更新する
        //====================================================
        std::filesystem::copy_file(
            sourcePath,
            destinationPath,
            std::filesystem::copy_options::overwrite_existing,
            error
        );


        if (error)
        {
            std::wcerr
                << L"[Texture Copy Failed]\n"
                << L"  Source : "
                << sourcePath.wstring()
                << L"\n"
                << L"  Dest   : "
                << destinationPath.wstring()
                << L"\n";

            return {};
        }
    }


    //========================================================
    // VNMにはFull PathではなくFile Nameのみ保存
    //========================================================
    return destinationPath.filename().wstring();
}

//============================================================
// ImportedMaterial
//      ↓
// vnModel_MaterialDataV2
// VNM V2用Materialを作成する
//============================================================
vnModel_MaterialDataV2 ModelWriter::BuildMaterialV2(const ImportedMaterial& source)
{
    vnModel_MaterialDataV2 destination = {};


    //========================================================
    // Structure Header
    //========================================================
    destination.StructSize = sizeof(vnModel_MaterialDataV2);
    destination.FloatNum = 0u;
    destination.Flag = vnMaterialFlag_PBR;


    //========================================================
    // Legacy Material
    // 既存の非PBR描画でも最低限使えるように残す
    //========================================================
    destination.Diffuse[0] = source.BaseColor.x;
    destination.Diffuse[1] = source.BaseColor.y;
    destination.Diffuse[2] = source.BaseColor.z;
    destination.Diffuse[3] = source.BaseColor.w;

    destination.Ambient[0] = 0.0f;
    destination.Ambient[1] = 0.0f;
    destination.Ambient[2] = 0.0f;
    destination.Ambient[3] = 0.0f;

    destination.Specular[0] = 0.0f;
    destination.Specular[1] = 0.0f;
    destination.Specular[2] = 0.0f;
    destination.Specular[3] = 50.0f;

    //========================================================
    // UV Tiling / Offset
    //========================================================
    destination.TilingOffset[0] = 1.0f;
    destination.TilingOffset[1] = 1.0f;
    destination.TilingOffset[2] = 0.0f;
    destination.TilingOffset[3] = 0.0f;

    //========================================================
    // Normal Map
    // x : Enabled
    // y : Strength
    // z : Y Sign
    // w : unused
    //========================================================
    destination.NormalParam[0] = source.Textures.Normal.empty() ? 0.0f : 1.0f;
    destination.NormalParam[1] = 1.0f;
    destination.NormalParam[2] = IsOpenGLNormalMap(source.Textures.Normal) ? -1.0f : 1.0f;
    destination.NormalParam[3] = 0.0f;


    //========================================================
    // PBR
    // x = Metallic
    // y = Roughness
    // z = AO
    // w = Enabled
    //========================================================
    destination.PBRParam[0] = std::clamp(source.Metallic, 0.0f, 1.0f);
    destination.PBRParam[1] = std::clamp(source.Roughness, 0.0f, 1.0f);
    destination.PBRParam[2] = std::clamp(source.AO, 0.0f, 1.0f);
    destination.PBRParam[3] = 1.0f;


    //========================================================
    // ORM
    //
    // R = AO
    // G = Roughness
    // B = Metallic
    //========================================================
    destination.ORMParam[0] = source.Textures.ORM.empty() ? 0.0f : 1.0f;

    destination.ORMParam[1] = 1.0f;
    destination.ORMParam[2] = 1.0f;
    destination.ORMParam[3] = 1.0f;


    //========================================================
    // Emissive
    //========================================================
    const bool hasEmissiveTexture = !source.Textures.Emissive.empty();

    const bool hasEmissiveColor =
        source.Emissive.x > 0.0f ||
        source.Emissive.y > 0.0f ||
        source.Emissive.z > 0.0f;

    if (hasEmissiveTexture && !hasEmissiveColor)
    {
        destination.EmissiveParam[0] = 1.0f;
        destination.EmissiveParam[1] = 1.0f;
        destination.EmissiveParam[2] = 1.0f;
    }
    else
    {
        //================================================
        // Material側にEmissive Colorがある場合は、
        // TextureへのTint / Multiplierとしてそのまま使用
        //================================================
        destination.EmissiveParam[0] = (std::max)(source.Emissive.x, 0.0f);
        destination.EmissiveParam[1] = (std::max)(source.Emissive.y, 0.0f);
        destination.EmissiveParam[2] = (std::max)(source.Emissive.z, 0.0f);
    }

    destination.EmissiveParam[3] = hasEmissiveTexture ? 1.0f : 0.0f;

    //========================================================
    // Environment
    // Scene側で有効化するのでImporterではOFF
    //========================================================
    destination.EnvironmentParam[0] = 0.0f;
    destination.EnvironmentParam[1] = 1.0f;
    destination.EnvironmentParam[2] = 0.0f;
    destination.EnvironmentParam[3] = 0.0f;


    //========================================================
    // SSS
    //========================================================
    destination.SSSParam[0] = 0.0f;
    destination.SSSParam[1] = 0.5f;
    destination.SSSParam[2] = 0.5f;
    destination.SSSParam[3] = 0.25f;


    destination.SSSColor[0] = 1.0f;
    destination.SSSColor[1] = 0.25f;
    destination.SSSColor[2] = 0.12f;
    destination.SSSColor[3] = 4.0f;


    //========================================================
    // Thickness
    //========================================================
    destination.SSSMapParam[0] = source.Textures.Thickness.empty() ? 0.0f : 1.0f;
    destination.SSSMapParam[1] = 1.0f;
    destination.SSSMapParam[2] = 0.0f;
    destination.SSSMapParam[3] = 0.0f;


    //========================================================
    // Diffusion Profile
    // 0ならEngine側Global値へFallback
    //========================================================
    destination.SSSDiffusionParam[0] = 0.0f;
    destination.SSSDiffusionParam[1] = 0.0f;
    destination.SSSDiffusionParam[2] = 0.0f;
    destination.SSSDiffusionParam[3] = 0.0f;


    return destination;
}

vnModel_MaterialDataV3 ModelWriter::BuildMaterialV3(const ImportedMaterial& source)
{
    vnModel_MaterialDataV3 destination = {};

    //========================================================
    // Structure Header
    //========================================================
    destination.StructSize = sizeof(vnModel_MaterialDataV3);
    destination.FloatNum = 0u;
    destination.Flag = vnMaterialFlag_PBR;

    //========================================================
    // Legacy Material
    // 非PBR描画でも最低限使えるように残す
    //========================================================
    destination.Diffuse[0] = source.BaseColor.x;
    destination.Diffuse[1] = source.BaseColor.y;
    destination.Diffuse[2] = source.BaseColor.z;
    destination.Diffuse[3] = source.BaseColor.w;


    destination.Ambient[0] = 0.0f;
    destination.Ambient[1] = 0.0f;
    destination.Ambient[2] = 0.0f;
    destination.Ambient[3] = 0.0f;


    destination.Specular[0] = 0.0f;
    destination.Specular[1] = 0.0f;
    destination.Specular[2] = 0.0f;
    destination.Specular[3] = 50.0f;


    //========================================================
    // UV Tiling / Offset
    //========================================================
    destination.TilingOffset[0] = 1.0f;
    destination.TilingOffset[1] = 1.0f;
    destination.TilingOffset[2] = 0.0f;
    destination.TilingOffset[3] = 0.0f;


    //========================================================
    // Normal Map
    // x : Enabled
    // y : Strength
    // z : Y Sign
    // w : unused
    //========================================================
    destination.NormalParam[0] = source.Textures.Normal.empty() ? 0.0f : 1.0f;
    destination.NormalParam[1] = 1.0f;
    destination.NormalParam[2] = IsOpenGLNormalMap(source.Textures.Normal) ? -1.0f : 1.0f;
    destination.NormalParam[3] = 0.0f;


    //========================================================
    // PBR
    // x = Metallic
    // y = Roughness
    // z = AO
    // w = Enabled
    //========================================================
    destination.PBRParam[0] = std::clamp(source.Metallic, 0.0f, 1.0f);
    destination.PBRParam[1] = std::clamp(source.Roughness, 0.0f, 1.0f);
    destination.PBRParam[2] = std::clamp(source.AO, 0.0f, 1.0f);
    destination.PBRParam[3] = 1.0f;

    //========================================================
    // ORM
    // R = AO
    // G = Roughness
    // B = Metallic
    //========================================================
    destination.ORMParam[0] = source.Textures.ORM.empty() ? 0.0f : 1.0f;
    destination.ORMParam[1] = 1.0f;
    destination.ORMParam[2] = 1.0f;
    destination.ORMParam[3] = 1.0f;


    //========================================================
    // Emissive
    //========================================================
    const bool hasEmissiveTexture = !source.Textures.Emissive.empty();

    const bool hasEmissiveColor =
        source.Emissive.x > 0.0f ||
        source.Emissive.y > 0.0f ||
        source.Emissive.z > 0.0f;

    if (hasEmissiveTexture && !hasEmissiveColor)
    {
        destination.EmissiveParam[0] = 1.0f;
        destination.EmissiveParam[1] = 1.0f;
        destination.EmissiveParam[2] = 1.0f;
    }
    else
    {
        //================================================
        // Material側にEmissive Colorがある場合は、
        // TextureへのTint / Multiplierとしてそのまま使用
        //================================================
        destination.EmissiveParam[0] = (std::max)(source.Emissive.x, 0.0f);
        destination.EmissiveParam[1] = (std::max)(source.Emissive.y, 0.0f);
        destination.EmissiveParam[2] = (std::max)(source.Emissive.z, 0.0f);
    }

    destination.EmissiveParam[3] = hasEmissiveTexture ? 1.0f : 0.0f;

    //========================================================
    // Environment
    // Scene側で有効化するのでImporterではOFF
    //========================================================
    destination.EnvironmentParam[0] = 0.0f;
    destination.EnvironmentParam[1] = 1.0f;
    destination.EnvironmentParam[2] = 0.0f;
    destination.EnvironmentParam[3] = 0.0f;


    //========================================================
    // SSS
    //========================================================
    destination.SSSParam[0] = 0.0f;
    destination.SSSParam[1] = 0.5f;
    destination.SSSParam[2] = 0.5f;
    destination.SSSParam[3] = 0.25f;


    destination.SSSColor[0] = 1.0f;
    destination.SSSColor[1] = 0.25f;
    destination.SSSColor[2] = 0.12f;
    destination.SSSColor[3] = 4.0f;


    //========================================================
    // Thickness
    //========================================================
    destination.SSSMapParam[0] = source.Textures.Thickness.empty() ? 0.0f : 1.0f;
    destination.SSSMapParam[1] = 1.0f;
    destination.SSSMapParam[2] = 0.0f;
    //============================================================
    // SkinMask
    //============================================================
    destination.SSSMapParam[3] = source.Textures.SkinMask.empty() ? 0.0f : 1.0f;

    if (!source.Textures.SkinMask.empty())
    {
        destination.Flag |= vnMaterialFlag_SkinMask;
    }

    //========================================================
    // Diffusion Profile
    // 0ならEngine側Global値へFallback
    //========================================================
    destination.SSSDiffusionParam[0] = 0.0f;
    destination.SSSDiffusionParam[1] = 0.0f;
    destination.SSSDiffusionParam[2] = 0.0f;
    destination.SSSDiffusionParam[3] = 0.0f;

    return destination;
}

bool ModelWriter::WriteStaticModelV2(const ImportedModel& model, const std::filesystem::path& outputFile)
{
    //========================================================
    // 基本チェック
    //========================================================
    if (model.Meshes.empty())
    {
        std::cerr << "[VNM V2] Mesh is empty.\n";

        return false;
    }


    if (model.Materials.empty())
    {
        std::cerr << "[VNM V2] Material is empty.\n";

        return false;
    }


    //========================================================
    // 今回はStatic Meshのみ
    //========================================================
    if (model.HasSkinning)
    {
        std::cerr
            << "[VNM V2] "
            << "Skinned Model is not supported yet.\n";

        return false;
    }


    //========================================================
    // Output Directory
    //========================================================
    const std::filesystem::path outputDirectory = outputFile.parent_path();


    if (!outputDirectory.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(outputDirectory, error);

        if (error)
        {
            std::cerr
                << "[VNM V2] "
                << "Could not create output directory.\n";

            return false;
        }
    }

    const std::wstring outputStem = outputFile.stem().wstring();

    //========================================================
    // Vertex / Index数
    //========================================================
    std::uint64_t totalVertexCount = 0u;
    std::uint64_t totalIndexCount = 0u;


    for (const ImportedMesh& mesh : model.Meshes)
    {
        totalVertexCount += mesh.Vertices.size();

        totalIndexCount += mesh.Indices.size();
    }


    //========================================================
    // Index Format
    //========================================================
    const bool use16BitIndices = totalVertexCount <= (std::numeric_limits< std::uint16_t >::max)();

    const std::uint32_t indexStride = use16BitIndices ? sizeof(std::uint16_t) : sizeof(std::uint32_t);

    //========================================================
    // V2 Materials
    //========================================================
    std::vector<vnModel_MaterialDataV2> outputMaterials;

    outputMaterials.resize(model.Materials.size());

    for (std::size_t i = 0; i < model.Materials.size(); ++i)
    {
        const ImportedMaterial& source = model.Materials[i];

        vnModel_MaterialDataV2& destination = outputMaterials[i];

        //====================================================
        // Parameter生成
        //====================================================
        destination = BuildMaterialV2(source);

        //========================================================
        // DDS File NameだけVNMへ記録する
        //========================================================
        const std::wstring baseColorFile =
            GetTextureFileName(source.Textures.BaseColor);

        const std::wstring normalFile =
            GetTextureFileName(source.Textures.Normal);

        const std::wstring ormFile =
            GetTextureFileName(source.Textures.ORM);

        const std::wstring thicknessFile =
            GetTextureFileName(source.Textures.Thickness);

        const std::wstring emissiveFile =
            GetTextureFileName(source.Textures.Emissive);

        //========================================================
        // Texture File Name
        //========================================================
        CopyWideString(
            destination.BaseColorTexture,
            baseColorFile
        );

        CopyWideString(
            destination.NormalTexture,
            normalFile
        );

        CopyWideString(
            destination.ORMTexture,
            ormFile
        );

        CopyWideString(
            destination.ThicknessTexture,
            thicknessFile
        );

        CopyWideString(
            destination.EmissiveTexture,
            emissiveFile
        );

        if (!normalFile.empty())
        {
            destination.NormalParam[2] = 1.0f;
        }

        //====================================================
        // Texture Count
        //====================================================
        UINT textureCount = 0u;


        if (!baseColorFile.empty())
        {
            ++textureCount;
        }


        if (!normalFile.empty())
        {
            ++textureCount;
        }


        if (!ormFile.empty())
        {
            ++textureCount;
        }


        if (!thicknessFile.empty())
        {
            ++textureCount;
        }


        if (!emissiveFile.empty())
        {
            ++textureCount;
        }

        destination.TextureNum = textureCount;
    }

    //========================================================
    // Mesh Data
    //========================================================
    std::vector<vnModel_MeshData>  outputMeshes(model.Meshes.size());

    UINT currentIndex = 0u;

    for (std::size_t i = 0; i < model.Meshes.size(); ++i)
    {
        const ImportedMesh& source = model.Meshes[i];

        vnModel_MeshData& destination = outputMeshes[i];

        destination.MaterialID = source.MaterialIndex;

        destination.IndexNum = static_cast<UINT>(source.Indices.size());

        destination.StartIndex = currentIndex;

        //====================================================
        // Unity互換Global Index方式
        //====================================================
        destination.StartVertex = 0u;

        currentIndex += destination.IndexNum;
    }

    //========================================================
    // Vertex Data
    //========================================================
    std::vector<vnVertex3DFile> outputVertices;

    outputVertices.reserve(static_cast<std::size_t>(totalVertexCount));

    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const ImportedVertex& source : mesh.Vertices)
        {
            vnVertex3DFile destination = {};

            destination.x = source.Position.x;
            destination.y = source.Position.y;
            destination.z = source.Position.z;

            destination.nx = source.Normal.x;
            destination.ny = source.Normal.y;
            destination.nz = source.Normal.z;

            destination.r = source.Color.x;
            destination.g = source.Color.y;
            destination.b = source.Color.z;
            destination.a = source.Color.w;

            destination.u = source.TexCoord.x;


            //================================================
            // Unity VNM互換
            //================================================
            destination.v = 1.0f - source.TexCoord.y;

            outputVertices.push_back(destination);
        }
    }

    //========================================================
    // Index
    //========================================================
    std::vector<std::uint16_t> indices16;

    std::vector<std::uint32_t> indices32;

    if (use16BitIndices)
    {
        indices16.reserve(static_cast<std::size_t>(totalIndexCount));
    }
    else
    {
        indices32.reserve(static_cast<std::size_t>(totalIndexCount));
    }

    std::uint32_t vertexBase = 0u;

    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const std::uint32_t localIndex : mesh.Indices)
        {
            if (localIndex >= mesh.Vertices.size())
            {
                return false;
            }


            const std::uint32_t globalIndex = vertexBase + localIndex;


            if (use16BitIndices)
            {
                indices16.push_back(static_cast<std::uint16_t>(globalIndex));
            }
            else
            {
                indices32.push_back(globalIndex);
            }
        }

        vertexBase += static_cast<std::uint32_t>(mesh.Vertices.size());
    }


    //========================================================
    // V2 File Layout
    //
    // Header
    // Material V2
    // Mesh
    // Vertex
    // Index
    //========================================================
    const std::uint64_t materialOffset = sizeof(vnModelData);

    const std::uint64_t meshOffset =
        materialOffset +
        sizeof(vnModel_MaterialDataV2) *
        outputMaterials.size();


    const std::uint64_t vertexOffset =
        meshOffset +
        sizeof(vnModel_MeshData) *
        outputMeshes.size();


    const std::uint64_t indexOffset =
        vertexOffset +
        sizeof(vnVertex3DFile) *
        outputVertices.size();


    const std::uint64_t fileSize =
        indexOffset +
        indexStride *
        totalIndexCount;


    //========================================================
    // Header
    //========================================================
    vnModelData header = {};
    header.Magic = 0x006D6E76u;
    header.Date = CreateVNMDate();


    //========================================================
    // V2
    //========================================================
    header.Version = vnMODEL_DATA_V2_VER;
    header.HeaderSize = sizeof(vnModelData);
    header.MaterialNum = static_cast<UINT>(outputMaterials.size());
    header.MeshNum = static_cast<UINT>(outputMeshes.size());
    header.VertexNum = static_cast<UINT>(outputVertices.size());
    header.IndexNum = static_cast<UINT>(totalIndexCount);
    header.MaterialAccess = static_cast<UINT>(materialOffset);
    header.MeshAccess = static_cast<UINT>(meshOffset);
    header.VertexAccess = static_cast<UINT>(vertexOffset);
    header.IndexAccess = static_cast<UINT>(indexOffset);
    header.BoneNum = 0u;
    header.BoneTableAccess = 0u;
    header.iBindAccess = 0u;

    header.VertexStride = sizeof(vnVertex3DFile);


    //========================================================
    // Write
    //========================================================
    std::ofstream stream(outputFile, std::ios::binary | std::ios::trunc);


    if (!stream)
    {
        return false;
    }


    stream.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );


    stream.write(
        reinterpret_cast<const char*>(outputMaterials.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MaterialDataV2) * outputMaterials.size())
    );


    stream.write(
        reinterpret_cast<const char*>(outputMeshes.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MeshData) * outputMeshes.size())
    );


    stream.write(
        reinterpret_cast<const char*>(outputVertices.data()),
        static_cast<std::streamsize>(sizeof(vnVertex3DFile) * outputVertices.size())
    );


    if (use16BitIndices)
    {
        stream.write(
            reinterpret_cast<const char*>(indices16.data()),
            static_cast<std::streamsize>(sizeof(std::uint16_t) * indices16.size())
        );
    }
    else
    {
        stream.write(
            reinterpret_cast<const char*>(indices32.data()),
            static_cast<std::streamsize>(sizeof(std::uint32_t) * indices32.size())
        );
    }


    if (!stream.good())
    {
        return false;
    }


    stream.close();


    std::wcout
        << L"\nVNM V2 Export Success\n"
        << L"File : "
        << outputFile.wstring()
        << L"\n";


    return true;
}

bool ModelWriter::WriteStaticModelV3(const ImportedModel& model, const std::filesystem::path& outputFile)
{
    //========================================================
    // 基本チェック
    //========================================================
    if (model.Meshes.empty())
    {
        std::cerr << "[VNM V3] Mesh is empty.\n";

        return false;
    }


    if (model.Materials.empty())
    {
        std::cerr << "[VNM V3] Material is empty.\n";

        return false;
    }


    //========================================================
    // 今回はStatic Meshのみ
    //========================================================
    if (model.HasSkinning)
    {
        std::cerr
            << "[VNM V3] "
            << "Skinned Model is not supported yet.\n";

        return false;
    }

    //========================================================
    // Output Directory
    //========================================================
    const std::filesystem::path outputDirectory = outputFile.parent_path();

    if (!outputDirectory.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(outputDirectory, error);

        if (error)
        {
            std::cerr
                << "[VNM V3] "
                << "Could not create output directory.\n";

            return false;
        }
    }


    const std::wstring outputStem = outputFile.stem().wstring();


    //========================================================
    // Vertex / Index数
    //========================================================
    std::uint64_t totalVertexCount = 0u;
    std::uint64_t totalIndexCount = 0u;


    for (const ImportedMesh& mesh : model.Meshes)
    {
        totalVertexCount += mesh.Vertices.size();

        totalIndexCount += mesh.Indices.size();
    }


    //========================================================
    // Index Format
    //========================================================
    const bool use16BitIndices = totalVertexCount <= (std::numeric_limits< std::uint16_t >::max)();

    const std::uint32_t indexStride = use16BitIndices ? sizeof(std::uint16_t) : sizeof(std::uint32_t);

    //========================================================
    // V3 Materials
    //========================================================
    std::vector<vnModel_MaterialDataV3> outputMaterials;

    outputMaterials.resize(model.Materials.size());

    for (std::size_t i = 0; i < model.Materials.size(); ++i)
    {
        const ImportedMaterial& source = model.Materials[i];

        vnModel_MaterialDataV3& destination = outputMaterials[i];


        //====================================================
        // Parameter生成
        //====================================================
        destination = BuildMaterialV3(source);


        //========================================================
        // DDS File NameだけVNMへ記録する
        //========================================================
        const std::wstring baseColorFile =
            GetTextureFileName(source.Textures.BaseColor);

        const std::wstring normalFile =
            GetTextureFileName(source.Textures.Normal);

        const std::wstring ormFile =
            GetTextureFileName(source.Textures.ORM);

        const std::wstring thicknessFile =
            GetTextureFileName(source.Textures.Thickness);

        const std::wstring emissiveFile =
            GetTextureFileName(source.Textures.Emissive);

        const std::wstring skinMaskFile =
            GetTextureFileName(source.Textures.SkinMask);

        //========================================================
        // Texture File Name
        //========================================================
        CopyWideString(
            destination.BaseColorTexture,
            baseColorFile
        );

        CopyWideString(
            destination.NormalTexture,
            normalFile
        );

        CopyWideString(
            destination.ORMTexture,
            ormFile
        );

        CopyWideString(
            destination.ThicknessTexture,
            thicknessFile
        );

        CopyWideString(
            destination.EmissiveTexture,
            emissiveFile
        );

        CopyWideString(
            destination.SkinMaskTexture,
            skinMaskFile
        );

        if (!normalFile.empty())
        {
            destination.NormalParam[2] = 1.0f;
        }

        //====================================================
        // Texture Count
        //====================================================
        UINT textureCount = 0u;


        if (!baseColorFile.empty())
        {
            ++textureCount;
        }

        if (!normalFile.empty())
        {
            ++textureCount;
        }

        if (!ormFile.empty())
        {
            ++textureCount;
        }

        if (!thicknessFile.empty())
        {
            ++textureCount;
        }

        if (!emissiveFile.empty())
        {
            ++textureCount;
        }

        if (!skinMaskFile.empty())
        {
            ++textureCount;
        }

        destination.TextureNum = textureCount;
    }


    //========================================================
    // Mesh Data
    //========================================================
    std::vector<vnModel_MeshData>  outputMeshes(model.Meshes.size());

    UINT currentIndex = 0u;

    for (std::size_t i = 0; i < model.Meshes.size(); ++i)
    {
        const ImportedMesh& source = model.Meshes[i];

        vnModel_MeshData& destination = outputMeshes[i];

        destination.MaterialID = source.MaterialIndex;

        destination.IndexNum = static_cast<UINT>(source.Indices.size());

        destination.StartIndex = currentIndex;

        //====================================================
        // Unity互換Global Index方式
        //====================================================
        destination.StartVertex = 0u;

        currentIndex += destination.IndexNum;
    }


    //========================================================
    // Vertex Data
    //========================================================
    std::vector<vnVertex3DFile> outputVertices;

    outputVertices.reserve(static_cast<std::size_t>(totalVertexCount));

    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const ImportedVertex& source : mesh.Vertices)
        {
            vnVertex3DFile destination = {};

            destination.x = source.Position.x;
            destination.y = source.Position.y;
            destination.z = source.Position.z;

            destination.nx = source.Normal.x;
            destination.ny = source.Normal.y;
            destination.nz = source.Normal.z;

            destination.r = source.Color.x;
            destination.g = source.Color.y;
            destination.b = source.Color.z;
            destination.a = source.Color.w;

            destination.u = source.TexCoord.x;

            //================================================
            // Unity VNM互換
            //================================================
            destination.v = 1.0f - source.TexCoord.y;

            outputVertices.push_back(destination);
        }
    }

    //========================================================
    // Index
    //========================================================
    std::vector<std::uint16_t> indices16;

    std::vector<std::uint32_t> indices32;

    if (use16BitIndices)
    {
        indices16.reserve(static_cast<std::size_t>(totalIndexCount));
    }
    else
    {
        indices32.reserve(static_cast<std::size_t>(totalIndexCount));
    }


    std::uint32_t vertexBase = 0u;


    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const std::uint32_t localIndex : mesh.Indices)
        {
            if (localIndex >= mesh.Vertices.size())
            {
                return false;
            }

            const std::uint32_t globalIndex = vertexBase + localIndex;

            if (use16BitIndices)
            {
                indices16.push_back(static_cast<std::uint16_t>(globalIndex));
            }
            else
            {
                indices32.push_back(globalIndex);
            }
        }


        vertexBase += static_cast<std::uint32_t>(mesh.Vertices.size());
    }

    //========================================================
    // V2 File Layout
    // Header
    // Material V2
    // Mesh
    // Vertex
    // Index
    //========================================================
    const std::uint64_t materialOffset = sizeof(vnModelData);


    const std::uint64_t meshOffset =
        materialOffset +
        sizeof(vnModel_MaterialDataV3) *
        outputMaterials.size();


    const std::uint64_t vertexOffset =
        meshOffset +
        sizeof(vnModel_MeshData) *
        outputMeshes.size();


    const std::uint64_t indexOffset =
        vertexOffset +
        sizeof(vnVertex3DFile) *
        outputVertices.size();


    const std::uint64_t fileSize =
        indexOffset +
        indexStride *
        totalIndexCount;


    //========================================================
    // Header
    //========================================================
    vnModelData header = {};
    header.Magic = 0x006D6E76u;
    header.Date = CreateVNMDate();


    //========================================================
    // V3
    //========================================================
    header.Version = vnMODEL_DATA_V3_VER;
    header.HeaderSize = sizeof(vnModelData);
    header.MaterialNum = static_cast<UINT>(outputMaterials.size());
    header.MeshNum = static_cast<UINT>(outputMeshes.size());
    header.VertexNum = static_cast<UINT>(outputVertices.size());
    header.IndexNum = static_cast<UINT>(totalIndexCount);
    header.MaterialAccess = static_cast<UINT>(materialOffset);
    header.MeshAccess = static_cast<UINT>(meshOffset);
    header.VertexAccess = static_cast<UINT>(vertexOffset);
    header.IndexAccess = static_cast<UINT>(indexOffset);
    header.BoneNum = 0u;
    header.BoneTableAccess = 0u;
    header.iBindAccess = 0u;

    header.VertexStride = sizeof(vnVertex3DFile);

    //========================================================
    // Write
    //========================================================
    std::ofstream stream(outputFile, std::ios::binary | std::ios::trunc);

    if (!stream)
    {
        std::cerr
            << "[VNM V3] "
            << "Could not open output file.\n";

        return false;
    }


    //============================================================
    // Header
    //============================================================
    stream.seekp(0, std::ios::beg);


    stream.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );


    //============================================================
    // Material V3
    //============================================================
    stream.seekp(
        static_cast<std::streamoff>(materialOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(outputMaterials.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MaterialDataV3) * outputMaterials.size())
    );


    //============================================================
    // Mesh
    //============================================================
    stream.seekp(
        static_cast<std::streamoff>(meshOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(outputMeshes.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MeshData) * outputMeshes.size())
    );


    //============================================================
    // Vertex
    //============================================================
    stream.seekp(
        static_cast<std::streamoff>(vertexOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(outputVertices.data()),
        static_cast<std::streamsize>(sizeof(vnVertex3DFile) * outputVertices.size())
    );


    //============================================================
    // Index
    //============================================================
    stream.seekp(
        static_cast<std::streamoff>(indexOffset),
        std::ios::beg
    );


    if (use16BitIndices)
    {
        stream.write(
            reinterpret_cast<const char*>(indices16.data()),
            static_cast<std::streamsize>(sizeof(std::uint16_t) * indices16.size())
        );
    }
    else
    {
        stream.write(
            reinterpret_cast<const char*>(indices32.data()),
            static_cast<std::streamsize>(sizeof(std::uint32_t) * indices32.size())
        );
    }


    //============================================================
    // 書き込み成功確認
    //============================================================
    if (!stream.good())
    {
        std::cerr
            << "[VNM V3] "
            << "File write failed.\n";

        return false;
    }


    stream.close();


    //============================================================
    // Export Result
    //============================================================
    std::wcout
        << L"\n"
        << L"========================================\n"
        << L" VNM V3 Export Success\n"
        << L"========================================\n"
        << L"File      : "
        << outputFile.wstring()
        << L"\n"
        << L"Materials : "
        << outputMaterials.size()
        << L"\n"
        << L"Meshes    : "
        << outputMeshes.size()
        << L"\n"
        << L"Vertices  : "
        << outputVertices.size()
        << L"\n"
        << L"Indices   : "
        << totalIndexCount
        << L"\n"
        << L"IndexBits : "
        << (use16BitIndices ? 16 : 32)
        << L"\n"
        << L"FileSize  : "
        << fileSize
        << L"\n"
        << L"========================================\n";

    return true;
}

bool ModelWriter::ValidateStaticModel(const std::filesystem::path& filePath)
{
    //========================================================
    // File存在確認
    //========================================================
    if (!std::filesystem::exists(filePath))
    {
        std::cerr << "[VNM Validate] File does not exist.\n";

        return false;
    }


    //========================================================
    // Binaryとして開く
    //========================================================
    std::ifstream stream(filePath, std::ios::binary);


    if (!stream)
    {
        std::cerr
            << "[VNM Validate] "
            << "Could not open file.\n";

        return false;
    }


    //========================================================
    // File Size取得
    //========================================================
    stream.seekg(0, std::ios::end);

    const std::streamoff fileSize = stream.tellg();

    stream.seekg(0, std::ios::beg);


    //========================================================
    // ※Headerすら入らないFileは壊れている
    //========================================================
    if (fileSize < static_cast<std::streamoff>(sizeof(vnModelData)))
    {
        std::cerr
            << "[VNM Validate] "
            << "File is smaller than header.\n";

        return false;
    }

    //========================================================
    // Header読込
    //========================================================
    vnModelData header = {};

    stream.read(
        reinterpret_cast<char*>(&header),
        sizeof(header)
    );


    if (!stream)
    {
        std::cerr
            << "[VNM Validate] "
            << "Could not read header.\n";

        return false;
    }


    //========================================================
    // Magic
    //========================================================
    constexpr UINT expectedMagic = 0x006D6E76u;

    if (header.Magic != expectedMagic)
    {
        std::cerr
            << "[VNM Validate] Invalid Magic.\n"
            << "  Actual   : 0x"
            << std::hex
            << header.Magic
            << "\n"
            << "  Expected : 0x"
            << expectedMagic
            << std::dec
            << "\n";

        return false;
    }


    //========================================================
    // Header Size
    //========================================================
    if (header.HeaderSize != sizeof(vnModelData))
    {
        std::cerr
            << "[VNM Validate] "
            << "Invalid HeaderSize.\n";

        return false;
    }


    //============================================================
    // Supported Version
    //============================================================
    if (header.Version != vnMODEL_DATA_V1_VER &&
        header.Version != vnMODEL_DATA_V2_VER &&
        header.Version != vnMODEL_DATA_V3_VER)
    {
        std::cerr
            << "[VNM Validate] "
            << "Unsupported Version.\n";

        return false;
    }

    //========================================================
    // Static Model確認
    //========================================================
    if (header.BoneNum != 0u)
    {
        std::cerr
            << "[VNM Validate] "
            << "This validator currently supports "
            << "static models only.\n";

        return false;
    }


    //========================================================
    // Vertex Stride
    // 現在のV1 Static Vertexは48 bytes
    //========================================================
    if (header.VertexStride != sizeof(vnVertex3DFile))
    {
        std::cerr
            << "[VNM Validate] "
            << "Invalid VertexStride.\n"
            << "  Actual   : "
            << header.VertexStride
            << "\n"
            << "  Expected : "
            << sizeof(vnVertex3DFile)
            << "\n";

        return false;
    }

    //============================================================
    // Material Stride
    // V1 : 144
    // V2 : 864
    // V2 : 992
    //============================================================
    std::uint64_t materialStride = 0u;


    if (header.Version == vnMODEL_DATA_V1_VER)
    {
        materialStride = sizeof(vnModel_MaterialData);
    }
    else if (header.Version == vnMODEL_DATA_V2_VER)
    {
        materialStride = sizeof(vnModel_MaterialDataV2);
    }
    else if (header.Version == vnMODEL_DATA_V3_VER)
    {
        materialStride = sizeof(vnModel_MaterialDataV3);
    }
    else
    {
        std::cerr
            << "[VNM Validate] "
            << "Unsupported version.\n";

        return false;
    }

    const std::uint64_t expectedMaterialAccess = sizeof(vnModelData);


    const std::uint64_t expectedMeshAccess =
        expectedMaterialAccess + materialStride *
        static_cast<std::uint64_t>(header.MaterialNum);

    const std::uint64_t expectedVertexAccess =
        expectedMeshAccess + sizeof(vnModel_MeshData) *
        static_cast<std::uint64_t>(header.MeshNum);


    const std::uint64_t expectedIndexAccess =
        expectedVertexAccess + sizeof(vnVertex3DFile) *
        static_cast<std::uint64_t>(header.VertexNum);


    //========================================================
    // Headerに書かれたOffsetと比較
    //========================================================
    if (header.MaterialAccess != expectedMaterialAccess)
    {
        std::cerr
            << "[VNM Validate] "
            << "MaterialAccess mismatch.\n";

        return false;
    }


    if (header.MeshAccess != expectedMeshAccess)
    {
        std::cerr
            << "[VNM Validate] "
            << "MeshAccess mismatch.\n";

        return false;
    }


    if (header.VertexAccess != expectedVertexAccess)
    {
        std::cerr
            << "[VNM Validate] "
            << "VertexAccess mismatch.\n";

        return false;
    }


    if (header.IndexAccess != expectedIndexAccess)
    {
        std::cerr
            << "[VNM Validate] "
            << "IndexAccess mismatch.\n";

        return false;
    }


    //========================================================
    // Index Size
    // Unity互換:
    // VertexNum <= 65535
    //      ↓
    // R16_UINT
    //
    // VertexNum > 65535
    //      ↓
    // R32_UINT
    //========================================================
    const std::uint64_t indexStride =
        header.VertexNum <= (std::numeric_limits<std::uint16_t>::max)() ?
        sizeof(std::uint16_t) : sizeof(std::uint32_t);


    //========================================================
    // 期待するFile Size
    //========================================================
    const std::uint64_t expectedFileSize =
        expectedIndexAccess +
        static_cast<std::uint64_t>(header.IndexNum) *
        indexStride;


    if (expectedFileSize != static_cast<std::uint64_t>(fileSize))
    {
        std::cerr
            << "[VNM Validate] "
            << "File Size mismatch.\n"
            << "  Actual   : "
            << fileSize
            << "\n"
            << "  Expected : "
            << expectedFileSize
            << "\n";

        return false;
    }


    //========================================================
    // Mesh情報も確認
    //========================================================
    stream.seekg(header.MeshAccess, std::ios::beg);


    for (UINT meshIndex = 0u; meshIndex < header.MeshNum; ++meshIndex)
    {
        vnModel_MeshData mesh = {};

        stream.read(
            reinterpret_cast<char*>(&mesh),
            sizeof(mesh)
        );

        if (!stream)
        {
            std::cerr
                << "[VNM Validate] "
                << "Could not read MeshData.\n";

            return false;
        }

        //====================================================
        // Material Index確認
        //====================================================
        if (mesh.MaterialID >= header.MaterialNum)
        {
            std::cerr
                << "[VNM Validate] "
                << "Invalid MaterialID.\n";

            return false;
        }

        //====================================================
        // Unity方式ではStartVertex = 0
        //====================================================
        if (mesh.StartVertex != 0u)
        {
            std::cerr
                << "[VNM Validate] "
                << "StartVertex must be zero "
                << "for Unity-compatible VNM.\n";

            return false;
        }

        //====================================================
        // Index Buffer範囲確認
        //====================================================
        const std::uint64_t meshIndexEnd =
            static_cast<std::uint64_t>(mesh.StartIndex) + mesh.IndexNum;


        if (meshIndexEnd > header.IndexNum)
        {
            std::cerr
                << "[VNM Validate] "
                << "Mesh index range is invalid.\n";

            return false;
        }
    }


    //========================================================
    // Validation Success
    //========================================================
    std::cout
        << "\n"
        << "========================================\n"
        << " VNM Validation Success\n"
        << "========================================\n"
        << "Materials    : "
        << header.MaterialNum
        << "\n"
        << "Meshes       : "
        << header.MeshNum
        << "\n"
        << "Vertices     : "
        << header.VertexNum
        << "\n"
        << "Indices      : "
        << header.IndexNum
        << "\n"
        << "VertexStride : "
        << header.VertexStride
        << "\n"
        << "IndexBits    : "
        << (indexStride == 2u ? 16 : 32)
        << "\n"
        << "FileSize     : "
        << fileSize
        << "\n"
        << "========================================\n";

    return true;
}

bool ModelWriter::WriteSkinnedModelV2(const ImportedModel& model, const std::filesystem::path& outputFile)
{
    //========================================================
     // 基本チェック
     //========================================================
    if (model.Meshes.empty())
    {
        std::cerr << "[Skinned VNM V2] Mesh is empty.\n";

        return false;
    }


    if (model.Materials.empty())
    {
        std::cerr << "[Skinned VNM V2] Material is empty.\n";

        return false;
    }


    //========================================================
    // Skinned Model専用Writer
    //========================================================
    if (!model.HasSkinning)
    {
        std::cerr
            << "[Skinned VNM V2] "
            << "Model has no skinning data.\n";

        return false;
    }


    //========================================================
    // Skin Boneが無ければSkinned VNMを作れない
    //========================================================
    if (model.SkinBones.empty())
    {
        std::cerr
            << "[Skinned VNM V2] "
            << "SkinBones is empty.\n";

        return false;
    }


    //========================================================
    // Output Directory
    //========================================================
    const std::filesystem::path outputDirectory = outputFile.parent_path();


    if (!outputDirectory.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(outputDirectory, error);


        if (error)
        {
            std::cerr
                << "[Skinned VNM V2] "
                << "Could not create output directory.\n";

            return false;
        }
    }

    const std::wstring outputStem = outputFile.stem().wstring();

    //========================================================
    // Vertex / Index総数
    //========================================================
    std::uint64_t totalVertexCount = 0u;
    std::uint64_t totalIndexCount = 0u;


    for (const ImportedMesh& mesh : model.Meshes)
    {
        totalVertexCount += mesh.Vertices.size();
        totalIndexCount += mesh.Indices.size();
    }


    //========================================================
    // VNM HeaderはUINTなので範囲確認
    //========================================================
    if (totalVertexCount > (std::numeric_limits<UINT>::max)() || totalIndexCount > (std::numeric_limits<UINT>::max)())
    {
        std::cerr << "[Skinned VNM V2] Model is too large.\n";

        return false;
    }


    //========================================================
    // Index Format
    //========================================================
    const bool use16BitIndices = totalVertexCount <= (std::numeric_limits<std::uint16_t>::max)();

    const std::uint32_t indexStride = use16BitIndices ? sizeof(std::uint16_t) : sizeof(std::uint32_t);

    //========================================================
    // V2 Materials
    //========================================================
    std::vector<vnModel_MaterialDataV2> outputMaterials;

    outputMaterials.resize(model.Materials.size());

    for (std::size_t i = 0; i < model.Materials.size(); ++i)
    {
        const ImportedMaterial& source = model.Materials[i];

        vnModel_MaterialDataV2& destination = outputMaterials[i];

        //====================================================
        // Parameter生成
        //====================================================
        destination = BuildMaterialV2(source);

        //============================================================
        // Texture File Name取得
        //============================================================
        const std::wstring baseColorFile =
            GetTextureFileName(source.Textures.BaseColor);

        const std::wstring normalFile =
            GetTextureFileName(source.Textures.Normal);

        const std::wstring ormFile =
            GetTextureFileName(source.Textures.ORM);

        const std::wstring thicknessFile =
            GetTextureFileName(source.Textures.Thickness);

        const std::wstring emissiveFile =
            GetTextureFileName(source.Textures.Emissive);


        //============================================================
        // VNM MaterialへTexture File Nameを保存
        //============================================================
        CopyWideString(
            destination.BaseColorTexture,
            baseColorFile
        );


        CopyWideString(
            destination.NormalTexture,
            normalFile
        );


        CopyWideString(
            destination.ORMTexture,
            ormFile
        );


        CopyWideString(
            destination.ThicknessTexture,
            thicknessFile
        );


        CopyWideString(
            destination.EmissiveTexture,
            emissiveFile
        );

        if (!normalFile.empty())
        {
            destination.NormalParam[2] = 1.0f;
        }

        //====================================================
        // Texture Count
        //====================================================
        UINT textureCount = 0u;

        if (!baseColorFile.empty())
        {
            ++textureCount;
        }

        if (!normalFile.empty())
        {
            ++textureCount;
        }

        if (!ormFile.empty())
        {
            ++textureCount;
        }

        if (!thicknessFile.empty())
        {
            ++textureCount;
        }

        if (!emissiveFile.empty())
        {
            ++textureCount;
        }

        destination.TextureNum = textureCount;
    }

    //========================================================
    // Mesh Data
    //========================================================
    std::vector<vnModel_MeshData> outputMeshes(model.Meshes.size());

    UINT currentIndex = 0u;

    for (std::size_t i = 0; i < model.Meshes.size(); ++i)
    {
        const ImportedMesh& source = model.Meshes[i];


        //====================================================
        // Material ID範囲確認
        //====================================================
        if (source.MaterialIndex >= model.Materials.size())
        {
            std::cerr
                << "[Skinned VNM V2] "
                << "Invalid MaterialIndex.\n";

            return false;
        }


        vnModel_MeshData& destination = outputMeshes[i];

        destination.MaterialID = static_cast<UINT>(source.MaterialIndex);

        destination.IndexNum = static_cast<UINT>(source.Indices.size());

        destination.StartIndex = currentIndex;


        //====================================================
        // Global Index方式なのでBaseVertexは0
        //====================================================
        destination.StartVertex = 0u;
        currentIndex += destination.IndexNum;
    }

    //========================================================
    // Bone Table
    // Vertex側BoneIndices[]が参照するのは
    // model.SkinBones内のIndex
    // Runtimeでは
    // SkinBone Index
    //        ↓
    // BoneTable
    //        ↓
    // Full Skeleton Index (.bone側)
    //========================================================
    std::vector<std::uint16_t> boneTable;

    boneTable.reserve(model.SkinBones.size());

    for (const ImportedSkinBone& skinBone : model.SkinBones)
    {
        boneTable.push_back(skinBone.SkeletonIndex);
    }

    //========================================================
    // Inverse Bind Matrix
    // SkinBonesと完全に同じ順番で格納する
    // 現在はAssimp aiBone::mOffsetMatrixを
    // ImportedSkinBone::InverseBindへ保持している
    //========================================================
    std::vector<DirectX::XMFLOAT4X4> inverseBindMatrices;

    inverseBindMatrices.reserve(model.SkinBones.size());

    for (const ImportedSkinBone& skinBone : model.SkinBones)
    {
        inverseBindMatrices.push_back(skinBone.InverseBind);
    }
    //========================================================
    // Runtimeは64byte Matrixとして読む
    //========================================================
    static_assert(sizeof(DirectX::XMFLOAT4X4) == 64, "XMFLOAT4X4 must be 64 bytes.");

    //========================================================
    // Skin Vertex Data
    // Static:
    //     vnVertex3DFile
    //     48 byte
    // Skinned:
    //     vnVertex3DFileSkin
    //     72 byte
    //========================================================
    std::vector<vnVertex3DFileSkin> outputVertices;

    outputVertices.reserve(static_cast<std::size_t>(totalVertexCount));

    //========================================================
    // File Format確認
    //========================================================
    static_assert(sizeof(vnVertex3DFileSkin) == 72, "vnVertex3DFileSkin must be 72 bytes.");

    //========================================================
    // 全Mesh Vertexを1本のVertex Bufferへまとめる
    //========================================================
    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const ImportedVertex& source : mesh.Vertices)
        {
            vnVertex3DFileSkin destination = {};

            //================================================
            // Position
            //================================================
            destination.x = source.Position.x;
            destination.y = source.Position.y;
            destination.z = source.Position.z;

            //================================================
            // Normal
            //================================================
            destination.nx = source.Normal.x;
            destination.ny = source.Normal.y;
            destination.nz = source.Normal.z;

            //================================================
            // Vertex Color
            //================================================
            destination.r = source.Color.x;
            destination.g = source.Color.y;
            destination.b = source.Color.z;

            destination.a = source.Color.w;


            //================================================
            // UV
            // 既存VNM互換でVを反転
            //================================================
            destination.u = source.TexCoord.x;
            destination.v = 1.0f - source.TexCoord.y;

            //================================================
            // Skinning
            // BoneIndices: model.SkinBones内のIndex
            // BoneWeights: Top4へ絞りImport時に正規化済み
            //================================================
            for (int influence = 0; influence < 4; ++influence)
            {
                destination.skinIndex[influence] = source.BoneIndices[influence];
                destination.skinWeight[influence] = source.BoneWeights[influence];
            }


            outputVertices.push_back(destination);
        }
    }


    //========================================================
    // Vertex数確認
    //========================================================
    if (outputVertices.size() != totalVertexCount)
    {
        std::cerr
            << "[Skinned VNM V2] "
            << "Vertex count mismatch.\n";

        return false;
    }

    //========================================================
    // Skin Weight最終Validation
    //========================================================
    for (std::size_t vertexIndex = 0; vertexIndex < outputVertices.size(); ++vertexIndex)
    {
        const vnVertex3DFileSkin& vertex = outputVertices[vertexIndex];

        float totalWeight = 0.0f;

        for (int influence = 0; influence < 4; ++influence)
        {
            const float weight = vertex.skinWeight[influence];

            //================================================
            // NaN / Infチェック
            //================================================
            if (!std::isfinite(weight))
            {
                std::cerr
                    << "[Skinned VNM V2] "
                    << "Skin weight contains NaN / Inf.\n"
                    << "Vertex : "
                    << vertexIndex
                    << "\n";

                return false;
            }

            //================================================
            // Weightが0より大きい場合のみ
            // BoneIndexを検証する。
            //================================================
            if (weight > 0.000001f)
            {
                if (vertex.skinIndex[influence] >= model.SkinBones.size())
                {
                    std::cerr
                        << "[Skinned VNM V2] "
                        << "Invalid Skin Bone Index.\n"
                        << "Vertex    : "
                        << vertexIndex
                        << "\n"
                        << "Influence : "
                        << influence
                        << "\n"
                        << "BoneIndex : "
                        << vertex.skinIndex[influence]
                        << "\n"
                        << "BoneNum   : "
                        << model.SkinBones.size()
                        << "\n";

                    return false;
                }
            }

            totalWeight += weight;
        }

        //====================================================
        // 多少の浮動小数誤差は許可する
        //====================================================
        if (std::abs(totalWeight - 1.0f) > 0.01f)
        {
            std::cerr
                << "[Skinned VNM V2] "
                << "Invalid Skin Weight Sum.\n"
                << "Vertex : "
                << vertexIndex
                << "\n"
                << "Weight : "
                << totalWeight
                << "\n";

            return false;
        }
    }


    //========================================================
    // Index Buffer生成
    // 複数Meshを1つのVertex Bufferへ統合するため
    // Local IndexにvertexBaseを加えてGlobal Indexへ変換する
    // Mesh0: Vertex 0～999
    // Mesh1: Local Index 0
    //           ↓
    //        Global Index 1000
    //========================================================
    std::vector<std::uint16_t> indices16;

    std::vector<std::uint32_t> indices32;

    if (use16BitIndices)
    {
        indices16.reserve(static_cast<std::size_t>(totalIndexCount));
    }
    else
    {
        indices32.reserve(static_cast<std::size_t>(totalIndexCount));
    }

    std::uint32_t vertexBase = 0u;

    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const std::uint32_t localIndex : mesh.Indices)
        {
            //================================================
            // Local Index範囲確認
            //================================================
            if (localIndex >= mesh.Vertices.size())
            {
                std::cerr
                    << "[Skinned VNM V2] "
                    << "Local Index out of range.\n"
                    << "Index : "
                    << localIndex
                    << "\n"
                    << "VertexNum : "
                    << mesh.Vertices.size()
                    << "\n";

                return false;
            }


            //================================================
            // Local → Global Index
            //================================================
            const std::uint32_t globalIndex = vertexBase + localIndex;


            if (use16BitIndices)
            {
                //============================================
                // uint16範囲確認
                //============================================
                if (globalIndex > (std::numeric_limits<std::uint16_t>::max)())
                {
                    std::cerr
                        << "[Skinned VNM V2] "
                        << "16-bit Index Overflow.\n";

                    return false;
                }

                indices16.push_back(static_cast<std::uint16_t>(globalIndex));
            }
            else
            {
                indices32.push_back(globalIndex);
            }
        }

        //====================================================
        // 次MeshのVertex開始位置
        //====================================================
        vertexBase += static_cast<std::uint32_t>(mesh.Vertices.size());
    }

    //========================================================
    // Index数確認
    //========================================================
    const std::size_t writtenIndexCount = use16BitIndices ? indices16.size() : indices32.size();

    if (writtenIndexCount != totalIndexCount)
    {
        std::cerr
            << "[Skinned VNM V2] "
            << "Index Count mismatch.\n";

        return false;
    }

    //========================================================
    // VNM V3 File Layout
    //
    // +-----------------------------+
    // | vnModelData Header          |
    // +-----------------------------+
    // | Material V2                 |
    // +-----------------------------+
    // | Mesh Data                   |
    // +-----------------------------+
    // | BoneTable (uint16)          |
    // +-----------------------------+
    // | Padding                     |
    // +-----------------------------+
    // | Inverse Bind Matrix         |
    // +-----------------------------+
    // | Skin Vertex (72 byte)       |
    // +-----------------------------+
    // | Index                       |
    // +-----------------------------+
    //========================================================
    //========================================================
    // Material
    //========================================================
    const std::uint64_t materialOffset = sizeof(vnModelData);

    //========================================================
    // Mesh
    //========================================================
    const std::uint64_t meshOffset =
        materialOffset +
        sizeof(vnModel_MaterialDataV2) *
        outputMaterials.size();

    //========================================================
    // BoneTable
    //========================================================
    const std::uint64_t boneTableOffset =
        meshOffset +
        sizeof(vnModel_MeshData) *
        outputMeshes.size();

    //========================================================
    // BoneTable終了
    // Skin Bone 1つにつきuint16_t
    //========================================================
    const std::uint64_t boneTableEnd =
        boneTableOffset +
        sizeof(std::uint16_t) *
        boneTable.size();

    //========================================================
    // Inverse Bind Matrix
    // RuntimeでXMMATRIXとして扱うので
    // 16byte境界へAlignする
    //========================================================
    const std::uint64_t inverseBindOffset = AlignUp(boneTableEnd, 16u);

    //========================================================
    // Inverse Bind終了位置
    //========================================================
    const std::uint64_t inverseBindEnd =
        inverseBindOffset +
        sizeof(DirectX::XMFLOAT4X4) *
        inverseBindMatrices.size();

    //========================================================
    // Skin Vertex
    //========================================================
    const std::uint64_t vertexOffset = inverseBindEnd;

    //========================================================
    // Index
    //========================================================
    const std::uint64_t indexOffset =
        vertexOffset +
        sizeof(vnVertex3DFileSkin) *
        outputVertices.size();


    //========================================================
    // File Size
    //========================================================
    const std::uint64_t fileSize =
        indexOffset +
        static_cast<std::uint64_t>(indexStride) *
        totalIndexCount;


    //========================================================
    // Header内OffsetはUINTなので4GBを超えるVNMは現在扱えない
    //========================================================
    if (fileSize > (std::numeric_limits<UINT>::max)())
    {
        std::cerr
            << "[Skinned VNM V2] "
            << "File is too large.\n";

        return false;
    }


    //========================================================
    // VNM Header
    //========================================================
    vnModelData header = {};

    header.Magic = VN_MODEL_MAGIC;

    header.Date = CreateVNMDate();


    //========================================================
    // V2
    //========================================================
    header.Version = vnMODEL_DATA_V2_VER;

    header.HeaderSize = sizeof(vnModelData);

    //========================================================
    // Counts
    //========================================================
    header.MaterialNum = static_cast<UINT>(outputMaterials.size());

    header.MeshNum = static_cast<UINT>(outputMeshes.size());

    header.VertexNum = static_cast<UINT>(outputVertices.size());

    header.IndexNum = static_cast<UINT>(totalIndexCount);


    //========================================================
    // Standard Sections
    //========================================================
    header.MaterialAccess = static_cast<UINT>(materialOffset);

    header.MeshAccess = static_cast<UINT>(meshOffset);

    header.VertexAccess = static_cast<UINT>(vertexOffset);

    header.IndexAccess = static_cast<UINT>(indexOffset);


    //========================================================
    // Skinning Sections
    //========================================================
    header.BoneNum = static_cast<UINT>(model.SkinBones.size());

    header.BoneTableAccess = static_cast<UINT>(boneTableOffset);

    header.iBindAccess = static_cast<UINT>(inverseBindOffset);

    //========================================================
    // Skinned Vertex
    // 48ではなく72byte
    //========================================================
    header.VertexStride = sizeof(vnVertex3DFileSkin);

    //========================================================
    // File Open
    //========================================================
    std::ofstream stream(outputFile, std::ios::binary | std::ios::trunc);


    if (!stream)
    {
        std::wcerr
            << L"[Skinned VNM V2] "
            << L"Could not open output file.\n"
            << L"Path : "
            << outputFile.wstring()
            << L"\n";

        return false;
    }

    //========================================================
    // 最初にFile全体を0で埋めて確保する
    // Padding部分も自動的に0になる
    //========================================================
    std::vector<char>  zeroData(static_cast<std::size_t>(fileSize), 0);

    stream.write(
        zeroData.data(),
        static_cast<std::streamsize>(zeroData.size())
    );


    if (!stream.good())
    {
        std::cerr
            << "[Skinned VNM V2] "
            << "Could not allocate output file.\n";

        return false;
    }


    //========================================================
    // Header
    //========================================================
    stream.seekp(0, std::ios::beg);


    stream.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );


    //========================================================
    // Material V2
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(materialOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(outputMaterials.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MaterialDataV2) * outputMaterials.size())
    );


    //========================================================
    // Mesh
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(meshOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(outputMeshes.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MeshData) * outputMeshes.size())
    );


    //========================================================
    // BoneTable
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(boneTableOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(boneTable.data()),
        static_cast<std::streamsize>(sizeof(std::uint16_t) * boneTable.size())
    );


    //========================================================
    // Inverse Bind Matrix
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(inverseBindOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(inverseBindMatrices.data()),
        static_cast<std::streamsize>(sizeof(DirectX::XMFLOAT4X4) * inverseBindMatrices.size())
    );


    //========================================================
    // Skin Vertex
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(vertexOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(outputVertices.data()),
        static_cast<std::streamsize>(sizeof(vnVertex3DFileSkin) * outputVertices.size())
    );


    //========================================================
    // Index
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(indexOffset),
        std::ios::beg
    );


    if (use16BitIndices)
    {
        stream.write(
            reinterpret_cast<const char*>(indices16.data()),
            static_cast<std::streamsize>(sizeof(std::uint16_t) * indices16.size())
        );
    }
    else
    {
        stream.write(
            reinterpret_cast<const char*>(indices32.data()),
            static_cast<std::streamsize>(sizeof(std::uint32_t) * indices32.size())
        );
    }


    //========================================================
    // 最終Write確認
    //========================================================
    if (!stream.good())
    {
        std::cerr
            << "[Skinned VNM V2] "
            << "File write failed.\n";

        return false;
    }


    stream.close();


    //========================================================
    // Export結果
    //========================================================
    std::wcout
        << L"\n"
        << L"========================================\n"
        << L" Skinned VNM V2 Export Success\n"
        << L"========================================\n"
        << L"File            : "
        << outputFile.wstring()
        << L"\n"
        << L"Materials       : "
        << outputMaterials.size()
        << L"\n"
        << L"Meshes          : "
        << outputMeshes.size()
        << L"\n"
        << L"Vertices        : "
        << outputVertices.size()
        << L"\n"
        << L"Indices         : "
        << totalIndexCount
        << L"\n"
        << L"Skin Bones      : "
        << model.SkinBones.size()
        << L"\n"
        << L"BoneTableAccess : "
        << boneTableOffset
        << L"\n"
        << L"iBindAccess     : "
        << inverseBindOffset
        << L"\n"
        << L"VertexAccess    : "
        << vertexOffset
        << L"\n"
        << L"IndexAccess     : "
        << indexOffset
        << L"\n"
        << L"VertexStride    : "
        << sizeof(vnVertex3DFileSkin)
        << L"\n"
        << L"IndexBits       : "
        << (use16BitIndices ? 16 : 32)
        << L"\n"
        << L"FileSize        : "
        << fileSize
        << L"\n"
        << L"========================================\n";

    return true;
}

bool ModelWriter::WriteSkinnedModelV3(const ImportedModel& model, const std::filesystem::path& outputFile)
{
    //========================================================
    // 基本チェック
    //========================================================
    if (model.Meshes.empty())
    {
        std::cerr << "[Skinned VNM V3] Mesh is empty.\n";

        return false;
    }


    if (model.Materials.empty())
    {
        std::cerr << "[Skinned VNM V3] Material is empty.\n";

        return false;
    }


    //========================================================
    // Skinned Model専用Writer
    //========================================================
    if (!model.HasSkinning)
    {
        std::cerr
            << "[Skinned VNM V3] "
            << "Model has no skinning data.\n";

        return false;
    }


    //========================================================
    // Skin Boneが無ければSkinned VNMを作れない
    //========================================================
    if (model.SkinBones.empty())
    {
        std::cerr
            << "[Skinned VNM V3] "
            << "SkinBones is empty.\n";

        return false;
    }


    //========================================================
    // Output Directory
    //========================================================
    const std::filesystem::path outputDirectory = outputFile.parent_path();


    if (!outputDirectory.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(outputDirectory, error);

        if (error)
        {
            std::cerr
                << "[Skinned VNM V3] "
                << "Could not create output directory.\n";

            return false;
        }
    }

    const std::wstring outputStem = outputFile.stem().wstring();

    //========================================================
    // Vertex / Index総数
    //========================================================
    std::uint64_t totalVertexCount = 0u;
    std::uint64_t totalIndexCount = 0u;


    for (const ImportedMesh& mesh : model.Meshes)
    {
        totalVertexCount += mesh.Vertices.size();
        totalIndexCount += mesh.Indices.size();
    }


    //========================================================
    // VNM HeaderはUINTなので範囲確認
    //========================================================
    if (totalVertexCount > (std::numeric_limits<UINT>::max)() || totalIndexCount > (std::numeric_limits<UINT>::max)())
    {
        std::cerr
            << "[Skinned VNM V3] Model is too large.\n";

        return false;
    }


    //========================================================
    // Index Format
    //========================================================
    const bool use16BitIndices = totalVertexCount <= (std::numeric_limits<std::uint16_t>::max)();

    const std::uint32_t indexStride = use16BitIndices ? sizeof(std::uint16_t) : sizeof(std::uint32_t);

    //========================================================
    // V3 Materials
    //========================================================
    std::vector<vnModel_MaterialDataV3> outputMaterials;

    outputMaterials.resize(model.Materials.size());

    for (std::size_t i = 0; i < model.Materials.size(); ++i)
    {
        const ImportedMaterial& source = model.Materials[i];

        vnModel_MaterialDataV3& destination = outputMaterials[i];

        //====================================================
        // Parameter生成
        //====================================================
        destination = BuildMaterialV3(source);

        //============================================================
        // Texture File Name取得
        //============================================================
        const std::wstring baseColorFile =
            GetTextureFileName(source.Textures.BaseColor);

        const std::wstring normalFile =
            GetTextureFileName(source.Textures.Normal);

        const std::wstring ormFile =
            GetTextureFileName(source.Textures.ORM);

        const std::wstring thicknessFile =
            GetTextureFileName(source.Textures.Thickness);

        const std::wstring emissiveFile =
            GetTextureFileName(source.Textures.Emissive);

        const std::wstring skinMaskFile =
            GetTextureFileName(source.Textures.SkinMask);

        //============================================================
        // VNM MaterialへTexture File Nameを保存
        //============================================================
        CopyWideString(
            destination.BaseColorTexture,
            baseColorFile
        );

        CopyWideString(
            destination.NormalTexture,
            normalFile
        );

        CopyWideString(
            destination.ORMTexture,
            ormFile
        );

        CopyWideString(
            destination.ThicknessTexture,
            thicknessFile
        );

        CopyWideString(
            destination.EmissiveTexture,
            emissiveFile
        );

        CopyWideString(
            destination.SkinMaskTexture,
            skinMaskFile
        );

        if (!normalFile.empty())
        {
            destination.NormalParam[2] = 1.0f;
        }

        //====================================================
        // Texture Count
        //====================================================
        UINT textureCount = 0u;

        if (!baseColorFile.empty())
        {
            ++textureCount;
        }

        if (!normalFile.empty())
        {
            ++textureCount;
        }

        if (!ormFile.empty())
        {
            ++textureCount;
        }

        if (!thicknessFile.empty())
        {
            ++textureCount;
        }

        if (!emissiveFile.empty())
        {
            ++textureCount;
        }

        if (!skinMaskFile.empty())
        {
            ++textureCount;
        }

        destination.TextureNum = textureCount;
    }

    //========================================================
    // Mesh Data
    //========================================================
    std::vector<vnModel_MeshData> outputMeshes(model.Meshes.size());

    UINT currentIndex = 0u;

    for (std::size_t i = 0; i < model.Meshes.size(); ++i)
    {
        const ImportedMesh& source = model.Meshes[i];

        //====================================================
        // Material ID範囲確認
        //====================================================
        if (source.MaterialIndex >= model.Materials.size())
        {
            std::cerr
                << "[Skinned VNM V3] "
                << "Invalid MaterialIndex.\n";

            return false;
        }


        vnModel_MeshData& destination = outputMeshes[i];

        destination.MaterialID = static_cast<UINT>(source.MaterialIndex);

        destination.IndexNum = static_cast<UINT>(source.Indices.size());

        destination.StartIndex = currentIndex;

        //====================================================
        // Global Index方式なのでBaseVertexは0
        //====================================================
        destination.StartVertex = 0u;
        currentIndex += destination.IndexNum;
    }

    //========================================================
    // Bone Table
    // Vertex側BoneIndices[]が参照するのは
    // model.SkinBones内のIndex
    // Runtimeでは
    // SkinBone Index
    //        ↓
    // BoneTable
    //        ↓
    // Full Skeleton Index (.bone側)
    //========================================================
    std::vector<std::uint16_t> boneTable;

    boneTable.reserve(model.SkinBones.size());

    for (const ImportedSkinBone& skinBone : model.SkinBones)
    {
        boneTable.push_back(skinBone.SkeletonIndex);
    }

    //========================================================
    // Inverse Bind Matrix
    // SkinBonesと完全に同じ順番で格納する
    // 現在はAssimp aiBone::mOffsetMatrixを
    // ImportedSkinBone::InverseBindへ保持している
    //========================================================
    std::vector<DirectX::XMFLOAT4X4> inverseBindMatrices;

    inverseBindMatrices.reserve(model.SkinBones.size());

    for (const ImportedSkinBone& skinBone : model.SkinBones)
    {
        inverseBindMatrices.push_back(skinBone.InverseBind);
    }
    //========================================================
    // Runtimeは64byte Matrixとして読む
    //========================================================
    static_assert(sizeof(DirectX::XMFLOAT4X4) == 64, "XMFLOAT4X4 must be 64 bytes.");

    //========================================================
    // Skin Vertex Data
    // Static:
    //     vnVertex3DFile
    //     48 byte
    // Skinned:
    //     vnVertex3DFileSkin
    //     72 byte
    //========================================================
    std::vector<vnVertex3DFileSkin> outputVertices;

    outputVertices.reserve(static_cast<std::size_t>(totalVertexCount));

    //========================================================
    // File Format確認
    //========================================================
    static_assert(sizeof(vnVertex3DFileSkin) == 72, "vnVertex3DFileSkin must be 72 bytes.");

    //========================================================
    // 全Mesh Vertexを1本のVertex Bufferへまとめる
    //========================================================
    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const ImportedVertex& source : mesh.Vertices)
        {
            vnVertex3DFileSkin destination = {};


            //================================================
            // Position
            //================================================
            destination.x = source.Position.x;
            destination.y = source.Position.y;
            destination.z = source.Position.z;

            //================================================
            // Normal
            //================================================
            destination.nx = source.Normal.x;
            destination.ny = source.Normal.y;
            destination.nz = source.Normal.z;

            //================================================
            // Vertex Color
            //================================================
            destination.r = source.Color.x;
            destination.g = source.Color.y;
            destination.b = source.Color.z;
            destination.a = source.Color.w;

            //================================================
            // UV
            // 既存VNM互換でVを反転
            //================================================
            destination.u = source.TexCoord.x;
            destination.v = 1.0f - source.TexCoord.y;


            //================================================
            // Skinning
            // BoneIndices: model.SkinBones内のIndex
            // BoneWeights: Top4へ絞りImport時に正規化済み
            //================================================
            for (int influence = 0; influence < 4; ++influence)
            {
                destination.skinIndex[influence] = source.BoneIndices[influence];

                destination.skinWeight[influence] = source.BoneWeights[influence];
            }

            outputVertices.push_back(destination);
        }
    }

    //========================================================
    // Vertex数確認
    //========================================================
    if (outputVertices.size() != totalVertexCount)
    {
        std::cerr
            << "[Skinned VNM V3] "
            << "Vertex count mismatch.\n";

        return false;
    }

    //========================================================
    // Skin Weight最終Validation
    //========================================================
    for (std::size_t vertexIndex = 0; vertexIndex < outputVertices.size(); ++vertexIndex)
    {
        const vnVertex3DFileSkin& vertex = outputVertices[vertexIndex];

        float totalWeight = 0.0f;

        for (int influence = 0; influence < 4; ++influence)
        {
            const float weight = vertex.skinWeight[influence];


            //================================================
            // NaN / Infチェック
            //================================================
            if (!std::isfinite(weight))
            {
                std::cerr
                    << "[Skinned VNM V3] "
                    << "Skin weight contains NaN / Inf.\n"
                    << "Vertex : "
                    << vertexIndex
                    << "\n";

                return false;
            }

            //================================================
            // Weightが0より大きい場合のみ
            // BoneIndexを検証する
            //================================================
            if (weight > 0.000001f)
            {
                if (vertex.skinIndex[influence] >= model.SkinBones.size())
                {
                    std::cerr
                        << "[Skinned VNM V3] "
                        << "Invalid Skin Bone Index.\n"
                        << "Vertex    : "
                        << vertexIndex
                        << "\n"
                        << "Influence : "
                        << influence
                        << "\n"
                        << "BoneIndex : "
                        << vertex.skinIndex[influence]
                        << "\n"
                        << "BoneNum   : "
                        << model.SkinBones.size()
                        << "\n";

                    return false;
                }
            }


            totalWeight += weight;
        }


        //====================================================
        // 多少の浮動小数誤差は許可する
        //====================================================
        if (std::abs(totalWeight - 1.0f) > 0.01f)
        {
            std::cerr
                << "[Skinned VNM V2] "
                << "Invalid Skin Weight Sum.\n"
                << "Vertex : "
                << vertexIndex
                << "\n"
                << "Weight : "
                << totalWeight
                << "\n";

            return false;
        }
    }

    //========================================================
    // Index Buffer生成
    // 複数Meshを1つのVertex Bufferへ統合するため
    // Local IndexにvertexBaseを加えてGlobal Indexへ変換する
    // Mesh0: Vertex 0～999
    // Mesh1: Local Index 0
    //           ↓
    //        Global Index 1000
    //========================================================
    std::vector<std::uint16_t> indices16;

    std::vector<std::uint32_t> indices32;

    if (use16BitIndices)
    {
        indices16.reserve(static_cast<std::size_t>(totalIndexCount));
    }
    else
    {
        indices32.reserve(static_cast<std::size_t>(totalIndexCount));
    }

    std::uint32_t vertexBase = 0u;

    for (const ImportedMesh& mesh : model.Meshes)
    {
        for (const std::uint32_t localIndex : mesh.Indices)
        {
            //================================================
            // Local Index範囲確認
            //================================================
            if (localIndex >= mesh.Vertices.size())
            {
                std::cerr
                    << "[Skinned VNM V3] "
                    << "Local Index out of range.\n"
                    << "Index : "
                    << localIndex
                    << "\n"
                    << "VertexNum : "
                    << mesh.Vertices.size()
                    << "\n";

                return false;
            }


            //================================================
            // Local → Global Index
            //================================================
            const std::uint32_t globalIndex = vertexBase + localIndex;


            if (use16BitIndices)
            {
                //============================================
                // uint16範囲確認
                //============================================
                if (globalIndex > (std::numeric_limits<std::uint16_t>::max)())
                {
                    std::cerr
                        << "[Skinned VNM V3] "
                        << "16-bit Index Overflow.\n";

                    return false;
                }


                indices16.push_back(static_cast<std::uint16_t>(globalIndex));
            }
            else
            {
                indices32.push_back(globalIndex);
            }
        }

        //====================================================
        // 次MeshのVertex開始位置
        //====================================================
        vertexBase += static_cast<std::uint32_t>(mesh.Vertices.size());
    }

    //========================================================
    // Index数確認
    //========================================================
    const std::size_t writtenIndexCount = use16BitIndices ? indices16.size() : indices32.size();


    if (writtenIndexCount != totalIndexCount)
    {
        std::cerr
            << "[Skinned VNM V2] "
            << "Index Count mismatch.\n";

        return false;
    }


    //========================================================
    // VNM V3 File Layout
    //
    // +-----------------------------+
    // | vnModelData Header          |
    // +-----------------------------+
    // | Material V2                 |
    // +-----------------------------+
    // | Mesh Data                   |
    // +-----------------------------+
    // | BoneTable (uint16)          |
    // +-----------------------------+
    // | Padding                     |
    // +-----------------------------+
    // | Inverse Bind Matrix         |
    // +-----------------------------+
    // | Skin Vertex (72 byte)       |
    // +-----------------------------+
    // | Index                       |
    // +-----------------------------+
    // | Index                       |
    // +-----------------------------+

    //========================================================

    //========================================================
    // Material
    //========================================================
    const std::uint64_t materialOffset = sizeof(vnModelData);

    //========================================================
    // Mesh
    //========================================================
    const std::uint64_t meshOffset =
        materialOffset +
        sizeof(vnModel_MaterialDataV3) *
        outputMaterials.size();


    //========================================================
    // BoneTable
    //========================================================
    const std::uint64_t boneTableOffset =
        meshOffset +
        sizeof(vnModel_MeshData) *
        outputMeshes.size();


    //========================================================
    // BoneTable終了
    // Skin Bone 1つにつきuint16_t
    //========================================================
    const std::uint64_t boneTableEnd =
        boneTableOffset +
        sizeof(std::uint16_t) *
        boneTable.size();


    //========================================================
    // Inverse Bind Matrix
    // RuntimeでXMMATRIXとして扱うので
    // 16byte境界へAlignする
    //========================================================
    const std::uint64_t inverseBindOffset = AlignUp(boneTableEnd, 16u);


    //========================================================
    // Inverse Bind終了位置
    //========================================================
    const std::uint64_t inverseBindEnd =
        inverseBindOffset +
        sizeof(DirectX::XMFLOAT4X4) *
        inverseBindMatrices.size();

    //========================================================
    // Skin Vertex
    //========================================================
    const std::uint64_t vertexOffset = inverseBindEnd;

    //========================================================
    // Index
    //========================================================
    const std::uint64_t indexOffset =
        vertexOffset +
        sizeof(vnVertex3DFileSkin) *
        outputVertices.size();

    //========================================================
    // File Size
    //========================================================
    const std::uint64_t fileSize =
        indexOffset +
        static_cast<std::uint64_t>(indexStride) *
        totalIndexCount;

    //========================================================
    // Header内OffsetはUINTなので、
    // 4GBを超えるVNMは現在扱えない
    //========================================================
    if (fileSize > (std::numeric_limits<UINT>::max)())
    {
        std::cerr
            << "[Skinned VNM V3] "
            << "File is too large.\n";

        return false;
    }


    //========================================================
    // VNM Header
    //========================================================
    vnModelData header = {};

    header.Magic = VN_MODEL_MAGIC;

    header.Date = CreateVNMDate();


    //========================================================
    // V2
    //========================================================
    header.Version = vnMODEL_DATA_V3_VER;

    header.HeaderSize = sizeof(vnModelData);


    //========================================================
    // Counts
    //========================================================
    header.MaterialNum = static_cast<UINT>(outputMaterials.size());

    header.MeshNum = static_cast<UINT>(outputMeshes.size());

    header.VertexNum = static_cast<UINT>(outputVertices.size());

    header.IndexNum = static_cast<UINT>(totalIndexCount);


    //========================================================
    // Standard Sections
    //========================================================
    header.MaterialAccess = static_cast<UINT>(materialOffset);

    header.MeshAccess = static_cast<UINT>(meshOffset);

    header.VertexAccess = static_cast<UINT>(vertexOffset);

    header.IndexAccess = static_cast<UINT>(indexOffset);


    //========================================================
    // Skinning Sections
    //========================================================
    header.BoneNum = static_cast<UINT>(model.SkinBones.size());

    header.BoneTableAccess = static_cast<UINT>(boneTableOffset);

    header.iBindAccess = static_cast<UINT>(inverseBindOffset);


    //========================================================
    // Skinned Vertex
    // 48ではなく72byte
    //========================================================
    header.VertexStride = sizeof(vnVertex3DFileSkin);

    //========================================================
    // File Open
    //========================================================
    std::ofstream stream(outputFile, std::ios::binary | std::ios::trunc);


    if (!stream)
    {
        std::wcerr
            << L"[Skinned VNM V3] "
            << L"Could not open output file.\n"
            << L"Path : "
            << outputFile.wstring()
            << L"\n";

        return false;
    }


    //========================================================
    // 最初にFile全体を0で埋めて確保する
    // Padding部分も自動的に0になる
    //========================================================
    std::vector<char>  zeroData(static_cast<std::size_t>(fileSize), 0);


    stream.write(
        zeroData.data(),
        static_cast<std::streamsize>(zeroData.size())
    );


    if (!stream.good())
    {
        std::cerr
            << "[Skinned VNM V3] "
            << "Could not allocate output file.\n";

        return false;
    }


    //========================================================
    // Header
    //========================================================
    stream.seekp(0, std::ios::beg);


    stream.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );


    //========================================================
    // Material V3
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(materialOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(outputMaterials.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MaterialDataV3) * outputMaterials.size())
    );


    //========================================================
    // Mesh
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(meshOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(outputMeshes.data()),
        static_cast<std::streamsize>(sizeof(vnModel_MeshData) * outputMeshes.size())
    );


    //========================================================
    // BoneTable
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(boneTableOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(boneTable.data()),
        static_cast<std::streamsize>(sizeof(std::uint16_t) * boneTable.size())
    );


    //========================================================
    // Inverse Bind Matrix
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(inverseBindOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(inverseBindMatrices.data()),
        static_cast<std::streamsize>(sizeof(DirectX::XMFLOAT4X4) * inverseBindMatrices.size())
    );


    //========================================================
    // Skin Vertex
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(vertexOffset),
        std::ios::beg
    );


    stream.write(
        reinterpret_cast<const char*>(outputVertices.data()),
        static_cast<std::streamsize>(sizeof(vnVertex3DFileSkin) * outputVertices.size())
    );

    //========================================================
    // Index
    //========================================================
    stream.seekp(
        static_cast<std::streamoff>(indexOffset),
        std::ios::beg
    );

    if (use16BitIndices)
    {
        stream.write(
            reinterpret_cast<const char*>(indices16.data()),
            static_cast<std::streamsize>(sizeof(std::uint16_t) * indices16.size())
        );
    }
    else
    {
        stream.write(
            reinterpret_cast<const char*>(indices32.data()),
            static_cast<std::streamsize>(sizeof(std::uint32_t) * indices32.size())
        );
    }


    //========================================================
    // 最終Write確認
    //========================================================
    if (!stream.good())
    {
        std::cerr
            << "[Skinned VNM V3] "
            << "File write failed.\n";

        return false;
    }


    stream.close();


    //========================================================
    // Export結果
    //========================================================
    std::wcout
        << L"\n"
        << L"========================================\n"
        << L" Skinned VNM V3 Export Success\n"
        << L"========================================\n"
        << L"File            : "
        << outputFile.wstring()
        << L"\n"
        << L"Materials       : "
        << outputMaterials.size()
        << L"\n"
        << L"Meshes          : "
        << outputMeshes.size()
        << L"\n"
        << L"Vertices        : "
        << outputVertices.size()
        << L"\n"
        << L"Indices         : "
        << totalIndexCount
        << L"\n"
        << L"Skin Bones      : "
        << model.SkinBones.size()
        << L"\n"
        << L"BoneTableAccess : "
        << boneTableOffset
        << L"\n"
        << L"iBindAccess     : "
        << inverseBindOffset
        << L"\n"
        << L"VertexAccess    : "
        << vertexOffset
        << L"\n"
        << L"IndexAccess     : "
        << indexOffset
        << L"\n"
        << L"VertexStride    : "
        << sizeof(vnVertex3DFileSkin)
        << L"\n"
        << L"IndexBits       : "
        << (use16BitIndices ? 16 : 32)
        << L"\n"
        << L"FileSize        : "
        << fileSize
        << L"\n"
        << L"========================================\n";

    return true;
}

bool ModelWriter::ValidateSkinnedModel(const std::filesystem::path& filePath)
{
    //============================================================
    // File Open
    //============================================================
    std::ifstream stream( filePath, std::ios::binary |std::ios::ate );

    if (!stream)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Could not open file.\n";

        return false;
    }

    //============================================================
    // File Size
    //============================================================
    const std::streamsize fileSizeSigned = stream.tellg();


    if (fileSizeSigned <= 0)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "File is empty.\n";

        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(fileSizeSigned);

    //============================================================
    // Headerより小さい時点で壊れている
    //============================================================
    if (fileSize < sizeof(vnModelData))
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "File is smaller than header.\n";

        return false;
    }

    //============================================================
    // File全体をMemoryへ読み込む
    //============================================================
    stream.seekg(0, std::ios::beg );

    std::vector<std::uint8_t> fileData(static_cast<std::size_t>(fileSize));

    stream.read(
        reinterpret_cast<char*>(fileData.data()),
        fileSizeSigned
    );

    if (!stream)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Could not read file.\n";

        return false;
    }

    stream.close();

    //============================================================
    // 指定範囲がFile内に収まっているか確認するHelper
    // offset + size でOverflowしないように
    // size <= fileSize - offset
    // の形で判定する
    //============================================================
    const auto IsValidRange = [fileSize](std::uint64_t offset, std::uint64_t size) -> bool
        {
            if (offset > fileSize)
            {
                return false;
            }

            return size <= (fileSize - offset);
        };


    //============================================================
    // Header取得
    // reinterpret_castで直接参照するとAlignment依存が発生するので
    // memcpyでLocal変数へ取得する
    //============================================================
    vnModelData header = {};


    std::memcpy(
        &header,
        fileData.data(),
        sizeof(header)
    );


    //============================================================
    // Magic
    // "vnm\0"
    //============================================================
    if (header.Magic != VN_MODEL_MAGIC)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Invalid Magic.\n"
            << "Magic = 0x"
            << std::hex
            << header.Magic
            << std::dec
            << "\n";

        return false;
    }


    //============================================================
    // Version
    //============================================================
    if (header.Version != vnMODEL_DATA_V2_VER &&
        header.Version != vnMODEL_DATA_V3_VER)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Invalid Version.\n";

        return false;
    }

    //============================================================
    // Header Size
    //============================================================
    if (header.HeaderSize != sizeof(vnModelData))
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Invalid HeaderSize.\n"
            << "HeaderSize = "
            << header.HeaderSize
            << "\n"
            << "Expected   = "
            << sizeof(vnModelData)
            << "\n";

        return false;
    }


    //============================================================
    // Skinned Vertex Stride
    //============================================================
    if (header.VertexStride != sizeof(vnVertex3DFileSkin))
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Invalid VertexStride.\n"
            << "VertexStride = "
            << header.VertexStride
            << "\n"
            << "Expected     = "
            << sizeof(vnVertex3DFileSkin)
            << "\n";

        return false;
    }


    //============================================================
    // Bone確認
    //============================================================
    if (header.BoneNum == 0)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "BoneNum is zero.\n";

        return false;
    }


    //============================================================
    // Vertex / Indexも0なら異常
    //============================================================
    if (header.VertexNum == 0 || header.IndexNum == 0)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "VertexNum or IndexNum is zero.\n";

        return false;
    }


    //============================================================
    // Section Size計算
    //============================================================
    std::uint64_t materialStride = 0u;

    if (header.Version == vnMODEL_DATA_V2_VER)
    {
        materialStride = sizeof(vnModel_MaterialDataV2);
    }
    else if (header.Version == vnMODEL_DATA_V3_VER)
    {
        materialStride = sizeof(vnModel_MaterialDataV3);
    }

    //============================================================
    // Material Section Size
    //============================================================
    const std::uint64_t materialSize =
        static_cast<std::uint64_t>(header.MaterialNum) * materialStride;

    const std::uint64_t meshSize =
        static_cast<std::uint64_t>(header.MeshNum) * sizeof(vnModel_MeshData);

    const std::uint64_t boneTableSize =
        static_cast<std::uint64_t>(header.BoneNum) * sizeof(std::uint16_t);

    const std::uint64_t inverseBindSize =
        static_cast<std::uint64_t>(header.BoneNum) * sizeof(DirectX::XMFLOAT4X4);

    const std::uint64_t vertexSize =
        static_cast<std::uint64_t>(header.VertexNum) * sizeof(vnVertex3DFileSkin);


    //============================================================
    // Index Format
    // Writerと同じ規則
    // VertexNum <= 65535
    //      → uint16
    // それ以上
    //      → uint32
    //============================================================
    const bool use16BitIndices = header.VertexNum <= (std::numeric_limits<std::uint16_t>::max)();

    const std::uint32_t indexStride = use16BitIndices ? sizeof(std::uint16_t) : sizeof(std::uint32_t);

    const std::uint64_t indexSize = static_cast<std::uint64_t>(header.IndexNum) * indexStride;

    //============================================================
    // 各SectionがFile内か
    //============================================================
    if (!IsValidRange(header.MaterialAccess, materialSize))
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Material section is out of range.\n";

        return false;
    }


    if (!IsValidRange(header.MeshAccess, meshSize))
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Mesh section is out of range.\n";

        return false;
    }


    if (!IsValidRange(header.BoneTableAccess, boneTableSize))
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "BoneTable section is out of range.\n";

        return false;
    }


    if (!IsValidRange(header.iBindAccess, inverseBindSize))
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "InverseBind section is out of range.\n";

        return false;
    }


    if (!IsValidRange(header.VertexAccess, vertexSize))
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Vertex section is out of range.\n";

        return false;
    }


    if (!IsValidRange(header.IndexAccess, indexSize))
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Index section is out of range.\n";

        return false;
    }


    //============================================================
    // Section順序
    //
    // Header
    // Material
    // Mesh
    // BoneTable
    // Padding
    // iBind
    // Vertex
    // Index
    //============================================================
    if (header.MaterialAccess >= header.MeshAccess ||
        header.MeshAccess >= header.BoneTableAccess ||
        header.BoneTableAccess >= header.iBindAccess ||
        header.iBindAccess >= header.VertexAccess ||
        header.VertexAccess >= header.IndexAccess)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "Invalid section order.\n";

        return false;
    }


    //============================================================
    // iBindは16byte Alignment
    //============================================================
    if ((header.iBindAccess % 16u) != 0u)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "iBindAccess is not 16-byte aligned.\n"
            << "iBindAccess = "
            << header.iBindAccess
            << "\n";

        return false;
    }


    //============================================================
    // BoneTable取得
    //============================================================
    std::vector<std::uint16_t> boneTable(header.BoneNum);


    std::memcpy(
        boneTable.data(),
        fileData.data() + header.BoneTableAccess,
        static_cast<std::size_t>(boneTableSize)
    );


    //============================================================
    // 同名.boneが存在する場合
    // BoneTableがFull Skeleton範囲内か確認する
    // TestBoneModel.vnm
    //      ↓
    // TestBoneModel.bone
    //============================================================
    std::filesystem::path bonePath = filePath;

    bonePath.replace_extension(L".bone");

    std::size_t fullSkeletonBoneCount = 0;

    if (std::filesystem::exists(bonePath))
    {
        std::error_code error;

        const std::uintmax_t boneFileSize = std::filesystem::file_size( bonePath,  error );

        if (!error)
        {

            //====================================================
            // .bone V2 Size Validation
            //====================================================
            if (boneFileSize % sizeof(vnModel_BoneDataV2) != 0)
            {
                std::cerr
                    << "[Skinned VNM Validate] "
                    << "Invalid V2 .bone file size.\n"
                    << "FileSize     : "
                    << boneFileSize
                    << "\n"
                    << "BoneDataSize : "
                    << sizeof(vnModel_BoneDataV2)
                    << "\n";

                return false;
            }

            //====================================================
            // Full Skeleton Bone Count
            //====================================================
            fullSkeletonBoneCount = static_cast<std::size_t>(boneFileSize / sizeof(vnModel_BoneDataV2));

            //====================================================
            // BoneTable
            // Skin Bone Index
            //      ↓
            // Full Skeleton Index
            // がV2 .boneのBone数内か確認する
            //====================================================
            for (std::size_t i = 0; i < boneTable.size(); ++i)
            {
                if (boneTable[i] >= fullSkeletonBoneCount)
                {
                    std::cerr
                        << "[Skinned VNM Validate] "
                        << "BoneTable index out of range.\n"
                        << "SkinBone     : "
                        << i
                        << "\n"
                        << "SkeletonBone : "
                        << boneTable[i]
                        << "\n"
                        << "BoneCount    : "
                        << fullSkeletonBoneCount
                        << "\n";

                    return false;
                }
            }
        }
    }
    else
    {
        //========================================================
        // VNM単体のValidation自体はできるのでFailにはしない
        //========================================================
        std::cout
            << "[Skinned VNM Validate] "
            << "Warning: .bone file was not found.\n";
    }


    //============================================================
    // Inverse Bind確認
    // NaN / Infだけ検出する
    //============================================================
    for (UINT boneIndex = 0; boneIndex < header.BoneNum; ++boneIndex)
    {
        DirectX::XMFLOAT4X4 matrix = {};

        const std::uint64_t matrixOffset =
            static_cast<std::uint64_t>(header.iBindAccess) +
            static_cast<std::uint64_t>(boneIndex) *
            sizeof(DirectX::XMFLOAT4X4);

        std::memcpy(
            &matrix,
            fileData.data() + matrixOffset,
            sizeof(matrix)
        );

        const float* values = reinterpret_cast<const float*>(&matrix);

        for (int element = 0; element < 16; ++element)
        {
            if (!std::isfinite(values[element]))
            {
                std::cerr
                    << "[Skinned VNM Validate] "
                    << "InverseBind contains NaN/Inf.\n"
                    << "Bone    : "
                    << boneIndex
                    << "\n"
                    << "Element : "
                    << element
                    << "\n";

                return false;
            }
        }
    }


    //============================================================
    // Skin Vertex Validation
    //============================================================
    for (UINT vertexIndex = 0; vertexIndex < header.VertexNum; ++vertexIndex)
    {
        vnVertex3DFileSkin vertex = {};

        const std::uint64_t vertexOffset =
            static_cast<std::uint64_t>(header.VertexAccess) +
            static_cast<std::uint64_t>(vertexIndex) *
            sizeof(vnVertex3DFileSkin);

        std::memcpy(
            &vertex,
            fileData.data() + vertexOffset,
            sizeof(vertex)
        );

        float totalWeight = 0.0f;

        for (int influence = 0; influence < 4; ++influence)
        {
            const float weight = vertex.skinWeight[influence];

            //====================================================
            // Weight自体がNaN / Infなら即異常
            //====================================================
            if (!std::isfinite(weight))
            {
                std::cerr
                    << "[Skinned VNM Validate] "
                    << "Skin weight contains NaN/Inf.\n"
                    << "Vertex : "
                    << vertexIndex
                    << "\n";

                return false;
            }


            //====================================================
            // -Weightは異常
            //====================================================
            if (weight < 0.0f)
            {
                std::cerr
                    << "[Skinned VNM Validate] "
                    << "Negative skin weight.\n"
                    << "Vertex : "
                    << vertexIndex
                    << "\n";

                return false;
            }


            //====================================================
            // Weightが存在するBoneだけIndexを確認
            //====================================================
            if (weight > 0.000001f)
            {
                if (vertex.skinIndex[influence] >= header.BoneNum)
                {
                    std::cerr
                        << "[Skinned VNM Validate] "
                        << "Skin bone index out of range.\n"
                        << "Vertex    : "
                        << vertexIndex
                        << "\n"
                        << "Influence : "
                        << influence
                        << "\n"
                        << "BoneIndex : "
                        << vertex.skinIndex[influence]
                        << "\n"
                        << "BoneNum   : "
                        << header.BoneNum
                        << "\n";

                    return false;
                }
            }


            totalWeight += weight;
        }


        //========================================================
        // Import時にTop4へ絞ったあと
        // 再正規化しているので Sum ≒ 1.0になる
        //========================================================
        if (std::abs(totalWeight - 1.0f) > 0.01f)
        {
            std::cerr
                << "[Skinned VNM Validate] "
                << "Invalid skin weight sum.\n"
                << "Vertex : "
                << vertexIndex
                << "\n"
                << "Weight : "
                << totalWeight
                << "\n";

            return false;
        }
    }


    //============================================================
    // Index Validation
    //============================================================
    if (use16BitIndices)
    {
        for (UINT index = 0; index < header.IndexNum; ++index)
        {
            std::uint16_t value = 0;

            const std::uint64_t offset =
                static_cast<std::uint64_t>(header.IndexAccess) +
                static_cast<std::uint64_t>(index) *
                sizeof(std::uint16_t);


            std::memcpy(
                &value,
                fileData.data() + offset,
                sizeof(value)
            );


            if (value >= header.VertexNum)
            {
                std::cerr
                    << "[Skinned VNM Validate] "
                    << "Index out of range.\n"
                    << "IndexPosition : "
                    << index
                    << "\n"
                    << "IndexValue    : "
                    << value
                    << "\n";

                return false;
            }
        }
    }
    else
    {
        for (UINT index = 0; index < header.IndexNum; ++index)
        {
            std::uint32_t value = 0;


            const std::uint64_t offset =
                static_cast<std::uint64_t>(header.IndexAccess) +
                static_cast<std::uint64_t>(index) *
                sizeof(std::uint32_t);


            std::memcpy(
                &value,
                fileData.data() + offset,
                sizeof(value)
            );


            if (value >= header.VertexNum)
            {
                std::cerr
                    << "[Skinned VNM Validate] "
                    << "Index out of range.\n"
                    << "IndexPosition : "
                    << index
                    << "\n"
                    << "IndexValue    : "
                    << value
                    << "\n";

                return false;
            }
        }
    }


    //============================================================
    // Mesh Validation
    //============================================================
    for (UINT meshIndex = 0; meshIndex < header.MeshNum; ++meshIndex)
    {
        vnModel_MeshData mesh = {};

        const std::uint64_t offset =
            static_cast<std::uint64_t>(header.MeshAccess) +
            static_cast<std::uint64_t>(meshIndex) *
            sizeof(vnModel_MeshData);

        std::memcpy(
            &mesh,
            fileData.data() + offset,
            sizeof(mesh)
        );


        //========================================================
        // Material ID
        //========================================================
        if (mesh.MaterialID >= header.MaterialNum)
        {
            std::cerr
                << "[Skinned VNM Validate] "
                << "Mesh MaterialID out of range.\n"
                << "Mesh       : "
                << meshIndex
                << "\n"
                << "MaterialID : "
                << mesh.MaterialID
                << "\n";

            return false;
        }


        //========================================================
        // Index範囲
        // StartIndex + IndexNum <= Total IndexNum
        //========================================================
        const std::uint64_t meshIndexEnd =
            static_cast<std::uint64_t>(mesh.StartIndex) +
            static_cast<std::uint64_t>(mesh.IndexNum);


        if (meshIndexEnd > header.IndexNum)
        {
            std::cerr
                << "[Skinned VNM Validate] "
                << "Mesh index range is invalid.\n"
                << "Mesh : "
                << meshIndex
                << "\n";

            return false;
        }


        //========================================================
        // 今のConverterはGlobal Index方式なので
        // StartVertexは0であることを確認
        //========================================================
        if (mesh.StartVertex != 0)
        {
            std::cerr
                << "[Skinned VNM Validate] "
                << "Unexpected StartVertex.\n"
                << "Mesh        : "
                << meshIndex
                << "\n"
                << "StartVertex : "
                << mesh.StartVertex
                << "\n";

            return false;
        }
    }


    //============================================================
    // File Size
    // WriterではIndexが最後のSectionなので
    // IndexAccess
    //      +
    // IndexNum * IndexStride
    //  File Size
    //============================================================
    const std::uint64_t expectedFileSize =
        static_cast<std::uint64_t>(header.IndexAccess) + indexSize;


    if (expectedFileSize != fileSize)
    {
        std::cerr
            << "[Skinned VNM Validate] "
            << "FileSize mismatch.\n"
            << "Actual   : "
            << fileSize
            << "\n"
            << "Expected : "
            << expectedFileSize
            << "\n";

        return false;
    }


    //============================================================
    // Validation Success
    //============================================================
    std::cout
        << "\n========================================\n"
        << " Skinned VNM V2 Validation Success\n"
        << "========================================\n"
        << "Materials       : "
        << header.MaterialNum
        << "\n"
        << "Meshes          : "
        << header.MeshNum
        << "\n"
        << "Vertices        : "
        << header.VertexNum
        << "\n"
        << "Indices         : "
        << header.IndexNum
        << "\n"
        << "Skin Bones      : "
        << header.BoneNum
        << "\n"
        << "Full Bones      : "
        << fullSkeletonBoneCount
        << "\n"
        << "BoneTableAccess : "
        << header.BoneTableAccess
        << "\n"
        << "iBindAccess     : "
        << header.iBindAccess
        << "\n"
        << "VertexAccess    : "
        << header.VertexAccess
        << "\n"
        << "IndexAccess     : "
        << header.IndexAccess
        << "\n"
        << "VertexStride    : "
        << header.VertexStride
        << "\n"
        << "IndexBits       : "
        << (use16BitIndices ? 16 : 32)
        << "\n"
        << "FileSize        : "
        << fileSize
        << "\n"
        << "========================================\n";

    return true;
}

//============================================================
// Alignment
// valueをalignment境界へ切り上げる
//============================================================
std::uint64_t ModelWriter::AlignUp(std::uint64_t value, std::uint64_t alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

UINT ModelWriter::CreateVNMDate()
{
    const std::time_t now = std::time(nullptr);

    std::tm localTime = {};

    localtime_s(&localTime, &now);

    const UINT year = static_cast<UINT>(localTime.tm_year + 1900);

    const UINT month = static_cast<UINT>(localTime.tm_mon + 1);

    const UINT day = static_cast<UINT>(localTime.tm_mday);

    //========================================================
    // Decimal Digit
    //      ↓
    // Hex Nibble
    //========================================================
    UINT result = 0u;

    const UINT digits[8] =
    {
        (year / 1000) % 10,
        (year / 100) % 10,
        (year / 10) % 10,
        year % 10,
        (month / 10) % 10,
        month % 10,
        (day / 10) % 10,
        day % 10
    };


    for (UINT digit : digits)
    {
        result <<= 4u;
        result |= digit;
    }

    return result;
}
