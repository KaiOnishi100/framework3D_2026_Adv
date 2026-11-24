#include "TextureProcessor.h"

//============================================================
// Standard Library
//============================================================
#include <algorithm>
#include <iostream>
#include <vector>
#include <cmath>
#include <cwctype>

//============================================================
// DirectXTex
//============================================================
#include <DirectXTex.h>

//============================================================
// DirectXTex OpenEXR Extension
// vcpkg: directxtex[openexr]
// を有効にして使用する
//============================================================
#include <DirectXTexEXR.h>
using namespace DirectX;


bool TextureProcessor::ConvertToDDS(const std::filesystem::path& sourcePath, const std::filesystem::path& outputPath, bool sRGB)
{
    //========================================================
    // 処理開始
    //========================================================
    std::wcout
        << L"[TextureProcessor] Start\n"
        << L"  Source : "
        << sourcePath.wstring()
        << L"\n";


    //========================================================
    // Load
    //========================================================
    ScratchImage source;


    std::wcout
        << L"[TextureProcessor] Loading...\n";


    if (!LoadToFloatRGBA(sourcePath, source))
    {
        std::wcerr
            << L"[TextureProcessor] Load Failed\n";

        return false;
    }


    std::wcout
        << L"[TextureProcessor] Load Complete\n";


    //========================================================
    // RGBA8へ変換
    //========================================================
    ScratchImage converted;


    std::wcout
        << L"[TextureProcessor] Converting RGBA8...\n";


    HRESULT hr = Convert(
        source.GetImages(),
        source.GetImageCount(),
        source.GetMetadata(),
        DXGI_FORMAT_R8G8B8A8_UNORM,
        TEX_FILTER_DEFAULT,
        TEX_THRESHOLD_DEFAULT,
        converted
    );


    if (FAILED(hr))
    {
        return false;
    }


    std::wcout
        << L"[TextureProcessor] RGBA8 Complete\n";


    //========================================================
    // BaseColor / EmissiveならsRGB
    //========================================================
    if (sRGB)
    {
        if (!converted.OverrideFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))
        {
            return false;
        }
    }


    //========================================================
    // MipMap
    //========================================================
    ScratchImage mipChain;


    std::wcout << L"[TextureProcessor] Generating MipMaps...\n";


    hr = GenerateMipMaps(
        converted.GetImages(),
        converted.GetImageCount(),
        converted.GetMetadata(),
        TEX_FILTER_DEFAULT,
        0,
        mipChain
    );


    if (FAILED(hr))
    {
        return false;
    }


    std::wcout
        << L"[TextureProcessor] MipMaps Complete\n"
        << L"  MipLevels : "
        << mipChain.GetMetadata().mipLevels
        << L"\n";


    //========================================================
    // BC7圧縮
    //========================================================
    std::wcout << L"[TextureProcessor] Finalizing DDS...\n";


    const bool result = SaveCompressedDDS(
        mipChain,
        outputPath,
        sRGB ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM
    );


    std::wcout << L"[TextureProcessor] BC7 Compression Finished\n";


    return result;
}

bool TextureProcessor::ConvertNormalToDDS(const std::filesystem::path& sourcePath, const std::filesystem::path& outputPath, bool isOpenGL)
{
    ScratchImage image;


    if (!LoadToFloatRGBA(sourcePath, image))
    {
        std::wcerr
            << L"[Normal] LoadToFloatRGBA Failed\n"
            << L"  "
            << sourcePath.wstring()
            << L"\n";

        return false;
    }


    const Image* src =
        image.GetImage(
            0,
            0,
            0
        );


    if (src == nullptr)
    {
        std::wcerr << L"[Normal] GetImage Failed\n";

        return false;
    }


    std::wcout
        << L"[Normal] Loaded : "
        << src->width
        << L" x "
        << src->height
        << L"\n";

    //========================================================
    // Float RGBAなので1pixel = float4
    //========================================================
    for (size_t y = 0; y < src->height; ++y)
    {
        float* row =
            reinterpret_cast<float*>(src->pixels + y * src->rowPitch);


        for (size_t x = 0; x < src->width; ++x)
        {
            float& r = row[x * 4 + 0];
            float& g = row[x * 4 + 1];
            float& b = row[x * 4 + 2];

            if (!std::isfinite(r))
            {
                // Normal XのDefault
                r = 0.5f;
            }


            if (!std::isfinite(g))
            {
                // Normal YのDefault
                g = 0.5f;
            }


            if (!std::isfinite(b))
            {
                // Normal ZのDefault
                b = 1.0f;
            }

            //================================================
            // OpenGL → DirectX
            // Green Channel反転
            //================================================
            if (isOpenGL)
            {
                g = 1.0f - g;
            }

            //================================================
            // 安全のため0～1へClamp
            //================================================
            r = std::clamp(r, 0.0f, 1.0f);
            g = std::clamp(g, 0.0f, 1.0f);
            b = std::clamp(b, 0.0f, 1.0f);
        }
    }


    //========================================================
    // Runtime用UNORMへ変換
    //========================================================
    ScratchImage converted;

    HRESULT hr = Convert(
        image.GetImages(),
        image.GetImageCount(),
        image.GetMetadata(),
        DXGI_FORMAT_R8G8B8A8_UNORM,
        TEX_FILTER_DEFAULT,
        TEX_THRESHOLD_DEFAULT,
        converted
    );


    if (FAILED(hr))
    {
        std::wcerr
            << L"[Normal] Convert Failed\n"
            << L"  HRESULT = 0x"
            << std::hex
            << static_cast<unsigned long>(hr)
            << std::dec
            << L"\n";

        return false;
    }

    //============================================================
    // Normal Map MipMap
    //============================================================
    ScratchImage mipChain;


    hr = GenerateMipMaps(
        converted.GetImages(),
        converted.GetImageCount(),
        converted.GetMetadata(),
        TEX_FILTER_DEFAULT,
        0,
        mipChain
    );


    if (FAILED(hr))
    {
        std::wcerr
            << L"[Normal] GenerateMipMaps Failed\n";

        return false;
    }

    return
        SaveCompressedDDS(
            mipChain,
            outputPath,
            DXGI_FORMAT_BC7_UNORM
        );
}

bool TextureProcessor::SaveCompressedDDS(const DirectX::ScratchImage& source, 
    const std::filesystem::path& outputPath,
    DXGI_FORMAT format,
    bool forceCompression
)
{
    using namespace DirectX;


#ifdef _DEBUG

    //============================================================
    // Debug Build
    //
    // 通常Texture:
    //   圧縮時間短縮のため非圧縮保存可能
    // Thickness / SkinMask:
    //   BC4そのものを確認したいため、
    //   forceCompression=trueならここを通過して
    //   下のBC圧縮処理へ進む
    //============================================================
    if (!forceCompression)
    {
        std::wcout
            << L"[TextureProcessor] DEBUG Build\n"
            << L"  Skip BC Compression\n"
            << L"  Saving uncompressed DDS...\n";


        const HRESULT saveHr =
            SaveToDDSFile(
                source.GetImages(),
                source.GetImageCount(),
                source.GetMetadata(),
                DDS_FLAGS_NONE,
                outputPath.c_str()
            );


        if (FAILED(saveHr))
        {
            std::wcerr
                << L"[TextureProcessor] DDS Save Failed\n"
                << L"  HRESULT : 0x"
                << std::hex
                << static_cast<unsigned long>(
                    saveHr
                    )
                << std::dec
                << L"\n";

            return false;
        }


        std::wcout
            << L"[TextureProcessor] DDS Save Complete\n";


        return true;
    }


    std::wcout
        << L"[TextureProcessor] DEBUG Build\n"
        << L"  Forced BC Compression\n";

#endif


    //============================================================
    // ここから先はDebug / Release共通の圧縮処理
    //============================================================
    ScratchImage compressed;


    TEX_COMPRESS_FLAGS flags = TEX_COMPRESS_DEFAULT;


    //============================================================
    // BC7だけは圧縮コストが高いためQUICKを使用
    // BC4は通常設定のままでよい
    //============================================================
    if (format == DXGI_FORMAT_BC7_UNORM || format == DXGI_FORMAT_BC7_UNORM_SRGB)
    {
        flags =
            static_cast<TEX_COMPRESS_FLAGS>(
                TEX_COMPRESS_BC7_QUICK |
                TEX_COMPRESS_PARALLEL
                );
    }


    std::wcout
        << L"[TextureProcessor] Compress Start\n"
        << L"  Width     : "
        << source.GetMetadata().width
        << L"\n"
        << L"  Height    : "
        << source.GetMetadata().height
        << L"\n"
        << L"  MipLevels : "
        << source.GetMetadata().mipLevels
        << L"\n"
        << L"  Format    : "
        << static_cast<int>(format)
        << L"\n";


    HRESULT hr =
        Compress(
            source.GetImages(),
            source.GetImageCount(),
            source.GetMetadata(),
            format,
            flags,
            TEX_THRESHOLD_DEFAULT,
            compressed
        );


    //============================================================
    // OpenMPなしDirectXTexでPARALLELが使えない場合
    // BC7だけ該当する可能性がある
    //============================================================
    if (hr == E_NOTIMPL)
    {
        TEX_COMPRESS_FLAGS retryFlags =
            TEX_COMPRESS_DEFAULT;


        if (format == DXGI_FORMAT_BC7_UNORM ||
            format == DXGI_FORMAT_BC7_UNORM_SRGB)
        {
            retryFlags =
                TEX_COMPRESS_BC7_QUICK;
        }


        hr =
            Compress(
                source.GetImages(),
                source.GetImageCount(),
                source.GetMetadata(),
                format,
                retryFlags,
                TEX_THRESHOLD_DEFAULT,
                compressed
            );
    }


    if (FAILED(hr))
    {
        std::wcerr
            << L"[TextureProcessor] Compression Failed\n"
            << L"  HRESULT : 0x"
            << std::hex
            << static_cast<unsigned long>(
                hr
                )
            << std::dec
            << L"\n";

        return false;
    }


    //============================================================
    // BC圧縮済みDDS保存
    //============================================================
    const HRESULT saveHr =
        SaveToDDSFile(
            compressed.GetImages(),
            compressed.GetImageCount(),
            compressed.GetMetadata(),
            DDS_FLAGS_NONE,
            outputPath.c_str()
        );


    if (FAILED(saveHr))
    {
        std::wcerr
            << L"[TextureProcessor] DDS Save Failed\n"
            << L"  HRESULT : 0x"
            << std::hex
            << static_cast<unsigned long>(
                saveHr
                )
            << std::dec
            << L"\n";

        return false;
    }


    std::wcout
        << L"[TextureProcessor] "
        << L"Compressed DDS Save Complete\n";


    return true;
}

bool TextureProcessor::BuildORM(const std::filesystem::path& aoPath, const std::filesystem::path& roughnessPath, const std::filesystem::path& metallicPath, const std::filesystem::path& outputPath, float defaultAO, float defaultRoughness, float defaultMetallic)
{
    ScratchImage aoImage;
    ScratchImage roughnessImage;
    ScratchImage metallicImage;


    const bool hasAO =
        !aoPath.empty() &&
        LoadToFloatRGBA(
            aoPath,
            aoImage
        );


    const bool hasRoughness =
        !roughnessPath.empty() &&
        LoadToFloatRGBA(
            roughnessPath,
            roughnessImage
        );


    const bool hasMetallic =
        !metallicPath.empty() &&
        LoadToFloatRGBA(
            metallicPath,
            metallicImage
        );


    //========================================================
    // どの画像もない場合はTextureを作る必要なし
    //========================================================
    if (!hasAO &&
        !hasRoughness &&
        !hasMetallic)
    {
        return false;
    }


    //========================================================
    // 出力Resolutionを決定
    // 最初に存在するTextureのResolutionを基準にする
    //========================================================
    size_t width = 0;
    size_t height = 0;


    if (hasAO)
    {
        width = aoImage.GetMetadata().width;
        height = aoImage.GetMetadata().height;
    }
    else if (hasRoughness)
    {
        width = roughnessImage.GetMetadata().width;
        height = roughnessImage.GetMetadata().height;
    }
    else
    {
        width = metallicImage.GetMetadata().width;
        height = metallicImage.GetMetadata().height;
    }


    //========================================================
    // 現段階ではResolutionが違ったらError
    //========================================================
    auto CheckSize = [width, height](const ScratchImage& image)
        {
            return
                image.GetMetadata().width ==
                width &&
                image.GetMetadata().height ==
                height;
        };


    if (hasAO && !CheckSize(aoImage))
    {
        return false;
    }


    if (hasRoughness && !CheckSize(roughnessImage))
    {
        return false;
    }


    if (hasMetallic && !CheckSize(metallicImage))
    {
        return false;
    }


    //========================================================
    // ORM用Float Texture作成
    //========================================================
    ScratchImage orm;


    HRESULT hr =
        orm.Initialize2D(
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            width,
            height,
            1,
            1
        );


    if (FAILED(hr))
    {
        return false;
    }


    const Image* outputImage =
        orm.GetImage(
            0,
            0,
            0
        );


    const Image* ao =
        hasAO
        ? aoImage.GetImage(0, 0, 0)
        : nullptr;


    const Image* roughness =
        hasRoughness
        ? roughnessImage.GetImage(0, 0, 0)
        : nullptr;


    const Image* metallic =
        hasMetallic
        ? metallicImage.GetImage(0, 0, 0)
        : nullptr;


    //========================================================
    // Pixel単位でORM Packing
    //========================================================
    for (size_t y = 0; y < height; ++y)
    {
        float* destinationRow =
            reinterpret_cast<float*>(
                outputImage->pixels +
                y *
                outputImage->rowPitch
                );


        const float* aoRow =
            hasAO
            ? reinterpret_cast<const float*>(
                ao->pixels +
                y *
                ao->rowPitch
                )
            : nullptr;


        const float* roughnessRow =
            hasRoughness
            ? reinterpret_cast<const float*>(
                roughness->pixels +
                y *
                roughness->rowPitch
                )
            : nullptr;


        const float* metallicRow =
            hasMetallic
            ? reinterpret_cast<const float*>(
                metallic->pixels +
                y *
                metallic->rowPitch
                )
            : nullptr;


        for (size_t x = 0; x < width; ++x)
        {
            //================================================
            // 各Textureは基本GrayscaleなのでR Channel使用
            //================================================
            const float aoValue =
                hasAO
                ? aoRow[x * 4]
                : defaultAO;


            const float roughnessValue =
                hasRoughness
                ? roughnessRow[x * 4]
                : defaultRoughness;


            const float metallicValue =
                hasMetallic
                ? metallicRow[x * 4]
                : defaultMetallic;


            destinationRow[x * 4 + 0] =
                std::clamp(
                    aoValue,
                    0.0f,
                    1.0f
                );


            destinationRow[x * 4 + 1] =
                std::clamp(
                    roughnessValue,
                    0.0f,
                    1.0f
                );


            destinationRow[x * 4 + 2] =
                std::clamp(
                    metallicValue,
                    0.0f,
                    1.0f
                );


            destinationRow[x * 4 + 3] =
                1.0f;
        }
    }


    //========================================================
    // Runtime Textureへ変換
    //========================================================
    ScratchImage converted;


    hr = Convert(
        orm.GetImages(),
        orm.GetImageCount(),
        orm.GetMetadata(),
        DXGI_FORMAT_R8G8B8A8_UNORM,
        TEX_FILTER_DEFAULT,
        TEX_THRESHOLD_DEFAULT,
        converted
    );


    if (FAILED(hr))
    {
        return false;
    }

    //============================================================
    // ORM MipMap
    // ORMは色ではなくData TextureなのでLinearのまま
    //============================================================
    ScratchImage mipChain;


    hr = GenerateMipMaps(
        converted.GetImages(),
        converted.GetImageCount(),
        converted.GetMetadata(),
        TEX_FILTER_DEFAULT,
        0,
        mipChain
    );


    if (FAILED(hr))
    {
        return false;
    }

    return
        SaveCompressedDDS(
            mipChain,
            outputPath,
            DXGI_FORMAT_BC7_UNORM
        );
}

bool TextureProcessor::BuildSkinMaskFromMaterialMasks(
    const std::filesystem::path& mask01Path,
    const std::filesystem::path& mask02Path,
    const std::filesystem::path& outputPath)
{
    using namespace DirectX;


    //============================================================
    // 両方のMaskをFloat RGBAとして読み込む
    //============================================================
    ScratchImage mask01;
    ScratchImage mask02;


    if (!LoadToFloatRGBA(
        mask01Path,
        mask01)
        )
    {
        std::wcerr
            << L"[SkinMask Build] Mask01 Load Failed\n"
            << L"  "
            << mask01Path.wstring()
            << L"\n";

        return false;
    }


    if (!LoadToFloatRGBA(
        mask02Path,
        mask02)
        )
    {
        std::wcerr
            << L"[SkinMask Build] Mask02 Load Failed\n"
            << L"  "
            << mask02Path.wstring()
            << L"\n";

        return false;
    }


    const Image* image01 =
        mask01.GetImage(
            0,
            0,
            0
        );

    const Image* image02 =
        mask02.GetImage(
            0,
            0,
            0
        );


    if (image01 == nullptr || image02 == nullptr)
    {
        return false;
    }


    //============================================================
    // 同解像度を要求する
    //============================================================
    if (image01->width != image02->width || image01->height != image02->height)
    {
        std::wcerr
            << L"[SkinMask Build] "
            << L"Mask Size Mismatch\n"
            << L"  Mask01 : "
            << image01->width
            << L" x "
            << image01->height
            << L"\n"
            << L"  Mask02 : "
            << image02->width
            << L" x "
            << image02->height
            << L"\n";

        return false;
    }


    //============================================================
    // SkinMask作業Texture
    // R32_FLOATで作る
    //============================================================
    ScratchImage skinMaskFloat;


    HRESULT hr =
        skinMaskFloat.Initialize2D(
            DXGI_FORMAT_R32_FLOAT,
            image01->width,
            image01->height,
            1,
            1
        );


    if (FAILED(hr))
    {
        return false;
    }


    const Image* destinationImage =
        skinMaskFloat.GetImage(
            0,
            0,
            0
        );


    if (destinationImage == nullptr)
    {
        return false;
    }


    //============================================================
    // mask01 / mask02を合成
    //============================================================
    for (size_t y = 0; y < image01->height; ++y)
    {
        const float* row01 =
            reinterpret_cast<const float*>(image01->pixels + y * image01->rowPitch);


        const float* row02 =
            reinterpret_cast<const float*>(image02->pixels + y * image02->rowPitch);


        float* destinationRow =
            reinterpret_cast<float*>(destinationImage->pixels + y * destinationImage->rowPitch);


        for (size_t x = 0; x < image01->width; ++x)
        {
            //================================================
            // RGBA32FなのでR Channelは x * 4 + 0
            //================================================
            float maskValue01 =
                row01[x * 4 + 0];

            float maskValue02 =
                row02[x * 4 + 0];


            //================================================
            // 安全対策
            //================================================
            if (!std::isfinite(maskValue01))
            {
                maskValue01 = 0.0f;
            }

            if (!std::isfinite(maskValue02))
            {
                maskValue02 = 0.0f;
            }


            maskValue01 =
                std::clamp(
                    maskValue01,
                    0.0f,
                    1.0f
                );

            maskValue02 =
                std::clamp(
                    maskValue02,
                    0.0f,
                    1.0f
                );


            //================================================
            // 衣服領域のUnion
            // max()はOR相当
            // どちらかが白ならNon-Skin
            //================================================
            const float nonSkin =
                (std::max)(maskValue01, maskValue02);


            //================================================
            // Engine Conventionへ反転
            // 0 = Non-Skin
            // 1 = Skin
            //================================================
            const float skin =
                1.0f - nonSkin;


            destinationRow[x] = skin;
        }
    }


    //============================================================
    // R32_FLOAT
    //      ↓
    // R8_UNORM
    //============================================================
    ScratchImage skinMaskR8;


    hr =
        Convert(
            skinMaskFloat.GetImages(),
            skinMaskFloat.GetImageCount(),
            skinMaskFloat.GetMetadata(),
            DXGI_FORMAT_R8_UNORM,
            TEX_FILTER_DEFAULT,
            TEX_THRESHOLD_DEFAULT,
            skinMaskR8
        );


    if (FAILED(hr))
    {
        std::wcerr
            << L"[SkinMask Build] "
            << L"R8 Conversion Failed\n";

        return false;
    }


    //============================================================
    // MipMap
    // Mask境界を滑らかに保つ
    //============================================================
    ScratchImage mipChain;


    hr =
        GenerateMipMaps(
            skinMaskR8.GetImages(),
            skinMaskR8.GetImageCount(),
            skinMaskR8.GetMetadata(),
            TEX_FILTER_DEFAULT,
            0,
            mipChain
        );


    if (FAILED(hr))
    {
        std::wcerr
            << L"[SkinMask Build] "
            << L"MipMap Generation Failed\n";

        return false;
    }


    //============================================================
    // 6. BC4
    // 1Channel MaskなのでBC4で十分
    // Debugでも強制圧縮する
    //============================================================
    if (!SaveCompressedDDS(
        mipChain,
        outputPath,
        DXGI_FORMAT_BC4_UNORM,
        true)
        )
    {
        std::wcerr
            << L"[SkinMask Build] "
            << L"BC4 Save Failed\n";

        return false;
    }


    std::wcout
        << L"[SkinMask Build] Complete\n"
        << L"  Mask01 : "
        << mask01Path.wstring()
        << L"\n"
        << L"  Mask02 : "
        << mask02Path.wstring()
        << L"\n"
        << L"  Output : "
        << outputPath.wstring()
        << L"\n";


    return true;
}

bool TextureProcessor::LoadToFloatRGBA(const std::filesystem::path& path, DirectX::ScratchImage& output)
{
    //========================================================
       // Pathが空
       //========================================================
    if (path.empty())
    {
        std::wcerr
            << L"[TextureProcessor] "
            << L"Texture Path Is Empty.\n";

        return false;
    }


    //========================================================
    // Fileが存在するか確認
    //========================================================
    if (!std::filesystem::exists(path))
    {
        std::wcerr
            << L"[TextureProcessor] "
            << L"Texture File Not Found.\n"
            << L"  Path : "
            << path.wstring()
            << L"\n";

        return false;
    }


    //========================================================
    // Fileであることも確認
    //========================================================
    if (!std::filesystem::is_regular_file(path))
    {
        std::wcerr
            << L"[TextureProcessor] "
            << L"Texture Path Is Not A File.\n"
            << L"  Path : "
            << path.wstring()
            << L"\n";

        return false;
    }


    const std::wstring extension = GetLowerExtension(path);


    //========================================================
    // EXR
    //========================================================
    if (extension == L".exr")
    {
        return  LoadEXR(path, output);
    }


    //========================================================
    // 一旦Source Formatで読み込む
    //========================================================
    ScratchImage source;

    TexMetadata metadata;


    HRESULT hr = E_FAIL;


    //========================================================
    // DDS
    //========================================================
    if (extension == L".dds")
    {
        hr = LoadFromDDSFile(
            path.c_str(),
            DDS_FLAGS_NONE,
            &metadata,
            source
        );
    }

    //========================================================
    // TGA
    //========================================================
    else if (extension == L".tga")
    {
        hr = LoadFromTGAFile(
            path.c_str(),
            TGA_FLAGS_NONE,
            &metadata,
            source
        );
    }

    //========================================================
    // HDR
    //========================================================
    else if (extension == L".hdr")
    {
        hr = LoadFromHDRFile(
            path.c_str(),
            &metadata,
            source
        );
    }

    //========================================================
    // PNG / JPG / BMP / TIFF 
    //========================================================
    else
    {
        hr = LoadFromWICFile(
            path.c_str(),
            WIC_FLAGS_NONE,
            &metadata,
            source
        );
    }


    if (FAILED(hr))
    {
        std::wcerr
            << L"[TextureProcessor] Texture Load Failed\n"
            << L"File : "
            << path.wstring()
            << L"\n";

        return false;
    }


    //========================================================
    // Float RGBAへ変換
    //
    // ORM PackingやNormal変換を
    // Format依存なしで行えるようにする。
    //========================================================
    ScratchImage converted;


    hr = Convert(
        source.GetImages(),
        source.GetImageCount(),
        source.GetMetadata(),
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        TEX_FILTER_DEFAULT,
        TEX_THRESHOLD_DEFAULT,
        converted
    );


    if (FAILED(hr))
    {
        return false;
    }


    output = std::move(converted);


    return true;
}

bool TextureProcessor::LoadEXR(const std::filesystem::path& path, DirectX::ScratchImage& output)
{
    using namespace DirectX;


    //========================================================
    // OpenEXRから読み込んだ元画像
    //========================================================
    ScratchImage source;


    TexMetadata metadata = {};


    //========================================================
    //  EXR
    //   ↓
    // R16G16B16A16_FLOAT
    //========================================================
    const HRESULT loadHr =
        LoadFromEXRFile(
            path.c_str(),
            &metadata,
            source
        );


    if (FAILED(loadHr))
    {
        std::wcerr
            << L"[TextureProcessor] OpenEXR Load Failed\n"
            << L"  File    : "
            << path.wstring()
            << L"\n"
            << L"  HRESULT : 0x"
            << std::hex
            << static_cast<unsigned long>(
                loadHr
                )
            << std::dec
            << L"\n";

        return false;
    }


    //========================================================
    // Debug
    //========================================================
    std::wcout
        << L"[TextureProcessor] EXR Loaded\n"
        << L"  File   : "
        << path.filename().wstring()
        << L"\n"
        << L"  Width  : "
        << metadata.width
        << L"\n"
        << L"  Height : "
        << metadata.height
        << L"\n"
        << L"  Format : "
        << static_cast<int>(
            metadata.format
            )
        << L"\n";


    //========================================================
    // Converter内部形式へ統一
    //========================================================
    ScratchImage converted;


    const HRESULT convertHr = Convert(
        source.GetImages(),
        source.GetImageCount(),
        source.GetMetadata(),
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        TEX_FILTER_DEFAULT,
        TEX_THRESHOLD_DEFAULT,
        converted
    );


    if (FAILED(convertHr))
    {
        std::wcerr
            << L"[TextureProcessor] "
            << L"EXR Float Conversion Failed\n"
            << L"  File    : "
            << path.wstring()
            << L"\n"
            << L"  HRESULT : 0x"
            << std::hex
            << static_cast<unsigned long>(
                convertHr
                )
            << std::dec
            << L"\n";

        return false;
    }


    //========================================================
    // 呼び出し元へ所有権を移動
    //========================================================
    output = std::move(converted);


    return true;
}

//============================================================
// Extensionを小文字化
//============================================================
std::wstring TextureProcessor::GetLowerExtension(const std::filesystem::path& path)
{
    std::wstring extension =  path.extension().wstring();


    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](wchar_t c)
        {
            return static_cast<wchar_t>(std::towlower(c));
        }
    );

    return extension;
}

bool TextureProcessor::ConvertThicknessToDDS(const std::filesystem::path& sourcePath, 
    const std::filesystem::path& outputPath,
    bool invertThickness
)
{
    using namespace DirectX;


    //========================================================
    // 元TextureをRGBA32Fとして読み込む
    //========================================================
    ScratchImage source;


    if (!LoadToFloatRGBA(sourcePath, source))
    {
        std::wcerr
            << L"[Thickness] Load Failed\n"
            << L"  "
            << sourcePath.wstring()
            << L"\n";

        return false;
    }


    //========================================================
    // Thickness Map反転
    //========================================================
    if (invertThickness)
    {

        const Image* image =
            source.GetImage(
                0,
                0,
                0
            );


        if (image == nullptr)
        {
            std::wcerr
                << L"[Thickness] "
                << L"GetImage Failed\n";

            return false;
        }


        //====================================================
        // Pixel単位でThicknessを反転する
        //====================================================
        for (size_t y = 0; y < image->height; ++y)
        {
            float* row = reinterpret_cast<float*>(image->pixels + y * image->rowPitch);


            for (size_t x = 0; x < image->width; ++x)
            {
                //================================================
                // RGBA32Fなので、
                //
                // x * 4 + 0 = R
                // x * 4 + 1 = G
                // x * 4 + 2 = B
                // x * 4 + 3 = A
                //================================================
                float& thickness = row[x * 4 + 0];


                //================================================
                // 不正値対策
                // NaN / Infなどが入っていた場合は0として扱う
                //================================================
                if (!std::isfinite(thickness))
                {
                    thickness = 0.0f;
                }


                //================================================
                // 念のため0～1へClampしてから反転
                //================================================
                thickness =
                    std::clamp(
                        thickness,
                        0.0f,
                        1.0f
                    );


                //================================================
                // Thickness反転
                //================================================
                thickness = 1.0f - thickness;
            }
        }


        std::wcout
            << L"[Thickness] "
            << L"Thickness Map Inverted\n";
    }


    //========================================================
    // ThicknessはR Channelだけ使用するので
    // R8_UNORMへ変換する
    //========================================================
    ScratchImage converted;


    HRESULT hr = Convert(
        source.GetImages(),
        source.GetImageCount(),
        source.GetMetadata(),
        DXGI_FORMAT_R8_UNORM,
        TEX_FILTER_DEFAULT,
        TEX_THRESHOLD_DEFAULT,
        converted
    );


    if (FAILED(hr))
    {
        std::wcerr
            << L"[Thickness] Convert Failed\n";

        return false;
    }


    //========================================================
    // MipMap生成
    // 反転後のThicknessからMipMapを生成する
    //========================================================
    ScratchImage mipChain;


    hr = GenerateMipMaps(
        converted.GetImages(),
        converted.GetImageCount(),
        converted.GetMetadata(),
        TEX_FILTER_DEFAULT,
        0, // 1x1まで生成
        mipChain
    );


    if (FAILED(hr))
    {
        std::wcerr
            << L"[Thickness] GenerateMipMaps Failed\n";

        return false;
    }


    //========================================================
    // Thicknessは1Channel Data TextureなのでBC4で保存
    //========================================================
    return SaveCompressedDDS(
        mipChain,
        outputPath,
        DXGI_FORMAT_BC4_UNORM,
        true
    );
}

bool TextureProcessor::ConvertSkinMaskToDDS(const std::filesystem::path& sourcePath, const std::filesystem::path& outputPath, bool invertMask)
{
    using namespace DirectX;


    //============================================================
    // Source TextureをRGBA32Fへ統一して読み込む
    // PNG / JPG / DDSなど元Formatを意識せず、
    // 以降はfloat4として処理できる
    //============================================================
    ScratchImage source;


    if (!LoadToFloatRGBA(
        sourcePath,
        source))
    {
        std::wcerr
            << L"[SkinMask] Load Failed\n"
            << L"  Source : "
            << sourcePath.wstring()
            << L"\n";

        return false;
    }


    const TexMetadata& sourceMetadata =
        source.GetMetadata();


    if (sourceMetadata.width == 0 ||
        sourceMetadata.height == 0)
    {
        std::wcerr
            << L"[SkinMask] Invalid Texture Size\n";

        return false;
    }

    const Image* sourceImage =
        source.GetImage(
            0,
            0,
            0
        );


    if (sourceImage == nullptr)
    {
        std::wcerr
            << L"[SkinMask] GetImage Failed\n";

        return false;
    }


    std::wcout
        << L"[SkinMask] Loaded\n"
        << L"  Source : "
        << sourcePath.wstring()
        << L"\n"
        << L"  Size   : "
        << sourceImage->width
        << L" x "
        << sourceImage->height
        << L"\n"
        << L"  Invert : "
        << (invertMask ? L"Yes" : L"No")
        << L"\n";


    //============================================================
    // R Channel専用Float Textureを作成
    //
    // Sourceはfloat4だが、
    // SkinMaskに必要なのはRだけ
    // この段階で明示的に1Channelへ落とす
    //============================================================
    ScratchImage extractedMask;


    HRESULT hr =
        extractedMask.Initialize2D(
            DXGI_FORMAT_R32_FLOAT,
            sourceImage->width,
            sourceImage->height,
            1,
            1
        );


    if (FAILED(hr))
    {
        std::wcerr
            << L"[SkinMask] "
            << L"R32 Texture Initialization Failed\n";

        return false;
    }


    const Image* destinationImage =
        extractedMask.GetImage(
            0,
            0,
            0
        );


    if (destinationImage == nullptr)
    {
        std::wcerr
            << L"[SkinMask] "
            << L"Destination GetImage Failed\n";

        return false;
    }


    //============================================================
    // R Channel抽出
    //
    // Source:
    //
    //   R G B A
    //
    //     ↓ Rのみ
    //
    // Destination:
    //
    //   R
    //
    // Anti-Alias境界にある0～1の中間値はそのまま残す
    // Binary化(threshold)はしない
    //============================================================
    for (size_t y = 0; y < sourceImage->height; ++y)
    {
        const float* sourceRow =
            reinterpret_cast<const float*>(sourceImage->pixels + y * sourceImage->rowPitch);


        float* destinationRow =
            reinterpret_cast<float*>(destinationImage->pixels + y * destinationImage->rowPitch);


        for (size_t x = 0; x < sourceImage->width; ++x)
        {
            //====================================================
            // RGBA32F
            //
            // x * 4 + 0 = R
            // x * 4 + 1 = G
            // x * 4 + 2 = B
            // x * 4 + 3 = A
            //
            // SkinMaskではRだけ採用する。
            //====================================================
            float maskValue = sourceRow[x * 4 + 0];


            //====================================================
            // NaN / Inf対策
            // 不正値はNon-Skinとして扱う
            //====================================================
            if (!std::isfinite(maskValue))
            {
                maskValue = 0.0f;
            }


            //====================================================
            // Data Textureなので0～1へClamp
            //====================================================
            maskValue =
                std::clamp(
                    maskValue,
                    0.0f,
                    1.0f
                );


            //====================================================
            // 必要なら極性反転
            //
            // Source:
            //
            //   White = Non-Skin
            //   Black = Skin
            //
            //      ↓ invert
            //
            // Engine:
            //
            //   White = Skin
            //   Black = Non-Skin
            //====================================================
            if (invertMask)
            {
                maskValue = 1.0f - maskValue;
            }


            //====================================================
            // R32_FLOATなので1Pixelにつきfloat 1個
            //====================================================
            destinationRow[x] = maskValue;
        }
    }


    //============================================================
    // R32_FLOAT
    //   ↓
    // R8_UNORM
    // GPU上では0～1のMaskなので8bitで十分
    //============================================================
    ScratchImage converted;


    hr =
        Convert(
            extractedMask.GetImages(),
            extractedMask.GetImageCount(),
            extractedMask.GetMetadata(),
            DXGI_FORMAT_R8_UNORM,
            TEX_FILTER_DEFAULT,
            TEX_THRESHOLD_DEFAULT,
            converted
        );


    if (FAILED(hr))
    {
        std::wcerr
            << L"[SkinMask] R8 Conversion Failed\n"
            << L"  HRESULT : 0x"
            << std::hex
            << static_cast<unsigned long>(
                hr
                )
            << std::dec
            << L"\n";

        return false;
    }


    //============================================================
    // MipMap生成
    // 反転する場合は、
    // 必ず反転後のMaskからMipを作る
    // Mask境界の中間値を保つためLinear Filterを使用
    //============================================================
    ScratchImage mipChain;


    hr =
        GenerateMipMaps(
            converted.GetImages(),
            converted.GetImageCount(),
            converted.GetMetadata(),
            TEX_FILTER_DEFAULT,
            0,      // 1x1まで生成
            mipChain
        );


    if (FAILED(hr))
    {
        std::wcerr
            << L"[SkinMask] "
            << L"GenerateMipMaps Failed\n"
            << L"  HRESULT : 0x"
            << std::hex
            << static_cast<unsigned long>(
                hr
                )
            << std::dec
            << L"\n";

        return false;
    }


    //============================================================
    // BC4_UNORMへ圧縮
    // SkinMaskは1ChannelだけなのでBC7は不要
    // BC4:
    //   4bit / pixel相当
    //   1Channel UNORM専用
    // forceCompression=true:
    //   Debug BuildでもBC4として保存する
    //============================================================
    const bool saveResult =
        SaveCompressedDDS(
            mipChain,
            outputPath,
            DXGI_FORMAT_BC4_UNORM,
            true
        );


    if (!saveResult)
    {
        std::wcerr
            << L"[SkinMask] "
            << L"BC4 DDS Save Failed\n";

        return false;
    }


    std::wcout
        << L"[SkinMask] Conversion Complete\n"
        << L"  Output : "
        << outputPath.wstring()
        << L"\n"
        << L"  Format : BC4_UNORM\n"
        << L"  Mips   : "
        << mipChain.GetMetadata().mipLevels
        << L"\n";


    return true;
}

bool TextureProcessor::ConvertMemoryToDDS(const std::uint8_t* data, std::size_t dataSize, const std::string& formatHint, const std::filesystem::path& outputPath, bool sRGB)
{
    using namespace DirectX;

    if (data == nullptr || dataSize == 0)
    {
        return false;
    }

    //========================================================
    // MemoryからTexture Load
    //========================================================
    ScratchImage source;

    TexMetadata metadata = {};

    HRESULT hr = LoadFromWICMemory(
        data,
        dataSize,
        WIC_FLAGS_NONE,
        &metadata,
        source
    );

    if (FAILED(hr))
    {
        std::cerr
            << "[TextureProcessor] "
            << "Embedded WIC Load Failed.\n"
            << "FormatHint : "
            << formatHint
            << "\n";

        return false;
    }

    //========================================================
    // RGBA8
    //========================================================
    ScratchImage converted;

    hr = Convert(
        source.GetImages(),
        source.GetImageCount(),
        source.GetMetadata(),
        DXGI_FORMAT_R8G8B8A8_UNORM,
        TEX_FILTER_DEFAULT,
        TEX_THRESHOLD_DEFAULT,
        converted
    );


    if (FAILED(hr))
    {
        return false;
    }


    //========================================================
    // BaseColor / EmissiveならsRGB
    //========================================================
    if (sRGB)
    {
        if (!converted.OverrideFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))
        {
            return false;
        }
    }

    //========================================================
    // MipMap生成
    //========================================================
    ScratchImage mipChain;


    hr = GenerateMipMaps(
        converted.GetImages(),
        converted.GetImageCount(),
        converted.GetMetadata(),
        TEX_FILTER_DEFAULT,
        0,
        mipChain
    );


    if (FAILED(hr))
    {
        return false;
    }


    //========================================================
    // 今作った共通保存処理
    // Debug: 非圧縮
    // Release: BC7
    //========================================================
    return
        SaveCompressedDDS(
            mipChain,
            outputPath,
            sRGB ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM
        );
}
