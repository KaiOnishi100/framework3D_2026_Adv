#pragma once

class vnScene;
class ModelBase;
class InteractionSystem;
class ExplorationPlayer;

//============================================================
// AnnexSafetyPuzzle
// 別館B1の安全装置復旧パズル
// Observation: 排気弁
// Experimental:
//   冷却装置
//   安全制御盤
// 排気と冷却が両方ONの場合に限り、
// 安全制御盤でPuzzleを解決できる。
// Save:
//   State = PuzzleBaseのSolved状態
//   Flags bit0 = Vent
//   Flags bit1 = Cooling
//============================================================
class AnnexSafetyPuzzle final : public PuzzleBase
{
public:

    enum class PanelType : std::uint32_t
    {
        Vent = 0,
        Cooling = 1,
        Controller = 2
    };

    using SolvedCallback = std::function<void(bool restored)>;

    //========================================================
    // 操作パネル
    // パズル本体と操作Objectを分離する
    //========================================================
    class Panel final : public IInteractable
    {
    public:

        bool Initialize(
            vnScene* scene,
            InteractionSystem* interactionSystem,
            AnnexSafetyPuzzle* owner,
            PanelType type,
            const XMFLOAT3& position,
            const XMFLOAT3& scale,
            const XMFLOAT3& rotation
        );

        void Terminate(
            vnScene* scene,
            InteractionSystem* interactionSystem
        );

        //====================================================
        // 操作状態に合わせてCubeの色を変更する
        //====================================================
        void SetIndicator(bool active);

        //====================================================
        // IInteractable
        //====================================================
        XMVECTOR GetInteractionPosition() const override;

        bool CanInteract() const override;

        void Interact(ExplorationPlayer& player) override;

        const wchar_t* GetInteractionText() const override;

    private:

        AnnexSafetyPuzzle* mOwner = nullptr;

        PanelType mType = PanelType::Vent;

        XMFLOAT3 mPosition = {};

        ModelBase* mModel = nullptr;

        bool mRegistered = false;
    };

public:

    bool Initialize(
        vnScene* scene,
        InteractionSystem* interactionSystem,
        const std::string& persistentId,
        const std::array<XMFLOAT3, 3>& positions,
        const std::array<XMFLOAT3, 3>& scales,
        const std::array<XMFLOAT3, 3>& rotations
    );

    void Terminate(vnScene* scene,InteractionSystem* interactionSystem  );

    //========================================================
    // Panelからの操作通知
    //========================================================
    void Operate(PanelType type);

    //========================================================
    // 個別設備の状態
    //========================================================
    bool IsVentReady() const
    {
        return mVentReady;
    }

    bool IsCoolingReady() const
    {
        return mCoolingReady;
    }

    //========================================================
    // Door解錠用Callback
    //========================================================
    void SetSolvedCallback(SolvedCallback callback)
    {
        mSolvedCallback = std::move(callback);
    }

protected:

    //========================================================
    // PuzzleBaseのSave/Load拡張
    //========================================================
    void CaptureCustomState(WorldObjectSaveState& data) const override;

    void RestoreCustomState(const WorldObjectSaveState& data) override;

    void OnSolved() override;

    void OnSolvedRestored() override;

private:

    //========================================================
    // 現在状態をモデル表示に反映
    //========================================================
    void RefreshVisuals();

private:

    std::array<Panel, 3> mPanels;

    bool mVentReady = false;

    bool mCoolingReady = false;

    SolvedCallback mSolvedCallback;
};