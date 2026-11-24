//--------------------------------------------------------------//
//	"vn_motionData.h"											//
//		モーションデータ定義									//
//													2026/01/01	//
//														Ichii	//
//--------------------------------------------------------------//
#pragma once

//バージョン履歴
#define vnMOTION_DATA_LATEST_VER (0x20251201)

#define vnMOTION_DATA_V2_VER      (0x20260905)


struct vnMotionData
{
	UINT Magic;
	UINT Date;
	UINT Version;
	UINT HeaderSize;

	float Length;
	UINT ChannelNum;
	UINT KeyFrameNum;
	float iFrameRate;

	UINT ChannelAccess;
	UINT KeyFrameAccess;
	UINT Reserve1;
	UINT Reserve2;

	UINT Align[4];
};

enum eMotionChannel
{
	PosX,
	PosY,
	PosZ,
	RotX,
	RotY,
	RotZ,
	SclX,
	SclY,
	SclZ,
	QutX,
	QutY,
	QutZ,
	QutW,
	ChannelMax,
};

struct vnMotionData_Channel
{
	WCHAR Name[32];
	int ChannelID;
	int KeyFrameNum;
	int StartIndex;
	int Reserve;
};

struct vnMotionData_KeyFrame
{
	float	Time;
	float	Value;
	float	InTangent;
	float	OutTangent;
};

static_assert(sizeof(vnMotionData) == 64,"vnMotionData binary layout has changed.");

static_assert(sizeof(vnMotionData_Channel) == 80,"vnMotionData_Channel binary layout has changed.");

static_assert(sizeof(vnMotionData_KeyFrame) == 16,"vnMotionData_KeyFrame binary layout has changed.");

//============================================================
// MOT V2 Header
//
// File:
//
// [Header]
// [Track]
// [VectorKey]
// [QuaternionKey]
//
// VectorKey:
//   Position + Scale 共用
//
// QuaternionKey:
//   Rotation専用
//============================================================
struct vnMotionDataV2
{
    //========================================================
    // Common Header
    //========================================================
    UINT Magic;

    UINT Date;

    UINT Version;

    UINT HeaderSize;


    //========================================================
    // Animation Length [seconds]
    //========================================================
    float Length;


    //========================================================
    // Bone Track数
    //========================================================
    UINT TrackNum;


    //========================================================
    // Position + Scale Keyの合計
    //========================================================
    UINT VectorKeyNum;


    //========================================================
    // Quaternion Keyの合計
    //========================================================
    UINT QuaternionKeyNum;


    //========================================================
    // 元AssetのTicks Per Second
    // KeyのTimeは既にsecondsへ変換済みなので、
    // 主にDebug / 情報保持用
    //========================================================
    float SourceTicksPerSecond;


    //========================================================
    // File Offset
    //========================================================
    UINT TrackAccess;

    UINT VectorKeyAccess;

    UINT QuaternionKeyAccess;


    //========================================================
    // Future Use
    //========================================================
    UINT Reserve0;


    //========================================================
    // Headerを64byteに維持
    //========================================================
    UINT Align[3];
};


//============================================================
// MOT V2 Track
// 1 Bone / Nodeにつき1Track
// SkeletonIndex: .boneで読み込んだ
// mBones[SkeletonIndex]
// へ直接適用できる
//============================================================
struct vnMotionData_TrackV2
{
    //========================================================
    // Bone / Node Name
    //========================================================
    WCHAR Name[32];


    //========================================================
    // Full Skeleton Index
    //========================================================
    int SkeletonIndex;


    //========================================================
    // Position Keys
    //========================================================
    UINT PositionKeyNum;

    UINT PositionStartIndex;


    //========================================================
    // Rotation Keys
    //========================================================
    UINT RotationKeyNum;

    UINT RotationStartIndex;


    //========================================================
    // Scale Keys
    //========================================================
    UINT ScaleKeyNum;

    UINT ScaleStartIndex;


    //========================================================
    // Record Size
    //========================================================
    UINT StructSize;
};


//============================================================
// Position / Scale Key
//============================================================
struct vnMotionData_VectorKeyV2
{
    float Time;

    float Value[3];
};


//============================================================
// Quaternion Key
//============================================================
struct vnMotionData_QuaternionKeyV2
{
    float Time;

    float Value[4];
};


//============================================================
// V2 Binary Layout Check
//============================================================
static_assert(sizeof(vnMotionDataV2) == 64, "vnMotionDataV2 binary layout has changed.");
static_assert(sizeof(vnMotionData_TrackV2) == 96, "vnMotionData_TrackV2 binary layout has changed.");
static_assert(sizeof(vnMotionData_VectorKeyV2) == 16, "vnMotionData_VectorKeyV2 binary layout has changed.");
static_assert(sizeof(vnMotionData_QuaternionKeyV2) == 20, "vnMotionData_QuaternionKeyV2 binary layout has changed.");