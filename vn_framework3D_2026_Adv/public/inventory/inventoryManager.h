#pragma once

#include "inventoryItem.h"

//============================================================
// InventoryManager
// 所持品を一元管理する
//============================================================
class InventoryManager
{
public:

    InventoryManager() = default;

    ~InventoryManager() = default;


    //========================================================
    // Initialize
    //========================================================
    void Initialize();

    //========================================================
    // Terminate
    //========================================================
    void Terminate();

    //========================================================
    // Item Definition登録
    //========================================================
    bool RegisterItem(const InventoryItemDefinition& definition);

    //========================================================
    // Add
    // 実際にItemを取得する
    //========================================================
    bool AddItem(const std::string& itemId, std::uint32_t count = 1);

    //========================================================
    // Remove
    //========================================================
    bool RemoveItem(const std::string& itemId, std::uint32_t count = 1);

    //========================================================
    // 所持確認
    //========================================================
    bool HasItem(const std::string& itemId, std::uint32_t count = 1) const;

    //========================================================
    // Count
    //========================================================
    std::uint32_t GetItemCount(const std::string& itemId) const;

    //========================================================
    // Definition取得
    //========================================================
    const InventoryItemDefinition* FindDefinition(const std::string& itemId) const;

    //========================================================
    // UI用
    // 所持中Item一覧を返す
    //========================================================
    std::vector<InventoryEntry> GetEntries() const;

    //========================================================
    // Save / Load
    //========================================================
    void RestoreEntries(const std::vector<InventoryEntry>& entries);

private:

    //========================================================
    // Item Database
    //========================================================
    std::unordered_map<std::string, InventoryItemDefinition > mDefinitions;

    //========================================================
    // Runtime Inventory
    //
    // ItemId -> Count
    //========================================================
    std::unordered_map< std::string, std::uint32_t > mItems;
};