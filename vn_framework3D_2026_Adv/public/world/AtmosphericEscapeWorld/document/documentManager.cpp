#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "documentManager.h"


//============================================================
// Definition登録
//============================================================
bool DocumentManager::RegisterDefinition(const DocumentDefinition& definition)
{
    //============================================================
    // IDなしDocumentは登録不可
    //============================================================
    if (definition.Id.empty())
    {
        return false;
    }


    //============================================================
    // 重複ID確認
    //============================================================
    if (mDefinitions.find(definition.Id) != mDefinitions.end())
    {
        return false;
    }

    //============================================================
    // Definition追加
    //============================================================
    mDefinitions.emplace(definition.Id, definition);

    return true;
}

//============================================================
// Definition検索
//============================================================
const DocumentDefinition* DocumentManager::FindDefinition(const std::string& documentId) const
{
    const auto found =
        mDefinitions.find(documentId );

    //============================================================
    // 未登録
    //============================================================
    if (found == mDefinitions.end())
    {
        return nullptr;
    }

    return &found->second;
}

//============================================================
// Document取得
//============================================================
bool DocumentManager::CollectDocument(const std::string& documentId)
{
    //============================================================
    // まずDefinitionが存在するか確認
    // 存在しないIDをSave状態などへ混入させない
    //============================================================
    const DocumentDefinition* definition =
        FindDefinition(documentId);

    if (definition == nullptr)
    {
        return false;
    }

    //============================================================
    // 既に取得済み
    //============================================================
    if (mCollectedDocumentSet.find(documentId) != mCollectedDocumentSet.end())
    {
        return false;
    }

    //============================================================
    // Set
    // 重複判定用
    //============================================================
    mCollectedDocumentSet.insert(documentId);

    //============================================================
    // Vector
    // 実際に取得した順番を保存する
    //============================================================
    mCollectedDocumentIds.push_back(documentId);

    return true;
}

//============================================================
// 取得済み判定
//============================================================
bool DocumentManager::IsCollected(const std::string& documentId) const
{
    return mCollectedDocumentSet.find(documentId) != mCollectedDocumentSet.end();
}

//============================================================
// 取得済みDocument一覧
//============================================================
std::vector<const DocumentDefinition*> DocumentManager::GetCollectedDocuments() const
{
    std::vector<const DocumentDefinition*> documents;


    documents.reserve(mCollectedDocumentIds.size());


    //============================================================
    // vector側の順番をそのまま使用することで
    // 取得順を維持する
    //============================================================
    for (const std::string& documentId : mCollectedDocumentIds)
    {
        const DocumentDefinition* definition =
            FindDefinition(documentId);

        //========================================================
        // 通常はあり得ないが、
        // Definitionが削除された場合などの安全対策
        //========================================================
        if (definition == nullptr)
        {
            continue;
        }

        documents.push_back(definition);
    }

    return documents;
}


//============================================================
// Saveから取得状態を復元
//============================================================
void DocumentManager::RestoreCollectedDocuments(const std::vector<std::string>& documentIds)
{
    //============================================================
    // 現在の取得状態だけClear
    // DefinitionはContentLoaderが登録済みなので残す
    //============================================================
    mCollectedDocumentIds.clear();
    mCollectedDocumentSet.clear();

    //============================================================
    // SaveData順に復元
    //============================================================
    for (const std::string& documentId :documentIds)
    {
        CollectDocument(documentId);
    }
}


//============================================================
// Clear
//============================================================
void DocumentManager::Clear()
{
    //============================================================
    // 固定Definition
    //============================================================
    mDefinitions.clear();

    //============================================================
    // Runtime取得状態
    //============================================================
    mCollectedDocumentIds.clear();
    mCollectedDocumentSet.clear();
}