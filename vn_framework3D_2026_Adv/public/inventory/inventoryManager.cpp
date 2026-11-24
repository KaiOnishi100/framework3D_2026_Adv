#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "inventoryManager.h"


//============================================================
// Initialize
//============================================================
void InventoryManager::Initialize()
{
    mDefinitions.clear();

    mItems.clear();
}


//============================================================
// Terminate
//============================================================
void InventoryManager::Terminate()
{
    mDefinitions.clear();

    mItems.clear();
}


//============================================================
// Register Item
//============================================================
bool InventoryManager::RegisterItem(const InventoryItemDefinition& definition)
{
    //========================================================
    // ID必須
    //========================================================
    if (definition.Id.empty())
    {
        return false;
    }


    //========================================================
    // 最大所持数0は無効
    //========================================================
    if (definition.MaxCount == 0)
    {
        return false;
    }


    const auto [iterator, inserted] = mDefinitions.emplace(definition.Id, definition);


#if defined(_DEBUG)

    if (!inserted)
    {
        std::string message =
            "[InventoryManager] Duplicate Item ID : " +
            definition.Id +
            "\n";


        OutputDebugStringA(message.c_str());
    }

#endif

    return inserted;
}


//============================================================
// Add Item
//============================================================
bool InventoryManager::AddItem(const std::string& itemId, std::uint32_t count)
{
    if (count == 0)
    {
        return false;
    }


    //========================================================
    // Definition確認
    //========================================================
    const InventoryItemDefinition* definition = FindDefinition(itemId);


    if (definition == nullptr)
    {
#if defined(_DEBUG)

        std::string message =
            "[InventoryManager] Unknown Item : " +
            itemId +
            "\n";


        OutputDebugStringA(message.c_str());

#endif

        return false;
    }


    //========================================================
    // 現在数
    //========================================================
    std::uint32_t currentCount = GetItemCount(itemId);

    //========================================================
    // MaxCountを超えない
    //========================================================
    const std::uint32_t newCount = (std::min)(currentCount + count, definition->MaxCount);


    //========================================================
    // すでにMaxなら追加失敗
    //========================================================
    if (newCount == currentCount)
    {
        return false;
    }

    mItems[itemId] = newCount;

#if defined(_DEBUG)

    std::string message =
        "[InventoryManager] Add Item : " +
        itemId +
        " / Count = " +
        std::to_string(newCount) +
        "\n";


    OutputDebugStringA(message.c_str());

#endif

    return true;
}


//============================================================
// Remove Item
//============================================================
bool InventoryManager::RemoveItem(const std::string& itemId,std::uint32_t count)
{
    if (count == 0)
    {
        return false;
    }

    const auto found = mItems.find(  itemId  );

    if (found == mItems.end())
    {
        return false;
    }

    if (found->second < count)
    {
        return false;
    }

    found->second -= count;

    //========================================================
    // 0になったItemは削除
    //========================================================
    if (found->second == 0)
    {
        mItems.erase(  found   );
    }

    return true;
}


//============================================================
// Has Item
//============================================================
bool InventoryManager::HasItem(const std::string& itemId, std::uint32_t count) const
{
    if (count == 0)
    {
        return true;
    }

    const auto found = mItems.find(itemId);

    if (found == mItems.end())
    {
        return false;
    }

    return found->second >= count;
}


//============================================================
// Get Count
//============================================================
std::uint32_t InventoryManager::GetItemCount(const std::string& itemId) const
{
    const auto found = mItems.find(itemId);

    if (found == mItems.end())
    {
        return 0;
    }

    return found->second;
}

//============================================================
// Find Definition
//============================================================
const InventoryItemDefinition* InventoryManager::FindDefinition(const std::string& itemId) const
{
    const auto found = mDefinitions.find(itemId);

    if (found == mDefinitions.end())
    {
        return nullptr;
    }

    return &found->second;
}


//============================================================
// UI用Entry取得
//============================================================
std::vector<InventoryEntry> InventoryManager::GetEntries() const
{
    std::vector<InventoryEntry> result;

    result.reserve(mItems.size());


    for (const auto& [itemId, count] : mItems)
    {
        if (count == 0)
        {
            continue;
        }


        InventoryEntry entry;

        entry.ItemId = itemId;
        entry.Count = count;

        result.push_back(entry);
    }

    //========================================================
    // ※unordered_mapで順序が毎回変わる可能性があるため
    // ※UI上で順番がガタガタ変わらないよう
    // ※ItemIdで安定Sortしておく
    //========================================================
    std::sort(result.begin(), result.end(), [](const InventoryEntry& a, const InventoryEntry& b)
        {
            return a.ItemId < b.ItemId;
        }
    );


    return result;
}


//============================================================
// Load
//============================================================
void InventoryManager::RestoreEntries(const std::vector<InventoryEntry>& entries)
{
    mItems.clear();


    for (const InventoryEntry& entry : entries)
    {
        //====================================================
        // Definitionが存在しない旧Save Itemは無視
        //====================================================
        const InventoryItemDefinition* definition = FindDefinition(entry.ItemId);


        if (definition == nullptr)
        {
            continue;
        }

        if (entry.Count == 0)
        {
            continue;
        }

        //====================================================
        // MaxCountを超えないよう補正
        //====================================================
        const std::uint32_t count = (std::min)(entry.Count, definition->MaxCount);

        mItems[entry.ItemId] = count;
    }
}