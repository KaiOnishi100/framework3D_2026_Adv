#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "atmosphericEscapeContentLoader.h"

//============================================================
// Trim
//============================================================
std::string AtmosphericEscapeContentLoader::Trim(const std::string& text)
{
    //========================================================
    // 先頭の空白を探す
    //========================================================
    auto begin =
        std::find_if_not(
            text.begin(),
            text.end(),
            [](unsigned char c)
            {
                return std::isspace(c);
            }
        );


    //========================================================
    // 末尾の空白を探す
    //========================================================
    auto end =
        std::find_if_not(
            text.rbegin(),
            text.rend(),
            [](unsigned char c)
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
// CSV Split
//============================================================
std::vector<std::string>AtmosphericEscapeContentLoader::SplitCsvLine(const std::string& line)
{
    std::vector<std::string> result;

    std::string current;

    bool insideQuotes = false;

    //========================================================
    // 1文字ずつ解析する
    // "Old Hall, East Wing"
    // のようにCell内に','が存在しても分割されない
    //========================================================
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        const char c = line[i];

        //====================================================
        // Quote
        //====================================================
        if (c == '"')
        {
            //================================================
            // CSVでは
            // ""
            // を文字としての " として扱う
            //================================================
            if (insideQuotes && i + 1 < line.size() && line[i + 1] == '"')
            {
                current.push_back('"');

                ++i;

                continue;
            }

            //================================================
            // Quote開始 / 終了
            //================================================
            insideQuotes = !insideQuotes;

            continue;
        }


        //====================================================
        // Cell Separator
        // Quote内のCommaはSeparatorとして扱わない
        //====================================================
        if (c == ',' && !insideQuotes)
        {
            result.push_back(Trim(current));
            current.clear();
            continue;
        }

        current.push_back(c);
    }


    //========================================================
    // Quoteが閉じられていないCSV
    //========================================================
    if (insideQuotes)
    {
        return {};
    }

    //========================================================
    // 最後のCell
    //========================================================
    result.push_back(Trim(current));

    return result;
}

bool AtmosphericEscapeContentLoader::LoadItems(const std::filesystem::path& filePath, InventoryManager& inventoryManager)
{
    //========================================================
    // File Open
    //========================================================
    std::ifstream file(filePath);


    if (!file.is_open())
    {
        OutputDebugStringA(
            "[AtmosphericEscapeContentLoader] "
            "Items.csv open failed.\n"
        );

        return false;
    }


    std::string line;

    std::size_t lineNumber = 0;

    //============================================================
    // CSV Column
    // 0 : Id
    // 1 : Name
    // 2 : Description
    // 3 : Type
    // 4 : MaxCount
    // Total = 5
    //============================================================
    constexpr std::size_t ExpectedColumnCount = 5;


    while (std::getline(file, line))
    {
        ++lineNumber;


        //========================================================
        // UTF-8 BOM除去
        //========================================================
        if (lineNumber == 1 && line.size() >= 3)
        {
            const unsigned char b0 =
                static_cast<unsigned char>(line[0]);

            const unsigned char b1 =
                static_cast<unsigned char>(line[1]);

            const unsigned char b2 =
                static_cast<unsigned char>(line[2]);

            if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF)
            {
                line.erase(0, 3);
            }
        }

        //========================================================
        // 前後の空白削除
        //========================================================
        line = Trim(line);

        //========================================================
        // Empty
        //========================================================
        if (line.empty())
        {
            continue;
        }

        //========================================================
        // Comment
        //========================================================
        if (line[0] == '#')
        {
            continue;
        }

        //========================================================
        // CSV Split
        //========================================================
        const std::vector<std::string> columns = SplitCsvLine(line);

        //========================================================
        // Header
        //========================================================
        if (!columns.empty() && columns[0] == "Id")
        {
            continue;
        }


        //========================================================
        // Column Validation
        //========================================================
        if (columns.size() != ExpectedColumnCount)
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Items.csv invalid column count. "
                "Line=%zu Columns=%zu\n",
                lineNumber,
                columns.size()
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }


        //========================================================
        // Item Definition
        //========================================================
        InventoryItemDefinition item;

        //========================================================
        // ID
        //========================================================
        item.Id = columns[0];

        if (item.Id.empty())
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Item ID is empty. "
                "Line=%zu\n",
                lineNumber
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }


        //========================================================
        // Name
        // CSV = UTF-8
        // Runtime = std::wstring
        //========================================================
        if (!Utf8ToWide(columns[1], item.Name))
        {
#if defined(_DEBUG)

            OutputDebugStringA(
                "[ContentLoader] "
                "Item Name UTF-8 conversion failed.\n"
            );

#endif
            return false;
        }


        //========================================================
        // Description
        //========================================================
        if (!Utf8ToWide(columns[2], item.Description))
        {
#if defined(_DEBUG)

            OutputDebugStringA(
                "[ContentLoader] "
                "Item Description UTF-8 conversion failed.\n"
            );

#endif
            return false;
        }


        //========================================================
        // Item Type
        //========================================================
        if (!ParseInventoryItemType(columns[3], item.Type))
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Unknown Inventory Item Type. "
                "Line=%zu Value=%s\n",
                lineNumber,
                columns[3].c_str()
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }


        //========================================================
        // Max Count
        //========================================================
        try
        {
            const unsigned long parsedMaxCount = std::stoul(columns[4]);

            //====================================================
            // uint32_tへ入る範囲か確認
            //====================================================
            if (parsedMaxCount > (std::numeric_limits<std::uint32_t>::max)())
            {
                return false;
            }

            item.MaxCount = static_cast<std::uint32_t>(parsedMaxCount);
        }
        catch (...)
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Invalid Item MaxCount. "
                "Line=%zu Id=%s\n",
                lineNumber,
                item.Id.c_str()
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }


        //========================================================
        // MaxCount Validation
        // InventoryManager側でも検証しているが、
        // CSVエラーとして分かりやすくするため
        // Loader側でも確認する
        //========================================================
        if (item.MaxCount == 0)
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Item MaxCount must be greater than zero. "
                "Line=%zu Id=%s\n",
                lineNumber,
                item.Id.c_str()
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }

        //========================================================
        // Inventory Managerへ登録
        // Duplicate ID等の最終Validationは
        // InventoryManager自身に任せる
        //========================================================
        if (!inventoryManager.RegisterItem(item))
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Failed to register Inventory Item. "
                "Line=%zu Id=%s\n",
                lineNumber,
                item.Id.c_str()
            );

            OutputDebugStringA(buffer);

#endif

            return false;
        }
    }


#if defined(_DEBUG)

    OutputDebugStringA(
        "[AtmosphericEscapeContentLoader] "
        "Items loaded successfully.\n"
    );

#endif

    return true;
}

bool AtmosphericEscapeContentLoader::LoadAll(
    ExplorationLocationSystem& locationSystem, 
    ObjectiveManager& objectiveManager, 
    InventoryManager& inventoryManager
)
{
    //========================================================
    // Runtime System Initialize
    //========================================================
    locationSystem.Initialize();

    objectiveManager.Initialize();

    inventoryManager.Initialize();

    //========================================================
    // Rooms
    // ObjectiveがRoomを参照するため、
    // Roomsを必ず先に読み込む
    //========================================================
    if (!LoadRooms(
        L"data/csv/world/gameplay/Rooms.csv",
        locationSystem
    ))
    {
        OutputDebugStringA(
            "[ContentLoader] "
            "Failed to load Rooms.\n"
        );

        return false;
    }


    //========================================================
    // Objectives
    // TargetRoomIdをRooms.csvと照合する
    //========================================================
    if (!LoadObjectives(
        L"data/csv/world/gameplay/Objectives.csv",
        objectiveManager,
        locationSystem
    ))
    {
        OutputDebugStringA(
            "[ContentLoader] "
            "Failed to load Objectives.\n"
        );

        return false;
    }


    //========================================================
    // Inventory Definitions
    //========================================================
    if (!LoadItems( L"data/csv/world/gameplay/Items.csv", inventoryManager))
    {
        OutputDebugStringA(
            "[ContentLoader] "
            "Failed to load Items.\n"
        );

        return false;
    }

#if defined(_DEBUG)

    OutputDebugStringA(
        "[AtmosphericEscapeContentLoader] "
        "All gameplay content loaded successfully.\n"
    );

#endif

    return true;
}


//============================================================
// UTF-8 -> UTF-16
//============================================================
bool AtmosphericEscapeContentLoader::Utf8ToWide(const std::string& text,std::wstring& outText)
{
    outText.clear();


    if (text.empty())
    {
        return true;
    }


    //========================================================
    // 必要なUTF-16文字数を取得
    //========================================================
    const int requiredLength =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0
        );


    if (requiredLength <= 0)
    {
        return false;
    }


    //========================================================
    // Buffer確保
    //========================================================
    outText.resize(static_cast<std::size_t>(requiredLength));


    //========================================================
    // UTF-8 -> UTF-16
    //========================================================
    const int convertedLength =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            outText.data(),
            requiredLength
        );


    return convertedLength == requiredLength;
}


//============================================================
// Area Parse
//============================================================
bool AtmosphericEscapeContentLoader::ParseArea(
    const std::string& text,
    ExplorationArea& outArea
)
{
    //========================================================
    // 数値ではなく名前でCSVへ保存する
    // CSV: MainBuilding
    // C++: ExplorationArea::MainBuilding
    //========================================================

    if (text == "Unknown")
    {
        outArea = ExplorationArea::Unknown;
        return true;
    }

    if (text == "MainBuilding")
    {
        outArea = ExplorationArea::MainBuilding;
        return true;
    }

    if (text == "Courtyard")
    {
        outArea = ExplorationArea::Courtyard;
        return true;
    }

    if (text == "Annex")
    {
        outArea = ExplorationArea::Annex;
        return true;
    }

    //========================================================
    // Legacy Test Facility
    // ※Debug CSVを読み込めるように残しておく
    //========================================================
    if (text == "TestFacility")
    {
        outArea = ExplorationArea::TestFacility;
        return true;
    }

    return false;
}


//============================================================
// Floor Parse
//============================================================
bool AtmosphericEscapeContentLoader::ParseFloor(
    const std::string& text,
    ExplorationFloor& outFloor
)
{
    if (text == "Unknown")
    {
        outFloor = ExplorationFloor::Unknown;
        return true;
    }

    if (text == "Basement1")
    {
        outFloor = ExplorationFloor::Basement1;
        return true;
    }

    if (text == "Floor1")
    {
        outFloor = ExplorationFloor::Floor1;
        return true;
    }

    if (text == "Floor2")
    {
        outFloor = ExplorationFloor::Floor2;
        return true;
    }

    if (text == "Outdoor")
    {
        outFloor = ExplorationFloor::Outdoor;
        return true;
    }

    return false;
}

bool AtmosphericEscapeContentLoader::ParseObjectiveNavigation(const std::string& text, ObjectiveNavigationType& outNavigation)
{
    //========================================================
    // Navigationなし
    //========================================================
    if (text == "None")
    {
        outNavigation = ObjectiveNavigationType::None;
        return true;
    }


    //========================================================
    // Room単位で目的地を表示
    //========================================================
    if (text == "Room")
    {
        outNavigation = ObjectiveNavigationType::Room;
        return true;
    }


    //========================================================
    // 正確なWorld Positionを使用
    // Tutorialや特殊Objective用
    //========================================================
    if (text == "WorldPosition")
    {
        outNavigation = ObjectiveNavigationType::WorldPosition;
        return true;
    }


    //========================================================
    // CSV Typo等
    //========================================================
    return false;
}

bool AtmosphericEscapeContentLoader::ParseInventoryItemType(const std::string& text, InventoryItemType& outType)
{
    //========================================================
    // Key Item
    //========================================================
    if (text == "KeyItem")
    {
        outType = InventoryItemType::KeyItem;
        return true;
    }

    //========================================================
    // Puzzle Item
    //========================================================
    if (text == "PuzzleItem")
    {
        outType = InventoryItemType::PuzzleItem;
        return true;
    }

    //========================================================
    // Consumable
    //========================================================
    if (text == "Consumable")
    {
        outType = InventoryItemType::Consumable;
        return true;
    }

    //========================================================
    // Document
    //========================================================
    if (text == "Document")
    {
        outType = InventoryItemType::Document;
        return true;
    }

    //========================================================
    // 未知のType
    // Typoをゲーム起動時に検出する
    //========================================================
    return false;
}


//============================================================
// Room Load
//============================================================
bool AtmosphericEscapeContentLoader::LoadRooms(
    const std::filesystem::path& filePath,
    ExplorationLocationSystem& locationSystem
)
{
    //========================================================
    // File Open
    // CSVはUTF-8で保存する
    //========================================================
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        OutputDebugStringA(
            "[AtmosphericEscapeContentLoader] "
            "Rooms.csv open failed.\n"
        );

        return false;
    }

    std::string line;

    std::size_t lineNumber = 0;

    //============================================================
    // Expected CSV:
    // 0  Id
    // 1  DisplayName
    // 2  Area
    // 3  Floor
    // 4  MinX
    // 5  MinY
    // 6  MinZ
    // 7  MaxX
    // 8  MaxY
    // 9  MaxZ
    // Total = 10
    //============================================================
    constexpr std::size_t ExpectedColumnCount = 10;


    while (std::getline(file, line))
    {
        ++lineNumber;

        //========================================================
        // UTF-8 BOM除去
        // Excel等からUTF-8 CSVを保存した場合、
        // 先頭にBOMが付く可能性がある
        //========================================================
        if (lineNumber == 1 && line.size() >= 3)
        {
            const unsigned char b0 =
                static_cast<unsigned char>(line[0]);

            const unsigned char b1 =
                static_cast<unsigned char>(line[1]);

            const unsigned char b2 =
                static_cast<unsigned char>(line[2]);

            if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF)
            {
                line.erase(0, 3);
            }
        }


        line = Trim(line);


        //========================================================
        // Empty Line
        //========================================================
        if (line.empty())
        {
            continue;
        }


        //========================================================
        // Comment
        // # Main Building 1F
        // のようにCSV内を整理できる
        //========================================================
        if (line[0] == '#')
        {
            continue;
        }


        //========================================================
        // CSV Split
        //========================================================
        const std::vector<std::string> columns = SplitCsvLine(line);


        //========================================================
        // Header
        //========================================================
        if (!columns.empty() && columns[0] == "Id")
        {
            continue;
        }


        //========================================================
        // Column Count
        //========================================================
        if (columns.size() != ExpectedColumnCount)
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Rooms.csv invalid column count. "
                "Line=%zu Columns=%zu\n",
                lineNumber,
                columns.size()
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }


        //========================================================
        // Room Definition
        //========================================================
        RoomDefinition room;


        //========================================================
        // ID
        //========================================================
        room.Id = columns[0];


        if (room.Id.empty())
        {
#if defined(_DEBUG)

            OutputDebugStringA(
                "[ContentLoader] "
                "Room ID is empty.\n"
            );

#endif

            return false;
        }


        //========================================================
        // Display Name
        // UTF-8 -> std::wstring
        //========================================================
        if (!Utf8ToWide(columns[1], room.DisplayName))
        {
#if defined(_DEBUG)

            OutputDebugStringA(
                "[ContentLoader] "
                "Room DisplayName UTF-8 conversion failed.\n"
            );

#endif

            return false;
        }


        //========================================================
        // Area
        //========================================================
        if (!ParseArea(columns[2], room.Area))
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Unknown Room Area. "
                "Line=%zu Value=%s\n",
                lineNumber,
                columns[2].c_str()
            );

            OutputDebugStringA(buffer);

#endif

            return false;
        }


        //========================================================
        // Floor
        //========================================================
        if (!ParseFloor(columns[3], room.Floor))
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Unknown Room Floor. "
                "Line=%zu Value=%s\n",
                lineNumber,
                columns[3].c_str()
            );

            OutputDebugStringA(buffer);

#endif

            return false;
        }


        //========================================================
        // AABB
        // stofは不正な数値なら例外になるため、
        // CSV破損をその場で検出する
        //========================================================
        try
        {
            room.MinBounds =
            {
                std::stof(columns[4]),
                std::stof(columns[5]),
                std::stof(columns[6])
            };


            room.MaxBounds =
            {
                std::stof(columns[7]),
                std::stof(columns[8]),
                std::stof(columns[9])
            };
        }
        catch (...)
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Rooms.csv invalid numeric value. "
                "Line=%zu\n",
                lineNumber
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }


        //========================================================
        // AABB Validation
        // Min > MaxだとContains()が成立しなくなるため、
        // 起動時点で検出する
        //========================================================
        if (room.MinBounds.x > room.MaxBounds.x ||
            room.MinBounds.y > room.MaxBounds.y ||
            room.MinBounds.z > room.MaxBounds.z)
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Invalid Room AABB. "
                "Line=%zu Id=%s\n",
                lineNumber,
                room.Id.c_str()
            );

            OutputDebugStringA(buffer);

#endif

            return false;
        }


        //========================================================
        // Runtime Systemへ登録
        // Duplicate ID等の最終Validationは
        // ExplorationLocationSystem自身に任せる
        //========================================================
        if (!locationSystem.RegisterRoom(room))
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Failed to register Room. "
                "Line=%zu Id=%s\n",
                lineNumber,
                room.Id.c_str()
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }
    }


#if defined(_DEBUG)

    OutputDebugStringA(
        "[AtmosphericEscapeContentLoader] "
        "Rooms loaded successfully.\n"
    );

#endif

    return true;
}

bool AtmosphericEscapeContentLoader::LoadObjectives(const std::filesystem::path& filePath, ObjectiveManager& objectiveManager, const ExplorationLocationSystem& locationSystem)
{
    //========================================================
    // File Open
    //========================================================
    std::ifstream file(filePath);


    if (!file.is_open())
    {
        OutputDebugStringA(
            "[AtmosphericEscapeContentLoader] "
            "Objectives.csv open failed.\n"
        );

        return false;
    }


    std::string line;

    std::size_t lineNumber = 0;

    //============================================================
    // CSV Column
    // 0 : Id
    // 1 : Text
    // 2 : Hint
    // 3 : Navigation
    // 4 : TargetArea
    // 5 : TargetFloor
    // 6 : TargetRoomId
    // 7 : TargetX
    // 8 : TargetY
    // 9 : TargetZ
    // Total = 10
    //============================================================
    constexpr std::size_t ExpectedColumnCount = 10;


    while (std::getline(file, line))
    {
        ++lineNumber;


        //========================================================
        // UTF-8 BOM
        //========================================================
        if (lineNumber == 1 && line.size() >= 3)
        {
            const unsigned char b0 =
                static_cast<unsigned char>(line[0]);

            const unsigned char b1 =
                static_cast<unsigned char>(line[1]);

            const unsigned char b2 =
                static_cast<unsigned char>(line[2]);

            if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF)
            {
                line.erase(0, 3);
            }
        }


        line = Trim(line);


        //========================================================
        // Empty / Comment
        //========================================================
        if (line.empty())
        {
            continue;
        }

        if (line[0] == '#')
        {
            continue;
        }

        //========================================================
        // Split
        //========================================================
        const std::vector<std::string> columns = SplitCsvLine(line);


        //========================================================
        // Header
        //========================================================
        if (!columns.empty() &&
            columns[0] == "Id")
        {
            continue;
        }


        //========================================================
        // Column Validation
        //========================================================
        if (columns.size() != ExpectedColumnCount)
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Objectives.csv invalid column count. "
                "Line=%zu Columns=%zu\n",
                lineNumber,
                columns.size()
            );

            OutputDebugStringA(buffer);

#endif

            return false;
        }


        //========================================================
        // Objective
        //========================================================
        ExplorationObjective objective;


        //========================================================
        // ID
        //========================================================
        objective.Id = columns[0];


        if (objective.Id.empty())
        {
#if defined(_DEBUG)

            OutputDebugStringA(
                "[ContentLoader] "
                "Objective ID is empty.\n"
            );

#endif

            return false;
        }


        //========================================================
        // Main Text
        //========================================================
        if (!Utf8ToWide(columns[1], objective.Text))
        {
            return false;
        }


        //========================================================
        // Hint
        // 空文字も許可する
        //========================================================
        if (!Utf8ToWide(columns[2], objective.Hint))
        {
            return false;
        }


        //========================================================
        // Navigation Type
        //========================================================
        if (!ParseObjectiveNavigation(columns[3], objective.Navigation))
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Unknown Objective Navigation. "
                "Line=%zu Value=%s\n",
                lineNumber,
                columns[3].c_str()
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }


        //========================================================
        // Area
        //========================================================
        if (!ParseArea(columns[4], objective.TargetArea))
        {
            return false;
        }


        //========================================================
        // Floor
        //========================================================
        if (!ParseFloor(columns[5], objective.TargetFloor))
        {
            return false;
        }


        //========================================================
        // Room ID
        //========================================================
        objective.TargetRoomId = columns[6];

        //========================================================
        // Target World Position
        // NavigationがNone/RoomでもCSV Schemaを固定するため
        // 0,0,0を保持する
        //========================================================
        try
        {
            objective.TargetPosition =
            {
                std::stof(columns[7]),
                std::stof(columns[8]),
                std::stof(columns[9])
            };
        }
        catch (...)
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Objective Position parse failed. "
                "Line=%zu Id=%s\n",
                lineNumber,
                objective.Id.c_str()
            );

            OutputDebugStringA(buffer);

#endif

            return false;
        }


        //========================================================
        // Navigation別Validation
        //========================================================
        switch (objective.Navigation)
        {
            //====================================================
            // Navigationなし
            //====================================================
        case ObjectiveNavigationType::None:
        {
            // 特別なValidation不要
            break;
        }


        //====================================================
        // Room Navigation
        //====================================================
        case ObjectiveNavigationType::Room:
        {
            //================================================
            // Room ID必須
            //================================================
            if (objective.TargetRoomId.empty())
            {
#if defined(_DEBUG)

                char buffer[256] = {};

                sprintf_s(
                    buffer,
                    "[ContentLoader] "
                    "Room Objective has empty TargetRoomId. "
                    "Line=%zu Id=%s\n",
                    lineNumber,
                    objective.Id.c_str()
                );

                OutputDebugStringA(buffer);

#endif

                return false;
            }


            //================================================
            // Rooms.csvに実在するRoomか検証
            // ObjectiveだけTypoして
            // Map Navigationが壊れる問題を
            // ゲーム起動時点で検出する
            //================================================
            if (locationSystem.FindRoom(objective.TargetRoomId) == nullptr)
            {
#if defined(_DEBUG)

                char buffer[512] = {};

                sprintf_s(
                    buffer,
                    "[ContentLoader] "
                    "Objective TargetRoom not found. "
                    "Line=%zu Objective=%s Room=%s\n",
                    lineNumber,
                    objective.Id.c_str(),
                    objective.TargetRoomId.c_str()
                );

                OutputDebugStringA(buffer);

#endif

                return false;
            }


            break;
        }


        //====================================================
        // Exact World Position
        //====================================================
        case ObjectiveNavigationType::WorldPosition:
        {
            // 現段階ではPositionが読めればOK
            break;
        }

        default:
            return false;
        }


        //========================================================
        // Objective Managerへ登録
        // Duplicate IDの最終判定は
        // ObjectiveManager自身へ任せる
        //========================================================
        if (!objectiveManager.RegisterObjective(objective))
        {
#if defined(_DEBUG)

            char buffer[256] = {};

            sprintf_s(
                buffer,
                "[ContentLoader] "
                "Failed to register Objective. "
                "Line=%zu Id=%s\n",
                lineNumber,
                objective.Id.c_str()
            );

            OutputDebugStringA(buffer);

#endif
            return false;
        }
    }


#if defined(_DEBUG)

    OutputDebugStringA(
        "[AtmosphericEscapeContentLoader] "
        "Objectives loaded successfully.\n"
    );

#endif

    return true;
}
