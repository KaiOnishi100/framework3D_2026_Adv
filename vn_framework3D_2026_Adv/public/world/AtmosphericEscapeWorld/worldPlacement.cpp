#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "worldPlacement.h"

//============================================================
// Trim
// CSVセル前後のSpace / Tab / 改行を除去する
//============================================================
std::string WorldPlacementTable::Trim(const std::string& text)
{
    auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char c)
        {
            return std::isspace(c);
        }
    );


    auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c)
        {
            return std::isspace(c);
        }
    ).base();


    if (begin >= end)
    {
        return {};
    }

    return std::string(begin, end);
}


//============================================================
// SplitLine
//============================================================
std::vector<std::string>WorldPlacementTable::SplitLine(const std::string& line)
{
    std::vector<std::string> result;

    std::stringstream stream(line);

    std::string cell;

    while (std::getline(stream, cell, ','))
    {
        result.push_back(Trim(cell));
    }

    return result;
}


bool WorldPlacementTable::LoadFiles(const std::vector<std::filesystem::path>& filePaths)
{
    //========================================================
    // ※World Placementを最初から構築するため最初に1度だけClear
    //========================================================
    Clear();

    //========================================================
    // Area CSVを順番にMerge
    //========================================================
    for (const std::filesystem::path& filePath : filePaths)
    {
        if (!Append(filePath))
        {
            //================================================
            // ※一部だけ読み込まれたWorldを残すと危険なので
            // ※失敗時は全部破棄する
            //================================================
            Clear();


#if defined(_DEBUG)

            OutputDebugStringW(
                L"[WorldPlacementTable] "
                L"World placement loading failed.\n"
            );
#endif
            return false;
        }
    }

    return true;
}

//============================================================
// Clear
//============================================================
void WorldPlacementTable::Clear()
{
    mPlacements.clear();

    mIdLookup.clear();
}


//============================================================
// Load
//============================================================
bool WorldPlacementTable::Load(const std::filesystem::path& filePath)
{
    Clear();

    return Append(filePath);
}

bool WorldPlacementTable::Append(const std::filesystem::path& filePath)
{
    //========================================================
    // File Open
    //========================================================
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        OutputDebugStringA("[WorldPlacementTable] CSV open failed.\n");
        return false;
    }


    std::string line;

    size_t lineNumber = 0;


    while (std::getline(file, line))
    {
        ++lineNumber;

        //====================================================
        // UTF-8 BOM除去
        //====================================================
        if (lineNumber == 1 && line.size() >= 3)
        {
            const unsigned char b0 = static_cast<unsigned char>(line[0]);
            const unsigned char b1 = static_cast<unsigned char>(line[1]);
            const unsigned char b2 = static_cast<unsigned char>(line[2]);

            if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF)
            {
                line.erase(0, 3);
            }
        }

        line = Trim(line);

        //====================================================
        // 空行
        //====================================================
        if (line.empty())
        {
            continue;
        }


        //====================================================
        // Comment
        // CSVでも
        // # Forest Area
        // のように書けるようにする
        //====================================================
        if (line[0] == '#')
        {
            continue;
        }


        std::vector<std::string> columns = SplitLine(line);


        //====================================================
        // Header
        //====================================================
        if (!columns.empty() && columns[0] == "Id")
        {
            continue;
        }


        //====================================================
        // Column数確認
        // Id
        // Prefab
        // Position 3
        // Rotation 3
        // Scale    3
        // DataId付きPlacement 1
        // 合計12
        //====================================================
        if (columns.size() != 11 && columns.size() != 12)
        {
            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[WorldPlacementTable] "
                "Invalid column count. "
                "Line=%zu Columns=%zu\n",
                lineNumber,
                columns.size()
            );

            OutputDebugStringA(buffer);

            return false;
        }



        WorldObjectPlacement placement;

        placement.Id = columns[0];

        placement.Prefab = columns[1];

        //============================================================
        // ID Duplicate Check
        // 複数CSVをMergeするため、
        // World全体でPlacement IDは一意でなければならない
        //============================================================
        if (mIdLookup.find(placement.Id) != mIdLookup.end())
        {
#if defined(_DEBUG)

            std::string message =
                "[WorldPlacementTable] "
                "Duplicate placement ID : " +
                placement.Id +
                "\n";

            OutputDebugStringA(message.c_str());

#endif
            return false;
        }

        //====================================================
        // Float変換
        // stof()は不正値で例外を投げるため
        // CSV破損を検出できる
        //====================================================
        try
        {
            placement.Position =
            {
                std::stof(columns[2]),
                std::stof(columns[3]),
                std::stof(columns[4])
            };


            placement.Rotation =
            {
                std::stof(columns[5]),
                std::stof(columns[6]),
                std::stof(columns[7])
            };


            placement.Scale =
            {
                std::stof(columns[8]),
                std::stof(columns[9]),
                std::stof(columns[10])
            };
        }
        catch (...)
        {
            char buffer[256] = {};


            sprintf_s(
                buffer,
                "[WorldPlacementTable] "
                "Invalid numeric value. "
                "Line=%zu\n",
                lineNumber
            );

            OutputDebugStringA(buffer);

            return false;
        }

        //============================================================
        // Optional Data ID
        //============================================================
        if (columns.size() >= 12)
        {
            placement.DataId = columns[11];
        }
        else
        {
            placement.DataId.clear();
        }

        //====================================================
        // ID空チェック
        //====================================================
        if (placement.Id.empty())
        {
            OutputDebugStringA(
                "[WorldPlacementTable] "
                "Empty Id detected.\n"
            );

            return false;
        }


        //====================================================
        // ID重複チェック
        //====================================================
        if (mIdLookup.find(placement.Id) != mIdLookup.end())
        {
            OutputDebugStringA(
                "[WorldPlacementTable] "
                "Duplicate Id detected.\n"
            );

            return false;
        }


        const size_t index = mPlacements.size();

        mPlacements.push_back(placement);

        mIdLookup.emplace(placement.Id, index);
    }


    //========================================================
    // Debug
    //========================================================
    char message[256] = {};


    sprintf_s(
        message,
        "[WorldPlacementTable] "
        "Loaded %zu objects.\n",
        mPlacements.size()
    );

    OutputDebugStringA(message);

    return true;

}


//============================================================
// Find
//============================================================
const WorldObjectPlacement* WorldPlacementTable::Find(const std::string& id) const
{
    const auto iterator = mIdLookup.find(id);

    if (iterator == mIdLookup.end())
    {
        return nullptr;
    }

    return &mPlacements[iterator->second];
}


//============================================================
// FindByPrefab
//============================================================
std::vector<const WorldObjectPlacement*>WorldPlacementTable::FindByPrefab(const std::string& prefab) const
{
    std::vector<const WorldObjectPlacement*> result;

    for (const WorldObjectPlacement& placement : mPlacements)
    {
        if (placement.Prefab == prefab)
        {
            result.push_back(&placement);
        }
    }

    return result;
}