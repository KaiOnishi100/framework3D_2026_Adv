#pragma once

#include "../exploration/explorationLocation.h"
#include "../exploration/ObjectiveManager.h"
#include "../../../inventory/inventoryManager.h"

class ExplorationLocationSystem;
class ObjectiveManager;
class InventoryManager;

//============================================================
// AtmosphericEscapeContentLoader
// Atmospheric Escapeで使用する
// 「静的Gameplay Definition」をCSVから読み込む
//============================================================
class AtmosphericEscapeContentLoader
{
public:

    AtmosphericEscapeContentLoader() = delete;


    //============================================================
    // Room Definition Load
    //
    // CSV
    //      ↓
    // RoomDefinition
    //      ↓
    // ExplorationLocationSystem::RegisterRoom()
    //============================================================
    static bool LoadRooms(
        const std::filesystem::path& filePath, 
        ExplorationLocationSystem& locationSystem
    );

    //============================================================
    // Objective Definition Load
    //
    // Objectives.csv
    //      ↓
    // ExplorationObjective
    //      ↓
    // ObjectiveManager::RegisterObjective()
    //
    // locationSystemは
    // Room Navigationが指定されたObjectiveの
    // TargetRoomId検証に使用する
    //===========================================================
    static bool LoadObjectives(
        const std::filesystem::path& filePath,
        ObjectiveManager& objectiveManager,
        const ExplorationLocationSystem& locationSystem
    );

    //============================================================
    // Inventory Item Definition Load
    // Items.csv
    //      ↓
    // InventoryItemDefinition
    //      ↓
    // InventoryManager::RegisterItem()
    //============================================================
    static bool LoadItems(
        const std::filesystem::path& filePath,
        InventoryManager& inventoryManager
    );

    //============================================================
    // Atmospheric Escape Gameplay Content
    // Room / Objective / Itemの静的Definitionを
    // 一括で読み込む
    //============================================================
    static bool LoadAll(
        ExplorationLocationSystem& locationSystem,
        ObjectiveManager& objectiveManager,
        InventoryManager& inventoryManager
    );

private:

    //============================================================
    // CSV Utility
    //============================================================

    // Cell前後の空白除去
    static std::string Trim(const std::string& text);


    //============================================================
    // CSV 1行をCellへ分割
    // 単純な ',' splitではなく
    // "Old Hall, East Wing"
    // のようなQuoted CSVも扱えるようにする
    // ※ObjectiveのHintやItem Descriptionを
    // ※CSV化したときにも再利用するため
    //============================================================
    static std::vector<std::string> SplitCsvLine(const std::string& line);


    //============================================================
    // UTF-8 -> UTF-16
    // CSVはUTF-8で保存する
    // UI側ではstd::wstringを使用しているため
    // DisplayNameを変換する
    //============================================================
    static bool Utf8ToWide(const std::string& text, std::wstring& outText);


    //============================================================
    // String -> Enum
    //============================================================
    static bool ParseArea(const std::string& text,ExplorationArea& outArea);

    static bool ParseFloor(const std::string& text, ExplorationFloor& outFloor);

    static bool ParseObjectiveNavigation(const std::string& text, ObjectiveNavigationType& outNavigation);

    //============================================================
    // Inventory Item Type Parse
    //============================================================
    static bool ParseInventoryItemType(const std::string& text, InventoryItemType& outType);
};