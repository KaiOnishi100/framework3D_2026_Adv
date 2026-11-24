#include "TextureResolver.h"

#include <algorithm>
#include <cctype>
#include <iostream>


//============================================================
// 小文字化
//============================================================
std::string TextureResolver::ToLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        }
    );

    return text;
}

//============================================================
// Material名からTexture検索用のKeyを生成
//============================================================
static std::string MakeMaterialSearchKey(std::string materialName)
{
    //========================================================
    // 小文字化
    //========================================================
    std::transform(materialName.begin(), materialName.end(), materialName.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        }
    );


    //========================================================
    // Material名によく付く一般Suffix
    //========================================================
    const std::vector<std::string> genericSuffixes =
    {
        "textures",
        "texture",
        "materials",
        "material",
        "_mat",
        "mat"
    };


    for (const std::string& suffix : genericSuffixes)
    {
        //====================================================
        // suffixより短い名前には使用しない
        //====================================================
        if (materialName.size() <= suffix.size())
        {
            continue;
        }

        //====================================================
        // 末尾位置
        //====================================================
        const std::size_t position = materialName.size() - suffix.size();

        //====================================================
        // Material名末尾がSuffixと一致するか
        //====================================================
        if (materialName.compare(position,suffix.size(),suffix) == 0)
        {
            materialName.erase(position);
            break;
        }
    }


    //========================================================
    // 末尾に残った _ や - を消す
    //========================================================
    while (!materialName.empty() && (materialName.back() == '_' || materialName.back() == '-' || materialName.back() == ' '))
    {
        materialName.pop_back();
    }

    return materialName;
}

//============================================================
// Assimp MaterialからTexture Path取得
//============================================================
ResolvedTexture  TextureResolver::GetAssimpTexture(
    const aiMaterial* material,
    aiTextureType type,
    const std::filesystem::path& modelPath,
    bool* outHadReference
)
{
    if (outHadReference != nullptr)
    {
        *outHadReference = false;
    }

    ResolvedTexture result;


    if (material == nullptr)
    {
        return result;
    }


    //========================================================
    // 指定SlotにTextureが無い
    //========================================================
    if (material->GetTextureCount(type) == 0u)
    {
        return result;
    }

    //========================================================
    // Texture参照自体は存在する
    //========================================================
    if (outHadReference != nullptr)
    {
        *outHadReference = true;
    }

    aiString texturePath;


    if (material->GetTexture(type, 0u, &texturePath) != AI_SUCCESS)
    {
        return result;
    }

    const std::string source = texturePath.C_Str();

    if (source.empty())
    {
        return result;
    }

    //========================================================
    // Embedded Texture
    //========================================================
    if (source[0] == '*')
    {
        try
        {
            //================================================
            // '*'より後ろを整数へ変換
            // "*3"
            //   ↓
            // 3
            //================================================
            const int embeddedIndex = std::stoi(source.substr(1));

            result.Type = TextureSourceType::Embedded;

            result.EmbeddedIndex = embeddedIndex;

            return result;
        }
        catch (...)
        {
            //================================================
            // 不正なEmbedded ID
            //================================================
            return {};
        }
    }

    //============================================================
    // External Texture
    //============================================================
    std::filesystem::path referencedPath = texturePath.C_Str();

    if (std::filesystem::exists(referencedPath))
    {
        ResolvedTexture result;

        result.Type = TextureSourceType::ExternalFile;

        result.Path = referencedPath.lexically_normal().string();

        return result;
    }

    const std::filesystem::path modelDirectory = modelPath.parent_path();

    const std::filesystem::path fileName = referencedPath.filename();

    const std::filesystem::path localCandidate = modelDirectory / fileName;


    if (std::filesystem::exists(localCandidate))
    {
        ResolvedTexture result;

        result.Type = TextureSourceType::ExternalFile;

        result.Path = localCandidate.lexically_normal().string();


        std::cout
            << "[TextureResolver] "
            << "Referenced texture recovered by filename.\n"
            << "    Original : "
            << referencedPath.string()
            << "\n"
            << "    Local    : "
            << localCandidate.string()
            << "\n";


        return result;
    }

    const std::filesystem::path texturesCandidate = modelDirectory / "textures" / fileName;


    if (std::filesystem::exists(texturesCandidate))
    {
        ResolvedTexture result;

        result.Type = TextureSourceType::ExternalFile;

        result.Path = texturesCandidate.lexically_normal().string();

        return result;
    }


    //============================================================
    // ここまで見つからなければ未解決
    //============================================================
    std::cout
        << "[TextureResolver] "
        << "Referenced texture not found.\n"
        << "    Referenced : "
        << referencedPath.string()
        << "\n";

    return result;
}


//============================================================
// NormalizeTextureStemForSearch
// Texture検索用にファイル名を正規化する
// 例:
//   floor_tiles_06_diff_4k   -> floor_tiles_06_diff
//   floor_tiles_06_nor_dx_4k -> floor_tiles_06_nor
//   floor_tiles_06_nor_gl_2k -> floor_tiles_06_nor
//   floor_tiles_06_orm_4k    -> floor_tiles_06_orm
//============================================================
static std::string NormalizeTextureStemForSearch(std::string stem)
{
    //========================================================
    // 大文字・小文字を統一する
    //========================================================
    std::transform(stem.begin(), stem.end(), stem.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        }
    );

    //========================================================
    // 指定した文字列が末尾にあれば取り除く
    //========================================================
    const auto RemoveSuffix = [&stem](const std::string& suffix) -> bool
        {
            if (stem.size() < suffix.size())
            {
                return false;
            }

            const std::size_t offset = stem.size() - suffix.size();

            if (stem.compare(offset, suffix.size(), suffix) != 0)
            {
                return false;
            }

            stem.erase(offset);
            return true;
        };

    //========================================================
    // 解像度Suffixを削除
    // Poly Haven等の _1k / _2k / _4k / _8k に対応
    //========================================================
    for (const char* suffix : { "_512", "_1k", "_2k", "_4k", "_8k", "_16k" })
    {
        if (RemoveSuffix(suffix))
        {
            break;
        }
    }

    //========================================================
    // Normal Mapの形式Suffixを検索名から削除
    // _nor_dx_4k -> _nor
    // _nor_gl_4k -> _nor
    //========================================================
    RemoveSuffix("_dx");
    RemoveSuffix("_gl");

    return stem;
}


//============================================================
// MakeTextureFamilyKey
// BaseColorと同じ素材に属するNormal / ORMを探すため
// 共通するTexture名を取得する
//============================================================
static std::string MakeTextureFamilyKey(
    const ResolvedTexture& baseColor,
    const std::string& materialName
)
{
    //========================================================
    // File System上のBaseColorを優先する
    //========================================================
    if (baseColor.Type != TextureSourceType::ExternalFile || baseColor.Path.empty())
    {
        return materialName;
    }

    //========================================================
    // BaseColorファイル名を取得して正規化
    //========================================================
    std::string stem =
        NormalizeTextureStemForSearch(std::filesystem::path(baseColor.Path).stem().string());

    //========================================================
    // BaseColorを示すSuffixを取り除く
    //========================================================
    for (const std::string suffix :
    {
        "_basecolor",
        "_base_color",
        "_diffuse",
        "_albedo",
        "_diff",
        "_color"
    })
    {
        if (stem.size() < suffix.size())
        {
            continue;
        }

        const std::size_t offset = stem.size() - suffix.size();

        if (stem.compare(offset, suffix.size(), suffix) == 0)
        {
            stem.erase(offset);
            break;
        }
    }

    //========================================================
    // 名前を正しく取得できた場合はその名前を使用
    //========================================================
    return stem.empty() ? materialName : stem;
}


//============================================================
// Directory内を検索
//============================================================
ResolvedTexture  TextureResolver::FindTextureBySuffix(
    const std::filesystem::path& directory,
    const std::string& materialName,
    const std::vector<std::string>& suffixes,
    bool allowLooseFallback
)
{
    ResolvedTexture result;

    //========================================================
    // Directory確認
    //========================================================
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory))
    {
        return result;
    }

    const std::string lowerMaterialName = ToLower(materialName);

    //============================================================
    // Material名から検索用Keyを作る
    //============================================================
    const std::string materialKey = MakeMaterialSearchKey(materialName);

    //============================================================
    // Materialとの関連性が確認できる候補
    //============================================================
    std::vector<std::filesystem::path> relatedCandidates;

    //============================================================
    // Materialとの関連性は無いが
    // Suffixだけ一致した候補
    // Single Material Assetの場合のみ使用する
    //============================================================
    std::vector<std::filesystem::path> looseCandidates;

    //========================================================
    // Texture候補
    //========================================================
    std::vector<std::filesystem::path> semanticCandidates;


    //========================================================
    // Directory内を検索
    //========================================================
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }


        const std::filesystem::path path = entry.path();

        const std::string extension = ToLower(path.extension().string());

        //====================================================
        // Texture形式だけ
        //====================================================
        if (extension != ".png" &&
            extension != ".jpg" &&
            extension != ".jpeg" &&
            extension != ".tga" &&
            extension != ".bmp" &&
            extension != ".dds" &&
            extension != ".exr")
        {
            continue;
        }

        const std::string lowerStem = NormalizeTextureStemForSearch(path.stem().string());

        //====================================================
        // Suffixを確認
        //====================================================
        for (const std::string& suffix : suffixes)
        {
            const std::string lowerSuffix = ToLower(suffix);


            //================================================
            // 第1候補
            // Material名 + Suffix
            //================================================
            const std::string expectedName = lowerMaterialName + lowerSuffix;


            if (lowerStem == expectedName)
            {
                result.Type = TextureSourceType::ExternalFile;

                result.Path = path.lexically_normal().string();

                result.EmbeddedIndex = -1;


                return result;
            }


            //================================================
            // 第2候補
            // Material名は違うが
            // Texture用途Suffixは一致している場合
            //================================================
            if (lowerStem.size() >= lowerSuffix.size())
            {
                const std::size_t suffixPosition = lowerStem.size() - lowerSuffix.size();


                if (lowerStem.compare(suffixPosition, lowerSuffix.size(), lowerSuffix) == 0)
                {
                    //====================================================
                    // Texture名とMaterial名に関連性があるか
                    //====================================================
                    const bool relatedToMaterial =
                        !materialKey.empty() && lowerStem.find(materialKey) != std::string::npos;


                    if (relatedToMaterial)
                    {
                        relatedCandidates.push_back(path);
                    }
                    else
                    {
                        looseCandidates.push_back(path);
                    }
                }
            }
        }
    }


    if (relatedCandidates.size() == 1)
    {
        const std::filesystem::path& path = relatedCandidates[0];

        result.Type = TextureSourceType::ExternalFile;

        result.Path = path.lexically_normal().string();

        result.EmbeddedIndex = -1;


        std::cout
            << "[TextureResolver] "
            << "Related fallback texture found.\n"
            << "    Material : "
            << materialName
            << "\n"
            << "    Texture  : "
            << path.string()
            << "\n";


        return result;
    }


    //============================================================
    // Related候補が複数ある場合
    //============================================================
    if (relatedCandidates.size() > 1)
    {
        std::cout
            << "[TextureResolver] "
            << "Multiple related texture candidates found.\n"
            << "    Material : "
            << materialName
            << "\n";


        for (const auto& candidate : relatedCandidates)
        {
            std::cout
                << "        "
                << candidate.string()
                << "\n";
        }


        return result;
    }


    //============================================================
    // 第2優先:
    // Loose Fallback
    //============================================================
    if (allowLooseFallback && looseCandidates.size() == 1)
    {
        const std::filesystem::path& path = looseCandidates[0];

        result.Type = TextureSourceType::ExternalFile;

        result.Path = path.lexically_normal().string();

        result.EmbeddedIndex = -1;


        std::cout
            << "[TextureResolver] "
            << "Loose fallback texture found.\n"
            << "    Material : "
            << materialName
            << "\n"
            << "    Texture  : "
            << path.string()
            << "\n";


        return result;
    }


    //============================================================
    // Multi MaterialではLoose Candidateを使わない
    //============================================================
    if (!allowLooseFallback && !looseCandidates.empty())
    {
        std::cout
            << "[TextureResolver] "
            << "Loose fallback skipped for multi-material asset.\n"
            << "    Material : "
            << materialName
            << "\n";
    }

    return result;
}

ResolvedTexture  TextureResolver::FindBaseColorTexture(
    const std::filesystem::path& directory, 
    const std::string& materialName,
    bool allowLooseFallback
)
{
    //========================================================
    // Material名完全一致
    //========================================================
    if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory))
    {
        const std::string lowerMaterialName = ToLower(materialName);

        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path path = entry.path();

            const std::string extension = ToLower(path.extension().string());

            if (extension != ".png" &&
                extension != ".jpg" &&
                extension != ".jpeg" &&
                extension != ".tga" &&
                extension != ".bmp" &&
                extension != ".dds" &&
                extension != ".exr")
            {
                continue;
            }

            if (ToLower(path.stem().string()) == lowerMaterialName)
            {
                ResolvedTexture result;

                result.Type = TextureSourceType::ExternalFile;
                result.Path = path.lexically_normal().string();


                return result;
            }
        }
    }

    //========================================================
    // BaseColor系Suffixを検索
    // FindTextureBySuffix()は
    // 1. Material + suffix 2. suffixだけで唯一一致
    // の両方を見る
    //========================================================
    return
        FindTextureBySuffix(
            directory,
            materialName,
            {
                "_basecolor",
                "_base_color",
                "_albedo",
                "_diffuse",
                "_diff",
                "_color"
            },
            allowLooseFallback
        );
}

//============================================================
// Material Texture一括解決
//============================================================
ResolvedTextureSet  TextureResolver::Resolve(
    const aiMaterial* material,
    const std::filesystem::path& modelPath,
    bool allowLooseFallback
)
{
    ResolvedTextureSet result;


    if (material == nullptr)
    {
        return result;
    }


    //============================================================
    // Model Directory
    //============================================================
    const std::filesystem::path directory = modelPath.parent_path();


    //============================================================
    // Material Name
    //============================================================
    aiString aiMaterialName;

    material->Get(AI_MATKEY_NAME, aiMaterialName);

    const std::string materialName = aiMaterialName.C_Str();

    bool hadBaseColorReference = false;

    //============================================================
    // BaseColor
    // まず正式なPBR BaseColor Slotを確認
    // glTFではこちらへ入ることが多い
    //============================================================
    result.BaseColor =
        GetAssimpTexture(
            material,
            aiTextureType_BASE_COLOR,
            modelPath
        );

    //============================================================
    // BaseColorが無ければDiffuse
    //============================================================
    if (!result.BaseColor.IsValid())
    {
        bool hadDiffuseReference = false;

        result.BaseColor =
            GetAssimpTexture(
                material,
                aiTextureType_DIFFUSE,
                modelPath,
                &hadDiffuseReference
            );

        hadBaseColorReference = hadBaseColorReference || hadDiffuseReference;
    }


    //============================================================
    // FBX自体にTexture Pathが入っていない場合
    // Material名から周辺ファイルを探す
    //============================================================
    if (!result.BaseColor.IsValid())
    {
        result.BaseColor =
            FindBaseColorTexture(
                directory,
                materialName,
                allowLooseFallback
            );
    }

    //============================================================
    // BaseColorから共通素材名を特定
    //============================================================
    const std::string textureFamilyKey = MakeTextureFamilyKey(result.BaseColor, materialName);

    //============================================================
    // Normal
    //============================================================
    result.Normal =
        GetAssimpTexture(
            material,
            aiTextureType_NORMALS,
            modelPath
        );


    //============================================================
    // 一部FBXではHEIGHTへNormalが格納される場合がある
    //============================================================
    if (!result.Normal.IsValid())
    {
        result.Normal =
            GetAssimpTexture(
                material,
                aiTextureType_HEIGHT,
                modelPath
            );
    }


    //============================================================
    // File Name Fallback
    //============================================================
    if (!result.Normal.IsValid())
    {
        result.Normal =
            FindTextureBySuffix(
                directory,
                textureFamilyKey,
                {
                    "_normal",
                    "_normalmap",
                    "_nor",
                    "_nrm",
                    "_n"
                },
                allowLooseFallback
            );
    }

    result.ORM =
        FindTextureBySuffix(
            directory,
            textureFamilyKey,
            {
                "_orm",
                "_ormmap",
                "_arm",
                "_armmap",
                "_ao_rough_metal",
                "_ao_roughness_metallic",
                "_occlusionroughnessmetallic"
            },
            allowLooseFallback
        );

    //============================================================
    // Metallic
    //============================================================
    result.Metallic =
        GetAssimpTexture(
            material,
            aiTextureType_METALNESS,
            modelPath
        );

    if (!result.Metallic.IsValid())
    {
        result.Metallic =
            FindTextureBySuffix(
                directory,
                materialName,
                {
                    "_metallic",
                    "_metalness",
                    "_metal",
                    "_mtl"
                },
                allowLooseFallback
            );
    }

    //============================================================
    // Roughness
    //============================================================
    result.Roughness =
        GetAssimpTexture(
            material,
            aiTextureType_DIFFUSE_ROUGHNESS,
            modelPath
        );

    if (!result.Roughness.IsValid())
    {
        result.Roughness =
            FindTextureBySuffix(
                directory,
                materialName,
                {
                    "_Roughness",
                    "_roughness",
                    "_rough",
                    "_rgh"
                },
                allowLooseFallback
            );
    }

    //============================================================
    // AO
    //============================================================
    result.AO =
        GetAssimpTexture(
            material,
            aiTextureType_AMBIENT_OCCLUSION,
            modelPath
        );

    if (!result.AO.IsValid())
    {
        result.AO =
            FindTextureBySuffix(
                directory,
                materialName,
                {
                    "_ao",
                    "_ambientocclusion",
                    "_ambient_occlusion",
                    "_occlusion"
                },
                allowLooseFallback
            );
    }

    result.Thickness =
        FindTextureBySuffix(
            directory,
            materialName,
            {
                "_thickness",
                "_thicknessmap",
                "_thick"
            },
            allowLooseFallback
        );

    //============================================================
    // Emissive
    //============================================================
    result.Emissive =
        GetAssimpTexture(
            material,
            aiTextureType_EMISSIVE,
            modelPath
        );

    if (!result.Emissive.IsValid())
    {
        result.Emissive =
            FindTextureBySuffix(
                directory,
                materialName,
                {
                    "_emissive",
                    "_emission",
                    "_emit"
                },
                allowLooseFallback
            );
    }

    //============================================================
    // Skin Mask
    //============================================================
    result.SkinMask =
        FindTextureBySuffix(
            directory,
            materialName,
            {
                "_skinmask",
                "_skin_mask",
                "_skin",
                "_sss_mask",
                "_sssmask",
                "_mask",
            },
            allowLooseFallback
            );

    //============================================================
    // Material Mask 01
    //============================================================
    result.MaterialMask01 =
        FindTextureBySuffix(
            directory,
            materialName,
            {
                "_mask01",
                "_mask_01"
            },
            allowLooseFallback
        );


    //============================================================
    // Material Mask 02
    //============================================================
    result.MaterialMask02 =
        FindTextureBySuffix(
            directory,
            materialName,
            {
                "_mask02",
                "_mask_02"
            },
            allowLooseFallback
        );

    return result;
}
