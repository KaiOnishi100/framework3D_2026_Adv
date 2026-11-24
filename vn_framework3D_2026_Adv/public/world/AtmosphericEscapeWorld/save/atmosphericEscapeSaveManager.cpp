#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "atmosphericEscapeSaveManager.h"

//============================================================
// Save
//============================================================
bool AtmosphericEscapeSaveManager::Save(const AtmosphericEscapeSaveData& data)
{
    //========================================================
    // Save Directory作成
    // 初回起動時にsaveフォルダがなくても動くようにする
    //========================================================
    std::error_code errorCode;

    std::filesystem::create_directories(L"save", errorCode);


    if (errorCode)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[SaveManager] "
            L"Failed to create save directory.\n"
        );

#endif

        return false;
    }


    //========================================================
    // Binary File
    //========================================================
    std::ofstream file(
        SavePath,
        std::ios::binary |
        std::ios::trunc
    );


    if (!file.is_open())
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[SaveManager] "
            L"Failed to open save file.\n"
        );

#endif

        return false;
    }


    //============================================================
    // Fixed Header
    //============================================================
    file.write(
        reinterpret_cast<const char*>(&data.Magic),
        sizeof(data.Magic)
    );

    file.write(
        reinterpret_cast<const char*>(&data.Version),
        sizeof(data.Version)
    );


    //============================================================
    // Player
    //============================================================
    file.write(
        reinterpret_cast<const char*>(&data.PlayerPosition),
        sizeof(data.PlayerPosition)
    );

    file.write(
        reinterpret_cast<const char*>(&data.PlayerHealth),
        sizeof(data.PlayerHealth)
    );

    //============================================================
    // Progress
    //============================================================
    file.write(
        reinterpret_cast<const char*>(&data.ExplorationPhase),
        sizeof(data.ExplorationPhase)
    );

    file.write(
        reinterpret_cast<const char*>(&data.PlayTime),
        sizeof(data.PlayTime)
    );

    //============================================================
    // V3
    // Current Objective ID
    //============================================================
    if (!WriteString(file,data.CurrentObjectiveId))
    {
        return false;
    }


    //============================================================
    // Inventory Count
    //============================================================
    const std::uint32_t inventoryCount = static_cast<std::uint32_t>(data.Inventory.size());

    file.write(
        reinterpret_cast<const char*>(&inventoryCount),
        sizeof(inventoryCount)
    );

    if (!file.good())
    {
        return false;
    }


    //============================================================
    // Inventory
    // 各Entry:
    // [String ItemId]
    // [uint32 Count]
    //============================================================
    for (const InventoryEntry& entry : data.Inventory)
    {
        //========================================================
        // Item ID
        //========================================================
        if (!WriteString(file, entry.ItemId))
        {
            return false;
        }

        //========================================================
        // Count
        //========================================================
        file.write(
            reinterpret_cast<const char*>(&entry.Count),
            sizeof(entry.Count)
        );

        if (!file.good())
        {
            return false;
        }
    }


    //============================================================
    // Discovered Room Count
    //============================================================
    const std::uint32_t discoveredRoomCount = static_cast<std::uint32_t>(data.DiscoveredRooms.size());

    file.write(
        reinterpret_cast<const char*>(&discoveredRoomCount),
        sizeof(discoveredRoomCount)
    );

    if (!file.good())
    {
        return false;
    }


    //============================================================
    // Discovered Room IDs
    //============================================================
    for (const std::string& roomId : data.DiscoveredRooms)
    {
        if (!WriteString(file, roomId))
        {
            return false;
        }
    }


    //============================================================
    // World Object Count
    //============================================================
    const std::uint32_t objectCount =
        static_cast<std::uint32_t>(data.WorldObjects.size());


    file.write(
        reinterpret_cast<const char*>(&objectCount),
        sizeof(objectCount)
    );


    //============================================================
    // World Objects
    //============================================================
    for (const WorldObjectSaveState& object : data.WorldObjects)
    {
        //========================================================
        // ID
        //========================================================
        if (!WriteString(file, object.Id))
        {
            return false;
        }


        //========================================================
        // Type
        //========================================================
        const std::uint32_t type = static_cast<std::uint32_t>(object.Type);


        file.write(
            reinterpret_cast<const char*>(&type),
            sizeof(type)
        );

        //========================================================
        // State
        //========================================================
        file.write(
            reinterpret_cast<const char*>(&object.State),
            sizeof(object.State)
        );

        //========================================================
        // Flags
        //========================================================
        file.write(
            reinterpret_cast<const char*>(&object.Flags),
            sizeof(object.Flags)
        );

        //========================================================
        // Value
        //========================================================
        file.write(
            reinterpret_cast<const char*>(&object.Value),
            sizeof(object.Value)
        );


        if (!file.good())
        {
            return false;
        }
    }


#if defined(_DEBUG)

    wchar_t buffer[512];

    swprintf_s(
        buffer,
        L"[SaveManager] Save completed. "
        L"Version=%u "
        L"Inventory=%zu "
        L"Rooms=%zu "
        L"WorldObjects=%zu "
        L"Objective=%S\n",

        data.Version,
        data.Inventory.size(),
        data.DiscoveredRooms.size(),
        data.WorldObjects.size(),
        data.CurrentObjectiveId.c_str()
    );

    OutputDebugStringW(buffer);

#endif

    return true;
}


//============================================================
// Load
//============================================================
bool AtmosphericEscapeSaveManager::Load(AtmosphericEscapeSaveData& outData)
{
    //============================================================
    // File Open
    //============================================================
    std::ifstream file(
        std::filesystem::path(SavePath),
        std::ios::binary
    );


    if (!file.is_open())
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[SaveManager] "
            L"Save file not found.\n"
        );

#endif

        return false;
    }


    AtmosphericEscapeSaveData loadedData;


    //============================================================
    // Header
    //============================================================
    if (!ReadValue(file, loadedData.Magic))
    {
        return false;
    }


    if (!ReadValue(file, loadedData.Version))
    {
        return false;
    }


    //============================================================
    // Magic Check
    //============================================================
    constexpr std::uint32_t ExpectedMagic = 0x41544553;


    if (loadedData.Magic != ExpectedMagic)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[SaveManager] "
            L"Invalid save magic.\n"
        );

#endif

        return false;
    }


    //============================================================
    // Version Check
    //============================================================
    if (loadedData.Version != 2 && loadedData.Version != 3)
    {
#if defined(_DEBUG)

        OutputDebugStringW(
            L"[SaveManager] "
            L"Unsupported save version.\n"
        );

#endif

        return false;
    }


    //============================================================
    // Player
    //============================================================
    if (!ReadValue(file, loadedData.PlayerPosition))
    {
        return false;
    }


    if (!ReadValue(file, loadedData.PlayerHealth))
    {
        return false;
    }


    //============================================================
    // Exploration Phase
    //============================================================
    if (!ReadValue(file, loadedData.ExplorationPhase))
    {
        return false;
    }


    //============================================================
    // Play Time
    //============================================================
    if (!ReadValue(file, loadedData.PlayTime))
    {
        return false;
    }

    //============================================================
    // V3 Additional Data
    //============================================================
    if (loadedData.Version >= 3)
    {
        //========================================================
        // Current Objective
        //========================================================
        if (!ReadString(file, loadedData.CurrentObjectiveId))
        {
            return false;
        }


        //========================================================
        // Inventory Count
        //========================================================
        std::uint32_t inventoryCount = 0;


        if (!ReadValue(file, inventoryCount))
        {
            return false;
        }


        //========================================================
        // 壊れたSave対策
        //========================================================
        constexpr std::uint32_t MaxInventoryCount = 10000;

        if (inventoryCount > MaxInventoryCount)
        {
            return false;
        }

        loadedData.Inventory.clear();
        loadedData.Inventory.reserve(inventoryCount);


        //========================================================
        // Inventory
        //========================================================
        for (std::uint32_t i = 0; i < inventoryCount; ++i)
        {
            InventoryEntry entry;

            //====================================================
            // Item ID
            //====================================================
            if (!ReadString(file, entry.ItemId))
            {
                return false;
            }

            //====================================================
            // Count
            //====================================================
            if (!ReadValue(file, entry.Count))
            {
                return false;
            }

            loadedData.Inventory.push_back(std::move(entry));
        }

        //========================================================
        // Discovered Room Count
        //========================================================
        std::uint32_t roomCount = 0;

        if (!ReadValue(file, roomCount))
        {
            return false;
        }

        constexpr std::uint32_t MaxDiscoveredRoomCount = 10000;

        if (roomCount > MaxDiscoveredRoomCount)
        {
            return false;
        }

        loadedData.DiscoveredRooms.clear();

        loadedData.DiscoveredRooms.reserve(roomCount);

        //========================================================
        // Discovered Rooms
        //========================================================
        for (std::uint32_t i = 0; i < roomCount; ++i)
        {
            std::string roomId;

            if (!ReadString(file, roomId))
            {
                return false;
            }

            loadedData.DiscoveredRooms.push_back(std::move(roomId));
        }
    }
    else
    {
        //========================================================
        // V2 Save
        // V2にはこれらの情報が存在しないためDefault値で開始する
        //========================================================
        loadedData.CurrentObjectiveId.clear();

        loadedData.Inventory.clear();

        loadedData.DiscoveredRooms.clear();
    }

    //============================================================
    // Persistent Object Count
    //============================================================
    std::uint32_t objectCount = 0;


    if (!ReadValue(file, objectCount))
    {
        return false;
    }


    //============================================================
    // 壊れたFile対策
    //============================================================
    constexpr std::uint32_t MaxObjectCount = 100000;


    if (objectCount > MaxObjectCount)
    {
        return false;
    }


    loadedData.WorldObjects.clear();

    loadedData.WorldObjects.reserve(objectCount);


    //============================================================
    // Persistent Objects
    //============================================================
    for (std::uint32_t i = 0; i < objectCount; ++i)
    {
        WorldObjectSaveState object;

        //========================================================
        // Persistent ID
        //========================================================
        if (!ReadString(file, object.Id))
        {
            return false;
        }

        //========================================================
        // Type
        //========================================================
        std::uint32_t type = 0;

        if (!ReadValue(file, type))
        {
            return false;
        }

        object.Type = static_cast<PersistentObjectType>(type);

        //========================================================
        // State
        //========================================================
        if (!ReadValue(file, object.State))
        {
            return false;
        }


        //========================================================
        // Flags
        //========================================================
        if (!ReadValue(file, object.Flags))
        {
            return false;
        }


        //========================================================
        // Generic Value
        //========================================================
        if (!ReadValue(file, object.Value))
        {
            return false;
        }

        loadedData.WorldObjects.push_back(std::move(object));
    }


    //============================================================
    // Load成功
    // 全データを正常に読めた後でoutDataへ渡す
    //============================================================
    outData = std::move(loadedData);


#if defined(_DEBUG)

    wchar_t buffer[512];

    swprintf_s(
        buffer,
        L"[SaveManager] Load completed. "
        L"Version=%u "
        L"Inventory=%zu "
        L"Rooms=%zu "
        L"WorldObjects=%zu "
        L"Objective=%S\n",

        outData.Version,
        outData.Inventory.size(),
        outData.DiscoveredRooms.size(),
        outData.WorldObjects.size(),
        outData.CurrentObjectiveId.c_str()
    );

    OutputDebugStringW(
        buffer
    );

#endif
    return true;
}


//============================================================
// Exists
//============================================================
bool AtmosphericEscapeSaveManager::Exists()
{
    return std::filesystem::exists(SavePath);
}


//============================================================
// Delete
//============================================================
bool AtmosphericEscapeSaveManager::DeleteSave()
{
    std::error_code errorCode;

    const bool removed =
        std::filesystem::remove(
            SavePath,
            errorCode
        );

    return removed && !errorCode;
}