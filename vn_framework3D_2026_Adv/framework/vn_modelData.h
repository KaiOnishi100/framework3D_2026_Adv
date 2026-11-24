//--------------------------------------------------------------//
//	"vn_modelData.h"											//
//		モデルデータ定義(*.vnm / バイナリファイル)				//
//													2026/01/01	//
//														Ichii	//
//--------------------------------------------------------------//
#pragma once

//バージョン履歴
#define vnMODEL_DATA_V1_VER (0x20260101)

#define vnMODEL_DATA_V2_VER (0x20260828)

#define vnMODEL_DATA_V3_VER (0x20260914)

#define vnMODEL_DATA_LATEST_VER   vnMODEL_DATA_V3_VER
struct vnModelData
{
	UINT Magic;
	UINT Date;
	UINT Version;
	UINT HeaderSize;

	UINT MaterialNum;
	UINT MeshNum;
	UINT VertexNum;
	UINT IndexNum;

	UINT MaterialAccess;
	UINT MeshAccess;
	UINT VertexAccess;
	UINT IndexAccess;

	//UINT Align[4];
	UINT BoneNum;
	UINT BoneTableAccess;
	UINT iBindAccess;
	UINT VertexStride;
};

struct vnModel_MaterialData
{
	UINT StructSize;
	UINT TextureNum;
	UINT FloatNum;
	UINT Flag;
	float Diffuse[4];
	float Ambient[4];
	float Specular[4];
	float TilingOffset[4];
	WCHAR Texture[32];
};

struct vnModel_MaterialDataV2
{
    //========================================================
    // Header
    //========================================================
    UINT StructSize;
    UINT TextureNum;
    UINT FloatNum;
    UINT Flag;

    //========================================================
    // Legacy Material
    // 旧Shader / 非PBR描画とも互換性を持たせる。
    //========================================================
    float Diffuse[4];
    float Ambient[4];
    float Specular[4];

    // x = Tile X
    // y = Tile Y
    // z = Offset X
    // w = Offset Y
    float TilingOffset[4];


    //========================================================
    // Normal Map
    // x : Use Normal Map
    // y : Strength
    // z : Y sign
    //     +1 = そのまま
    //     -1 = Flip Y
    // w : unused
    //========================================================
    float NormalParam[4];


    //========================================================
    // PBR
    // x : Metallic
    // y : Roughness
    // z : AO
    // w : Use PBR
    //========================================================
    float PBRParam[4];


    //========================================================
    // Emissive
    // xyz : Color
    // w   : Intensity
    //========================================================
    float EmissiveParam[4];


    //========================================================
    // ORM
    // x : Use ORM
    // y : AO Scale
    // z : Roughness Scale
    // w : Metallic Scale
    //========================================================
    float ORMParam[4];


    //========================================================
    // Environment Reflection
    // x : Enabled
    // y : Intensity
    // z : Max Mip
    // w : unused
    // MaxMipはRuntime側でCubeMapロード後に上書き可能
    //========================================================
    float EnvironmentParam[4];


    //========================================================
    // Subsurface Scattering
    // x : Enabled
    // y : Strength
    // z : Thickness
    // w : Distortion
    //========================================================
    float SSSParam[4];


    //========================================================
    // SSS Color
    // xyz : Scatter Color
    // w   : Transmission Power
    //========================================================
    float SSSColor[4];


    //========================================================
    // Thickness Map
    // x : Use Map
    // y : Scale
    // z : Invert
    // w : unused
    //========================================================
    float SSSMapParam[4];


    //========================================================
    // Screen Space SSS Profile
    // xyz : Scattering Profile
    // w   : World Space Radius
    // 0の場合はGlobal設定へFallback
    //========================================================
    float SSSDiffusionParam[4];

    //========================================================
    // Texture File Name
    //========================================================
    WCHAR BaseColorTexture[64];
    WCHAR NormalTexture[64];
    WCHAR ORMTexture[64];
    WCHAR ThicknessTexture[64];
    WCHAR EmissiveTexture[64];
};

//============================================================
// VNM Material V3
//
// V2 +
//   SkinMask Texture
//
// SSSMapParam.w:
//   0 = SkinMask OFF
//   1 = SkinMask ON
//============================================================
struct vnModel_MaterialDataV3
{
    //========================================================
    // Header
    //========================================================
    UINT StructSize;
    UINT TextureNum;
    UINT FloatNum;
    UINT Flag;


    //========================================================
    // Legacy Material
    //========================================================
    float Diffuse[4];
    float Ambient[4];
    float Specular[4];
    float TilingOffset[4];


    //========================================================
    // PBR
    //========================================================
    float NormalParam[4];
    float PBRParam[4];
    float EmissiveParam[4];
    float ORMParam[4];
    float EnvironmentParam[4];


    //========================================================
    // SSS
    //========================================================
    float SSSParam[4];
    float SSSColor[4];

    //========================================================
    // SSS Maps
    //
    // x : Use Thickness Map
    // y : Thickness Scale
    // z : Thickness Invert
    // w : Use Skin Mask
    //========================================================
    float SSSMapParam[4];


    //========================================================
    // Screen Space Diffusion
    //========================================================
    float SSSDiffusionParam[4];


    //========================================================
    // Texture File Names
    //========================================================
    WCHAR BaseColorTexture[64];
    WCHAR NormalTexture[64];
    WCHAR ORMTexture[64];
    WCHAR ThicknessTexture[64];
    WCHAR EmissiveTexture[64];

    //========================================================
    // V3追加
    // R:
    //   0 = Non Skin
    //   1 = Skin
    //========================================================
    WCHAR SkinMaskTexture[64];
};

struct vnModel_MeshData
{
	UINT MaterialID;
	UINT IndexNum;
	UINT StartIndex;
	UINT StartVertex;
};

//.boneファイルの中身
struct vnModel_BoneData
{
	WCHAR Name[32];	//ボーンの名前
	int ParentID;	//親の番号(-1は親がない)
	int Generation;	//世代
	int StructSize;	//この構造体のサイズ(Byte)
	float pos[3];	//標準状態でのPosition(x,y,z)
	float rot[3];	//標準状態でのRotation(x,y,z)
	float scl[3];	//標準状態でのScale(x,y,z)
};

//============================================================
// .bone V2
// 新しいSkeletalModel用
// EulerではなくQuaternionを直接保存する
// Assimpから取得したRotationをほぼそのままRuntimeへ渡せる
//============================================================
struct vnModel_BoneDataV2
{
    WCHAR Name[32]; // Bone Name
    int ParentID; // Hierarchy
    int Generation;    
    int StructSize;// このRecordのサイズ
    float pos[3];    // Local Position
    float rotQ[4];// Local Quaternion
    float scl[3];    // Local Scale
};


//============================================================
// VNM Material Flags
//============================================================
enum vnModelMaterialFlag : UINT
{
    vnMaterialFlag_None      = 0,
    vnMaterialFlag_PBR       = 1u << 0,
    vnMaterialFlag_NormalMap = 1u << 1,
    vnMaterialFlag_ORM       = 1u << 2,
    vnMaterialFlag_Thickness = 1u << 3,
    vnMaterialFlag_Emissive  = 1u << 4,
    vnMaterialFlag_SkinMask  = 1u << 5
};

//ポリゴン頂点構造体
struct vnVertex3D
{
    float x, y, z;		//座標
    float nx, ny, nz;		//法線
    float r, g, b, a;		//カラー
    float u, v;			//UV
    float tx, ty, tz, tw;	//tangent
};

//============================================================
// VNMファイル内に保存されている旧頂点形式
//
// 既存のVNMファイルとの互換性を維持するため、
// この構造体の並びやサイズは変更しない
//============================================================
struct vnVertex3DFile
{
    //========================================================
    // Position
    //========================================================
    float x, y, z;
    //========================================================
    // Normal
    //========================================================
    float nx, ny, nz;
    //========================================================
    // Vertex Color
    //========================================================
    float r, g, b, a;
    //========================================================
    // Texture Coordinate
    //========================================================
    float u, v;
};

//============================================================
// VNMファイル内のスキニング頂点
//============================================================
struct vnVertex3DFileSkin
{
    // Position
    float x, y, z;
    // Normal
    float nx, ny, nz;
    // Color
    float r, g, b, a;
    // UV
    float u, v;
    // Bone Index
    UINT16 skinIndex[4];
    // Bone Weight
    float skinWeight[4];
};

//============================================================
// GPU用スキニング頂点
//
// 通常頂点＋Tangent＋Skin情報
//============================================================
struct vnVertex3DSkin
{
    // Position
    float x, y, z;
    // Normal
    float nx, ny, nz;
    // Color
    float r, g, b, a;
    // UV
    float u, v;
    // Tangent
    float tx, ty, tz, tw;
    // Bone Index
    UINT16 skinIndex[4];
    // Bone Weight
    float skinWeight[4];
};

static_assert(sizeof(vnModelData) == 64, "vnModelData binary layout has changed.");
static_assert(sizeof(vnModel_MaterialData) == 144, "vnModel_MaterialData V1 binary layout has changed.");
static_assert(sizeof(vnModel_MaterialDataV2) == 864, "vnModel_MaterialData V2 binary layout has changed.");
static_assert(sizeof(vnModel_MaterialDataV3) == 992, "vnModel_MaterialDataV3 binary layout has changed.");
static_assert(sizeof(vnModel_MeshData) == 16, "vnModel_MeshData binary layout has changed.");
static_assert(sizeof(vnVertex3DFile) == 48, "vnVertex3DFile binary layout has changed.");
static_assert(sizeof(vnVertex3DFileSkin) == 72, "vnVertex3DFileSkin binary layout has changed.");
static_assert(sizeof(vnModel_BoneData) == 112, "vnModel_BoneData binary layout has changed.");
static_assert(sizeof(vnModel_BoneDataV2) == 116, "vnModel_BoneData binary layout has changed.");
