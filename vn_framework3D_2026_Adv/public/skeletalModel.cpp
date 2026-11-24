#include "../framework.h"
#include "../framework/vn_environment.h"

#include "skeletalModel.h"

//============================================================
// Constructor
//============================================================
SkeletalModel::SkeletalModel(
    const WCHAR* folder,
    const WCHAR* modelFile,
    const WCHAR* boneFile
) : ObjectBase()
{
    //========================================================
    // .bone Path
    //========================================================
    WCHAR bonePath[256] = {};


    swprintf_s(
        bonePath,
        L"%s%s",
        folder,
        boneFile
    );


    //========================================================
    // .bone Load
    //========================================================
    FILE* file = nullptr;


    if (_wfopen_s(&file, bonePath, L"rb") != 0)
    {
        vnFont::output(
            L"[SkeletalModel] cannot open bone file \"%s\"\n",
            bonePath
        );


        assert(false);

        return;
    }


    //========================================================
    // File Size
    //========================================================
    fseek(file, 0, SEEK_END);

    const long fileSize = ftell(file);

    fseek(file, 0, SEEK_SET);

    if (fileSize <= 0 ||
        fileSize %
        sizeof(vnModel_BoneDataV2)
        != 0)
    {
        fclose(file);

        vnFont::output(L"[SkeletalModel] invalid bone file.\n");

        assert(false);

        return;
    }


    //========================================================
    // Bone Count
    //========================================================
    mBoneCount =
        static_cast<int>(fileSize / sizeof(vnModel_BoneDataV2));


    //========================================================
    // Bone Data確保
    //========================================================
    mBoneData = new vnModel_BoneDataV2[mBoneCount];


    //========================================================
    // Read
    //========================================================
    const size_t readCount =
        fread(
            mBoneData,
            sizeof(vnModel_BoneDataV2),
            mBoneCount,
            file
        );


    fclose(file);


    if (readCount != static_cast<size_t>(mBoneCount))
    {
        vnFont::output(L"[SkeletalModel] Bone read failed.\n");


        assert(false);

        return;
    }


    //========================================================
    // Debug
    //========================================================
    vnFont::output(L"[SkeletalModel] BoneCount = %d\n", mBoneCount);

    //========================================================
    // Bone Object生成
    //========================================================
    mBones = new ObjectBase * [mBoneCount];


    for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
    {
        mBones[boneIndex] = new ObjectBase();
    }


    //========================================================
    // Bone Hierarchy構築
    //========================================================
    for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
    {
        const vnModel_BoneDataV2& bone = mBoneData[boneIndex];


        //====================================================
        // Parent
        //====================================================
        if (bone.ParentID >= 0)
        {
            assert(bone.ParentID < boneIndex);

            mBones[boneIndex]->setParent(mBones[bone.ParentID]);
        }
        else
        {
            //================================================
            // Root Bone
            //================================================
            mBones[boneIndex]->setParent(this);
        }
    }


    //========================================================
    // Bind Pose設定
    //========================================================
    BindPose();


    //========================================================
    // Bone Matrix
    //========================================================
    mBoneMatrices = new XMMATRIX[mBoneCount];


    for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
    {
        mBoneMatrices[boneIndex] = XMMatrixIdentity();
    }


    //========================================================
    // Skinned VNM Load
    //========================================================
    mModel = new ModelBase(folder, modelFile);
    
    //if (mModel != nullptr)
    //{
    //    const int materialCount = mModel->getMaterialNum();

    //    for (int materialIndex = 0; materialIndex < materialCount; ++materialIndex)
    //    {
    //        //====================================================
    //        // 完全不透明な白
    //        //====================================================
    //        mModel->setDiffuse(
    //            1.0f,
    //            1.0f,
    //            1.0f,
    //            1.0f,
    //            materialIndex
    //        );


    //        //====================================================
    //        // PBRも一旦OFF
    //        //
    //        // Lighting問題も切り離す
    //        //====================================================
    //        mModel->SetPBREnabled(
    //            false,
    //            materialIndex
    //        );
    //    }
    //}

    //========================================================
    // VNM側にもSkin Boneが存在するか確認
    //========================================================
    if (mModel != nullptr && mModel->getBoneNum() <= 0)
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Loaded VNM has no skin bones.\n"
        );
    }


    //mMotion = nullptr;

    mMotionV2 = nullptr;

    mTime = 0.0f;

    mPlaySpeed = 1.0f;

    mPlaying = false;

    mLoop = true;

    mBlending = false;

    mBlendSourceMotion = nullptr;

    mBlendTime = 0.0f;

    mBlendDuration = 0.0f;
}

SkeletalModel::~SkeletalModel()
{
    //========================================================
    // Motion
    //========================================================
    UnloadMotion();

    //========================================================
    // Model
    //========================================================
    if (mModel != nullptr)
    {
        vnScene::deleteObject(mModel);

        mModel = nullptr;
    }


    //========================================================
    // Bone Objects
    //========================================================
    if (mBones != nullptr)
    {
        for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
        {
            if (mBones[boneIndex] != nullptr)
            {
                delete mBones[boneIndex];
                mBones[boneIndex] = nullptr;
            }
        }


        delete[] mBones;

        mBones = nullptr;
    }


    //========================================================
    // Bone Matrix
    //========================================================
    delete[] mBoneMatrices;

    mBoneMatrices = nullptr;


    //========================================================
    // .bone Binary
    //========================================================
    delete[] mBoneData;

    mBoneData = nullptr;

    mBoneCount = 0;

    mMotionV2 = nullptr;

    mMotionClips.clear();

    mCurrentMotionName.clear();


    //========================================================
    // OneShot予約も解除
    //========================================================
    ClearQueuedMotion();


    //========================================================
    // Blend状態
    //========================================================
    mBlendSourceMotion = nullptr;

    mBlendSourceTime = 0.0f;

    mBlendTime = 0.0f;

    mBlendDuration = 0.0f;

    mBlending = false;


    //========================================================
    // Playback
    //========================================================
    mTime = 0.0f;

    mPlaying = false;
}

void SkeletalModel::execute(float dt)
{
    //========================================================
    // Motionなし
    //========================================================
    if (mMotionV2 == nullptr)
    {
        return;
    }


    //========================================================
    // Pause中でも現在Poseは維持する
    //========================================================
    if (!mPlaying)
    {
        return;
    }


    //========================================================
    // Animation Time用Delta
    //========================================================
    const float animationDelta =
        dt * mPlaySpeed;


    //========================================================
    // Destination Motion更新
    //========================================================
    const bool destinationEnded =
        AdvanceMotionTime(
            mMotionV2,
            mTime,
            mLoop,
            animationDelta
        );


    //========================================================
    // Cross Fade中
    //========================================================
    if (mBlending && mBlendSourceMotion != nullptr)
    {
        //====================================================
        // Source側Animationも進める
        //====================================================
        AdvanceMotionTime(
            mBlendSourceMotion,
            mBlendSourceTime,
            mBlendSourceLoop,
            animationDelta
        );


        //====================================================
        // Blend時間は実時間で進める
        //====================================================
        mBlendTime += std::fabs(dt);


        //====================================================
        // Blend Pose
        //====================================================
        ApplyBlendedMotionPose();


        //====================================================
        // Blend終了
        //====================================================
        if (mBlendTime >= mBlendDuration)
        {
            mBlending = false;

            mBlendSourceMotion = nullptr;

            //================================================
            // Destinationを100%適用
            //================================================
            ApplyMotionPose();

            //================================================
            // Destination自体も終了していた場合
            //================================================
            if (destinationEnded && !mLoop)
            {
                mPlaying = false;
            }
        }

        return;
    }


    //========================================================
    // 通常Animation
    //========================================================
    ApplyMotionPose();


    //============================================================
    // NonLoop Animation終了
    //============================================================
    if (destinationEnded && !mLoop)
    {
        //========================================================
        // QueueされたMotionがあれば、
        // Animation終了PoseからNext MotionへCross Fadeする
        //========================================================
        if (!HandleMotionFinished())
        {
            //====================================================
            // Next Motionなし
            // 最終Poseで停止する
            //====================================================
            mPlaying = false;
        }
    }
}

void SkeletalModel::postExecute()
{
    ObjectBase::postExecute();


    if (mModel == nullptr || mBones == nullptr || mBoneMatrices == nullptr)
    {
        return;
    }


    //========================================================
    // Full Skeleton更新
    //========================================================
    for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
    {
        mBones[boneIndex]->postExecute();


        //====================================================
        // 更新済みFull Skeleton World Matrix
        //====================================================
        mBoneMatrices[boneIndex] = *mBones[boneIndex]->getWorld();
    }

    mModel->setSkinMatrix(mBoneMatrices);

    static bool debugOnce =
        false;


    if (!debugOnce)
    {
        for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
        {
            const XMMATRIX& world =
                mBoneMatrices[boneIndex];


            XMFLOAT4X4 matrix = {};

            XMStoreFloat4x4(
                &matrix,
                world
            );


            wchar_t buffer[256] = {};


            swprintf_s(
                buffer,
                L"[Bone World] %d "
                L"Position=(%.3f, %.3f, %.3f)\n",
                boneIndex,
                matrix._41,
                matrix._42,
                matrix._43
            );


            OutputDebugStringW(buffer);
        }

        debugOnce = true;
    }

    mModel->SetShadowBoundsWorld(*getWorld());
}

void SkeletalModel::render()
{

}

void SkeletalModel::RegisterObject(vnScene* scene)
{
    if (scene == nullptr)
    {
        return;
    }

    //========================================================
    // Skeleton Root
    //========================================================
    scene->registerObject(this);

    //========================================================
    // 描画対象Model
    //========================================================
    if (mModel != nullptr)
    {
        scene->registerObject(mModel);
    }
}

void SkeletalModel::BindPose()
{
    if (mBones == nullptr || mBoneData == nullptr)
    {
        return;
    }


    for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
    {
        const vnModel_BoneDataV2& bone = mBoneData[boneIndex];


        ObjectBase* boneObject = mBones[boneIndex];


        //====================================================
        // Local Position
        //====================================================
        boneObject->setPosition(
            bone.pos[0],
            bone.pos[1],
            bone.pos[2]
        );


        //====================================================
        // Local Scale
        //====================================================
        boneObject->setScale(
            bone.scl[0],
            bone.scl[1],
            bone.scl[2]
        );


        //====================================================
        // Assimpから保存したQuaternionを直接復元
        //====================================================
        XMVECTOR quaternion =
            XMVectorSet(
                bone.rotQ[0],
                bone.rotQ[1],
                bone.rotQ[2],
                bone.rotQ[3]
            );


        //====================================================
        // 浮動小数誤差対策としてNormalize
        //====================================================
        quaternion = XMQuaternionNormalize(quaternion);

        boneObject->setQuaternion(&quaternion);

        boneObject->useQuaternion(true);
    }
}

ObjectBase* SkeletalModel::GetBone(int boneIndex) const
{
    //========================================================
    // Bone配列が無い
    //========================================================
    if (mBones == nullptr)
    {
        return nullptr;
    }

    //========================================================
    // Index範囲確認
    //========================================================
    if (boneIndex < 0 || boneIndex >= mBoneCount)
    {
        return nullptr;
    }

    return mBones[boneIndex];
}

int SkeletalModel::FindBoneIndex(const WCHAR* boneName) const
{
    //========================================================
    // 引数・Skeletonの検証
    //========================================================
    if (boneName == nullptr || boneName[0] == L'\0' || mBoneData == nullptr)
    {
        return -1;
    }

    //========================================================
    // Full Skeletonから名前を検索
    //========================================================
    for (int i = 0; i < mBoneCount; ++i)
    {
        //====================================================
        // 完全一致したBoneのIndexを返す
        //====================================================
        if (wcscmp(mBoneData[i].Name, boneName) == 0)
        {
            return i;
        }
    }

    return -1;
}

bool SkeletalModel::GetCurrentBoneWorldMatrix(int boneIndex, XMMATRIX& outWorld)
{
    World = XMMatrixIdentity();

    //========================================================
    // Bone Index / データ検証
    //========================================================
    if (mBones == nullptr || mBoneData == nullptr || boneIndex < 0 || boneIndex >= mBoneCount)
    {
        return false;
    }

    //========================================================
    // vnObjectの現在のLocal Transformから行列を作成
    // BoneとPlayer Rootの両方で使用する
    //========================================================
    const auto buildLocalMatrix = [](ObjectBase* object)
        {
            const XMVECTOR position = *object->getPosition();
            const XMVECTOR scale = *object->getScale();

            const XMVECTOR rotation = XMQuaternionNormalize(*object->getQuaternion());

            const XMMATRIX scaleMatrix =
                XMMatrixScalingFromVector(scale);

            const XMMATRIX rotationMatrix =
                XMMatrixRotationQuaternion(rotation);

            const XMMATRIX translationMatrix =
                XMMatrixTranslationFromVector(position);

            return scaleMatrix * rotationMatrix * translationMatrix;
        };

    //========================================================
    // 対象BoneからRoot Boneまで遡る
    // Child Local * Parent Local * ...
    //========================================================
    XMMATRIX boneWorld = XMMatrixIdentity();

    int currentIndex = boneIndex;

    for (int depth = 0; depth < mBoneCount; ++depth)
    {
        //====================================================
        // Root Boneより上まで到達
        //====================================================
        if (currentIndex < 0)
        {
            break;
        }

        //====================================================
        // 不正なParent Index対策
        //====================================================
        if (currentIndex >= mBoneCount || mBones[currentIndex] == nullptr)
        {
            return false;
        }

        //====================================================
        // 現在のBone Local Transformを掛ける
        //====================================================
        boneWorld *= buildLocalMatrix(mBones[currentIndex]);

        //====================================================
        // 親Boneへ移動
        //====================================================
        currentIndex = mBoneData[currentIndex].ParentID;
    }

    //========================================================
    // 循環したHierarchyなどの異常を検出
    //========================================================
    if (currentIndex >= 0)
    {
        return false;
    }

    //========================================================
    // Player Root Transform
    //========================================================
    const XMMATRIX playerWorld = buildLocalMatrix(this);

    //========================================================
    // Full Skeleton Bone World Matrix
    //========================================================
    outWorld = boneWorld * playerWorld;

    return true;
}

//============================================================
// Debug Print Bone Names
//============================================================
void SkeletalModel::DebugPrintBoneNames() const
{
#if defined(_DEBUG)

    if (mBoneData == nullptr)
    {
        return;
    }

    //========================================================
    // Visual StudioのOutput Windowへ出力
    //========================================================
    for (int i = 0; i < mBoneCount; ++i)
    {
        wchar_t buffer[256] = {};

        swprintf_s(
            buffer,
            L"[Skeleton Bone] "
            L"Index=%d Name=%ls Parent=%d\n",
            i,
            mBoneData[i].Name,
            mBoneData[i].ParentID
        );

        OutputDebugStringW(buffer);
    }

#endif
}

std::unique_ptr<MotionDataV2> SkeletalModel::LoadMotionFileV2(const WCHAR* motionFile) 
{
    //========================================================
    // Path確認
    //========================================================
    if (motionFile == nullptr)
    {
        return nullptr;
    }

    //========================================================
    // File Open
    //========================================================
    std::ifstream stream(
        motionFile,
        std::ios::binary |
        std::ios::ate
    );


    if (!stream)
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Cannot open MOT V2.\n"
            L"File : %s\n",
            motionFile
        );

        return nullptr;
    }


    //========================================================
    // File Size
    //========================================================
    const std::streamoff fileSize = stream.tellg();


    if (fileSize < static_cast<std::streamoff>(sizeof(vnMotionDataV2)))
    {
        vnFont::output(
            L"[SkeletalModel] MOT file is too small.\n"
        );

        return nullptr;
    }


    stream.seekg(
        0,
        std::ios::beg
    );


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
        return nullptr;
    }


    //========================================================
    // Version Check
    //========================================================
    if (header.Version != vnMOTION_DATA_V2_VER)
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Unsupported MOT Version.\n"
        );

        return nullptr;
    }


    //========================================================
    // Header Size
    //========================================================
    if (header.HeaderSize != sizeof(vnMotionDataV2))
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Invalid MOT HeaderSize.\n"
        );

        return nullptr;
    }


    //========================================================
    // File Layoutを検証
    //========================================================
    const std::uint64_t expectedTrackAccess = sizeof(vnMotionDataV2);


    const std::uint64_t expectedVectorAccess =
        expectedTrackAccess +
        static_cast<std::uint64_t>(header.TrackNum) *
        sizeof(vnMotionData_TrackV2);


    const std::uint64_t expectedQuaternionAccess =
        expectedVectorAccess +
        static_cast<std::uint64_t>(header.VectorKeyNum) *
        sizeof(vnMotionData_VectorKeyV2);


    const std::uint64_t expectedFileSize =
        expectedQuaternionAccess +
        static_cast<std::uint64_t>(header.QuaternionKeyNum) *
        sizeof(vnMotionData_QuaternionKeyV2);


    //========================================================
    // Offset Check
    //========================================================
    if (header.TrackAccess != expectedTrackAccess ||
        header.VectorKeyAccess != expectedVectorAccess ||
        header.QuaternionKeyAccess != expectedQuaternionAccess)
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Invalid MOT offsets.\n"
        );

        return nullptr;
    }


    //========================================================
    // File Size Check
    //========================================================
    if (expectedFileSize != static_cast<std::uint64_t>(fileSize))
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Invalid MOT file size.\n"
        );

        return nullptr;
    }


    //========================================================
    // Runtime Motion作成
    //========================================================
    auto motion = std::make_unique<MotionDataV2>();

    motion->Header = header;


    motion->Tracks.resize(header.TrackNum);

    motion->VectorKeys.resize(header.VectorKeyNum);

    motion->QuaternionKeys.resize(header.QuaternionKeyNum);


    //========================================================
    // Track
    //========================================================
    stream.seekg(
        header.TrackAccess,
        std::ios::beg
    );


    if (!motion->Tracks.empty())
    {
        stream.read(
            reinterpret_cast<char*>(motion->Tracks.data()),
            static_cast<std::streamsize>(sizeof(vnMotionData_TrackV2) * motion->Tracks.size())
        );
    }


    //========================================================
    // Vector Keys
    //========================================================
    stream.seekg(
        header.VectorKeyAccess,
        std::ios::beg
    );


    if (!motion->VectorKeys.empty())
    {
        stream.read(
            reinterpret_cast<char*>(motion->VectorKeys.data()),
            static_cast<std::streamsize>(sizeof(vnMotionData_VectorKeyV2) * motion->VectorKeys.size())
        );
    }


    //========================================================
    // Quaternion Keys
    //========================================================
    stream.seekg(
        header.QuaternionKeyAccess,
        std::ios::beg
    );


    if (!motion->QuaternionKeys.empty())
    {
        stream.read(
            reinterpret_cast<char*>(motion->QuaternionKeys.data()),
            static_cast<std::streamsize>(sizeof(vnMotionData_QuaternionKeyV2) * motion->QuaternionKeys.size())
        );
    }


    if (!stream)
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"MOT data read failed.\n"
        );

        return nullptr;
    }


    //========================================================
    // Track Validation
    // MOTのSkeletonIndexはConverter側で
    // .boneのFull Skeleton Indexへ変換済み
    // ここでRuntimeのBone数と一致するか確認する
    //========================================================
    for (UINT trackIndex = 0; trackIndex < header.TrackNum; ++trackIndex)
    {
        vnMotionData_TrackV2& track = motion->Tracks[trackIndex];

        //========================================================
        // Struct Size
        //========================================================
        if (track.StructSize != sizeof(vnMotionData_TrackV2))
        {
            vnFont::output(
                L"[SkeletalModel] "
                L"Invalid MOT Track StructSize.\n"
            );

            return nullptr;
        }


        //========================================================
        // Key Range Validation
        //========================================================
        if (static_cast<std::uint64_t>(track.PositionStartIndex) + track.PositionKeyNum > header.VectorKeyNum)
        {
            return nullptr;
        }


        if (static_cast<std::uint64_t>(track.ScaleStartIndex) + track.ScaleKeyNum > header.VectorKeyNum)
        {
            return nullptr;
        }


        if (static_cast<std::uint64_t>(track.RotationStartIndex) + track.RotationKeyNum > header.QuaternionKeyNum)
        {
            return nullptr;
        }


        //========================================================
        // Runtime Bone Index
        // まずConverterが保存したIndexを候補にする
        //========================================================
        int runtimeBoneIndex = -1;


        //========================================================
        // 保存IndexがRuntimeでもそのまま使える
        //========================================================
        if (track.SkeletonIndex >= 0 && track.SkeletonIndex < mBoneCount)
        {
            const vnModel_BoneDataV2& indexedBone = mBoneData[track.SkeletonIndex];


            //====================================================
            // IndexもNameも一致
            //====================================================
            if (wcscmp(track.Name, indexedBone.Name) == 0)
            {
                runtimeBoneIndex = track.SkeletonIndex;
            }
        }


        //========================================================
        // Indexが一致しない
        // Bone NameでFull Skeletonを検索する
        // ※.motにはNameも保存しているので
        // ※Runtime Skeletonへ安全にRemapできる
        //========================================================
        if (runtimeBoneIndex < 0)
        {
            for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
            {
                const vnModel_BoneDataV2& bone = mBoneData[boneIndex];


                if (wcscmp(track.Name, bone.Name) == 0)
                {
                    runtimeBoneIndex = boneIndex;

                    break;
                }
            }
        }


        //========================================================
        // 同名Boneすら存在しない
        //========================================================
        if (runtimeBoneIndex < 0)
        {
            vnFont::output(
                L"[SkeletalModel] "
                L"MOT Bone not found in runtime skeleton.\n"
                L"TrackIndex : %u\n"
                L"MotionBone : %s\n",
                trackIndex,
                track.Name
            );

            return nullptr;
        }


        //========================================================
        // Indexが変わった場合はDebug表示
        //========================================================
        if (runtimeBoneIndex != track.SkeletonIndex)
        {
            vnFont::output(
                L"[SkeletalModel] "
                L"MOT Bone Remap.\n"
                L"Name : %s\n"
                L"MOT  : %d\n"
                L"Bone : %d\n",
                track.Name,
                track.SkeletonIndex,
                runtimeBoneIndex
            );
        }

        track.SkeletonIndex = runtimeBoneIndex;
    }

    vnFont::output(
        L"[SkeletalModel] MOT V2 Loaded.\n"
        L"Tracks = %u\n"
        L"Length = %.3f sec\n",
        motion->Header.TrackNum,
        motion->Header.Length
    );

    return motion;
}

bool SkeletalModel::LoadMotionClip(const std::wstring& clipName, const WCHAR* motionFile)
{
    //========================================================
    // 名前なしClipは禁止
    //========================================================
    if (clipName.empty())
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Motion clip name is empty.\n"
        );

        return false;
    }


    //========================================================
    // File Load
    // ・Header検証
    // ・Key Range検証
    // ・Skeleton Name Remap
    //========================================================
    std::unique_ptr<MotionDataV2> motion = LoadMotionFileV2(motionFile);


    if (!motion)
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Motion clip load failed.\n"
            L"Clip : %s\n",
            clipName.c_str()
        );

        return false;
    }


    //========================================================
    // 現在再生中のClipを同名で置き換える場合への保険
    //========================================================
    const bool replacingCurrent = (mCurrentMotionName == clipName);


    //========================================================
    // Mapへ登録
    // 同名が存在する場合は新しいMotionへ置換する
    //========================================================
    mMotionClips[clipName] = std::move(motion);


    //========================================================
    // 現在再生中だったClipを置換した場合は
    // Pointerも新しいDataへ更新する
    //========================================================
    if (replacingCurrent)
    {
        mMotionV2 = mMotionClips[clipName].get();
    }


    vnFont::output(
        L"[SkeletalModel] "
        L"Motion Clip Loaded : %s\n",
        clipName.c_str()
    );


    return true;

}

void SkeletalModel::UnloadMotion()
{
    mMotionV2 = nullptr;
    mBlendSourceMotion = nullptr;

    mMotionClips.clear();
    mCurrentMotionName.clear();

    mPlaying = false;
    mBlending = false;

    mTime = 0.0f;
    mBlendSourceTime = 0.0f;
    mBlendTime = 0.0f;
    mBlendDuration = 0.0f;
}

void SkeletalModel::PlayMotion(bool restart)
{
    if (mMotionV2 == nullptr)
    {
        return;
    }

    if (restart)
    {
        mTime = 0.0f;
        //====================================================
        // 0秒Poseを即座に適用
        //====================================================
        ApplyMotionPose();
    }

    mPlaying = true;
}

void SkeletalModel::PauseMotion()
{
    mPlaying = false;
}

void SkeletalModel::StopMotion()
{
    mPlaying = false;
    mTime = 0.0f;

    BindPose();
}

bool SkeletalModel::SetMotion(const std::wstring& clipName, bool restart, bool loop)
{
    //========================================================
    // 登録済みMotionを検索
    //========================================================
    const auto iterator = mMotionClips.find(clipName);


    //========================================================
    // 未ロード
    //========================================================
    if (iterator == mMotionClips.end())
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Motion Clip not found : %s\n",
            clipName.c_str()
        );

        return false;
    }

    //========================================================
    // OneShotのAuto Return予約をキャンセル
    //========================================================
    ClearQueuedMotion();

    //========================================================
    // 同じMotionが既に再生中で、
    // restartも要求されていない場合は
    // Timeをリセットしない
    //========================================================
    if (mMotionV2 == iterator->second.get() && !restart)
    {
        mPlaying = true;

        mLoop = loop;

        return true;
    }


    //========================================================
    // 再生対象変更
    //========================================================
    mMotionV2 = iterator->second.get();


    mCurrentMotionName = clipName;


    mLoop = loop;


    //========================================================
    // 現段階ではMotion変更時は基本0秒へ戻す
    //========================================================
    if (restart)
    {
        mTime = 0.0f;
    }
    else
    {
        //====================================================
        // 新しいMotionのLengthを超えていた場合だけClamp
        //====================================================
        const float length = mMotionV2->Header.Length;


        if (length > 0.0f)
        {
            mTime = std::clamp(mTime, 0.0f, length);
        }
    }


    //========================================================
    // 変更直後からPoseを反映
    //========================================================
    ApplyMotionPose();


    //========================================================
    // 再生開始
    //========================================================
    mPlaying = true;


    vnFont::output(
        L"[SkeletalModel] "
        L"Motion Changed : %s\n",
        clipName.c_str()
    );


    return true;
}

bool SkeletalModel::HasMotionClip(const std::wstring& clipName) const
{
    return mMotionClips.find(clipName) != mMotionClips.end();
}

bool SkeletalModel::BlendToMotion(const std::wstring& clipName, float blendDuration, bool restart, bool loop)
{
    //========================================================
    // Destination検索
    //========================================================
    const auto iterator = mMotionClips.find(clipName);


    if (iterator == mMotionClips.end())
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Blend Motion not found : %s\n",
            clipName.c_str()
        );

        return false;
    }

    ClearQueuedMotion();

    MotionDataV2* destinationMotion = iterator->second.get();


    //========================================================
    // Current Motionなし
    // Blend元が存在しないので通常切り替え
    //========================================================
    if (mMotionV2 == nullptr)
    {
        return SetMotion(
            clipName,
            restart,
            loop
        );
    }


    //========================================================
    // Blend時間0ならHard Cut
    //========================================================
    if (blendDuration <= 0.000001f)
    {
        return SetMotion(
            clipName,
            restart,
            loop
        );
    }


    //========================================================
    // 現在のMotionをSourceとして保存
    //========================================================
    mBlendSourceMotion = mMotionV2;

    mBlendSourceTime = mTime;

    mBlendSourceLoop = mLoop;


    //========================================================
    // Destination Motion
    //========================================================
    mMotionV2 = destinationMotion;

    mCurrentMotionName = clipName;

    mLoop = loop;


    //========================================================
    // Destination Time
    //========================================================
    if (restart)
    {
        mTime = 0.0f;
    }
    else
    {
        const float length = mMotionV2->Header.Length;
        mTime = std::clamp(mTime, 0.0f, length);
    }


    //========================================================
    // Blend開始
    //========================================================
    mBlendDuration = blendDuration;
    mBlendTime = 0.0f;

    mBlending = true;
    mPlaying = true;


    //========================================================
    // Blend Rate = 0のPoseを即適用
    // Motion切り替えを呼んだ瞬間にPoseが跳ぶのを防ぐ
    //========================================================
    ApplyBlendedMotionPose();


    //vnFont::output(
    //    L"[SkeletalModel] "
    //    L"Motion Blend : %s "
    //    L"(%.3f sec)\n",
    //    clipName.c_str(),
    //    blendDuration
    //);

    return true;
}

bool SkeletalModel::PlayOneShot(const std::wstring& clipName,
    const std::wstring& returnMotionName,
    float blendInDuration,
    float blendOutDuration,
    bool returnMotionLoop)
{
    //========================================================
    // 再生するMotionが存在するか
    //========================================================
    if (!HasMotionClip(clipName))
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"OneShot Motion not found : %s\n",
            clipName.c_str()
        );

        return false;
    }


    //========================================================
    // 戻り先Motionが存在するか
    //========================================================
    if (!HasMotionClip(returnMotionName))
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Return Motion not found : %s\n",
            returnMotionName.c_str()
        );

        return false;
    }


    //========================================================
    // OneShot Animationへ切り替える
    //========================================================
    if (!BlendToMotion(
        clipName,
        blendInDuration,
        true,
        false
    ))
    {
        return false;
    }


    //========================================================
    // Animation終了後の戻り先を登録
    //========================================================
    mQueuedMotionName = returnMotionName;

    mQueuedMotionBlendDuration = (std::max)(blendOutDuration, 0.0f);

    mQueuedMotionLoop = returnMotionLoop;

    mHasQueuedMotion = true;

    vnFont::output(
        L"[SkeletalModel] "
        L"OneShot : %s -> %s\n",
        clipName.c_str(),
        returnMotionName.c_str()
    );

    return true;
}

void SkeletalModel::ApplyMotionPose()
{
    if (mMotionV2 == nullptr)
    {
        return;
    }

    //========================================================
    // Current AnimationからPose生成
    //========================================================
    BuildMotionPose(
        mMotionV2,
        mTime,
        mDestinationPose
    );

    //========================================================
    // Skeletonへ反映
    //========================================================
    ApplyPoseToBones(mDestinationPose);
}

bool SkeletalModel::SampleVectorKeys(const MotionDataV2* motion, UINT startIndex, UINT keyCount, float time, DirectX::XMFLOAT3& output) const
{
    //========================================================
    // Motionなし
    //========================================================
    if (motion == nullptr)
    {
        return false;
    }


    //========================================================
    // Keyなし
    //========================================================
    if (keyCount == 0)
    {
        return false;
    }

    const std::vector<vnMotionData_VectorKeyV2>& keys = motion->VectorKeys;

    //========================================================
    // Range Safety Check
    // startIndex + keyCount がVector配列を超えないか確認
    //========================================================
    const std::uint64_t endIndex =
        static_cast<std::uint64_t>(startIndex) +
        static_cast<std::uint64_t>(keyCount);


    if (endIndex > keys.size())
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Vector Key Range Error.\n"
            L"Start    : %u\n"
            L"Count    : %u\n"
            L"KeySize  : %zu\n",
            startIndex,
            keyCount,
            keys.size()
        );

        return false;
    }


    //========================================================
    // Key 1個
    //========================================================
    if (keyCount == 1)
    {
        const vnMotionData_VectorKeyV2& key = keys[startIndex];

        output =
        {
            key.Value[0],
            key.Value[1],
            key.Value[2]
        };

        return true;
    }


    //========================================================
    // Last Index
    //========================================================
    const UINT lastIndex = startIndex + keyCount - 1;


    //========================================================
    // 最初のKey以前
    //========================================================
    if (time <= keys[startIndex].Time)
    {
        const auto& key = keys[startIndex];

        output =
        {
            key.Value[0],
            key.Value[1],
            key.Value[2]
        };


        return true;
    }


    //========================================================
    // 最終Key以降
    //========================================================
    if (time >= keys[lastIndex].Time)
    {
        const auto& key = keys[lastIndex];

        output =
        {
            key.Value[0],
            key.Value[1],
            key.Value[2]
        };

        return true;
    }


    //========================================================
    // 現在Timeを挟む2Keyを検索
    //========================================================
    for (UINT localIndex = 0; localIndex < keyCount - 1; ++localIndex)
    {
        const UINT index0 = startIndex + localIndex;

        const UINT index1 = index0 + 1;

        //====================================================
        // ここまで来る時点でRange Check済み
        //====================================================
        const auto& key0 = keys[index0];

        const auto& key1 = keys[index1];


        if (time < key0.Time || time > key1.Time)
        {
            continue;
        }


        //====================================================
        // 補間区間
        //====================================================
        const float duration =
            key1.Time - key0.Time;


        float rate = 0.0f;


        if (duration > 0.000001f)
        {
            rate = (time - key0.Time) / duration;
        }


        rate = std::clamp(rate, 0.0f, 1.0f);


        //====================================================
        // Position / Scale LERP
        //====================================================
        output.x =
            key0.Value[0] + (key1.Value[0] - key0.Value[0]) * rate;
        output.y =
            key0.Value[1] + (key1.Value[1] - key0.Value[1]) * rate;
        output.z =
            key0.Value[2] + (key1.Value[2] - key0.Value[2]) * rate;


        return true;
    }

    return false;
}

bool SkeletalModel::SampleQuaternionKeys(const MotionDataV2* motion, UINT startIndex, UINT keyCount, float time, DirectX::XMFLOAT4& output) const
{
    if (mMotionV2 == nullptr || keyCount == 0)
    {
        return false;
    }


    const std::vector<vnMotionData_QuaternionKeyV2>& keys = motion->QuaternionKeys;


    //========================================================
    // Range Check
    //========================================================
    const std::uint64_t endIndex =
        static_cast<std::uint64_t>(startIndex) +
        static_cast<std::uint64_t>(keyCount);


    if (endIndex > keys.size())
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Quaternion Key Range Error.\n"
            L"Start    : %u\n"
            L"Count    : %u\n"
            L"KeySize  : %zu\n",
            startIndex,
            keyCount,
            keys.size()
        );

        return false;
    }


    //========================================================
    // Key 1個
    //========================================================
    if (keyCount == 1)
    {
        const auto& key = keys[startIndex];

        output =
        {
            key.Value[0],
            key.Value[1],
            key.Value[2],
            key.Value[3]
        };

        return true;
    }


    const UINT lastIndex = startIndex + keyCount - 1;


    //========================================================
    // 最初
    //========================================================
    if (time <= keys[startIndex].Time)
    {
        const auto& key = keys[startIndex];

        output =
        {
            key.Value[0],
            key.Value[1],
            key.Value[2],
            key.Value[3]
        };

        return true;
    }


    //========================================================
    // 最後
    //========================================================
    if (time >= keys[lastIndex].Time)
    {
        const auto& key = keys[lastIndex];

        output =
        {
            key.Value[0],
            key.Value[1],
            key.Value[2],
            key.Value[3]
        };

        return true;
    }


    //========================================================
    // Key Pair検索
    //========================================================
    for (UINT localIndex = 0; localIndex < keyCount - 1; ++localIndex)
    {
        const UINT index0 = startIndex + localIndex;

        const UINT index1 = index0 + 1;


        const auto& key0 = keys[index0];

        const auto& key1 = keys[index1];


        if (time < key0.Time || time > key1.Time)
        {
            continue;
        }

        const float duration = key1.Time - key0.Time;

        float rate = 0.0f;


        if (duration > 0.000001f)
        {
            rate = (time - key0.Time) / duration;
        }


        rate = std::clamp(rate, 0.0f, 1.0f);


        //====================================================
        // Quaternion 0
        //====================================================
        XMVECTOR q0 =
            XMVectorSet(
                key0.Value[0],
                key0.Value[1],
                key0.Value[2],
                key0.Value[3]
            );


        //====================================================
        // Quaternion 1
        //====================================================
        XMVECTOR q1 =
            XMVectorSet(
                key1.Value[0],
                key1.Value[1],
                key1.Value[2],
                key1.Value[3]
            );


        q0 = XMQuaternionNormalize(q0);

        q1 = XMQuaternionNormalize(q1);


        //====================================================
        // Quaternion SLERP
        //====================================================
        XMVECTOR result =
            XMQuaternionSlerp(
                q0,
                q1,
                rate
            );

        result = XMQuaternionNormalize(result);

        XMStoreFloat4(
            &output,
            result
        );


        return true;
    }

    return false;
}

void SkeletalModel::BuildMotionPose(const MotionDataV2* motion, float time, std::vector<BoneLocalPose>& outputPose) const
{
    if (motion == nullptr || mBoneData == nullptr || mBoneCount <= 0)
    {
        return;
    }


    //========================================================
    // Full Skeleton分確保
    //========================================================
    outputPose.resize(mBoneCount);


    //========================================================
    // 最初は全BoneをBindPoseにする
    // Animation Trackが存在しないBoneは
    // BindPoseをそのまま使用する
    //========================================================
    for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
    {
        const vnModel_BoneDataV2& bone = mBoneData[boneIndex];

        BoneLocalPose& pose = outputPose[boneIndex];


        //====================================================
        // Position
        //====================================================
        pose.Position =
        {
            bone.pos[0],
            bone.pos[1],
            bone.pos[2]
        };


        //====================================================
        // Rotation Quaternion
        //====================================================
        pose.Rotation =
        {
            bone.rotQ[0],
            bone.rotQ[1],
            bone.rotQ[2],
            bone.rotQ[3]
        };


        //====================================================
        // Scale
        //====================================================
        pose.Scale =
        {
            bone.scl[0],
            bone.scl[1],
            bone.scl[2]
        };
    }


    //========================================================
    // Motion Trackを上書き
    //========================================================
    for (const vnMotionData_TrackV2& track : motion->Tracks)
    {
        const int boneIndex = track.SkeletonIndex;


        if (boneIndex < 0 || boneIndex >= mBoneCount)
        {
            continue;
        }

        BoneLocalPose& pose = outputPose[boneIndex];

        //====================================================
        // Position
        //====================================================
        SampleVectorKeys(
            motion,
            track.PositionStartIndex,
            track.PositionKeyNum,
            time,
            pose.Position
        );


        //====================================================
        // Scale
        //====================================================
        SampleVectorKeys(
            motion,
            track.ScaleStartIndex,
            track.ScaleKeyNum,
            time,
            pose.Scale
        );


        //====================================================
        // Quaternion Rotation
        //====================================================
        SampleQuaternionKeys(
            motion,
            track.RotationStartIndex,
            track.RotationKeyNum,
            time,
            pose.Rotation
        );
    }
}

void SkeletalModel::ApplyPoseToBones(const std::vector<BoneLocalPose>& pose)
{
    if (mBones == nullptr || pose.size() < static_cast<std::size_t>(mBoneCount))
    {
        return;
    }


    for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
    {
        ObjectBase* bone = mBones[boneIndex];


        if (bone == nullptr)
        {
            continue;
        }

        const BoneLocalPose& bonePose = pose[boneIndex];

        //====================================================
        // Position
        //====================================================
        bone->setPosition(
            bonePose.Position.x,
            bonePose.Position.y,
            bonePose.Position.z
        );


        //====================================================
        // Scale
        //====================================================
        bone->setScale(
            bonePose.Scale.x,
            bonePose.Scale.y,
            bonePose.Scale.z
        );


        //====================================================
        // Rotation
        //====================================================
        XMVECTOR quaternion =
            XMLoadFloat4(&bonePose.Rotation);


        quaternion = XMQuaternionNormalize(quaternion);

        bone->setQuaternion(&quaternion);

        bone->useQuaternion(true);
    }
}

void SkeletalModel::ApplyBlendedMotionPose()
{
    if (mBlendSourceMotion == nullptr || mMotionV2 == nullptr)
    {
        ApplyMotionPose();

        return;
    }


    //========================================================
    // Source Pose
    //========================================================
    BuildMotionPose(
        mBlendSourceMotion,
        mBlendSourceTime,
        mSourcePose
    );


    //========================================================
    // Destination Pose
    //========================================================
    BuildMotionPose(
        mMotionV2,
        mTime,
        mDestinationPose
    );

    //========================================================
    // Pose Size Check
    //========================================================

    const std::size_t requiredBoneCount =
        static_cast<std::size_t>(mBoneCount);


    if (mSourcePose.size() != requiredBoneCount || mDestinationPose.size() != requiredBoneCount)
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Blend Pose Size Error.\n"
            L"BoneCount   : %d\n"
            L"SourceSize  : %zu\n"
            L"DestSize    : %zu\n",
            mBoneCount,
            mSourcePose.size(),
            mDestinationPose.size()
        );

        return;
    }


    //========================================================
    // Blend率
    // 0.0 = Source 100%
    // 1.0 = Destination 100%
    //========================================================
    float blendRate = 1.0f;

    if (mBlendDuration > 0.000001f)
    {
        blendRate = mBlendTime / mBlendDuration;
    }


    blendRate = std::clamp(blendRate, 0.0f, 1.0f);


    //========================================================
    // Output Pose
    //========================================================
    mBlendPose.resize(mBoneCount);


    for (int boneIndex = 0; boneIndex < mBoneCount; ++boneIndex)
    {
        const BoneLocalPose& source = mSourcePose[boneIndex];

        const BoneLocalPose& destination = mDestinationPose[boneIndex];

        BoneLocalPose& output = mBlendPose[boneIndex];


        //====================================================
        // Position LERP
        //====================================================
        const XMVECTOR sourcePosition =
            XMLoadFloat3(&source.Position);


        const XMVECTOR destinationPosition =
            XMLoadFloat3(&destination.Position);


        const XMVECTOR position =
            XMVectorLerp(
                sourcePosition,
                destinationPosition,
                blendRate
            );


        XMStoreFloat3(
            &output.Position,
            position
        );


        //====================================================
        // Scale LERP
        //====================================================
        const XMVECTOR sourceScale =
            XMLoadFloat3(&source.Scale);


        const XMVECTOR destinationScale =
            XMLoadFloat3(&destination.Scale);


        const XMVECTOR scale =
            XMVectorLerp(
                sourceScale,
                destinationScale,
                blendRate
            );


        XMStoreFloat3(
            &output.Scale,
            scale
        );


        //====================================================
        // Quaternion SLERP
        // Rotationだけは通常のLERPではなく
        // 球面線形補間を使う
        //====================================================
        XMVECTOR sourceRotation =
            XMLoadFloat4(&source.Rotation);


        XMVECTOR destinationRotation =
            XMLoadFloat4(&destination.Rotation);


        sourceRotation =
            XMQuaternionNormalize(sourceRotation);


        destinationRotation =
            XMQuaternionNormalize(destinationRotation);


        XMVECTOR rotation =
            XMQuaternionSlerp(
                sourceRotation,
                destinationRotation,
                blendRate
            );


        rotation =
            XMQuaternionNormalize(rotation);


        XMStoreFloat4(
            &output.Rotation,
            rotation
        );
    }


    //========================================================
    // Blend結果をSkeletonへ反映
    //========================================================
    ApplyPoseToBones(mBlendPose);
}

bool SkeletalModel::AdvanceMotionTime(const MotionDataV2* motion, float& time, bool loop, float deltaTime) const
{
    if (motion == nullptr)
    {
        return false;
    }


    const float length = motion->Header.Length;


    if (length <= 0.000001f)
    {
        time = 0.0f;

        return true;
    }


    time += deltaTime;


    //========================================================
    // Loop
    //========================================================
    if (loop)
    {
        if (time >= length)
        {
            time = std::fmod(time, length);
        }


        if (time < 0.0f)
        {
            time = length - std::fmod(-time, length);


            if (time >= length)
            {
                time = 0.0f;
            }
        }


        return false;
    }


    //========================================================
    // Non Loop
    //========================================================
    if (time >= length)
    {
        time = length;

        return true;
    }


    if (time <= 0.0f)
    {
        time = 0.0f;

        if (deltaTime < 0.0f)
        {
            return true;
        }
    }


    return false;
}

bool SkeletalModel::HandleMotionFinished()
{

    if (!mHasQueuedMotion)
    {
        return false;
    }


    //========================================================
    // BlendToMotion()を呼ぶとQueueがClearされる可能性が
    // あるため、先にLocal変数へコピーする
    //========================================================
    const std::wstring nextMotion = mQueuedMotionName;

    const float blendDuration = mQueuedMotionBlendDuration;

    const bool nextLoop = mQueuedMotionLoop;


    //========================================================
    // 現在Queueを解除
    //========================================================
    ClearQueuedMotion();


    //========================================================
    // 現在Animationの最終Pose
    //          ↓
    // Next Animation
    // へCross Fade
    //========================================================
    const bool result =
        BlendToMotion(
            nextMotion,
            blendDuration,
            true,
            nextLoop
        );


    if (result)
    {
        vnFont::output(
            L"[SkeletalModel] "
            L"Auto Motion Transition : %s\n",
            nextMotion.c_str()
        );
    }


    return result;
}

void SkeletalModel::ClearQueuedMotion()
{
    mQueuedMotionName.clear();

    mQueuedMotionBlendDuration = 0.15f;

    mQueuedMotionLoop = true;

    mHasQueuedMotion = false;
}
