#include "MaterialTexturePipeline.h"


//============================================================
// Texture Pipeline
//============================================================
#include "TextureSourceProcessor.h"
#include "TextureProcessor.h"


//============================================================
// Standard Library
//============================================================
#include <iostream>
#include <string>


//============================================================
// Model全体のTexture処理
//============================================================
bool MaterialTexturePipeline::Process(ImportedModel& model, const std::filesystem::path& outputModelPath)
{
    //========================================================
    // VNM出力Directory
    // outputModelPath C:/Game/shareModel/TestSKModel.vnm
    // outputDirectory C:/Game/shareModel/
    //========================================================
    const std::filesystem::path outputDirectory = outputModelPath.parent_path();


    //========================================================
    // Embedded Textureを一時的に展開するDirectory
    //========================================================
    const std::filesystem::path temporaryDirectory =
        outputDirectory / "_ImportedTextures" / outputModelPath.stem();


    //========================================================
    // Materialが無ければ何もしない
    //========================================================
    if (model.Materials.empty())
    {
        return true;
    }


    std::cout
        << "\n========================================\n"
        << " Material Texture Pipeline\n"
        << "========================================\n"
        << "Materials : "
        << model.Materials.size()
        << "\n";


    //========================================================
    // Materialごとに処理
    //========================================================
    for (std::size_t materialIndex = 0; materialIndex < model.Materials.size(); ++materialIndex)
    {
        ImportedMaterial& material = model.Materials[materialIndex];


        if (!ProcessMaterial(
            model,
            material,
            materialIndex,
            outputModelPath,
            temporaryDirectory))
        {
            std::cerr
                << "[MaterialTexturePipeline] "
                << "Material processing failed.\n"
                << "Material Index : "
                << materialIndex
                << "\n"
                << "Material Name  : "
                << material.Name
                << "\n";

            return false;
        }
    }


    std::cout
        << "========================================\n"
        << " Material Texture Pipeline Success\n"
        << "========================================\n";


    return true;
}


//============================================================
// Material単位
//============================================================
bool MaterialTexturePipeline::ProcessMaterial(
    ImportedModel& model,
    ImportedMaterial& material,
    std::size_t materialIndex,
    const std::filesystem::path& outputModelPath,
    const std::filesystem::path& temporaryDirectory
)
{
    //========================================================
    // 前回結果が残らないように初期化
    //========================================================
    material.Textures = ImportedTextureSet{};

    std::cout
        << "\n[Material Texture]\n"
        << "    Index : "
        << materialIndex
        << "\n"
        << "    Name  : "
        << material.Name
        << "\n";

    //========================================================
    // BaseColor
    //========================================================
    if (!ProcessBaseColor(
        model,
        material,
        materialIndex,
        outputModelPath,
        temporaryDirectory))
    {
        return false;
    }

    //============================================================
    // Normal
    //============================================================
    if (!ProcessNormal(
        model,
        material,
        materialIndex,
        outputModelPath,
        temporaryDirectory))
    {
        return false;
    }

    //============================================================
    // ORM
    // AO / Roughness / Metallicを1枚へPackingする
    //============================================================
    if (!ProcessORM(
        model,
        material,
        materialIndex,
        outputModelPath,
        temporaryDirectory))
    {
        return false;
    }

    //============================================================
    // Emissive
    //============================================================
    if (!ProcessEmissive(
            model,
            material,
            materialIndex,
            outputModelPath,
            temporaryDirectory))
    {
        return false;
    }

    //============================================================
    // Thickness
    //============================================================
    if (!ProcessThickness(
            model,
            material,
            materialIndex,
            outputModelPath,
            temporaryDirectory))
    {
        return false;
    }

    //============================================================
    // Skin Mask
    //============================================================
    if (!ProcessSkinMask(
        model,
        material,
        materialIndex,
        outputModelPath,
        temporaryDirectory))
    {
        return false;
    }
    return true;
}


//============================================================
// BaseColor
//============================================================
bool MaterialTexturePipeline::ProcessBaseColor(
    ImportedModel& model,
    ImportedMaterial& material,
    std::size_t materialIndex,
    const std::filesystem::path& outputModelPath,
    const std::filesystem::path& temporaryDirectory
)
{
    //========================================================
    // BaseColor Textureそのものが無い
    //========================================================
    if (!material.SourceTextures.BaseColor.IsValid())
    {
        std::cout << "    BaseColor : None\n";

        return true;
    }


    //========================================================
    // Embedded Texture展開用File名
    //========================================================
    const std::string sourceFileName =
        outputModelPath.stem().string() +
        "_Mat" +
        std::to_string(materialIndex)
        + "_BaseColor";


    //========================================================
    // External / Embeddedを実Pathへ統一
    // External: 元PNG/JPG Path
    // Embedded: _ImportedTexturesへ展開したPath
    //========================================================
    const std::filesystem::path sourcePath =
        TextureSourceProcessor::Materialize(
            material.SourceTextures.BaseColor,
            model,
            temporaryDirectory,
            sourceFileName
        );


    //========================================================
    // Source解決失敗
    //========================================================
    if (sourcePath.empty())
    {
        std::cerr
            << "[MaterialTexturePipeline] "
            << "BaseColor materialize failed.\n"
            << "Material : "
            << material.Name
            << "\n";

        return false;
    }


    //========================================================
    // 最終DDS Path
    //========================================================
    const std::filesystem::path outputDDS =
        outputModelPath.parent_path() / (sourceFileName + ".dds");


    //========================================================
    // BaseColorはColor TextureなのでsRGB
    // TextureProcessor内部:
    // Load
    //   ↓
    // RGBA8
    //   ↓
    // sRGB
    //   ↓
    // MipMap
    //   ↓
    // BC7
    //   ↓
    // DDS
    //========================================================
    if (!TextureProcessor::ConvertToDDS(sourcePath, outputDDS, true))
    {
        std::cerr
            << "[MaterialTexturePipeline] "
            << "BaseColor DDS conversion failed.\n"
            << "Source : "
            << sourcePath.string()
            << "\n";

        return false;
    }


    //========================================================
    // 最終DDS PathをImportedMaterialへ保存
    //========================================================
    material.Textures.BaseColor = outputDDS.string();

    std::cout
        << "    BaseColor Source : "
        << sourcePath.string()
        << "\n"
        << "    BaseColor DDS    : "
        << outputDDS.string()
        << "\n";


    return true;
}

bool MaterialTexturePipeline::ProcessNormal(ImportedModel& model,
    ImportedMaterial& material,
    std::size_t materialIndex,
    const std::filesystem::path& outputModelPath,
    const std::filesystem::path& temporaryDirectory)
{
    //========================================================
    // Normal Textureが無いMaterial
    //========================================================
    if (!material.SourceTextures.Normal.IsValid())
    {
        std::cout << "    Normal : None\n";

        return true;
    }


    //========================================================
    // 出力File Name
    // TestBoxModel_Mat0_Normal
    //========================================================
    const std::string fileName =
        outputModelPath.stem().string() + "_Mat" + std::to_string(materialIndex) + "_Normal";


    //========================================================
    // External / Embeddedを実際のFile Pathへ統一
    //========================================================
    const std::filesystem::path sourcePath =
        TextureSourceProcessor::Materialize(
            material.SourceTextures.Normal,
            model,
            temporaryDirectory,
            fileName
        );


    //========================================================
    // Source解決失敗
    //========================================================
    if (sourcePath.empty())
    {
        std::cerr
            << "[MaterialTexturePipeline] "
            << "Normal materialize failed.\n"
            << "Material : "
            << material.Name
            << "\n";

        return false;
    }


    //========================================================
    // 最終DDS Path
    // VNMと同じDirectoryへ出力する
    //========================================================
    const std::filesystem::path outputDDS = outputModelPath.parent_path() / (fileName + ".dds");

    //============================================================
    // Normal Map Convention Detection
    // DirectX:
    //   _nor_dx
    //   _normal_dx
    // OpenGL:
    //   _nor_gl
    //   _normal_gl
    //   opengl
    // OpenGL形式だけGreen Channelを反転する
    //============================================================
    std::string normalFileName = sourcePath.filename().string();

    //============================================================
    // 大文字・小文字を統一する
    //============================================================
    std::transform(normalFileName.begin(), normalFileName.end(), normalFileName.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        }
    );

    //============================================================
    // OpenGL形式か判定
    // 今回の floor_tiles_06_nor_dx_4k.jpg はfalseになる
    //============================================================
    const bool isOpenGLNormal =
        normalFileName.find("_nor_gl") != std::string::npos ||
        normalFileName.find("_normal_gl") != std::string::npos ||
        normalFileName.find("_nrm_gl") != std::string::npos ||
        normalFileName.find("opengl") != std::string::npos;

    //============================================================
    // Normal Map -> DDS
    // DirectX形式: Green反転なし
    // OpenGL形式: Green反転あり
    // DDSへ変換後はDirectX規約に統一する
    //============================================================
    if (!TextureProcessor::ConvertNormalToDDS(sourcePath, outputDDS, isOpenGLNormal))
    {
        std::cerr
            << "[MaterialTexturePipeline] "
            << "Normal DDS conversion failed.\n"
            << "Source : " << sourcePath.string() << "\n";

        return false;
    }

#if defined(_DEBUG)
    std::cout
        << "[Normal Convention] "
        << (isOpenGLNormal ? "OpenGL -> DirectX" : "DirectX")
        << "\n";
#endif

    //========================================================
    // VNM Writerが参照する最終Texture Path
    //========================================================
    material.Textures.Normal = outputDDS.string();


    std::cout
        << "    Normal Source : "
        << sourcePath.string()
        << "\n"
        << "    Normal DDS    : "
        << outputDDS.string()
        << "\n";

    return true;
}

bool MaterialTexturePipeline::ProcessORM(ImportedModel& model,
    ImportedMaterial& material,
    std::size_t materialIndex,
    const std::filesystem::path&
    outputModelPath,
    const std::filesystem::path& temporaryDirectory)
{
    std::cout
        << "\n========================================\n"
        << " ORM Debug\n"
        << "========================================\n"
        << "Material : "
        << material.Name
        << "\n";


    //========================================================
    // Debug表示用Helper
    //========================================================
    const auto PrintResolvedTexture = [](const char* name, const ResolvedTexture& texture)
        {
            std::cout
                << name
                << "\n"
                << "    Valid         : "
                << (texture.IsValid() ? "Yes" : "No")
                << "\n"
                << "    Type          : "
                << static_cast<int>(texture.Type)
                << "\n"
                << "    Path          : "
                << (texture.Path.empty() ? "<empty>" : texture.Path)
                << "\n"
                << "    EmbeddedIndex : "
                << texture.EmbeddedIndex
                << "\n";
        };


    PrintResolvedTexture("ORM", material.SourceTextures.ORM);
    PrintResolvedTexture("AO", material.SourceTextures.AO);
    PrintResolvedTexture("Roughness", material.SourceTextures.Roughness);
    PrintResolvedTexture("Metallic", material.SourceTextures.Metallic);

    //========================================================
    // 最終DDS File名
    //========================================================
    const std::string fileName =
        outputModelPath.stem().string() + "_Mat" + std::to_string(materialIndex) + "_ORM";


    const std::filesystem::path outputDDS =
        outputModelPath.parent_path() / (fileName + ".dds");


    //========================================================
    // Case 1
    // 元からEngine形式のORM Textureが存在する場合
    // こちらを最優先する
    // ※ORMはColor TextureではなくData Textureなので
    // ※sRGBではなくLinearとして扱う
    //========================================================
    if (material.SourceTextures.ORM.IsValid())
    {
        //====================================================
        // External / Embedded
        //      ↓
        // 実際に読み込めるPath
        //====================================================
        const std::filesystem::path sourceORM =
            TextureSourceProcessor::Materialize(
                material.SourceTextures.ORM,
                model,
                temporaryDirectory,
                fileName
            );


        if (sourceORM.empty())
        {
            std::cerr
                << "[MaterialTexturePipeline] "
                << "ORM materialize failed.\n"
                << "Material : "
                << material.Name
                << "\n";

            return false;
        }


        //====================================================
        // ORMはLinear Texture
        //
        // sRGB = false
        //====================================================
        if (!TextureProcessor::ConvertToDDS(sourceORM, outputDDS, false))
        {
            std::cerr
                << "[MaterialTexturePipeline] "
                << "ORM DDS conversion failed.\n"
                << "Source : "
                << sourceORM.string()
                << "\n"
                << "Output : "
                << outputDDS.string()
                << "\n";

            return false;
        }

        //====================================================
        // VNM Writerが参照する最終DDS
        //====================================================
        material.Textures.ORM = outputDDS.string();

        std::cout
            << "    ORM Source : "
            << sourceORM.string()
            << "\n"
            << "    ORM DDS    : "
            << outputDDS.string()
            << "\n";

        return true;
    }


    //========================================================
    // Case 2
    // ORMが無い場合
    // AO
    // Roughness
    // Metallic
    // の個別Textureから生成する
    //========================================================

    //========================================================
    // 個別Textureが1枚も存在しない場合
    // Material ParameterだけでPBR描画できるので
    // ORM Textureそのものを生成する必要はない
    //========================================================
    const bool hasAO = material.SourceTextures.AO.IsValid();

    const bool hasRoughness = material.SourceTextures.Roughness.IsValid();

    const bool hasMetallic = material.SourceTextures.Metallic.IsValid();


    if (!hasAO && !hasRoughness && !hasMetallic)
    {
        std::cout << "    ORM : None\n";

        return true;
    }


    //========================================================
    // BuildORMへ渡すSource Path
    // Textureが存在しないものについてはempty pathのまま
    // BuildORM側でFallback値を使用する
    //========================================================
    std::filesystem::path aoPath;

    std::filesystem::path roughnessPath;

    std::filesystem::path metallicPath;


    //========================================================
    // AO
    //========================================================
    if (hasAO)
    {
        aoPath =
            TextureSourceProcessor::Materialize(
                material.SourceTextures.AO,
                model,
                temporaryDirectory,
                fileName +
                "_AO"
            );


        //====================================================
        // Texture参照があるのに実データを取得できない場合は
        // Assetの不整合としてErrorにする
        //====================================================
        if (aoPath.empty())
        {
            std::cerr
                << "[MaterialTexturePipeline] "
                << "AO materialize failed.\n"
                << "Material : "
                << material.Name
                << "\n";

            return false;
        }
    }


    //========================================================
    // Roughness
    //========================================================
    if (hasRoughness)
    {
        roughnessPath =
            TextureSourceProcessor::Materialize(
                material.SourceTextures.Roughness,
                model,
                temporaryDirectory,
                fileName +
                "_Roughness"
            );


        if (roughnessPath.empty())
        {
            std::cerr
                << "[MaterialTexturePipeline] "
                << "Roughness materialize failed.\n"
                << "Material : "
                << material.Name
                << "\n";

            return false;
        }
    }


    //========================================================
    // Metallic
    //========================================================
    if (hasMetallic)
    {
        metallicPath =
            TextureSourceProcessor::Materialize(
                material.SourceTextures.Metallic,
                model,
                temporaryDirectory,
                fileName +
                "_Metallic"
            );


        if (metallicPath.empty())
        {
            std::cerr
                << "[MaterialTexturePipeline] "
                << "Metallic materialize failed.\n"
                << "Material : "
                << material.Name
                << "\n";

            return false;
        }
    }


    //========================================================
    // ORM生成
    //========================================================
    if (!TextureProcessor::BuildORM(
        aoPath,
        roughnessPath,
        metallicPath,
        outputDDS,
        material.AO,
        material.Roughness,
        material.Metallic))
    {
        std::cerr
            << "[MaterialTexturePipeline] "
            << "ORM Build failed.\n"
            << "Material : "
            << material.Name
            << "\n"
            << "AO        : "
            << (aoPath.empty() ? "<default>" : aoPath.string())
            << "\n"
            << "Roughness : "
            << (roughnessPath.empty() ? "<default>" : roughnessPath.string())
            << "\n"
            << "Metallic  : "
            << (metallicPath.empty() ? "<default>" : metallicPath.string())
            << "\n";

        return false;
    }


    //========================================================
    // 最終DDSをMaterialへ登録
    //========================================================
    material.Textures.ORM = outputDDS.string();


    std::cout
        << "    ORM Build\n"
        << "        AO        : "
        << (aoPath.empty() ? "<default>" : aoPath.string())
        << "\n"
        << "        Roughness : "
        << (roughnessPath.empty() ? "<default>" : roughnessPath.string())
        << "\n"
        << "        Metallic  : "
        << (metallicPath.empty() ? "<default>" : metallicPath.string())
        << "\n"
        << "        Output    : "
        << outputDDS.string()
        << "\n";


    return true;
}

bool MaterialTexturePipeline::ProcessEmissive(ImportedModel& model,
    ImportedMaterial& material,
    std::size_t materialIndex,
    const std::filesystem::path& outputModelPath,
    const std::filesystem::path& temporaryDirectory)
{
    //========================================================
    // Emissive Textureが存在しない場合
    //========================================================
    if (!material.SourceTextures.Emissive.IsValid())
    {
        std::cout << "    Emissive : None\n";

        return true;
    }

    //========================================================
    // 出力Texture名
    //========================================================
    const std::string fileName =
        outputModelPath.stem().string() +
        "_Mat" +
        std::to_string(materialIndex) +
        "_Emissive";


    //========================================================
    // External / Embedded Textureを
    // 実際に読み込めるFile Pathへ統一する
    // External: 元のpng/jpg/etc
    // Embedded: temporaryDirectoryへ一時展開
    //========================================================
    const std::filesystem::path sourcePath =
        TextureSourceProcessor::Materialize(
            material.SourceTextures.Emissive,
            model,
            temporaryDirectory,
            fileName
        );


    //========================================================
    // Source Texture取得失敗
    //========================================================
    if (sourcePath.empty())
    {
        std::cerr
            << "[MaterialTexturePipeline] "
            << "Emissive materialize failed.\n"
            << "Material : "
            << material.Name
            << "\n";

        return false;
    }


    //========================================================
    // 最終DDS出力先/
    // VNMと同じDirectoryへ保存する
    //========================================================
    const std::filesystem::path outputDDS = outputModelPath.parent_path() / (fileName + ".dds");


    //========================================================
    // EmissiveはColor Texture
    // true: sRGB Textureとして保存
    // false: Linear Data Texture
    // Normal / ORMとは違い、Emissiveはtrue
    //========================================================
    if (!TextureProcessor::ConvertToDDS(sourcePath, outputDDS, true))
    {
        std::cerr
            << "[MaterialTexturePipeline] "
            << "Emissive DDS conversion failed.\n"
            << "Source : "
            << sourcePath.string()
            << "\n"
            << "Output : "
            << outputDDS.string()
            << "\n";

        return false;
    }


    //========================================================
    // 最終DDSをMaterialへ登録
    //========================================================
    material.Textures.Emissive = outputDDS.string();


    //========================================================
    // Debug Log
    //========================================================
    std::cout
        << "    Emissive Source : "
        << sourcePath.string()
        << "\n"
        << "    Emissive DDS    : "
        << outputDDS.string()
        << "\n";


    return true;
}

bool MaterialTexturePipeline::ProcessThickness(ImportedModel& model, 
    ImportedMaterial& material, 
    std::size_t materialIndex, 
    const std::filesystem::path& outputModelPath,
    const std::filesystem::path& temporaryDirectory)
{
    //========================================================
    // Thickness Textureが存在しない
    // Thicknessは必須Textureではないため
    // 存在しない場合は正常終了する
    //========================================================
    if (!material.SourceTextures.Thickness.IsValid())
    {
        std::cout << "    Thickness : None\n";

        return true;
    }


    //========================================================
    // 出力File Name
    //========================================================
    const std::string fileName =
        outputModelPath.stem().string() + "_Mat" +
        std::to_string(materialIndex) + "_Thickness";


    //========================================================
    // External / Embedded Texture
    //        ↓
    // 実際に読み込めるFile Pathへ変換
    // External: 元Texture Path
    // Embedded: Temporary Directoryへ展開
    //========================================================
    const std::filesystem::path sourcePath =
        TextureSourceProcessor::Materialize(
            material.SourceTextures.Thickness,
            model,
            temporaryDirectory,
            fileName
        );


    //========================================================
    // Source取得失敗
    //========================================================
    if (sourcePath.empty())
    {
        std::cerr
            << "[MaterialTexturePipeline] "
            << "Thickness materialize failed.\n"
            << "Material : "
            << material.Name
            << "\n";

        return false;
    }


    //========================================================
    // 最終DDS Path
    // VNMと同じDirectoryへ生成する
    //========================================================
    const std::filesystem::path outputDDS =
        outputModelPath.parent_path() / (fileName + ".dds");


    //========================================================
    // Thickness
    //      ↓
    // Runtime DDS
    //========================================================
    if (!TextureProcessor::ConvertThicknessToDDS(
            sourcePath,
            outputDDS,
            false))
    {
        std::cerr
            << "[MaterialTexturePipeline] "
            << "Thickness DDS conversion failed.\n"
            << "Source : "
            << sourcePath.string()
            << "\n"
            << "Output : "
            << outputDDS.string()
            << "\n";

        return false;
    }

    //========================================================
    // ModelWriterが参照する最終DDS Path
    //========================================================
    material.Textures.Thickness = outputDDS.string();

    //========================================================
    // Debug Log
    //========================================================
    std::cout
        << "    Thickness Source : "
        << sourcePath.string()
        << "\n"
        << "    Thickness DDS    : "
        << outputDDS.string()
        << "\n";


    return true;
}

bool MaterialTexturePipeline::ProcessSkinMask(
    ImportedModel& model,
    ImportedMaterial& material,
    std::size_t materialIndex,
    const std::filesystem::path& outputModelPath,
    const std::filesystem::path& temporaryDirectory
)
{
    //============================================================
    // 最終DDS名
    //============================================================
    const std::string fileName =
        outputModelPath.stem().string() +
        "_Mat" +
        std::to_string(materialIndex) +
        "_SkinMask";


    const std::filesystem::path outputDDS =
        outputModelPath.parent_path() / (fileName + ".dds");


    //============================================================
    // Case 1:
    // 完成済みSkinMaskが存在する
    // これを最優先する
    //============================================================
    if (material.SourceTextures.SkinMask.IsValid())
    {
        const std::filesystem::path sourcePath =
            TextureSourceProcessor::Materialize(
                material.SourceTextures.SkinMask,
                model,
                temporaryDirectory,
                fileName
            );


        if (sourcePath.empty())
        {
            return false;
        }


        //========================================================
        // 完成済みSkinMaskは
        // White = Skin
        // Black = Non-Skin
        // を前提とするため反転しない
        //========================================================
        if (!TextureProcessor::ConvertSkinMaskToDDS(sourcePath, outputDDS, false))
        {
            return false;
        }

        material.Textures.SkinMask = outputDDS.string();

        std::cout
            << "    SkinMask Source : "
            << sourcePath.string()
            << "\n"
            << "    SkinMask DDS    : "
            << outputDDS.string()
            << "\n";


        return true;
    }


    //============================================================
    // Case 2:
    // RenderPeople Material Maskなどから自動生成
    //============================================================
    const bool hasMask01 = material.SourceTextures.MaterialMask01.IsValid();

    const bool hasMask02 = material.SourceTextures.MaterialMask02.IsValid();


    if (hasMask01 && hasMask02)
    {
        const std::filesystem::path mask01Path =
            TextureSourceProcessor::Materialize(
                material.SourceTextures.MaterialMask01,
                model,
                temporaryDirectory,
                fileName + "_Mask01"
            );


        const std::filesystem::path mask02Path =
            TextureSourceProcessor::Materialize(
                material.SourceTextures.MaterialMask02,
                model,
                temporaryDirectory,
                fileName + "_Mask02"
            );


        if (mask01Path.empty() || mask02Path.empty())
        {
            std::cerr
                << "[MaterialTexturePipeline] "
                << "SkinMask source materialization failed.\n";

            return false;
        }


        //========================================================
        // 2枚のNon-Skin Material Maskを合成して、
        // Engine用SkinMaskへ変換する
        //========================================================
        if (!TextureProcessor::BuildSkinMaskFromMaterialMasks(mask01Path, mask02Path, outputDDS))
        {
            return false;
        }

        material.Textures.SkinMask = outputDDS.string();

        std::cout
            << "    SkinMask Auto Build\n"
            << "        Mask01 : "
            << mask01Path.string()
            << "\n"
            << "        Mask02 : "
            << mask02Path.string()
            << "\n"
            << "        Output : "
            << outputDDS.string()
            << "\n";


        return true;
    }


    //============================================================
    // SkinMask Sourceなし
    //============================================================
    std::cout << "    SkinMask : None\n";

    return true;
}
