#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "documentContentLoader.h"

namespace
{
    //============================================================
    // UTF-8 BOM削除
    // ※UTF-8 with BOMで保存されたCSV/TXTでも
    // ※先頭文字にBOMが混入しないようにする
    //============================================================
    void RemoveUtf8Bom(std::string& text)
    {
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF)
        {
            text.erase(0, 3);
        }
    }


    //============================================================
    // UTF-8
    //      ↓
    // UTF-16 / std::wstring
    // UI側がwstringを使用するためファイル読み込み時点で変換する
    //============================================================
    bool Utf8ToWide(const std::string& utf8, std::wstring& outWide)
    {
        outWide.clear();


        //========================================================
        // 空文字は正常
        //========================================================
        if (utf8.empty())
        {
            return true;
        }


        //========================================================
        // 必要文字数を取得
        //========================================================
        const int requiredLength =
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                utf8.data(),
                static_cast<int>(utf8.size()),
                nullptr,
                0
            );


        if (requiredLength <= 0)
        {
            return false;
        }


        outWide.resize(static_cast<std::size_t>(requiredLength));


        //========================================================
        // 実際に変換
        //========================================================
        const int convertedLength =
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                utf8.data(),
                static_cast<int>(utf8.size()),
                outWide.data(),
                requiredLength
            );


        return convertedLength == requiredLength;
    }


    //============================================================
    // UTF-8 Text File読み込み
    // 改行を含め、ファイル全文をそのまま読み込む
    //============================================================
    bool ReadUtf8TextFile(const std::filesystem::path& path, std::wstring& outText)
    {
        outText.clear();


        //========================================================
        // binaryで開く
        // Windows側による改行変換などを避け
        // 元のUTF-8データをそのまま取得する
        //========================================================
        std::ifstream file(path, std::ios::binary);


        if (!file.is_open())
        {
            return false;
        }


        //========================================================
        // File全体をstringへ
        //========================================================
        std::string utf8Text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());


        RemoveUtf8Bom(utf8Text);


        return Utf8ToWide(utf8Text, outText);
    }


    //============================================================
    // CSV 1行Parse
    //============================================================
    std::vector<std::string> ParseCsvLine(const std::string& line)
    {
        std::vector<std::string> fields;

        std::string current;

        bool insideQuotes = false;


        for (std::size_t index = 0; index < line.size(); ++index)
        {
            const char character = line[index];

            //====================================================
            // "
            //====================================================
            if (character == '"')
            {
                if (insideQuotes && index + 1 < line.size() && line[index + 1] == '"')
                {
                    current.push_back('"');

                    ++index;

                    continue;
                }

                //================================================
                // Quote開始 / 終了
                //================================================
                insideQuotes = !insideQuotes;

                continue;
            }


            //====================================================
            // Quote外のComma
            // Column区切りとして扱う
            //====================================================
            if (character == ',' && !insideQuotes)
            {
                fields.push_back(current);

                current.clear();

                continue;
            }


            current.push_back(character);
        }


        //========================================================
        // 最終Column
        //========================================================
        fields.push_back(current);


        return fields;
    }


    //============================================================
    // CR削除
    // getline()後
    // Windows:
    //   \r\n
    // の\rだけ残るケースへ対応する
    //============================================================
    void RemoveTrailingCarriageReturn(std::string& text)
    {
        if (!text.empty() && text.back() == '\r')
        {
            text.pop_back();
        }
    }
}


//============================================================
// Load
//============================================================
bool DocumentContentLoader::Load(
    const std::filesystem::path& csvPath,
    const std::filesystem::path& textDirectory,
    DocumentManager& documentManager
)
{
    //============================================================
    // CSV Open
    //============================================================
    std::ifstream csvFile(csvPath, std::ios::binary);


    if (!csvFile.is_open())
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[DocumentContentLoader] "
            L"Documents.csv could not be opened.\n"
        );

#endif

        return false;
    }


    std::string line;


    //============================================================
    // Header
    // Id,Title,Author,TextFile,Order
    //============================================================
    if (!std::getline(csvFile, line))
    {
        return false;
    }


    RemoveUtf8Bom(line);
    RemoveTrailingCarriageReturn(line);

    //============================================================
    // Document行
    //============================================================
    std::uint32_t loadedCount = 0u;

    std::uint32_t lineNumber = 1u;


    while (std::getline(csvFile, line))
    {
        ++lineNumber;


        RemoveTrailingCarriageReturn(line);

        //========================================================
        // 空行は無視
        //========================================================
        if (line.empty())
        {
            continue;
        }


        //========================================================
        // コメント行
        //========================================================
        if (line[0] == '#')
        {
            continue;
        }

        const std::vector<std::string> fields = ParseCsvLine(line);


        //========================================================
        // 必須Column数
        // 0 = Id
        // 1 = Title
        // 2 = Author
        // 3 = TextFile
        // 4 = Order
        //========================================================
        if (fields.size() < 5)
        {
#if defined(_DEBUG)

            OutputDebugStringW(
                L"[DocumentContentLoader] "
                L"Invalid Documents.csv row.\n"
            );

#endif
            return false;
        }


        //========================================================
        // ID
        //========================================================
        const std::string& documentId = fields[0];

        if (documentId.empty())
        {
            return false;
        }

        //========================================================
        // Title
        //========================================================
        std::wstring title;

        if (!Utf8ToWide(fields[1], title))
        {
            return false;
        }


        //========================================================
        // Author
        // Authorは空でも許可する
        //========================================================
        std::wstring author;

        if (!Utf8ToWide(fields[2], author))
        {
            return false;
        }


        //========================================================
        // 本文File
        //========================================================
        const std::string& textFileName = fields[3];


        if (textFileName.empty())
        {
            return false;
        }


        //========================================================
        // UTF-8 filename
        //========================================================
        const std::filesystem::path textPath = textDirectory / std::filesystem::path(textFileName);


        std::wstring body;


        if (!ReadUtf8TextFile(textPath, body))
        {
#if defined(_DEBUG)

            std::wstring debugMessage =
                L"[DocumentContentLoader] "
                L"Document text file could not be loaded : ";
            debugMessage += textPath.wstring();
            debugMessage += L"\n";
            OutputDebugStringW(debugMessage.c_str());

#endif

            return false;
        }


        //========================================================
        // Display Order
        //========================================================
        std::uint32_t displayOrder = 0u;


        try
        {
            displayOrder = static_cast<std::uint32_t>(std::stoul(fields[4]));
        }
        catch (...)
        {
#if defined(_DEBUG)

            OutputDebugStringW(
                L"[DocumentContentLoader] "
                L"Invalid document order.\n"
            );
#endif
            return false;
        }


        //========================================================
        // Definition作成
        //========================================================
        DocumentDefinition definition;

        definition.Id = documentId;
        definition.Title = std::move(title);
        definition.Author = std::move(author);
        definition.Body = std::move(body);
        definition.DisplayOrder = displayOrder;


        //========================================================
        // DocumentManagerへ登録
        //========================================================
        if (!documentManager.RegisterDefinition(definition))
        {
#if defined(_DEBUG)

            OutputDebugStringW(
                L"[DocumentContentLoader] "
                L"Document registration failed.\n"
            );

#endif

            return false;
        }

        ++loadedCount;
    }


#if defined(_DEBUG)

    //============================================================
    // Loading Result
    //============================================================
    wchar_t message[256] = {};

    swprintf_s(
        message,
        L"[DocumentContentLoader] "
        L"Loaded Documents = %u\n",
        loadedCount
    );


    OutputDebugStringW(message);

#endif

    return true;
}
