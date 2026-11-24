#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "UIExplorationMapOverlay.h"
#include "UIInventoryOverlay.h"

bool UIExplorationMapOverlay::Initialize(
    vnScene* scene,
    const ExplorationLocationSystem* locationSystem,
    const ObjectiveManager* objectiveManager
)
{
    if (scene == nullptr || locationSystem == nullptr || objectiveManager == nullptr)
    {
        return false;
    }


    mLocationSystem = locationSystem;
    mObjectiveManager = objectiveManager;

    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);


    //========================================================
    // 背景Dim
    //========================================================
    mDimPlate =
        new vnSprite(
            sw * 0.5f,
            sh * 0.5f,
            sw,
            sh,
            L""
        );

    mDimPlate->setRenderPriority(3000);

    SetSpriteColor(
        mDimPlate,
        0.0f,
        0.0f,
        0.0f,
        0.80f
    );

    scene->registerObject(mDimPlate);


    //========================================================
    // Main Panel
    //========================================================
    mMapPanel =
        new vnSprite(
            sw * 0.56f,
            sh * 0.54f,
            sw * 0.70f,
            sh * 0.72f,
            L""
        );


    mMapPanel->setRenderPriority(3010);


    SetSpriteColor(
        mMapPanel,
        0.035f,
        0.045f,
        0.050f,
        0.94f
    );


    scene->registerObject(mMapPanel);


    //========================================================
    // Room Visual
    //========================================================
    if (!BuildRoomVisuals(scene))
    {
        return false;
    }


    //========================================================
    // 使用可能Areaを構築
    //========================================================
    static constexpr ExplorationArea areaOrder[] =
    {
        ExplorationArea::MainBuilding,
        ExplorationArea::Courtyard,
        ExplorationArea::Annex,
        ExplorationArea::TestFacility
    };


    const auto& roomDefinitions = mLocationSystem->GetRooms();


    for (ExplorationArea area : areaOrder)
    {
        bool exists = false;

        for (const RoomDefinition& room : roomDefinitions)
        {
            if (room.Area == area)
            {
                exists = true;

                break;
            }
        }


        if (exists)
        {
            mAreas.push_back(area);
        }
    }

    //========================================================
    // Default Selection
    //========================================================
    SelectCurrentPlayerLocation();

    BuildTabRects();

    Hide();

    return true;
}

void UIExplorationMapOverlay::Terminate(vnScene* scene)
{
    //========================================================
    // Room Visual
    //========================================================
    if (scene != nullptr)
    {
        for (RoomVisual& room : mRooms)
        {
            if (room.Plate != nullptr)
            {
                scene->deleteObject(room.Plate);
                room.Plate = nullptr;
            }
        }
    }

    mRooms.clear();

    //========================================================
    // Map Panel
    //========================================================
    if (scene != nullptr && mMapPanel != nullptr)
    {
        scene->deleteObject(mMapPanel);
        mMapPanel = nullptr;
    }


    //========================================================
    // Dim Plate
    //========================================================
    if (scene != nullptr && mDimPlate != nullptr)
    {
        scene->deleteObject(mDimPlate);
        mDimPlate = nullptr;
    }


    //========================================================
    // UI State
    //========================================================
    mAreas.clear();
    mFloors.clear();
    mAreaRects.clear();
    mFloorRects.clear();

    mAreaIndex = 0;
    mFloorIndex = 0;

    mVisible = false;
    mPending = Action::None;

    //========================================================
    // 外部Dataへの参照解除
    //========================================================
    mLocationSystem = nullptr;
    mObjectiveManager = nullptr;
}

//============================================================
// Show
//============================================================
void UIExplorationMapOverlay::Show()
{
    //========================================================
    // 二重Show防止
    //========================================================
    if (mVisible)
    {
        return;
    }

    mVisible = true;

    mPending = Action::None;


    //========================================================
    // Mapを開いた瞬間は、
    // Playerが現在いるArea / Floorを表示する
    //========================================================
    SelectCurrentPlayerLocation();

    //========================================================
    // Mouse用Tab位置を再計算
    //========================================================
    BuildTabRects();

    //========================================================
    // Background
    //========================================================
    if (mDimPlate != nullptr)
    {
        mDimPlate->setRenderEnable( true );
        mDimPlate->setExecuteEnable(true );
    }

    //========================================================
    // Main Panel
    //========================================================
    if (mMapPanel != nullptr)
    {
        mMapPanel->setRenderEnable(true);
        mMapPanel->setExecuteEnable( true);
    }


    //========================================================
    // 表示FloorのRoomだけON
    //========================================================
    RefreshRoomVisibility();


    //========================================================
    // Current / Objective / Discovered色を更新
    //========================================================
    RefreshRoomColors();
}


//============================================================
// Hide
//============================================================
void UIExplorationMapOverlay::Hide()
{
    mVisible = false;

    //========================================================
    // Background
    //========================================================
    if (mDimPlate != nullptr)
    {
        mDimPlate->setRenderEnable( false );
        mDimPlate->setExecuteEnable( false);
    }


    //========================================================
    // Panel
    //========================================================
    if (mMapPanel != nullptr)
    {
        mMapPanel->setRenderEnable(false);
        mMapPanel->setExecuteEnable( false );
    }


    //========================================================
    // Roomも全部非表示
    //========================================================
    for (RoomVisual& room :mRooms)
    {
        if (room.Plate == nullptr)
        {
            continue;
        }

        room.Plate->setRenderEnable(false  );
        room.Plate->setExecuteEnable( false );
    }
}

void UIExplorationMapOverlay::Update(float dt)
{
    (void)dt;

    if (!mVisible)
    {
        return;
    }


    //========================================================
    // Area Click
    //========================================================
    for (std::size_t i = 0; i < mAreaRects.size(); ++i)
    {
        if (UI_IsClicked(mAreaRects[i]))
        {
            mAreaIndex = static_cast<int>(i);


            RefreshFloorSelection();

            RefreshRoomVisibility();

            BuildTabRects();

            break;
        }
    }

    //========================================================
    // Floor Click
    //========================================================
    for (std::size_t i = 0; i < mFloorRects.size(); ++i)
    {
        if (UI_IsClicked(mFloorRects[i]))
        {
            mFloorIndex = static_cast<int>(i);

            RefreshRoomVisibility();

            break;
        }
    }

    RefreshRoomColors();
}

void UIExplorationMapOverlay::Draw() const
{
    if (!mVisible)
    {
        return;
    }


    const float sw = static_cast<float>(vnMainFrame::screenWidth);


    //========================================================
    // Title
    //========================================================
    AEUIFont::Scope font(AEUIFont::Style::Heading);
    vnFont::setColor(0xffffffff);

    vnFont::print(42.0f, 30.0f, L"MAP");


    font.Use(AEUIFont::Style::Tab);

    //========================================================
    // Area Tabs
    //========================================================
    for (std::size_t i = 0; i < mAreas.size() && i < mAreaRects.size(); ++i)
    {
        const bool selected = static_cast<int>(i) == mAreaIndex;

        //====================================================
        // 選択中は明るい色
        //====================================================
        vnFont::setColor(selected ? 0xffffd060 : 0xffb0b0b0);

        const UIRect& rect = mAreaRects[i];

        vnFont::print(
            rect.x + 10.0f,
            rect.y + 10.0f,
            selected ? L"> %s <" : L"%s",
            GetAreaName(mAreas[i])
        );
    }


    //========================================================
    // Floor Tabs
    //========================================================
    for (std::size_t i = 0; i < mFloors.size() && i < mFloorRects.size(); ++i)
    {
        const bool selected = static_cast<int>(i) == mFloorIndex;


        vnFont::setColor(selected ? 0xffffd060 : 0xffb0b0b0);


        const UIRect& rect = mFloorRects[i];

        vnFont::print(
            rect.x + 20.0f,
            rect.y + 8.0f,
            selected ? L"[%s]" : L"%s",
            GetFloorName(mFloors[i])
        );
    }


    //========================================================
    // 選択Area/Floor
    //========================================================
    if (mAreas.empty() ||
        mFloors.empty() ||
        mAreaIndex < 0 ||
        mFloorIndex < 0 ||
        mAreaIndex >=
        static_cast<int>(mAreas.size()) ||
        mFloorIndex >=
        static_cast<int>(mFloors.size()))
    {
        return;
    }


    const ExplorationArea selectedArea = mAreas[mAreaIndex];
    const ExplorationFloor selectedFloor = mFloors[mFloorIndex];

    //========================================================
    // Current Location
    //========================================================
    const ExplorationLocation& currentLocation = mLocationSystem->GetCurrentLocation();


    //========================================================
    // Objective
    //========================================================
    const ExplorationObjective* objective = mObjectiveManager->GetCurrentObjective();


    //========================================================
    // Room Names
    //========================================================
    for (const RoomVisual& room : mRooms)
    {
        //====================================================
        // 今表示しているArea/Floorだけ
        //====================================================
        if (room.Area != selectedArea || room.Floor != selectedFloor)
        {
            continue;
        }


        const bool discovered = mLocationSystem->IsRoomDiscovered(room.RoomId);
        const bool current = currentLocation.RoomId == room.RoomId;
        bool objectiveRoom = false;

        if (objective != nullptr && objective->Navigation == ObjectiveNavigationType::Room)
        {
            objectiveRoom = objective->TargetRoomId == room.RoomId;
        }


        //====================================================
        // Room名を公開する条件
        // ・探索済み
        // ・現在地
        // ・Objectiveで明示されたRoom
        // それ以外は???
        //====================================================
        const bool revealName = discovered || current || objectiveRoom;


        if (current)
        {
            vnFont::setColor(0xFFF8FCFF);
        }
        else if (objectiveRoom)
        {
            vnFont::setColor(0xffffd050);
        }
        else if (discovered)
        {
            vnFont::setColor(0xffdddddd);
        }
        else
        {
            vnFont::setColor(0xff707070);
        }


        font.Use(AEUIFont::Style::MapRoom);

        //====================================================
        // Room Name
        //====================================================
        vnFont::print(
            room.Rect.x + 7.0f,
            room.Rect.y + 7.0f,
            L"%s",
            revealName ? room.DisplayName.c_str() : L"???"
        );


        //====================================================
        // Current Player Marker
        //====================================================
        if (current)
        {
            font.Use(AEUIFont::Style::MapMarker);
            vnFont::setColor(0xFFB9EDFF);

            vnFont::print(
                room.Rect.x + 7.0f,
                room.Rect.y + 29.0f,
                L"YOU"
            );
        }


        //====================================================
        // Objective Room Marker
        //====================================================
        if (objectiveRoom && !current)
        {
            font.Use(AEUIFont::Style::MapMarker);
            vnFont::setColor(0xffffd050);


            vnFont::print(
                room.Rect.x + 7.0f,
                room.Rect.y + 29.0f,
                L"OBJECTIVE"
            );
        }
    }


    font.Use(AEUIFont::Style::Detail);

    //========================================================
    // Objective information
    //========================================================
    if (objective != nullptr)
    {
        vnFont::setColor(0xffffffff);

        vnFont::print(
            40.0f,
            static_cast<float>(vnMainFrame::screenHeight) - 100.0f,
            L"Objective: %s",
            objective->Text.c_str()
        );


        //====================================================
        // Puzzle Hint
        // NavigationType::NoneなのでMap Markerは出ないが
        // Hintだけここに表示できる
        //====================================================
        if (!objective->Hint.empty())
        {
            vnFont::setColor(0xffbdbdbd);

            vnFont::print(
                40.0f,
                static_cast<float>(vnMainFrame::screenHeight) - 70.0f,
                L"Hint: %s",
                objective->Hint.c_str()
            );
        }
    }


    font.Use(AEUIFont::Style::Hint);

    //========================================================
    // 操作説明
    //========================================================
    vnFont::setColor(0xff909090);


    vnFont::print(
        sw - 440.0f,
        static_cast<float>(vnMainFrame::screenHeight) - 40.0f,
        L"W/S: Area  A/D: Floor  M: Close"
    );
}


//============================================================
// Consume Action
//============================================================
UIExplorationMapOverlay::Action UIExplorationMapOverlay::ConsumeAction()
{
    const Action result = mPending;

    //========================================================
    // 一度取得したActionは消費する
    //========================================================
    mPending = Action::None;

    return result;
}

void UIExplorationMapOverlay::MoveUIHorizontal(int dir)
{
    if (!mVisible || dir == 0 || mFloors.empty())
    {
        return;
    }

    mFloorIndex += dir;

    if (mFloorIndex < 0)
    {
        mFloorIndex = static_cast<int>(mFloors.size()) - 1;
    }


    if (mFloorIndex >= static_cast<int>(mFloors.size()))
    {
        mFloorIndex = 0;
    }

    RefreshRoomVisibility();
    RefreshRoomColors();
    BuildTabRects();
}

void UIExplorationMapOverlay::MoveUIVertical(int dir)
{
    if (!mVisible || dir == 0 || mAreas.empty())
    {
        return;
    }


    mAreaIndex += dir;


    if (mAreaIndex < 0)
    {
        mAreaIndex = static_cast<int>(mAreas.size()) - 1;
    }


    if (mAreaIndex >= static_cast<int>(mAreas.size()))
    {
        mAreaIndex = 0;
    }

    RefreshFloorSelection();
    RefreshRoomVisibility();
    RefreshRoomColors();
    BuildTabRects();
}

void UIExplorationMapOverlay::Submit()
{
}

void UIExplorationMapOverlay::Cancel()
{
    if (!mVisible)
    {
        return;
    }

    mPending = Action::Close;
}

bool UIExplorationMapOverlay::BuildRoomVisuals(vnScene* scene)
{
    const auto& rooms = mLocationSystem->GetRooms();

    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);


    //========================================================
    // Map表示領域
    //========================================================
    const float mapCenterX = sw * 0.58f;
    const float mapCenterY = sh * 0.56f;
    const float mapWidth = sw * 0.56f;
    const float mapHeight = sh * 0.54f;


    for (const RoomDefinition& room : rooms)
    {
        //====================================================
        // 同じArea / Floor全体のWorld Boundsを求める
        //====================================================
        float sectionMinX = FLT_MAX;
        float sectionMaxX = -FLT_MAX;
        float sectionMinZ = FLT_MAX;
        float sectionMaxZ = -FLT_MAX;


        for (const RoomDefinition& other : rooms)
        {
            if (other.Area != room.Area || other.Floor != room.Floor)
            {
                continue;
            }


            sectionMinX = (std::min)(sectionMinX, other.MinBounds.x);
            sectionMaxX = (std::max)(sectionMaxX, other.MaxBounds.x);
            sectionMinZ = (std::min)(sectionMinZ, other.MinBounds.z);
            sectionMaxZ = (std::max)(sectionMaxZ, other.MaxBounds.z);
        }


        const float worldWidth = (std::max)(sectionMaxX - sectionMinX, 1.0f);
        const float worldDepth = (std::max)(sectionMaxZ - sectionMinZ, 1.0f);


        //====================================================
        // X/Zの縦横比を維持
        //====================================================
        const float scale = (std::min)(mapWidth / worldWidth, mapHeight / worldDepth);

        const float worldCenterX = (sectionMinX + sectionMaxX) * 0.5f;
        const float worldCenterZ = (sectionMinZ + sectionMaxZ) * 0.5f;

        const float roomCenterX = (room.MinBounds.x + room.MaxBounds.x) * 0.5f;
        const float roomCenterZ = (room.MinBounds.z + room.MaxBounds.z) * 0.5f;


        //====================================================
        // Screen Position
        //====================================================
        const float screenX = mapCenterX + (roomCenterX - worldCenterX) * scale;
        const float screenY = mapCenterY + (roomCenterZ - worldCenterZ) * scale;

        float roomWidth = (room.MaxBounds.x - room.MinBounds.x) * scale;
        float roomHeight = (room.MaxBounds.z - room.MinBounds.z) * scale;


        //====================================================
        // 小さすぎるRoomを最低サイズへ
        //====================================================
        roomWidth = (std::max)(roomWidth, 28.0f);
        roomHeight = (std::max)(roomHeight, 28.0f);


        //====================================================
        // Visual
        //====================================================
        RoomVisual visual;
        visual.RoomId = room.Id;
        visual.DisplayName = room.DisplayName;
        visual.Area = room.Area;
        visual.Floor = room.Floor;

        visual.Rect =
        {
            screenX - roomWidth * 0.5f,
            screenY - roomHeight * 0.5f,
            roomWidth,
            roomHeight
        };


        visual.Plate =
            new vnSprite(
                screenX,
                screenY,
                roomWidth - 4.0f,
                roomHeight - 4.0f,
                L""
            );


        visual.Plate->setRenderPriority(3020);

        SetSpriteColor(visual.Plate, 0.20f, 0.22f, 0.24f, 0.90f);


        scene->registerObject(visual.Plate);

        mRooms.push_back(visual);
    }


    return true;
}

std::vector<ExplorationFloor> UIExplorationMapOverlay::BuildFloorList(ExplorationArea area) const
{
    std::vector<ExplorationFloor> result;


    static constexpr ExplorationFloor floorOrder[] =
    {
        ExplorationFloor::Basement1,
        ExplorationFloor::Floor1,
        ExplorationFloor::Floor2,
        ExplorationFloor::Outdoor
    };


    for (ExplorationFloor floor : floorOrder)
    {
        bool exists = false;

        for (const RoomVisual& room : mRooms)
        {
            if (room.Area == area && room.Floor == floor)
            {
                exists = true;
                break;
            }
        }

        if (exists)
        {
            result.push_back(floor);
        }
    }

    return result;
}

void UIExplorationMapOverlay::SelectCurrentPlayerLocation()
{
    if (mLocationSystem == nullptr)
    {
        return;
    }

    //========================================================
    // Areaが1個もない
    //========================================================
    if (mAreas.empty())
    {
        mAreaIndex = 0;
        mFloorIndex = 0;
        mFloors.clear();

        return;
    }

    //========================================================
    // Playerの現在地
    //========================================================
    const ExplorationLocation& location = mLocationSystem->GetCurrentLocation();


    //========================================================
    // 現在Areaを探す
    //========================================================
    bool areaFound = false;


    if (location.Area != ExplorationArea::Unknown)
    {
        for (std::size_t i = 0; i < mAreas.size(); ++i)
        {
            if (mAreas[i] == location.Area)
            {
                mAreaIndex = static_cast<int>(i);
                areaFound = true;
                break;
            }
        }
    }


    //========================================================
    // ForestなどRoom外でMapを開いた場合
    // 既存選択が有効ならそのまま使う
    // 無効なら先頭Areaを表示
    //========================================================
    if (!areaFound)
    {
        if (mAreaIndex < 0 || mAreaIndex >= static_cast<int>(mAreas.size()))
        {
            mAreaIndex = 0;
        }
    }


    //========================================================
    // Areaに存在するFloor一覧を構築
    //========================================================
    mFloors = BuildFloorList(mAreas[mAreaIndex]);


    if (mFloors.empty())
    {
        mFloorIndex = 0;
        return;
    }


    //========================================================
    // 現在Floorを探す
    // 現在Areaと選択Areaが同じ場合のみ同期
    //========================================================
    bool floorFound = false;


    if (location.Area == mAreas[mAreaIndex] && location.Floor != ExplorationFloor::Unknown)
    {
        for (std::size_t i = 0; i < mFloors.size(); ++i)
        {
            if (mFloors[i] == location.Floor)
            {
                mFloorIndex = static_cast<int>(i);
                floorFound = true;
                break;
            }
        }
    }


    //========================================================
    // 現在Floorが見つからない場合
    // 1Fを優先
    // 1Fが存在しなければ先頭Floor
    //========================================================
    if (!floorFound)
    {
        mFloorIndex = 0;

        for (std::size_t i = 0; i < mFloors.size(); ++i)
        {
            if (mFloors[i] == ExplorationFloor::Floor1)
            {
                mFloorIndex = static_cast<int>(i);
                break;
            }
        }
    }
}

void UIExplorationMapOverlay::RefreshFloorSelection()
{
    if (mAreas.empty())
    {
        mFloors.clear();
        mFloorIndex = 0;
        return;
    }


    //========================================================
    // AreaIndex Safety
    //========================================================
    if (mAreaIndex < 0)
    {
        mAreaIndex = 0;
    }

    if (mAreaIndex >= static_cast<int>(mAreas.size()))
    {
        mAreaIndex = static_cast<int>(mAreas.size()) - 1;
    }


    //========================================================
    // 変更前のFloorを覚えておく
    // MainBuilding 1F
    // ↓
    // Annexへ移動
    // Annexにも1Fがあるなら1Fを維持
    //========================================================
    ExplorationFloor previousFloor = ExplorationFloor::Unknown;


    if (!mFloors.empty() && mFloorIndex >= 0 && mFloorIndex < static_cast<int>(mFloors.size()))
    {
        previousFloor = mFloors[mFloorIndex];
    }


    //========================================================
    // 新しいAreaのFloor一覧
    //========================================================
    const ExplorationArea selectedArea = mAreas[mAreaIndex];

    mFloors = BuildFloorList(selectedArea);

    if (mFloors.empty())
    {
        mFloorIndex = 0;
        return;
    }

    //========================================================
    // まず以前のFloorを探す
    //========================================================
    if (previousFloor != ExplorationFloor::Unknown)
    {
        for (std::size_t i = 0; i < mFloors.size(); ++i)
        {
            if (mFloors[i] == previousFloor)
            {
                mFloorIndex = static_cast<int>(i);

                return;
            }
        }
    }

    //========================================================
    // 見つからなければ1F優先
    //========================================================
    for (std::size_t i = 0; i < mFloors.size(); ++i)
    {
        if (mFloors[i] == ExplorationFloor::Floor1)
        {
            mFloorIndex = static_cast<int>(i);
            return;
        }
    }

    //========================================================
    // 最後は先頭
    //========================================================
    mFloorIndex = 0;
}

void UIExplorationMapOverlay::RefreshRoomVisibility()
{
    //========================================================
    // Map非表示ならRoomも全部OFF
    //========================================================
    if (!mVisible)
    {
        for (RoomVisual& room : mRooms)
        {
            if (room.Plate == nullptr)
            {
                continue;
            }

            room.Plate->setRenderEnable(false);
            room.Plate->setExecuteEnable(false);
        }

        return;
    }


    //========================================================
    // Selection Safety
    //========================================================
    if (mAreas.empty() ||
        mFloors.empty() ||
        mAreaIndex < 0 ||
        mFloorIndex < 0 ||
        mAreaIndex >= static_cast<int>(mAreas.size()) ||
        mFloorIndex >= static_cast<int>(mFloors.size()))
    {
        return;
    }


    const ExplorationArea selectedArea = mAreas[mAreaIndex];
    const ExplorationFloor selectedFloor = mFloors[mFloorIndex];

    //========================================================
    // 同じArea / Floorだけ表示
    //========================================================
    for (RoomVisual& room : mRooms)
    {
        if (room.Plate == nullptr)
        {
            continue;
        }

        const bool visible = room.Area == selectedArea && room.Floor == selectedFloor;

        room.Plate->setRenderEnable(visible);
        room.Plate->setExecuteEnable(visible);
    }
}

void UIExplorationMapOverlay::RefreshRoomColors()
{
    if (mLocationSystem == nullptr)
    {
        return;
    }

    const ExplorationLocation& currentLocation = mLocationSystem->GetCurrentLocation();

    const ExplorationObjective* objective = mObjectiveManager->GetCurrentObjective();

    for (RoomVisual& room : mRooms)
    {
        if (room.Plate == nullptr)
        {
            continue;
        }


        //====================================================
        // 未探索
        //====================================================
        bool discovered = mLocationSystem->IsRoomDiscovered(room.RoomId);

        //====================================================
        // Objective Room
        //====================================================
        bool objectiveRoom = false;

        if (objective != nullptr && objective->Navigation == ObjectiveNavigationType::Room)
        {
            objectiveRoom = objective->TargetRoomId == room.RoomId;
        }

        //====================================================
        // Current Room
        //====================================================
        const bool currentRoom = currentLocation.RoomId == room.RoomId;


        //====================================================
        // 色優先順位
        // Current > Objective > Discovered > Unknown
        //====================================================
        if (currentRoom)
        {
            // 現在地
            SetSpriteColor(
                room.Plate,
                0.04f,
                0.13f,
                0.20f,
                0.98f
            );
        }
        else if (objectiveRoom)
        {
            // Objective対象Room
            SetSpriteColor(
                room.Plate,
                0.75f,
                0.55f,
                0.10f,
                0.95f
            );
        }
        else if (discovered)
        {
            // 探索済み
            SetSpriteColor(
                room.Plate,
                0.20f,
                0.24f,
                0.27f,
                0.95f
            );
        }
        else
        {
            // 未探索
            SetSpriteColor(
                room.Plate,
                0.07f,
                0.08f,
                0.09f,
                0.90f
            );
        }
    }
}

void UIExplorationMapOverlay::BuildTabRects()
{
    mAreaRects.clear();

    mFloorRects.clear();

    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);

    //========================================================
    // Area Tabs
    //========================================================
    const float areaWidth = 190.0f;
    const float areaHeight = 42.0f;
    const float areaGap = 14.0f;

    const float areaTotalWidth =
        static_cast<float>(mAreas.size()) *
        areaWidth + static_cast<float>(mAreas.empty() ? 0 : mAreas.size() - 1) * areaGap;

    const float areaStartX = sw * 0.5f - areaTotalWidth * 0.5f;
    const float areaY = sh * 0.075f;


    for (std::size_t i = 0; i < mAreas.size(); ++i)
    {
        UIRect rect;

        rect.x = areaStartX + static_cast<float>(i) * (areaWidth + areaGap);
        rect.y = areaY;
        rect.w = areaWidth;
        rect.h = areaHeight;

        mAreaRects.push_back(rect);
    }

    //========================================================
    // Floor Tabs
    //========================================================
    const float floorWidth = 88.0f;
    const float floorHeight = 36.0f;
    const float floorGap = 10.0f;
    const float floorTotalWidth =
        static_cast<float>(mFloors.size()) *
        floorWidth + static_cast<float>(mFloors.empty() ? 0 : mFloors.size() - 1) * floorGap;

    const float floorStartX = sw * 0.5f - floorTotalWidth * 0.5f;
    const float floorY = sh * 0.135f;


    for (std::size_t i = 0; i < mFloors.size(); ++i)
    {
        UIRect rect;

        rect.x = floorStartX + static_cast<float>(i) * (floorWidth + floorGap);
        rect.y = floorY;
        rect.w = floorWidth;
        rect.h = floorHeight;

        mFloorRects.push_back(rect);
    }
}

const wchar_t* UIExplorationMapOverlay::GetAreaName(ExplorationArea area)
{
    switch (area)
    {
    case ExplorationArea::MainBuilding:
        return L"MAIN BUILDING";


    case ExplorationArea::Courtyard:
        return L"COURTYARD";


    case ExplorationArea::Annex:
        return L"ANNEX";


    case ExplorationArea::TestFacility:
        return L"TEST FACILITY";


    case ExplorationArea::Unknown:
    default:
        return L"UNKNOWN";
    }
}

const wchar_t* UIExplorationMapOverlay::GetFloorName(ExplorationFloor floor)
{
    switch (floor)
    {
    case ExplorationFloor::Basement1:
        return L"B1";


    case ExplorationFloor::Floor1:
        return L"1F";


    case ExplorationFloor::Floor2:
        return L"2F";


    case ExplorationFloor::Outdoor:
        return L"OUTDOOR";


    case ExplorationFloor::Unknown:
    default:
        return L"?";
    }
}

void UIExplorationMapOverlay::SetSpriteColor(vnSprite* sprite, float r, float g, float b, float a)
{
    if (sprite == nullptr)
    {
        return;
    }

    //========================================================
    // Safety
    //========================================================
    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
    a = std::clamp(a, 0.0f, 1.0f);

    //========================================================
    // Sprite全頂点へ同じ色
    //========================================================
    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        sprite->vtx[i].r = r;
        sprite->vtx[i].g = g;
        sprite->vtx[i].b = b;
        sprite->vtx[i].a = a;
    }
}
