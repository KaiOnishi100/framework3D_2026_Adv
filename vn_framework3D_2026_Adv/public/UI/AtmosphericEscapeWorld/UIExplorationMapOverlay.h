#pragma once


class vnScene;
class ExplorationLocationSystem;
class ObjectiveManager;


//============================================================
// UIExplorationMapOverlay
// 探索用Map
// 操作:
//   左右 : Floor変更
//   上下 : Area変更
//   B / Esc : 閉じる
// Mouse: Area / Floor Tabをクリック
// Map上では:
//   ・探索済みRoom
//   ・現在Room
//   ・Objective対象Room
// を表示する
//============================================================
class UIExplorationMapOverlay : public IUIControllable
{
public:

    enum class Action
    {
        None = 0,
        Close
    };


private:

    //========================================================
    // Map上のRoom表示
    //========================================================
    struct RoomVisual
    {
        std::string RoomId;

        std::wstring DisplayName;

        ExplorationArea Area = ExplorationArea::Unknown;
        ExplorationFloor Floor = ExplorationFloor::Unknown;

        //====================================================
        // Map画面上のRect
        //====================================================
        UIRect Rect;

        //====================================================
        // Room背景
        //====================================================
        vnSprite* Plate = nullptr;
    };


public:

    bool Initialize(
        vnScene* scene,
        const ExplorationLocationSystem* locationSystem,
        const ObjectiveManager* objectiveManager
    );


    void Terminate(vnScene* scene);


    //========================================================
    // Open / Close
    //========================================================
    void Show();

    void Hide();


    bool IsVisible() const
    {
        return mVisible;
    }


    //========================================================
    // Update
    //========================================================
    void Update(float dt);


    //========================================================
    // Font描画
    //========================================================
    void Draw() const;


    //========================================================
    // Action
    //========================================================
    Action ConsumeAction();


    //========================================================
    // IUIControllable
    //========================================================
    void MoveUIHorizontal(int dir) override;

    void MoveUIVertical(int dir) override;

    void Submit() override;

    void Cancel() override;


private:

    //========================================================
    // Room Visual構築
    //========================================================
    bool BuildRoomVisuals(vnScene* scene);


    //========================================================
    // 現在Areaに存在するFloor一覧
    //========================================================
    std::vector<ExplorationFloor> BuildFloorList(ExplorationArea area) const;


    //========================================================
    // Area / Floor同期
    //========================================================
    void SelectCurrentPlayerLocation();

    void RefreshFloorSelection();


    //========================================================
    // Visible State
    //========================================================
    void RefreshRoomVisibility();


    //========================================================
    // Room Color
    //========================================================
    void RefreshRoomColors();


    //========================================================
    // Mouse用Rect
    //========================================================
    void BuildTabRects();


    //========================================================
    // 表示名
    //========================================================
    static const wchar_t* GetAreaName(ExplorationArea area);
    static const wchar_t* GetFloorName(ExplorationFloor floor);


    //========================================================
    // Sprite Color Helper
    //========================================================
    static void SetSpriteColor(
        vnSprite* sprite,
        float r,
        float g,
        float b,
        float a
    );


private:

    bool mVisible = false;

    Action mPending = Action::None;


    //========================================================
    // Data
    //========================================================
    const ExplorationLocationSystem* mLocationSystem = nullptr;
    const ObjectiveManager* mObjectiveManager = nullptr;


    //========================================================
    // Background
    //========================================================
    vnSprite* mDimPlate = nullptr;
    vnSprite* mMapPanel = nullptr;


    //========================================================
    // Room Visual
    //========================================================
    std::vector<RoomVisual>  mRooms;

    //========================================================
    // Area
    //========================================================
    std::vector<ExplorationArea> mAreas;

    int mAreaIndex = 0;


    //========================================================
    // Floor
    //========================================================
    std::vector<ExplorationFloor> mFloors;

    int mFloorIndex = 0;


    //========================================================
    // Mouse Tabs
    //========================================================
    std::vector<UIRect> mAreaRects;

    std::vector<UIRect>  mFloorRects;
};