#include "BoneWriter.h"

//============================================================
// Windows
//============================================================
#include <Windows.h>

//============================================================
// Standard Library
//============================================================
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>


namespace
{
    //============================================================
    // UTF-8
    //    ↓
    // UTF-16
    // ※AssimpのNode Nameはstd::stringなので.boneのWCHAR[32]へ変換する
    //============================================================
    static std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }


        //========================================================
        // 必要文字数を取得
        //========================================================
        const int length =
            MultiByteToWideChar(
                CP_UTF8,
                0,
                text.c_str(),
                -1,
                nullptr,
                0
            );

        if (length <= 0)
        {
            return {};
        }

        //========================================================
        // '\0'込みで確保
        //========================================================
        std::wstring result;

        result.resize(static_cast<std::size_t>(length));

        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            -1,
            result.data(),
            length
        );


        //========================================================
        // std::wstring内の末尾'\0'は不要なので削る
        //========================================================
        if (!result.empty() && result.back() == L'\0')
        {
            result.pop_back();
        }

        return result;
    }


    //============================================================
    // WCHAR[32]へ安全にコピー
    //
    // 最大31文字 + '\0'
    //============================================================
    static void CopyBoneName(WCHAR destination[32], const std::string& source)
    {
        //========================================================
        // 全体を0初期化
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
        // 31文字まで
        // 32文字目はNull Terminator用
        //========================================================
        wcsncpy_s(
            destination,
            32,
            wide.c_str(),
            31
        );
    }


    //============================================================
    // ImportedBone
    //      ↓
    // vnModel_BoneData
    //============================================================
    static vnModel_BoneData BuildBoneData(const ImportedBone& source)
    {
        vnModel_BoneData destination = {};


        //========================================================
        // Name
        //========================================================
        CopyBoneName(
            destination.Name,
            source.Name
        );


        //========================================================
        // Hierarchy
        //========================================================
        destination.ParentID = source.ParentIndex;

        destination.Generation = source.Generation;

        //========================================================
        // Record自身のByte Sizeを保存する
        //========================================================
        destination.StructSize = sizeof(vnModel_BoneData);

        //========================================================
        // Local Position
        //========================================================
        destination.pos[0] = source.Position.x;
        destination.pos[1] = source.Position.y;
        destination.pos[2] = source.Position.z;


        //========================================================
        // Local Euler Rotation
        // 単位: Radian
        // ImportedSkeleton作成時にQuaternionから
        // Eulerへ変換しておいた値を使う
        //========================================================
        destination.rot[0] = source.EulerRotation.x;
        destination.rot[1] = source.EulerRotation.y;
        destination.rot[2] = source.EulerRotation.z;

        //========================================================
        // Local Scale
        //========================================================
        destination.scl[0] = source.Scale.x;
        destination.scl[1] = source.Scale.y;
        destination.scl[2] = source.Scale.z;

        return destination;
    }

    //============================================================
    // ImportedBone
    //      ↓
    // vnModel_BoneDataV2
    // Assimp QuaternionをEulerへ変換せず保存する
    //============================================================
    static vnModel_BoneDataV2 BuildBoneDataV2(const ImportedBone& source)
    {
        vnModel_BoneDataV2 destination = {};


        //========================================================
        // Name
        //========================================================
        CopyBoneName(destination.Name, source.Name);


        //========================================================
        // Hierarchy
        //========================================================
        destination.ParentID = source.ParentIndex;

        destination.Generation = source.Generation;


        //========================================================
        // V2 Record Size
        //========================================================
        destination.StructSize = sizeof(vnModel_BoneDataV2);


        //========================================================
        // Position
        //========================================================
        destination.pos[0] = source.Position.x;
        destination.pos[1] = source.Position.y;
        destination.pos[2] = source.Position.z;

        //========================================================
        // Quaternionで保持
        //========================================================
        destination.rotQ[0] = source.Rotation.x;
        destination.rotQ[1] = source.Rotation.y;
        destination.rotQ[2] = source.Rotation.z;
        destination.rotQ[3] = source.Rotation.w;

        //========================================================
        // Scale
        //========================================================
        destination.scl[0] = source.Scale.x;
        destination.scl[1] = source.Scale.y;
        destination.scl[2] = source.Scale.z;

        return destination;
    }
}


//============================================================
// .bone Writer
//============================================================
bool BoneWriter::Write(const ImportedModel& model, const std::filesystem::path& outputPath)
{
    //========================================================
    // Skeletonなし
    //========================================================
    if (model.Bones.empty())
    {
        std::cerr << "[BoneWriter] Skeleton is empty.\n";

        return false;
    }

    //========================================================
    // Output Folder
    //========================================================
    const std::filesystem::path directory = outputPath.parent_path();

    if (!directory.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(directory, error);


        if (error)
        {
            std::cerr
                << "[BoneWriter] "
                << "Could not create output directory.\n";

            return false;
        }
    }


    //========================================================
    // File Open
    // .boneにはHeaderなし
    // [Bone0]
    // [Bone1]
    // [Bone2]
    // とvnModel_BoneDataを並べる
    //========================================================
    std::ofstream stream(outputPath, std::ios::binary | std::ios::trunc);


    if (!stream)
    {
        std::cerr
            << "[BoneWriter] "
            << "Could not open output file.\n";

        return false;
    }


    //========================================================
    // Boneを順番に出力
    // ImportedModel::BonesはDepth Firstで構築されているので
    // Parentは基本的にChildより前に存在する
    //========================================================
    for (std::size_t boneIndex = 0; boneIndex < model.Bones.size(); ++boneIndex)
    {
        const ImportedBone& source = model.Bones[boneIndex];

        const vnModel_BoneDataV2  destination = BuildBoneDataV2(source);

        stream.write(
            reinterpret_cast<const char*>(&destination),
            sizeof(destination)
        );


        if (!stream.good())
        {
            std::cerr
                << "[BoneWriter] "
                << "Write Failed. BoneIndex = "
                << boneIndex
                << "\n";

            return false;
        }
    }


    stream.close();


    //========================================================
    // Debug
    //========================================================
    std::wcout
        << L"\n========================================\n"
        << L" BONE Export Success\n"
        << L"========================================\n"
        << L"File     : "
        << outputPath.wstring()
        << L"\n"
        << L"BoneNum  : "
        << model.Bones.size()
        << L"\n"
        << L"BoneSize : "
        << sizeof(vnModel_BoneDataV2)
        << L"\n"
        << L"FileSize : "
        << model.Bones.size() * sizeof(vnModel_BoneDataV2)
        << L"\n"
        << L"========================================\n";

    return true;
}

bool BoneWriter::Validate(const std::filesystem::path& filePath)
{
    //========================================================
    // File Open
    //========================================================
    std::ifstream stream(filePath, std::ios::binary | std::ios::ate);


    if (!stream)
    {
        return false;
    }


    //========================================================
    // File Size
    //========================================================
    const std::streamsize fileSize = stream.tellg();


    if (fileSize <= 0 || fileSize % sizeof(vnModel_BoneDataV2) != 0)
    {
        std::cerr
            << "[Bone Validate] "
            << "Invalid file size.\n";

        return false;
    }


    const std::size_t boneCount = static_cast<std::size_t>(fileSize / sizeof(vnModel_BoneDataV2));

    stream.seekg(0, std::ios::beg);

    std::vector<vnModel_BoneDataV2> bones(boneCount);

    stream.read(reinterpret_cast<char*>(bones.data()), fileSize);

    if (!stream)
    {
        return false;
    }

    //========================================================
    // Bone単位Validation
    //========================================================
    for (std::size_t i = 0; i < boneCount; ++i)
    {
        const vnModel_BoneDataV2& bone = bones[i];

        //====================================================
        // Struct Size
        //====================================================
        if (bone.StructSize != sizeof(vnModel_BoneDataV2))
        {
            std::cerr
                << "[Bone Validate] "
                << "Invalid StructSize. Bone = "
                << i
                << "\n";

            return false;
        }

        //====================================================
        // Parent
        // Root: -1
        // Child: 0 ～ BoneNum-1
        //====================================================
        if (bone.ParentID < -1 || bone.ParentID >= static_cast<int>(boneCount))
        {
            std::cerr
                << "[Bone Validate] "
                << "Invalid ParentID. Bone = "
                << i
                << "\n";

            return false;
        }


        //====================================================
        // Depth Firstで作成しているので
        // Parentは現在Boneより前に存在するはず
        //====================================================
        if (bone.ParentID >= static_cast<int>(i))
        {
            std::cerr
                << "[Bone Validate] "
                << "Parent must appear before child. Bone = "
                << i
                << "\n";

            return false;
        }


        //====================================================
        // RootならGeneration 0
        //====================================================
        if (bone.ParentID == -1 && bone.Generation != 0)
        {
            std::cerr
                << "[Bone Validate] "
                << "Invalid root generation.\n";

            return false;
        }
    }


    std::cout
        << "\n========================================\n"
        << " BONE Validation Success\n"
        << "========================================\n"
        << "Bones    : "
        << boneCount
        << "\n"
        << "BoneSize : "
        << sizeof(vnModel_BoneDataV2)
        << "\n"
        << "FileSize : "
        << fileSize
        << "\n"
        << "========================================\n";

    return true;
}
