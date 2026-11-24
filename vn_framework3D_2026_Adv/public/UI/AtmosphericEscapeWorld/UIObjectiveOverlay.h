#pragma once

class ObjectiveManager;
class ExplorationLocationSystem;

//============================================================
// UIObjectiveOverlay
// 現在のObjective情報を表示するページ
//============================================================
class UIObjectiveOverlay : public IUIControllable
{
public:

    bool Initialize(
        vnScene* scene,
        const ObjectiveManager* objectiveManager,
        const ExplorationLocationSystem* locationSystem
    );


    void Terminate(vnScene* scene);


    void Show();

    void Hide();


    bool IsVisible() const
    {
        return mVisible;
    }


    void Update(float dt);


    void Draw() const;


    //========================================================
    // IUIControllable
    //========================================================
    void MoveUIHorizontal(int dir) override;

    void MoveUIVertical(int dir) override;

    void Submit() override;

    void Cancel() override;


private:

    static void SetRender(vnSprite* sprite, bool enabled);


    static void SetSpriteColor(
        vnSprite* sprite,
        float r,
        float g,
        float b,
        float a
    );


private:

    bool mVisible = false;

    const ObjectiveManager* mObjectiveManager = nullptr;

    const ExplorationLocationSystem* mLocationSystem = nullptr;


    //========================================================
    // Background
    //========================================================
    vnSprite* mDimPlate = nullptr;

    vnSprite* mMainPanel = nullptr;
};