#pragma once

#include <filesystem>

class DocumentManager;


//============================================================
// Document Content Loader
// Documents.csv
//      ↓
// DocumentDefinition
//
// DOC_xxx.txt
//      ↓
// DocumentDefinition::Body
// へ変換してDocumentManagerへ登録する
//============================================================
class DocumentContentLoader
{
public:

    //========================================================
    // Document定義を一括ロード
    // csvPath: data/csv/world/gameplay/Documents.csv
    // textDirectory: data/text/AtmosphericEscape/documents/
    //========================================================
    static bool Load(
        const std::filesystem::path& csvPath,
        const std::filesystem::path& textDirectory,
        DocumentManager& documentManager
    );
};