#pragma once
//============================================================
// Standard Library
//============================================================
#include <filesystem>
#include <string>


//============================================================
// DirectXTex
//============================================================
#include <DirectXTex.h>


//============================================================
// TextureProcessor
// AssetConverter専用Texture変換処理
// Runtime側では PNG JPG EXR TGA
// ここでDDSへ統一する
//============================================================
class TextureProcessor
{
public:

    //============================================================
    // 通常TextureをDDSへ変換
    // BaseColor / EmissiveなどはsRGB=true
    // Normal / ORM / ThicknessはLinearなので
    // sRGB=false
    //============================================================
    static bool ConvertToDDS(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& outputPath,
        bool sRGB
    );


    //============================================================
    // Normal MapをDDSへ変換
    // OpenGL形式ならGreen Channelを反転する
    // OpenGL: Y+
    // DirectX: Y-
    //============================================================
    static bool ConvertNormalToDDS(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& outputPath,
        bool isOpenGL
    );

    static bool SaveCompressedDDS(
        const DirectX::ScratchImage& source,
        const std::filesystem::path& outputPath,
        DXGI_FORMAT format,
        bool forceCompression = false
    );


    //============================================================
    // ORM生成
    // R = AO
    // G = Roughness
    // B = Metallic
    // A = 1
    // 各Textureが存在しない場合はFallback値を使用する
    //============================================================
    static bool BuildORM(
        const std::filesystem::path& aoPath,
        const std::filesystem::path& roughnessPath,
        const std::filesystem::path& metallicPath,
        const std::filesystem::path& outputPath,
        float defaultAO,
        float defaultRoughness,
        float defaultMetallic
    );

    //============================================================
    // 複数のNon-Skin MaskからSkinMaskを生成
    //============================================================
    static bool BuildSkinMaskFromMaterialMasks(
        const std::filesystem::path& mask01Path,
        const std::filesystem::path& mask02Path,
        const std::filesystem::path& outputPath
    );

    //============================================================
    // Thickness Map専用変換
    // 1Channel Data Textureなので
    // BC4_UNORMとして保存する
    //============================================================
    static bool ConvertThicknessToDDS(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& outputPath,
        bool invertThickness = false
    );

    //============================================================
    // Skin Mask専用変換
    //============================================================
    static bool ConvertSkinMaskToDDS(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& outputPath,
        bool invertMask = false
    );

    //============================================================
    // Embedded Texture Binary
    //      ↓
    // Runtime DDS
    //============================================================
    static bool ConvertMemoryToDDS(
        const std::uint8_t* data,
        std::size_t dataSize,
        const std::string& formatHint,
        const std::filesystem::path& outputPath,
        bool sRGB
    );

private:

    //============================================================
    // どの形式からでもFloat RGBAへ統一して読み込む
    //============================================================
    static bool LoadToFloatRGBA(
        const std::filesystem::path& path,
        DirectX::ScratchImage& output
    );


    //============================================================
    // EXR専用Loader
    //============================================================
    static bool LoadEXR(
        const std::filesystem::path& path,
        DirectX::ScratchImage& output
    );


    //============================================================
    // Extension取得
    //============================================================
    static std::wstring GetLowerExtension(
        const std::filesystem::path& path
    );

};