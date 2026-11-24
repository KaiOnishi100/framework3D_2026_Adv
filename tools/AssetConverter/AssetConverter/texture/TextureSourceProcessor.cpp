#include "TextureSourceProcessor.h"


//============================================================
// Standard Library
//============================================================
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>


//============================================================
// Embedded Texture検索
//============================================================
const ImportedEmbeddedTexture*TextureSourceProcessor::FindEmbeddedTexture(
    const ImportedModel& model,
    int embeddedIndex
)
{
    //========================================================
    // 不正Index
    //========================================================
    if (embeddedIndex < 0)
    {
        return nullptr;
    }

    //========================================================
    // ImportedModel内を検索
    // ※vector indexへ直接アクセスしない理由:
    // ※Embedded TextureをFilterした場合でも
    // ※ImportedEmbeddedTexture::Indexとの対応を維持するため
    //========================================================
    for (const ImportedEmbeddedTexture& texture : model.EmbeddedTextures)
    {
        if (texture.Index == embeddedIndex)
        {
            return &texture;
        }
    }

    return nullptr;
}


//============================================================
// Format Hint → Extension
//============================================================
std::string TextureSourceProcessor::MakeExtension(const std::string& format)
{
    if (format.empty())
    {
        return ".bin";
    }

    std::string lower = format;

    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
        {
            return  static_cast<char>(std::tolower(c));
        }
    );

    if (lower[0] == '.')
    {
        return lower;
    }

    //========================================================
    // ※jpegは.jpgへ統一してもよい
    //========================================================
    if (lower == "jpeg")
    {
        return ".jpg";
    }

    return "." + lower;
}


//============================================================
// 圧縮Embedded Textureを書き出す
//============================================================
std::filesystem::path TextureSourceProcessor::WriteCompressedEmbeddedTexture(
    const ImportedEmbeddedTexture& texture,
    const std::filesystem::path& outputDirectory,
    const std::string& fileNameHint
)
{
    if (texture.Data.empty())
    {
        return {};
    }


    //========================================================
    // Directory作成
    //========================================================
    std::error_code error;

    std::filesystem::create_directories(outputDirectory, error);


    if (error)
    {
        std::cerr
            << "[TextureSourceProcessor] "
            << "Failed to create directory.\n"
            << outputDirectory.string()
            << "\n";

        return {};
    }


    //========================================================
    // Format Hintから拡張子生成
    //========================================================
    const std::string extension = MakeExtension(texture.Format);

    const std::filesystem::path outputPath = outputDirectory / (fileNameHint + extension);

    //========================================================
    // Binary Fileとして保存
    // PNG/JPGなどは既に完成したFile DataなのでEncode処理は不要
    //========================================================
    std::ofstream file(outputPath, std::ios::binary);


    if (!file)
    {
        std::cerr
            << "[TextureSourceProcessor] "
            << "Failed to open embedded texture output.\n"
            << outputPath.string()
            << "\n";

        return {};
    }


    file.write(
        reinterpret_cast<const char*>(texture.Data.data()),
        static_cast<std::streamsize>(texture.Data.size())
    );


    if (!file)
    {
        std::cerr
            << "[TextureSourceProcessor] "
            << "Failed to write embedded texture.\n";

        return {};
    }


    std::cout
        << "[Embedded Texture Extract]\n"
        << "    Source Index : "
        << texture.Index
        << "\n"
        << "    Format       : "
        << texture.Format
        << "\n"
        << "    Size         : "
        << texture.Data.size()
        << " bytes\n"
        << "    Output       : "
        << outputPath.string()
        << "\n";

    return outputPath;
}


//============================================================
// TGA Header
//============================================================
std::filesystem::path TextureSourceProcessor::WriteRawEmbeddedTextureAsTGA(
    const ImportedEmbeddedTexture& texture,
    const std::filesystem::path& outputDirectory,
    const std::string& fileNameHint
)
{
    if (texture.Width == 0 ||texture.Height == 0 ||texture.Data.empty())
    {
        return {};
    }

    std::error_code error;

    std::filesystem::create_directories(outputDirectory,  error );

    if (error)
    {
        return {};
    }

    const std::filesystem::path outputPath = outputDirectory / (fileNameHint + ".tga");

    std::ofstream file(outputPath, std::ios::binary);

    if (!file)
    {
        return {};
    }

    //========================================================
    // TGA Header
    // 0 : ID Length
    // 1 : Color Map Type
    // 2 : Image Type
    // Image Type = 2
    // 非圧縮True Color
    //========================================================
    std::uint8_t header[18] =
    {
        0,      // ID Length
        0,      // Color Map
        2,      // Uncompressed True Color

        0, 0,   // Color Map First Entry
        0, 0,   // Color Map Length
        0,      // Color Map Entry Size

        0, 0,   // X Origin
        0, 0,   // Y Origin

        0, 0,   // Width
        0, 0,   // Height

        32,     // Bits Per Pixel

        //====================================================
        // 8bit Alpha
        // 0x20: Top Left Origin
        // 0x08: Alpha 8bit
        //====================================================
        0x28
    };


    //========================================================
    // TGAは16bit Width / Height
    //========================================================
    const std::uint16_t width = static_cast<std::uint16_t>(texture.Width);

    const std::uint16_t height = static_cast<std::uint16_t>(texture.Height);

    header[12] = static_cast<std::uint8_t>(width & 0xFF);

    header[13] = static_cast<std::uint8_t>((width >> 8) & 0xFF);

    header[14] = static_cast<std::uint8_t>(height & 0xFF);

    header[15] = static_cast<std::uint8_t>((height >> 8) & 0xFF);


    //========================================================
    // Header
    //========================================================
    file.write(
        reinterpret_cast<const char*>(header),
        sizeof(header)
    );


    //========================================================
    // Pixel Data
    //========================================================
    file.write(
        reinterpret_cast<const char*>(texture.Data.data()),
        static_cast<std::streamsize>(texture.Data.size())
    );


    if (!file)
    {
        return {};
    }


    std::cout
        << "[Embedded Raw Texture Extract]\n"
        << "    Index  : "
        << texture.Index
        << "\n"
        << "    Size   : "
        << texture.Width
        << "x"
        << texture.Height
        << "\n"
        << "    Output : "
        << outputPath.string()
        << "\n";


    return outputPath;
}


//============================================================
// Materialize
//============================================================
std::filesystem::path TextureSourceProcessor::Materialize(
    const ResolvedTexture& texture,
    const ImportedModel& model,
    const std::filesystem::path& outputDirectory,
    const std::string& fileNameHint
)
{
    //========================================================
    // Textureなし
    //========================================================
    if (!texture.IsValid())
    {
        return {};
    }


    //========================================================
    // External Texture
    //========================================================
    if (texture.Type == TextureSourceType::ExternalFile)
    {
        const std::filesystem::path path =
            texture.Path;


        if (!std::filesystem::exists(path))
        {
            std::cerr
                << "[TextureSourceProcessor] "
                << "External texture not found.\n"
                << "    "
                << path.string()
                << "\n";

            return {};
        }


        return
            path.lexically_normal();
    }


    //========================================================
    // Embedded Texture
    //========================================================
    if (texture.Type == TextureSourceType::Embedded)
    {
        const ImportedEmbeddedTexture* embedded = FindEmbeddedTexture(model, texture.EmbeddedIndex);

        if (embedded == nullptr)
        {
            std::cerr
                << "[TextureSourceProcessor] "
                << "Embedded texture not found.\n"
                << "    Index = "
                << texture.EmbeddedIndex
                << "\n";

            return {};
        }


        //====================================================
        // PNG/JPG/DDSなど
        //====================================================
        if (embedded->IsCompressed)
        {
            return
                WriteCompressedEmbeddedTexture(
                    *embedded,
                    outputDirectory,
                    fileNameHint
                );
        }


        //====================================================
        // aiTexel Raw Data
        //====================================================
        return
            WriteRawEmbeddedTextureAsTGA(
                *embedded,
                outputDirectory,
                fileNameHint
            );
    }

    return {};
}