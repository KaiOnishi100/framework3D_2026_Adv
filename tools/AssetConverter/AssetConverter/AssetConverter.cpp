#include <iostream>
#include <filesystem>
#include <Windows.h>
#include <objbase.h>

#include "importer/ModelImporter.h"
#include "importer/ImportedModel.h"

#include "texture/MaterialTexturePipeline.h"

#include "writer/ModelWriter.h"
#include "writer/BoneWriter.h"
#include "writer/AnimationWriter.h"

#include "processor/AssetScaleProcessor.h"

static void DebugPrintModelBounds(const ImportedModel& model)
{
    //========================================================
    // Vertexが1つでも存在したか
    //========================================================
    bool hasVertex = false;

    //========================================================
    // 初期値
    // Min: 非常に大きい値から開始
    // Max: 非常に小さい値から開始
    //========================================================
    DirectX::XMFLOAT3 minPosition =
    {
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)()
    };


    DirectX::XMFLOAT3 maxPosition =
    {
        (std::numeric_limits<float>::lowest)(),
        (std::numeric_limits<float>::lowest)(),
        (std::numeric_limits<float>::lowest)()
    };


    //========================================================
    // 全Mesh
    //========================================================
    for (const ImportedMesh& mesh : model.Meshes)
    {
        //====================================================
        // 全Vertex
        //====================================================
        for (const ImportedVertex& vertex : mesh.Vertices)
        {
            hasVertex =
                true;


            const DirectX::XMFLOAT3& position =
                vertex.Position;


            //================================================
            // Minimum
            //================================================
            minPosition.x = (std::min)(minPosition.x, position.x);
            minPosition.y = (std::min)(minPosition.y, position.y);
            minPosition.z = (std::min)(minPosition.z, position.z);


            //================================================
            // Maximum
            //================================================
            maxPosition.x = (std::max)(maxPosition.x, position.x);
            maxPosition.y = (std::max)(maxPosition.y, position.y);
            maxPosition.z = (std::max)(maxPosition.z, position.z);
        }
    }

    //========================================================
    // Vertexなし
    //========================================================
    if (!hasVertex)
    {
        std::cout
            << "[Model Bounds] No vertices.\n";

        return;
    }

    //========================================================
    // Full Size
    // Max - Min
    //========================================================
    const DirectX::XMFLOAT3 size =
    {
        maxPosition.x - minPosition.x,
        maxPosition.y - minPosition.y,
        maxPosition.z - minPosition.z
    };

    //========================================================
    // Center
    //========================================================
    const DirectX::XMFLOAT3 center =
    {
        (minPosition.x + maxPosition.x) * 0.5f,
        (minPosition.y + maxPosition.y) * 0.5f,
        (minPosition.z + maxPosition.z) * 0.5f
    };

    //========================================================
    // Debug Output
    //========================================================
    std::cout
        << "\n"
        << "========================================\n"
        << " Model Bounds\n"
        << "========================================\n"
        << "Min    : "
        << minPosition.x << ", "
        << minPosition.y << ", "
        << minPosition.z << "\n"
        << "Max    : "
        << maxPosition.x << ", "
        << maxPosition.y << ", "
        << maxPosition.z << "\n"
        << "Size   : "
        << size.x << ", "
        << size.y << ", "
        << size.z << "\n"
        << "Center : "
        << center.x << ", "
        << center.y << ", "
        << center.z << "\n"
        << "========================================\n";
}

//============================================================
// Main
//============================================================
int main(int argc, char* argv[])
{
    //========================================================
    // COM初期化
    // ※DirectXTexのWIC Loaderなどで必要
    //========================================================
    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);


    //========================================================
    // COM初期化失敗
    // ※RPC_E_CHANGED_MODEの場合はすでに別Apartment Modelで初期化済みなので許可する
    //========================================================
    if (FAILED(comHr) && comHr != RPC_E_CHANGED_MODE)
    {
        std::cerr
            << "[AssetConverter] COM Initialize Failed.\n"
            << "HRESULT = 0x"
            << std::hex
            << static_cast<unsigned long>(comHr)
            << std::dec
            << "\n";

        return 1;
    }


    //========================================================
    // CoUninitializeが必要か
    // S_OK: 
    //      今回COMを初期化した
    // S_FALSE:
    //      すでに同じApartmentで初期化済みだが、
    //      CoInitializeExの呼び出し回数は増えているため
    //      CoUninitializeが必要
    //========================================================
    const bool shouldUninitializeCOM = (comHr == S_OK || comHr == S_FALSE);

    //========================================================
    // 終了用Helper
    // 途中returnでもCoUninitializeを忘れないために使用する
    // return Finish(1);
    //========================================================
    const auto Finish = [shouldUninitializeCOM](int result)
        {
            if (shouldUninitializeCOM)
            {
                CoUninitialize();
            }

            return result;
        };


    //========================================================
    // Command Line Debug
    //========================================================
    std::cout
        << "argc = "
        << argc
        << "\n";


    for (int i = 0; i < argc; ++i)
    {
        std::cout
            << "argv["
            << i
            << "] = "
            << argv[i]
            << "\n";
    }


    //========================================================
    // Usage
    // AssetConverter.exe
    //     Input.fbx
    //     Output.vnm
    //========================================================
    if (argc < 3)
    {
        std::cout
            << "Usage:\n"
            << "  AssetConverter.exe "
            << "<input model> <output vnm> [--scale value]\n"
            << "\n"
            << "Example:\n"
            << "  AssetConverter.exe cube.fbx cube.vnm --scale 0.005\n";

        return Finish(0);
    }


    //========================================================
    // Input / Output Path
    //========================================================
    const std::filesystem::path inputPath = argv[1];

    const std::filesystem::path outputPath = argv[2];

    std::cout
        << "\n========================================\n"
        << " Start Asset Export\n"
        << "========================================\n";


    std::wcout
        << L"Input  : "
        << inputPath.wstring()
        << L"\n"
        << L"Output : "
        << outputPath.wstring()
        << L"\n";

    //============================================================
    // Import Settings
    // Default: 1.0 = Source Scaleをそのまま使用
    // --scale: Source AssetからEngine Meter空間への倍率
    //============================================================
    float importScale = 1.0f;


    //============================================================
    // Optional Arguments
    // argv[0] = AssetConverter.exe
    // argv[1] = input
    // argv[2] = output
    // argv[3...] = option
    //============================================================
    for (int argumentIndex = 3; argumentIndex < argc; ++argumentIndex)
    {
        const std::string argument = argv[argumentIndex];


        //========================================================
        // --scale 0.005
        //========================================================
        if (argument == "--scale")
        {
            //====================================================
            // 次のArgumentが必要
            //====================================================
            if (argumentIndex + 1 >= argc)
            {
                std::cerr
                    << "[AssetConverter] "
                    << "--scale requires a value.\n";

                return Finish(1);
            }


            try
            {
                importScale = std::stof(argv[++argumentIndex]);
            }
            catch (...)
            {
                std::cerr
                    << "[AssetConverter] "
                    << "Invalid --scale value.\n";

                return Finish(1);
            }


            //====================================================
            // Scale Validation
            //====================================================
            if (!std::isfinite(importScale) || importScale <= 0.0f)
            {
                std::cerr
                    << "[AssetConverter] "
                    << "Scale must be greater than 0.\n";

                return Finish(1);
            }
        }
        else
        {
            std::cerr
                << "[AssetConverter] "
                << "Unknown option : "
                << argument
                << "\n";

            return Finish(1);
        }
    }

    //========================================================
    // Intermediate Model
    // FBX / glTF / OBJ
    //        ↓
    // ImportedModel
    // AssetConverter内部では一度この形式へ統一する
    //========================================================
    ImportedModel model;


    //========================================================
    // Model Import
    //========================================================
    if (!ModelImporter::Import(inputPath, model))
    {
        std::cerr << "Import Failed.\n";

        return Finish(1);
    }

    if (!AssetScaleProcessor::Apply(model, importScale))
    {
        std::cerr << "Asset Scale Processing Failed.\n";
        return Finish(1);
    }

    DebugPrintModelBounds(model);

    if (!MaterialTexturePipeline::Process(model, outputPath))
    {
        std::cerr << "Texture Processing Failed.\n";

        return Finish(1);
    }

    //========================================================
    // Import結果
    // Static / Skinned判定をここで確認する
    //========================================================
    std::cout
        << "\n========================================\n"
        << " Import Result\n"
        << "========================================\n"
        << "Meshes         : "
        << model.Meshes.size()
        << "\n"
        << "Materials      : "
        << model.Materials.size()
        << "\n"
        << "Skeleton Nodes : "
        << model.Bones.size()
        << "\n"
        << "Skin Bones     : "
        << model.SkinBones.size()
        << "\n"
        << "Animations     : "
        << model.Animations.size()
        << "\n"
        << "Has Skinning   : "
        << (model.HasSkinning ? "Yes" : "No")
        << "\n"
        << "========================================\n";

    //============================================================
    // Animation Debug
    //============================================================
    for (std::size_t animationIndex = 0; animationIndex < model.Animations.size(); ++animationIndex)
    {
        const ImportedAnimation& animation = model.Animations[animationIndex];


        std::cout
            << "[Animation Result]\n"
            << "    Index    : "
            << animationIndex
            << "\n"
            << "    Name     : "
            << animation.Name
            << "\n"
            << "    Duration : "
            << animation.DurationSeconds
            << " sec\n"
            << "    TPS      : "
            << animation.TicksPerSecond
            << "\n"
            << "    Channels : "
            << animation.Channels.size()
            << "\n";
    }

    //========================================================
    // Skinned Model
    //========================================================
    if (model.HasSkinning)
    {
        //========================================================
        // Full Skeleton
        //      ↓
        // .bone
        //========================================================
        std::filesystem::path bonePath = outputPath;

        bonePath.replace_extension(L".bone");

        if (!BoneWriter::Write(model, bonePath))
        {
            std::cerr << "BONE Export Failed.\n";

            return Finish(1);
        }

        if (!BoneWriter::Validate(bonePath))
        {
            std::cerr << "BONE Validation Failed.\n";

            return Finish(1);
        }

        //========================================================
        // Skinned VNM V3
        //========================================================
        if (!ModelWriter::WriteSkinnedModelV3(model, outputPath))
        {
            std::cerr << "Skinned VNM Export Failed.\n";

            return Finish(1);
        }

        //============================================================
        // Skinned VNM Binary Validation
        //============================================================
        if (!ModelWriter::ValidateSkinnedModel(outputPath))
        {
            std::cerr << "Skinned VNM Validation Failed.\n";

            return Finish(1);
        }

        //========================================================
        // MOT V2
        //========================================================
        if (!model.Animations.empty())
        {
            if (!AnimationWriter::WriteAll(model, outputPath))
            {
                std::cerr << "Animation MOT Export Failed.\n";
                return Finish(1);
            }

        }


        return Finish(0);
    }


    //========================================================
    // Static Model
    // SofaなどBone/SkinWeightを持たないModelはこちら
    //========================================================
    std::cout
        << "\n========================================\n"
        << " Static Model Detected\n"
        << "========================================\n";


    //========================================================
    // ImportedModel
    //      ↓
    // VNM V2
    //========================================================
    if (!ModelWriter::WriteStaticModelV3(model, outputPath))
    {
        std::cerr << "VNM Export Failed.\n";

        return Finish(1);
    }

    //========================================================
    // 書き出したVNMをその場で再度開いて検証
    //========================================================
    if (!ModelWriter::ValidateStaticModel(outputPath))
    {
        std::cerr << "VNM Validation Failed.\n";

        return Finish(1);
    }

    //========================================================
    // Static Asset完了
    //========================================================
    std::cout
        << "\n========================================\n"
        << " Static Asset Export Success\n"
        << "========================================\n"
        << "VNM Export + Validation Success.\n"
        << "========================================\n";

    return Finish(0);
}
