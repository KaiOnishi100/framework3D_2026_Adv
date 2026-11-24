#pragma once

#include <filesystem>
#include <string>

#include <assimp/material.h>

#include "ImportedModel.h"
#include "../texture/TextureSourceTypes.h"

//============================================================
// TextureResolver
// Assimp MaterialとModel周辺のTexture Fileを調べて
// BaseColor
// Normal
// ORM
// AO
// Roughness
// Metallic
// Thickness
// Emissive
// を自動的に解決する
//============================================================
class TextureResolver
{
public:

    //========================================================
    // Material Texture一括解決
    //========================================================
    static ResolvedTextureSet  Resolve(
        const aiMaterial* material,
        const std::filesystem::path& modelPath,
        bool allowLooseFallback
    );


private:

    //========================================================
    // Assimp Material SlotからTexture Path取得
    //========================================================
    static ResolvedTexture GetAssimpTexture(
        const aiMaterial* material,
        aiTextureType type,
        const std::filesystem::path& modelPath,
        bool* outHadReference = nullptr
    );


    //========================================================
    // ファイル名SuffixからTextureを探す
    //========================================================
    static ResolvedTexture  FindTextureBySuffix(
        const std::filesystem::path& directory,
        const std::string& materialName,
        const std::vector<std::string>& suffixes,
        bool allowLooseFallback
    );

    //============================================================
    // BaseColor専用Fallback
    //============================================================
    static ResolvedTexture  FindBaseColorTexture(
        const std::filesystem::path& directory,
        const std::string& materialName,
        bool allowLooseFallback
    );

    //========================================================
    // 小文字化
    //========================================================
    static std::string ToLower(std::string text);
};