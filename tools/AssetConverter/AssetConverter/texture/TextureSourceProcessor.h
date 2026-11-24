#pragma once

//============================================================
// Standard Library
//============================================================
#include <filesystem>
#include <string>


//============================================================
// Converter
//============================================================
#include "../Importer/ImportedModel.h"
#include "TextureSourceTypes.h"


//============================================================
// TextureSourceProcessor
// ResolvedTextureを、実際にFile System上から
// 読み込めるTexture Pathへ変換する
// External: 元画像Pathを返す
// Embedded: Dataを一時Fileへ展開して、そのPathを返す
// ※このクラスではDDS変換は行わない
//============================================================
class TextureSourceProcessor
{
public:

    //========================================================
    // Texture Sourceを実ファイルへ変換
    // texture: TextureResolverが返したTexture参照
    // model: EmbeddedTexturesを持つImportedModel
    // outputDirectory: Embedded Textureの展開先
    // fileNameHint: 出力File名用
    // 戻り値:
    //      成功: 実在するTexture Path
    //      失敗: empty path
    //========================================================
    static std::filesystem::path Materialize(
        const ResolvedTexture& texture,
        const ImportedModel& model,
        const std::filesystem::path& outputDirectory,
        const std::string& fileNameHint
    );


private:

    //========================================================
    // Embedded Texture検索
    //========================================================
    static const ImportedEmbeddedTexture* FindEmbeddedTexture(
        const ImportedModel& model,
        int embeddedIndex
    );


    //========================================================
    // 圧縮済みEmbedded Textureを書き出す
    //========================================================
    static std::filesystem::path WriteCompressedEmbeddedTexture(
        const ImportedEmbeddedTexture& texture,
        const std::filesystem::path& outputDirectory,
        const std::string& fileNameHint
    );


    //========================================================
    // 非圧縮aiTexel Textureを書き出す
    //========================================================
    static std::filesystem::path WriteRawEmbeddedTextureAsTGA(
        const ImportedEmbeddedTexture& texture,
        const std::filesystem::path& outputDirectory,
        const std::string& fileNameHint
    );


    //========================================================
    // Format Hint
    //========================================================
    static std::string MakeExtension(const std::string& format);
};