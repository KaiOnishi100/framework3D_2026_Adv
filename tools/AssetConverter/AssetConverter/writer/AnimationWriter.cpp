#include "AnimationWriter.h"

//============================================================
// Windows
//============================================================
#include <Windows.h>


//============================================================
// Standard Library
//============================================================
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>


namespace
{
    //============================================================
    // MOT Magic
    //============================================================
    constexpr UINT MOT_MAGIC = 0x00746F6Du;
}


//============================================================
// UTF-8 → UTF-16
//============================================================
std::wstring AnimationWriter::Utf8ToWide(const std::string& text)
{
    if (text.empty())
    {
        return {};
    }


    const int requiredLength = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.c_str(),
        -1,
        nullptr,
        0
    );


    if (requiredLength <= 0)
    {
        return {};
    }

    std::wstring result;

    result.resize(static_cast<std::size_t>(requiredLength));

    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.c_str(),
        -1,
        result.data(),
        requiredLength
    );

    if (!result.empty() && result.back() == L'\0')
    {
        result.pop_back();
    }

    return result;
}


//============================================================
// Track Name
//============================================================
void AnimationWriter::CopyTrackName(WCHAR destination[32],const std::string& source)
{
    //========================================================
    // 最初に全体を0にする
    //========================================================
    std::memset(
        destination,
        0,
        sizeof(WCHAR) * 32
    );


    const std::wstring wide = Utf8ToWide(source);


    if (wide.empty())
    {
        return;
    }


    //========================================================
    // 31文字 + Null
    //========================================================
    wcsncpy_s(
        destination,
        32,
        wide.c_str(),
        31
    );
}


//============================================================
// CharacterArmature|Idle
//      ↓
// Idle
//============================================================
std::string AnimationWriter::GetShortAnimationName(const std::string& name)
{
    if (name.empty())
    {
        return "Animation";
    }

    //========================================================
    // Quaternius FBX:
    // CharacterArmature|Idle
    // CharacterArmature|Run
    // 最後の | より後ろだけ使う
    //========================================================
    const std::size_t separator = name.find_last_of('|');


    if (separator != std::string::npos && separator + 1 < name.size())
    {
        return name.substr(separator + 1);
    }

    return name;
}


//============================================================
// Windows File Name用
//============================================================
std::string AnimationWriter::SanitizeFileName(std::string name)
{
    //========================================================
    // WindowsでFile Nameとして使用できない文字
    // < > : " / \ | ? *
    //========================================================
    for (char& character : name)
    {
        switch (character)
        {
        case '<':
        case '>':
        case ':':
        case '"':
        case '/':
        case '\\':
        case '|':
        case '?':
        case '*':

            character = '_';

            break;

        default:
            break;
        }
    }


    if (name.empty())
    {
        name = "Animation";
    }


    return name;
}


//============================================================
// Date
//============================================================
UINT AnimationWriter::CreateMotionDate()
{
    SYSTEMTIME time = {};

    GetLocalTime(&time);

    //========================================================
    // YYYYMMDD
    // Example:
    // 20260905
    //========================================================
    return static_cast<UINT>(time.wYear * 10000 + time.wMonth * 100 + time.wDay);
}

//============================================================
// Animation Clip → MOT V2
//============================================================
bool AnimationWriter::WriteClip(
    const ImportedAnimation& animation,
    const std::filesystem::path& outputPath
)
{
    //========================================================
    // Animation Channelなし
    //========================================================
    if (animation.Channels.empty())
    {
        std::cerr
            << "[AnimationWriter] "
            << "Animation has no channels.\n"
            << "Name : "
            << animation.Name
            << "\n";

        return false;
    }


    //========================================================
    // Output Directory
    //========================================================
    const std::filesystem::path directory = outputPath.parent_path();


    if (!directory.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(directory, error);

        if (error)
        {
            std::cerr
                << "[AnimationWriter] "
                << "Could not create output directory.\n";

            return false;
        }
    }


    //========================================================
    // Binary出力用
    //========================================================
    std::vector<vnMotionData_TrackV2> outputTracks;

    std::vector<vnMotionData_VectorKeyV2> outputVectorKeys;

    std::vector<vnMotionData_QuaternionKeyV2> outputQuaternionKeys;

    outputTracks.reserve(animation.Channels.size());

    //========================================================
    // 全Bone Track
    //========================================================
    for (const ImportedAnimationChannel& sourceChannel : animation.Channels)
    {
        //====================================================
        // Skeleton対応なし
        // ※Import段階で基本的に除外されているはずだが
        // ※Writerでも安全確認
        //====================================================
        if (sourceChannel.SkeletonIndex < 0)
        {
            std::cerr
                << "[AnimationWriter] "
                << "Invalid SkeletonIndex.\n"
                << "Node : "
                << sourceChannel.NodeName
                << "\n";

            return false;
        }


        vnMotionData_TrackV2 track = {};


        //====================================================
        // Track Name
        //====================================================
        CopyTrackName(track.Name, sourceChannel.NodeName);

        //====================================================
        // Skeleton Index
        //====================================================
        track.SkeletonIndex = sourceChannel.SkeletonIndex;

        //====================================================
        // Position
        //====================================================
        track.PositionStartIndex = static_cast<UINT>(outputVectorKeys.size());

        track.PositionKeyNum = static_cast<UINT>(sourceChannel.PositionKeys.size());

        for (const ImportedVectorKey& sourceKey : sourceChannel.PositionKeys)
        {
            vnMotionData_VectorKeyV2 key = {};

            key.Time = static_cast<float>(sourceKey.Time);

            key.Value[0] = sourceKey.Value.x;
            key.Value[1] = sourceKey.Value.y;
            key.Value[2] = sourceKey.Value.z;


            outputVectorKeys.push_back(key);
        }


        //====================================================
        // Rotation
        //====================================================
        track.RotationStartIndex = static_cast<UINT>(outputQuaternionKeys.size());

        track.RotationKeyNum = static_cast<UINT>(sourceChannel.RotationKeys.size());


        for (const ImportedQuaternionKey& sourceKey : sourceChannel.RotationKeys)
        {
            vnMotionData_QuaternionKeyV2 key = {};


            key.Time = static_cast<float>(sourceKey.Time);


            key.Value[0] = sourceKey.Value.x;
            key.Value[1] = sourceKey.Value.y;
            key.Value[2] = sourceKey.Value.z;
            key.Value[3] = sourceKey.Value.w;

            outputQuaternionKeys.push_back(key);
        }


        //====================================================
        // Scale
        //====================================================
        track.ScaleStartIndex = static_cast<UINT>(outputVectorKeys.size());

        track.ScaleKeyNum = static_cast<UINT>(sourceChannel.ScaleKeys.size());


        for (const ImportedVectorKey& sourceKey : sourceChannel.ScaleKeys)
        {
            vnMotionData_VectorKeyV2 key = {};


            key.Time = static_cast<float>(sourceKey.Time);


            key.Value[0] = sourceKey.Value.x;

            key.Value[1] = sourceKey.Value.y;

            key.Value[2] = sourceKey.Value.z;


            outputVectorKeys.push_back(key);
        }


        //====================================================
        // Track Record Size
        //====================================================
        track.StructSize = sizeof(vnMotionData_TrackV2);


        //====================================================
        // Track完成
        //====================================================
        outputTracks.push_back(track);
    }


    //========================================================
    // UINT Overflow Check
    //========================================================
    if (outputTracks.size() >
        (std::numeric_limits<UINT>::max)() ||
        outputVectorKeys.size() >
        (std::numeric_limits<UINT>::max)() ||
        outputQuaternionKeys.size() >
        (std::numeric_limits<UINT>::max)())
    {
        std::cerr
            << "[AnimationWriter] "
            << "Animation data is too large.\n";

        return false;
    }


    //========================================================
    // File Layout
    //========================================================

    //========================================================
    // Header
    //========================================================
    const std::uint64_t headerOffset = 0u;

    //========================================================
    // Track
    //========================================================
    const std::uint64_t trackOffset = sizeof(vnMotionDataV2);


    //========================================================
    // Vector Key
    //========================================================
    const std::uint64_t vectorKeyOffset =
        trackOffset +
        sizeof(vnMotionData_TrackV2) *
        outputTracks.size();


    //========================================================
    // Quaternion Key
    //========================================================
    const std::uint64_t quaternionKeyOffset =
        vectorKeyOffset +
        sizeof(vnMotionData_VectorKeyV2) *
        outputVectorKeys.size();


    //========================================================
    // End
    //========================================================
    const std::uint64_t fileSize =
        quaternionKeyOffset +
        sizeof(vnMotionData_QuaternionKeyV2) *
        outputQuaternionKeys.size();


    //========================================================
    // HeaderのOffsetはUINTなので確認
    //========================================================
    if (fileSize > (std::numeric_limits<UINT>::max)())
    {
        std::cerr
            << "[AnimationWriter] "
            << "MOT file is too large.\n";

        return false;
    }


    //========================================================
    // Header
    //========================================================
    vnMotionDataV2 header = {};

    header.Magic = MOT_MAGIC;
    header.Date = CreateMotionDate();
    header.Version = vnMOTION_DATA_V2_VER;
    header.HeaderSize = sizeof(vnMotionDataV2);


    //========================================================
    // Length
    //========================================================
    header.Length = static_cast<float>(animation.DurationSeconds);


    //========================================================
    // Counts
    //========================================================
    header.TrackNum = static_cast<UINT>(outputTracks.size());

    header.VectorKeyNum = static_cast<UINT>(outputVectorKeys.size());

    header.QuaternionKeyNum = static_cast<UINT>(outputQuaternionKeys.size());

    //========================================================
    // Source TPS
    //========================================================
    header.SourceTicksPerSecond = static_cast<float>(animation.TicksPerSecond);

    //========================================================
    // Offsets
    //========================================================
    header.TrackAccess = static_cast<UINT>(trackOffset);

    header.VectorKeyAccess = static_cast<UINT>(vectorKeyOffset);

    header.QuaternionKeyAccess = static_cast<UINT>(quaternionKeyOffset);


    //========================================================
    // File Open
    //========================================================
    std::ofstream stream(outputPath, std::ios::binary | std::ios::trunc);


    if (!stream)
    {
        std::cerr
            << "[AnimationWriter] "
            << "Could not open output file.\n";

        return false;
    }


    //========================================================
    // Header
    //========================================================
    stream.seekp(headerOffset);


    stream.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );


    //========================================================
    // Tracks
    //========================================================
    stream.seekp(trackOffset);


    if (!outputTracks.empty())
    {
        stream.write(
            reinterpret_cast<const char*>(outputTracks.data()),
            static_cast<std::streamsize>(sizeof(vnMotionData_TrackV2) * outputTracks.size())
        );
    }


    //========================================================
    // Vector Keys
    //========================================================
    stream.seekp(vectorKeyOffset);


    if (!outputVectorKeys.empty())
    {
        stream.write(
            reinterpret_cast<const char*>(outputVectorKeys.data()),
            static_cast<std::streamsize>(sizeof(vnMotionData_VectorKeyV2) * outputVectorKeys.size())
        );
    }


    //========================================================
    // Quaternion Keys
    //========================================================
    stream.seekp(quaternionKeyOffset);


    if (!outputQuaternionKeys.empty())
    {
        stream.write(
            reinterpret_cast<const char*>(outputQuaternionKeys.data()),
            static_cast<std::streamsize>(sizeof(vnMotionData_QuaternionKeyV2) * outputQuaternionKeys.size())
        );
    }


    //========================================================
    // Write Check
    //========================================================
    if (!stream.good())
    {
        std::cerr
            << "[AnimationWriter] "
            << "File write failed.\n";

        return false;
    }


    stream.close();


    //========================================================
    // Debug
    //========================================================
    std::wcout
        << L"\n========================================\n"
        << L" MOT V2 Export Success\n"
        << L"========================================\n"
        << L"File           : "
        << outputPath.wstring()
        << L"\n"
        << L"Tracks         : "
        << outputTracks.size()
        << L"\n"
        << L"Vector Keys    : "
        << outputVectorKeys.size()
        << L"\n"
        << L"Quaternion Keys: "
        << outputQuaternionKeys.size()
        << L"\n"
        << L"Length         : "
        << header.Length
        << L" sec\n"
        << L"Source TPS     : "
        << header.SourceTicksPerSecond
        << L"\n"
        << L"File Size      : "
        << fileSize
        << L"\n"
        << L"========================================\n";


    return true;
}

//============================================================
// MOT V2 Validation
//============================================================
bool AnimationWriter::ValidateV2(const std::filesystem::path& filePath)
{
    //========================================================
    // File Open
    //========================================================
    std::ifstream stream(filePath, std::ios::binary | std::ios::ate);


    if (!stream)
    {
        std::cerr
            << "[MOT Validate] "
            << "Could not open file.\n";

        return false;
    }

    //========================================================
    // File Size
    //========================================================
    const std::streamoff fileSize = stream.tellg();

    if (fileSize < static_cast<std::streamoff>(sizeof(vnMotionDataV2)))
    {
        std::cerr
            << "[MOT Validate] "
            << "File is too small.\n";

        return false;
    }

    stream.seekg(0, std::ios::beg);

    //========================================================
    // Header
    //========================================================
    vnMotionDataV2 header = {};


    stream.read(
        reinterpret_cast<char*>(&header),
        sizeof(header)
    );


    if (!stream)
    {
        return false;
    }


    //========================================================
    // Magic
    //========================================================
    if (header.Magic != MOT_MAGIC)
    {
        std::cerr
            << "[MOT Validate] "
            << "Invalid Magic.\n";

        return false;
    }


    //========================================================
    // Version
    //========================================================
    if (header.Version != vnMOTION_DATA_V2_VER)
    {
        std::cerr
            << "[MOT Validate] "
            << "Invalid Version.\n";

        return false;
    }


    //========================================================
    // Header Size
    //========================================================
    if (header.HeaderSize != sizeof(vnMotionDataV2))
    {
        std::cerr
            << "[MOT Validate] "
            << "Invalid HeaderSize.\n";

        return false;
    }


    //========================================================
    // Expected Offsets
    //========================================================
    const std::uint64_t expectedTrackAccess =
        sizeof(vnMotionDataV2);


    const std::uint64_t expectedVectorKeyAccess =
        expectedTrackAccess +
        static_cast<std::uint64_t>(header.TrackNum) *
        sizeof(vnMotionData_TrackV2);


    const std::uint64_t expectedQuaternionKeyAccess =
        expectedVectorKeyAccess +
        static_cast<std::uint64_t>(header.VectorKeyNum) *
        sizeof(vnMotionData_VectorKeyV2);


    const std::uint64_t expectedFileSize =
        expectedQuaternionKeyAccess +
        static_cast<std::uint64_t>(header.QuaternionKeyNum) *
        sizeof(vnMotionData_QuaternionKeyV2);


    //========================================================
    // Offset Check
    //========================================================
    if (header.TrackAccess != expectedTrackAccess)
    {
        std::cerr
            << "[MOT Validate] "
            << "TrackAccess mismatch.\n";

        return false;
    }


    if (header.VectorKeyAccess != expectedVectorKeyAccess)
    {
        std::cerr
            << "[MOT Validate] "
            << "VectorKeyAccess mismatch.\n";

        return false;
    }


    if (header.QuaternionKeyAccess != expectedQuaternionKeyAccess)
    {
        std::cerr
            << "[MOT Validate] "
            << "QuaternionKeyAccess mismatch.\n";

        return false;
    }


    //========================================================
    // File Size
    //========================================================
    if (expectedFileSize != static_cast<std::uint64_t>(fileSize))
    {
        std::cerr
            << "[MOT Validate] "
            << "File size mismatch.\n"
            << "Actual   : "
            << fileSize
            << "\n"
            << "Expected : "
            << expectedFileSize
            << "\n";

        return false;
    }


    //========================================================
    // Tracks
    //========================================================
    std::vector<vnMotionData_TrackV2>  tracks(header.TrackNum);

    stream.seekg(header.TrackAccess, std::ios::beg);

    if (!tracks.empty())
    {
        stream.read(
            reinterpret_cast<char*>(tracks.data()),
            static_cast<std::streamsize>(sizeof(vnMotionData_TrackV2) * tracks.size())
        );


        if (!stream)
        {
            return false;
        }
    }


    //========================================================
    // Track Range Check
    //========================================================
    for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
    {
        const vnMotionData_TrackV2& track = tracks[trackIndex];


        //====================================================
        // Struct Size
        //====================================================
        if (track.StructSize != sizeof(vnMotionData_TrackV2))
        {
            std::cerr
                << "[MOT Validate] "
                << "Invalid Track StructSize.\n"
                << "Track = "
                << trackIndex
                << "\n";

            return false;
        }


        //====================================================
        // Skeleton Index
        //====================================================
        if (track.SkeletonIndex < 0)
        {
            std::cerr
                << "[MOT Validate] "
                << "Invalid SkeletonIndex.\n";

            return false;
        }


        //====================================================
        // Position Range
        //====================================================
        if (static_cast<std::uint64_t>(track.PositionStartIndex) + track.PositionKeyNum > header.VectorKeyNum)
        {
            std::cerr
                << "[MOT Validate] "
                << "Position key range invalid.\n";

            return false;
        }


        //====================================================
        // Scale Range
        //====================================================
        if (static_cast<std::uint64_t>(track.ScaleStartIndex) + track.ScaleKeyNum > header.VectorKeyNum)
        {
            std::cerr
                << "[MOT Validate] "
                << "Scale key range invalid.\n";

            return false;
        }


        //====================================================
        // Rotation Range
        //====================================================
        if (static_cast<std::uint64_t>(track.RotationStartIndex) + track.RotationKeyNum > header.QuaternionKeyNum)
        {
            std::cerr
                << "[MOT Validate] "
                << "Rotation key range invalid.\n";

            return false;
        }
    }


    std::cout
        << "\n========================================\n"
        << " MOT V2 Validation Success\n"
        << "========================================\n"
        << "Tracks          : "
        << header.TrackNum
        << "\n"
        << "Vector Keys     : "
        << header.VectorKeyNum
        << "\n"
        << "Quaternion Keys : "
        << header.QuaternionKeyNum
        << "\n"
        << "Length          : "
        << header.Length
        << " sec\n"
        << "File Size       : "
        << fileSize
        << "\n"
        << "========================================\n";


    return true;
}


//============================================================
// 全Animation Clip Export
//============================================================
bool AnimationWriter::WriteAll(
    const ImportedModel& model,
    const std::filesystem::path& modelOutputPath
)
{
    //========================================================
    // Animationなし
    //========================================================
    if (model.Animations.empty())
    {
        std::cout
            << "[AnimationWriter] "
            << "No animations.\n";

        return true;
    }


    //========================================================
    // Output Folder
    //========================================================
    const std::filesystem::path motionDirectory = modelOutputPath.parent_path() / "motion";

    std::error_code error;

    std::filesystem::create_directories(motionDirectory, error);

    if (error)
    {
        std::cerr
            << "[AnimationWriter] "
            << "Could not create motion directory.\n";

        return false;
    }

    //========================================================
    // Model Name
    // TestAnimModel.vnm
    //
    //      ↓
    // TestAnimModel
    //========================================================
    const std::string modelName = modelOutputPath.stem().string();


    std::cout
        << "\n========================================\n"
        << " Animation Export Start\n"
        << "========================================\n"
        << "Animations : "
        << model.Animations.size()
        << "\n";


    //========================================================
    // 全Clip
    //========================================================
    for (std::size_t animationIndex = 0; animationIndex < model.Animations.size(); ++animationIndex)
    {
        const ImportedAnimation& animation = model.Animations[animationIndex];

        std::string clipName = GetShortAnimationName(animation.Name);

        clipName = SanitizeFileName(clipName);

        //====================================================
        // Output File Name
        //
        // TestAnimModel_Idle.mot
        //====================================================
        const std::string fileName = modelName + "_" + clipName + ".mot";

        const std::filesystem::path outputPath = motionDirectory / fileName;

        //====================================================
        // Export
        //====================================================
        if (!WriteClip(animation, outputPath))
        {
            std::cerr
                << "[AnimationWriter] "
                << "Animation export failed.\n"
                << "Animation : "
                << animation.Name
                << "\n";

            return false;
        }


        //====================================================
        // Validation
        //====================================================
        if (!ValidateV2(outputPath))
        {
            std::cerr
                << "[AnimationWriter] "
                << "Animation validation failed.\n"
                << "File : "
                << outputPath.string()
                << "\n";

            return false;
        }
    }


    std::cout
        << "\n========================================\n"
        << " Animation Export Complete\n"
        << "========================================\n"
        << "Animations : "
        << model.Animations.size()
        << "\n"
        << "========================================\n";


    return true;
}
