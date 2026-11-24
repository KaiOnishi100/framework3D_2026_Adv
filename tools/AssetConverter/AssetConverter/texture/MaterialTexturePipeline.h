#pragma once

//============================================================
// Standard Library
//============================================================
#include <filesystem>


//============================================================
// Converter Intermediate Model
//============================================================
#include "../Importer/ImportedModel.h"


//============================================================
// MaterialTexturePipeline
// ImportedMaterial::SourceTextures
//
//          ↓
// TextureSourceProcessor
//
//          ↓
// TextureProcessor
//          ↓
// ImportedMaterial::Textures
// をつなぐ高レベル処理
// ※Textureの検索そのものは行わない
// ※Textureの画像処理そのものも行わない
// ※各Texture処理をどの順番で実行するかを管理する
//============================================================
class MaterialTexturePipeline
{
public:

    //========================================================
    // Modelが持つ全MaterialのTextureを処理する
    // model: ModelImporterから取得したImportedModel
    // outputModelPath: 最終的に出力するVNM Path
    //========================================================
    static bool Process(ImportedModel& model, const std::filesystem::path& outputModelPath);


private:

    //========================================================
    // Material 1個分を処理
    //========================================================
    static bool ProcessMaterial(
        ImportedModel& model,
        ImportedMaterial& material,
        std::size_t materialIndex,
        const std::filesystem::path& outputModelPath,
        const std::filesystem::path& temporaryDirectory
    );


    //========================================================
    // BaseColor処理
    //========================================================
    static bool ProcessBaseColor(
        ImportedModel& model,
        ImportedMaterial& material,
        std::size_t materialIndex,
        const std::filesystem::path& outputModelPath,
        const std::filesystem::path& temporaryDirectory
    );

    //============================================================
    // Normal Map処理
    //============================================================
    static bool ProcessNormal(
        ImportedModel& model,
        ImportedMaterial& material,
        std::size_t materialIndex,
        const std::filesystem::path& outputModelPath,
        const std::filesystem::path& temporaryDirectory
    );

    //============================================================
    // ORM Texture処理
    //============================================================
    static bool ProcessORM(
        ImportedModel& model,
        ImportedMaterial& material,
        std::size_t materialIndex,
        const std::filesystem::path& outputModelPath,
        const std::filesystem::path& temporaryDirectory
    );

    //============================================================
    // Emissive Texture処理
    //============================================================
    static bool ProcessEmissive(
        ImportedModel& model,
        ImportedMaterial& material,
        std::size_t materialIndex,
        const std::filesystem::path& outputModelPath,
        const std::filesystem::path& temporaryDirectory
    );

    //============================================================
    // Thickness Texture処理
    // Color TextureではないためsRGB変換は行わない
    //============================================================
    static bool ProcessThickness(
        ImportedModel& model,
        ImportedMaterial& material,
        std::size_t materialIndex,
        const std::filesystem::path& outputModelPath,
        const std::filesystem::path& temporaryDirectory
    );

    //============================================================
    // Skin Mask
    // sRGB変換禁止
    //============================================================
    static bool ProcessSkinMask(
        ImportedModel& model,
        ImportedMaterial& material,
        std::size_t materialIndex,
        const std::filesystem::path& outputModelPath,
        const std::filesystem::path& temporaryDirectory
    );
};