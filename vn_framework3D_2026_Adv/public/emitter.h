#pragma once

//============================================================
// パーティクル最大数
//============================================================
//#ifndef vnPARTICLE_MAX
//#define vnPARTICLE_MAX (1024)
//#endif

//============================================================
// パーティクル1個分の情報
//============================================================
class Particle
{
public:
    float    Life = 0.0f;                         // 現在の寿命
    float    StartLife = 0.0f;                         // 生成時の寿命
    XMVECTOR Pos = XMVectorSet(0, 0, 0, 0);       // 現在位置
    XMVECTOR Vel = XMVectorSet(0, 0, 0, 0);       // 速度
    XMVECTOR color = XMVectorSet(1, 1, 1, 1);       // RGBA
    float    Size = 1.0f;                         // 基本サイズ
};

//============================================================
// エミット形状
//============================================================
enum class EmitShape
{
    Sphere,     // 全方向へ放出
    Cone,       // 指定方向を中心に扇状へ放出
    Circle,     // 円周上に配置して速度なし
    Direction,  // 指定方向へ直線的に放出
    Dust,       // 円周上から外側＋上方向へ粉塵っぽく放出
    Press,      // プレス攻撃専用。Dust に近いが上方向量を調整可能
};

//============================================================
// emit の挙動
//============================================================
enum class EmitMode
{
    Continuous, // setEmit(true) の間、毎フレーム出し続ける
    Once        // EmitOnce() したフレームだけ出す
};

//============================================================
// サイズ変化モード
//============================================================
enum class SizeFadeMode
{
    Normal,     // 寿命に応じて小さくなる
    KeepMin,    // 最低サイズを残しながら小さくなる
    Fixed,      // サイズ固定
};

//============================================================
// float の範囲
//============================================================
struct RangeF
{
    float Min = 0.0f;
    float Max = 0.0f;
};

//============================================================
// 色の範囲
//============================================================
struct RangeColor
{
    XMVECTOR Min = XMVectorSet(0, 0, 0, 0);
    XMVECTOR Max = XMVectorSet(1, 1, 1, 1);
};

//============================================================
// ランダム速度ゆらぎ
//============================================================
struct RandomVelocityParam
{
    XMVECTOR Min = XMVectorSet(0, 0, 0, 0);
    XMVECTOR Max = XMVectorSet(0, 0, 0, 0);
};

//============================================================
// コーン形状パラメータ
//============================================================
struct ConeParam
{
    float SpreadAngle = XM_PIDIV4;          // 基本拡散角度
    float YawScale = 1.0f;               // 横方向の広がり倍率
    float PitchScale = 0.35f;              // 縦方向の広がり倍率
    RangeF UpBias = { 0.0f, 0.0f };     // 上方向への追加速度
};

//============================================================
// 円形パラメータ
//============================================================
struct CircleParam
{
    float Radius = 1.0f;
};

//============================================================
// プレス攻撃専用パラメータ
//============================================================
struct PressParam
{
    RangeF Up = { 8.0f, 14.0f };
};

//============================================================
// 見た目パラメータ
//============================================================
struct VisualParam
{
    bool Dark = false;                  // 寿命に応じて暗くする
    bool Additive = false;              // 加算合成にする

    SizeFadeMode SizeFade = SizeFadeMode::Normal;
    float SizeFadeMin = 0.0f;               // KeepMin 時の最低サイズ割合
};

//============================================================
// 吸引パラメータ
//============================================================
struct AttractParam
{
    bool Enable = false;

    float Power = 20.0f;             // 中心へ吸う力
    float KillDistance = 7.5f;       // 中心に近づいたら消す距離

    float SwirlPower = 10.0f;        // 渦の強さ
    float SwirlSign = 1.0f;          // 1.0 / -1.0 で回転方向
    float MaxSpeed = 50.0f;          // 速度上限
};

//============================================================
// エミッター
//============================================================
class Emitter : public ObjectBase
{
public:
    //========================================================
    // エミッター設定
    //========================================================
    struct stEmitterDesc
    {
        WCHAR Texture[64] = L"";

        //====================================================
        // パラメータ
        //====================================================
        RangeF Life = { 30.0f, 60.0f };
        RangeF Size = { 0.5f, 1.0f };
        RangeF Speed = { 0.1f, 0.2f };

        RangeColor Color = {
            XMVectorSet(0, 0, 0, 0),
            XMVectorSet(1, 1, 1, 1)
        };
        EmitShape Shape = EmitShape::Sphere;
        EmitMode  Mode = EmitMode::Continuous;

        XMVECTOR Direction = XMVectorSet(0, 0, 1, 0);

        float SpreadAngle = XM_PIDIV4;      // Cone 用
        float Radius = 1.0f;           // Circle / Attract 用

        float EmitRate = 60.0f;
        int EmitCount = 1;
        bool emit = false;

        bool  UseAttract = false;           // 吸引用
        float AttractPower = 20.0f;         // 吸引用

        //====================================================
        // 詳細パラメータ
        //====================================================
        ConeParam Cone;
        CircleParam Circle;
        PressParam Press;
        RandomVelocityParam RandomVelocity;
        VisualParam Visual;
        AttractParam Attract;
    };

    static constexpr int ParticleMax = 1024;

public:
    Emitter(stEmitterDesc* desc);
    virtual ~Emitter();

    //========================================================
    // emit 操作
    //========================================================
    void setEmit(bool flag);
    bool isEmit();

    // 1回だけ出したい場合に使う
    void EmitOnce();

    //========================================================
    // 外部調整用
    //========================================================
    void SetEmitCount(int n);
    void SetDirection(const XMVECTOR& dir);
    void SetSpreadAngle(float rad);
    void SetCircleRadius(float r);
    float GetCircleRadius() const;
    void SetSizeRange(float minS, float maxS);
    void SetPressUp(float minUp, float maxUp);

    //============================================================
    // Continuous Emit Rate設定
    //
    // 単位: particle / second
    //============================================================
    void SetEmitRate(float particlesPerSecond);

    void SetAlphaMul(float a);
    float GetAlphaMul() const;

    //============================================================
    // Shadow Receive Strength設定
    // 0.0f: Shadowの影響を受けない
    // 1.0f: Shadowを100%受ける
    //============================================================
    void SetShadowReceiveStrength(float strength);
    float GetShadowReceiveStrength() const;

    //============================================================
    // Local Lightの影響度
    // 0.0f: Point / Spot Lightの影響を受けない
    // 1.0f: 通常の強さでLocal Lightの影響を受ける
    // Effect専用の軽量Lightingとして使用する
    //============================================================
    void SetLocalLightStrength(float strength);
    float GetLocalLightStrength() const;
    //============================================================
    // Local Light Shadowを受けるか
    // false: Point / Spot Lightからは照らされるがそのライトのShadow Mapは評価しない
    // true: Point / Spot LightのShadowも評価する
    //============================================================
    void SetLocalShadowEnabled(bool enabled);
    bool IsLocalShadowEnabled() const;

    bool HasAliveParticles() const;

    // 手動でワールド行列を同期したい時用
    void SyncTransform();

    //========================================================
    // vnObject 系
    //========================================================
    virtual void setVertexPosition();
    void execute(float dt);
    virtual void render();

private:
    //========================================================
    // 内部ヘルパー
    //========================================================

    // 空いているParticle Slotを取得
    int AcquireParticleSlot();

    float randFloat(float min, float max);

    XMVECTOR randVec(
        float minX, float minY, float minZ, float minW,
        float maxX, float maxY, float maxZ, float maxW
    );

    XMVECTOR randVecBetween(const XMVECTOR& minV, const XMVECTOR& maxV);

    float CalcSizeScale(float overLifeTime) const;

    void SpawnOneParticle(int index, const XMVECTOR& worldPos);
    void ApplyAttract(Particle& p, float dt);
    void WriteParticleVertex(const Particle& particle, float overLifeTime, const XMMATRIX& billboard);
    //============================================================
    // Active Particleを解放
    //============================================================
    void ReleaseParticleSlot(int activeListIndex);

    float GetConeSpreadAngle() const;
    float GetCircleRadiusInternal() const;

private:
    stEmitterDesc Desc;

    struct stMaterialBuffer
    {
        XMFLOAT4	Diffuse;		//拡散色
        XMFLOAT4	Ambient;		//環境色
        XMFLOAT4	Specular;		//鏡面反射色
        XMFLOAT4	TilingOffset;	//タイリングとオフセット
        XMFLOAT4    ShadowParam;
        XMFLOAT4	NormalParam;	//MormalMap設定
    };
    struct stMaterial
    {
        XMVECTOR	NormalParam;	//MormalMap設定
        //テクスチャ
        ID3D12Resource* texbuff;
        ID3D12Resource* normalTexBuff;// Normal Map Texture
        //定数バッファ
        ID3D12Resource* materialBuff;
        ID3D12DescriptorHeap* basicDescHeap;
        stMaterialBuffer* pMaterialBuffer;
    };

    // Particle本体
    Particle* pParticle = nullptr;

    int IndexNum = 0;
    int renderParticleNum = 0;

    // 現在生存しているParticleのIndexだけを保持する
    int mActiveParticleIndices[ParticleMax] = {};
    // 未使用SlotをStackとして保持する
    int mFreeParticleIndices[ParticleMax] = {};
    // Active数
    int mActiveParticleCount = 0;
    // Free数
    int mFreeParticleCount = 0;
    // Continuous Emit用Accumulator
    float mEmitAccumulator = 0.0f;


    vnVertex3D* vtx = nullptr;
    WORD* idx = nullptr;

    ID3D12Resource* vertBuff = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

    ID3D12Resource* idxBuffer = nullptr;
    D3D12_INDEX_BUFFER_VIEW indexBufferView = {};

    //========================================================
    // ・stMaterial / stMaterialBufferを使うためpMaterialsを保持
    //========================================================
    stMaterial* pMaterials = nullptr;

    ID3D12Resource* constBuff = nullptr;
    stConstantBuffer* pConstBuffer = nullptr;

    float mAlphaMul = 1.0f;

    //============================================================
    // EffectがShadowを受ける強さ
    //============================================================
    float mShadowReceiveStrength = 1.0f;
    float mLocalLightStrength = 0.0f;
    bool mLocalShadowEnabled = true;
};
