#include "../../../../framework.h"
#include "../../../../framework/vn_environment.h"

#include "playerNoiseEmitter.h"

#include "../../../noise/noiseSystem.h"

namespace
{
    struct FootstepArea
    {
        float minX;
        float maxX;
        float minZ;
        float maxZ;
        float floorY;  // 床の上面のワールド座標
        SoundID sound;
    };

    constexpr FootstepArea kFootstepAreas[] =
    {
        // 本館1F 西廊下・東廊下
        { -16.0f,  -9.0f,   8.0f,  24.0f,  0.0f, SoundID::AE_FootstepWood },
        {   9.0f,  16.0f,   8.0f,  24.0f,  0.0f, SoundID::AE_FootstepWood },

        // 本館1F 書斎・図書室
        { -26.0f, -16.0f,   8.0f,  24.0f,  0.0f, SoundID::AE_FootstepWood },

        // 本館B1 ボイラー室・南側通路・倉庫
        { -25.0f,   0.0f, -14.0f,  -8.25f, -4.5f, SoundID::AE_FootstepConcrete },

        // 本館B1 北側通路
        { -15.0f, -10.0f,  -8.25f, -1.75f, -4.5f, SoundID::AE_FootstepConcrete },
    };

    SoundID ResolveFootstepSound(const XMFLOAT3& feet, SoundID defaultSound)
    {
        // 床との微小な位置ずれを許容しつつ、別の階を拾わないようにする。
        constexpr float kFloorHeightTolerance = 0.25f;

        for (const FootstepArea& area : kFootstepAreas)
        {
            if (std::abs(feet.y - area.floorY) <= kFloorHeightTolerance &&
                feet.x >= area.minX && feet.x < area.maxX &&
                feet.z >= area.minZ && feet.z < area.maxZ)
            {
                return area.sound;
            }
        }

        // 中央ホール・大階段ホール表にない場所は従来の足音を使う
        return defaultSound;
    }
}

//============================================================
// Update
//============================================================
void PlayerNoiseEmitter::Update(float dt,const ObjectBase& owner,const PlayerMovement& movement)
{
    //========================================================
    // Noise Systemなし
    //========================================================
    if (mNoiseSystem == nullptr)
    {
        return;
    }


    //========================================================
    // Timer
    //========================================================
    if (mFootstepTimer > 0.0f)
    {
        mFootstepTimer -= dt;
    }

    if (mFootstepSfxTimer > 0.0f)
    {
        mFootstepSfxTimer -= dt;
    }

    //========================================================
    // Playerが動いていない
    //========================================================
    if (!movement.IsMoving())
    {
        return;
    }


    //========================================================
    // 空中では通常Footstepを発生させない
    //========================================================
    if (!movement.IsGrounded())
    {
        return;
    }


    //========================================================
    // 次のFootstepまで待つ
    //========================================================
    if (mFootstepTimer > 0.0f)
    {
        return;
    }


    //============================================================
    // Movement Noise State
    //============================================================
    const bool crouching = movement.IsCrouching();

    const bool sprinting = movement.IsSprinting();

    if (mFootstepSfxTimer <= 0.0f)
    {
        //============================================================
        // 実際にプレイヤーへ聞かせる足音
        // しゃがみと走りの入力が重なった場合はNoise判定と同じくしゃがみを優先する
        //============================================================
        const bool running = !crouching && sprinting;

        static constexpr SoundID walkSounds[] =
        {
            SoundID::AE_Footstep,
            SoundID::AE_Footstep2,
            SoundID::AE_Footstep3,
        };

        static constexpr SoundID runSounds[] =
        {
            SoundID::AE_PlayerRun,
            SoundID::AE_PlayerRun2,
            SoundID::AE_PlayerRun3,
        };

        // まず通常の歩行・走行の足音を現在のバリエーション番号から選ぶ
        const SoundID defaultSoundId =
            running ? runSounds[mFootstepVariant] : walkSounds[mFootstepVariant];

        // owner のワールド座標を床範囲の判定に使う足元座標へ変換する
        XMFLOAT3 feet{};
        XMStoreFloat3(&feet, *owner.getPosition());

        // 木やコンクリートの範囲に入っていればその床の音を使う
        const SoundID soundId = ResolveFootstepSound(feet, defaultSoundId);

        // しゃがみ・歩行・走行ごとの音量で再生する
        const float volume = crouching ? 0.13f : (running ? 0.34f : 0.24f);
        SoundBank::getInstance()->PlaySfx(soundId, volume);

        mFootstepVariant = (mFootstepVariant + 1) % 3;

        mFootstepSfxTimer = crouching ? 0.92f : (running ? 0.3f : 0.4f);
    }

    //============================================================
    // Noise Parameters
    //============================================================
    float radius = mWalkRadius;

    float interval = mWalkInterval;

    float intensity = 1.0f;

    NoiseType noiseType = NoiseType::FootstepWalk;

    //============================================================
    // Crouch
    //============================================================
    if (crouching)
    {
        radius = mCrouchRadius;

        interval = mCrouchInterval;

        intensity = mCrouchIntensity;
    }
    //============================================================
    // Sprint
    //============================================================
    else if (sprinting)
    {
        radius = mSprintRadius;

        interval = mSprintInterval;

        intensity = 1.25f;

        noiseType = NoiseType::FootstepSprint;
    }

    //============================================================
    // Noise Position
    //============================================================
    const XMVECTOR position = *owner.getPosition();

    const XMFLOAT3 noisePosition =
    {
        XMVectorGetX(position),
        XMVectorGetY(position),
        XMVectorGetZ(position)
    };

    //============================================================
    // 追跡者の聴覚に使うイベント
    // 既存の距離・種類・強度はそのまま渡す
    //============================================================
    if (mNoiseSystem != nullptr)
    {
        mNoiseSystem->Emit(noisePosition, radius, noiseType, intensity);
    }

    //============================================================
    // AIへのNoiseとSEを同じ歩調で発生させる
    //============================================================
    mFootstepTimer = interval;
}