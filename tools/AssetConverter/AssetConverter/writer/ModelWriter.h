#pragma once

//============================================================
// Standard Library
//============================================================
#include <filesystem>
#include <string>
#include <vector>
#include <wrl/client.h>
#include <algorithm>

//============================================================
// Converter Intermediate Data
//============================================================
#include "../importer/ImportedModel.h"
#include "../../../../vn_framework3D_2026_Adv/framework/vn_modelData.h"


//============================================================
// VNM Writer
// ImportedModel
//      ↓
//      .vnm
// へ変換する
//============================================================
class ModelWriter
{
public:

    //============================================================
    // Static Model書き出し
    // model: Assimpから変換された中間Model
    // outputFile: 出力先
    //============================================================
    static bool WriteStaticModel(
        const ImportedModel& model,
        const std::filesystem::path& outputFile
    );

    //============================================================
    // VNM V2
    // Assimp Converter用
    // BaseColor
    // Normal
    // ORM
    // Thickness
    // Emissive
    // PBR Parameters
    // を保存する
    //============================================================
    static bool WriteStaticModelV2(
        const ImportedModel& model,
        const std::filesystem::path& outputFile
    );

    //============================================================
    // Static VNM V3
    // Material V3:
    //   BaseColor
    //   Normal
    //   ORM
    //   Thickness
    //   Emissive
    //   SkinMask
    //============================================================
    static bool WriteStaticModelV3(
        const ImportedModel& model,
        const std::filesystem::path& outputFile
    );

    //============================================================
    // V1 / V2両対応Validation
    //============================================================
    static bool ValidateStaticModel(const std::filesystem::path& filePath);

    //============================================================
    // Skinned Model V2 Writer
    // Material V2
    // Mesh
    // BoneTable
    // InverseBind
    // SkinVertex
    // Index
    // をVNMへ出力する
    //============================================================
    static bool WriteSkinnedModelV2(const ImportedModel& model, const std::filesystem::path& outputFile);

    //============================================================
    // Skinned VNM V3
    // Material V3 +
    // Skinning
    //============================================================
    static bool WriteSkinnedModelV3(const ImportedModel& model, const std::filesystem::path& outputFile);

    //============================================================
    // Skinned VNM V2 Validation
    //============================================================
    static bool ValidateSkinnedModel(const std::filesystem::path& filePath);

private:

    //============================================================
    // Normal MapがOpenGL形式か判定
    // OpenGL: +Y
    // DirectX: -Y
    // Engine側はDirectX Conventionなので、
    // OpenGL形式ならShaderでYを反転できるよう
    // NormalParam.z = -1 にする
    //============================================================
    static bool IsOpenGLNormalMap(const std::string& path);

    //============================================================
    // TextureをOutput Directoryへコピーする
    // sourceTexture: Assimp側で解決済みのTexture Path
    // outputDirectory: VNM出力Folder
    // outputStem: Model名
    // materialIndex: Material番号
    // typeName: BaseColor / Normal / ORM / Thicknessなど
    // 戻り値: VNMへ保存するFile Name
    //============================================================
    static std::wstring CopyTexture(
        const std::string& sourceTexture,
        const std::filesystem::path& outputDirectory,
        const std::wstring& outputStem,
        std::size_t materialIndex,
        const std::wstring& typeName
    );

    static vnModel_MaterialDataV2 BuildMaterialV2(const ImportedMaterial& source);

    static vnModel_MaterialDataV3 BuildMaterialV3(const ImportedMaterial& source);

private:

    //============================================================
    // helper
    //============================================================

    //============================================================
    // WCHAR配列へ安全にコピー
    //============================================================
    template <std::size_t N>static void CopyWideString(WCHAR(&destination)[N], const std::wstring& source)
    {
        destination[0] = L'\0';


        if (source.empty())
        {
            return;
        }


        wcsncpy_s(
            destination,
            N,
            source.c_str(),
            _TRUNCATE
        );
    }

    static std::wstring GetTextureFileName(const std::string& texturePath)
    {
        if (texturePath.empty())
        {
            return {};
        }

        return std::filesystem::path(texturePath).filename().wstring();
    }

private:

    //============================================================
    // 指定Alignmentへ切り上げ
    //============================================================
    static std::uint64_t AlignUp(std::uint64_t value, std::uint64_t alignment);

    static UINT CreateVNMDate();

    static constexpr UINT VN_MODEL_MAGIC = 0x006D6E76u;
};