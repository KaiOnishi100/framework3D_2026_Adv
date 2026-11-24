#pragma once

#include "atmosphericEscapeSaveData.h"


namespace
{
    //============================================================
    // String Write
    //============================================================
    bool WriteString(std::ofstream& file, const std::string& value)
    {
        //========================================================
        // 文字列Length
        //========================================================
        const std::uint32_t length = static_cast<std::uint32_t>(value.size());

        file.write(
            reinterpret_cast<const char*>(&length),
            sizeof(length)
        );

        //========================================================
        // 本文
        //========================================================
        if (length > 0)
        {
            file.write(value.data(), length);
        }


        return file.good();
    }
}

namespace
{
    //============================================================
    // Binary Read Helper
    //============================================================
    template<typename T> bool ReadValue(std::ifstream& file, T& value)
    {
        file.read(reinterpret_cast<char*>(&value), sizeof(T));
        return file.good();
    }


    //============================================================
    // String Read
    //============================================================
    bool ReadString(std::ifstream& file, std::string& outString)
    {
        std::uint32_t length = 0;


        if (!ReadValue(file, length))
        {
            return false;
        }


        //========================================================
        // 壊れたSave File対策
        //========================================================
        constexpr std::uint32_t MaxStringLength = 1024;

        if (length > MaxStringLength)
        {
            return false;
        }

        outString.clear();

        if (length == 0)
        {
            return true;
        }

        outString.resize(length);

        file.read(outString.data(), length);

        return file.good();
    }
}

//============================================================
// AtmosphericEscapeSaveManager
// File I/Oだけ担当する
// Scene
//   ↓ SaveDataを作る
// SaveManager
//   ↓
// File
//============================================================
class AtmosphericEscapeSaveManager
{
public:

    //========================================================
    // Save
    //========================================================
    static bool Save(const AtmosphericEscapeSaveData& data);

    //========================================================
    // Load
    //========================================================
    static bool Load(AtmosphericEscapeSaveData& outData);


    //========================================================
    // Save File存在確認
    //========================================================
    static bool Exists();


    //========================================================
    // Save削除
    //========================================================
    static bool DeleteSave();


private:

    //========================================================
    // Save File Path
    //========================================================
    static constexpr const wchar_t* SavePath = L"save/atmospheric_escape.sav";
};