#pragma once

//============================================================
// Standard Library
//============================================================
#include <filesystem>
#include <string>
#include <wrl/client.h>

//============================================================
// Imported Model
//============================================================
#include "../importer/ImportedModel.h"


//============================================================
// Motion Format
//============================================================
#include "../../../../vn_framework3D_2026_Adv/framework/vn_motionData.h"


//============================================================
// AnimationWriter
// ImportedModel::Animations
//      ↓
// 1 Animation = 1 MOT V2
// Example:
// CharacterArmature|Idle
//      ↓
// TestAnimModel_Idle.mot
//============================================================
class AnimationWriter
{
public:

    //============================================================
    // Modelに含まれている全Animationを書き出す
    // modelOutputPath:
    //   C:/.../TestAnimModel.vnm
    // Output:
    //   C:/.../motion/TestAnimModel_Death.mot
    //   C:/.../motion/TestAnimModel_Idle.mot
    //   C:/.../motion/TestAnimModel_Run.mot
    //============================================================
    static bool WriteAll(
        const ImportedModel& model,
        const std::filesystem::path& modelOutputPath
    );


    //============================================================
    // Clipを書き出す
    //============================================================
    static bool WriteClip(
        const ImportedAnimation& animation,
        const std::filesystem::path& outputPath
    );


    //============================================================
    // MOT V2 Validation
    //============================================================
    static bool ValidateV2(const std::filesystem::path& filePath);


private:

    //============================================================
    // Animation Name
    // CharacterArmature|Idle
    //      ↓
    // Idle
    //============================================================
    static std::string GetShortAnimationName(const std::string& name);


    //============================================================
    // Windows File Nameに使えない文字を除去
    //============================================================
    static std::string SanitizeFileName(std::string name);


    //============================================================
    // UTF-8 → UTF-16
    //============================================================
    static std::wstring Utf8ToWide(const std::string& text);

    //============================================================
    // WCHAR[32]へ安全にコピー
    //============================================================
    static void CopyTrackName(WCHAR destination[32], const std::string& source);

    //============================================================
    // Date
    // YYYYMMDD
    //============================================================
    static UINT CreateMotionDate();
};