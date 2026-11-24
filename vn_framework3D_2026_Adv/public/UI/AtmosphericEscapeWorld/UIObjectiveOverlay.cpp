#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "UIObjectiveOverlay.h"

bool UIObjectiveOverlay::Initialize(vnScene* scene, const ObjectiveManager* objectiveManager, const ExplorationLocationSystem* locationSystem)
{
	if (scene == nullptr || objectiveManager == nullptr || locationSystem == nullptr) return false;

	mObjectiveManager = objectiveManager;

	mLocationSystem = locationSystem;

	const float sw = static_cast<float>(vnMainFrame::screenWidth);
	const float sh = static_cast<float>(vnMainFrame::screenHeight);

    //========================================================
    // Dim
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
        0.82f
    );

    scene->registerObject(mDimPlate);

    //========================================================
    // Panel
    //========================================================
    mMainPanel =
        new vnSprite(
            sw * 0.5f,
            sh * 0.52f,
            sw * 0.74f,
            sh * 0.72f,
            L""
        );


    mMainPanel->setRenderPriority(3010);


    SetSpriteColor(
        mMainPanel,
        0.035f,
        0.045f,
        0.050f,
        0.96f
    );

    scene->registerObject(mMainPanel);


    Hide();


    return true;
}

void UIObjectiveOverlay::Terminate(vnScene* scene)
{
    if (scene == nullptr)
    {
        return;
    }


    if (mMainPanel != nullptr)
    {
        scene->deleteObject(mMainPanel);
        mMainPanel = nullptr;
    }


    if (mDimPlate != nullptr)
    {
        scene->deleteObject(mDimPlate);
        mDimPlate = nullptr;
    }

    mObjectiveManager = nullptr;
    mLocationSystem = nullptr;
    mVisible = false;
}

void UIObjectiveOverlay::Show()
{
    mVisible = true;
    SetRender(mDimPlate, true);
    SetRender(mMainPanel, true);
}


void UIObjectiveOverlay::Hide()
{
    mVisible = false;
    SetRender(mDimPlate, false);
    SetRender(mMainPanel, false);
}

void UIObjectiveOverlay::Update(float dt)
{
    (void)dt;
}

void UIObjectiveOverlay::Draw() const
{
    if (!mVisible || mObjectiveManager == nullptr)
    {
        return;
    }


    const float sw = static_cast<float>(vnMainFrame::screenWidth);
    const float sh = static_cast<float>(vnMainFrame::screenHeight);


    //========================================================
    // Title
    //========================================================
    AEUIFont::Scope font(AEUIFont::Style::Heading);
    vnFont::setColor(0xffffffff);


    vnFont::print(
        sw * 0.16f,
        sh * 0.22f,
        L"CURRENT OBJECTIVE"
    );


    font.Use(AEUIFont::Style::Label);

    //========================================================
    // Current Objective
    //========================================================
    const ExplorationObjective* objective = mObjectiveManager->GetCurrentObjective();


    if (objective == nullptr)
    {
        vnFont::setColor(0xff909090);


        vnFont::print(
            sw * 0.16f,
            sh * 0.31f,
            L"No active objective."
        );


        return;
    }


    //========================================================
    // Main Objective
    //========================================================
    vnFont::setColor(0xffffd060);


    vnFont::print(
        sw * 0.16f,
        sh * 0.31f,
        L"%s",
        objective->Text.c_str()
    );


    font.Use(AEUIFont::Style::Detail);

    //========================================================
    // Hint
    //========================================================
    if (!objective->Hint.empty())
    {
        vnFont::setColor(0xffc8c8c8);

        vnFont::print(
            sw * 0.16f,
            sh * 0.40f,
            L"Hint:"
        );


        vnFont::print(
            sw * 0.19f,
            sh * 0.45f,
            L"%s",
            objective->Hint.c_str()
        );
    }


    font.Use(AEUIFont::Style::Hint);

    //========================================================
    // Navigation情報
    //========================================================
    vnFont::setColor(0xffaaaaaa);

    const float navigationY = sh * 0.58f;

    switch (objective->Navigation)
    {
        //========================================================
        // Puzzleなど
        // 具体的な場所を表示しない
        //========================================================
    case ObjectiveNavigationType::None:
    {
        vnFont::print(
            sw * 0.16f,
            navigationY,
            L"Navigation: Search the area."
        );

        break;
    }


    //========================================================
    // Roomまで案内
    //========================================================
    case ObjectiveNavigationType::Room:
    {
        const RoomDefinition* room = mLocationSystem->FindRoom(objective->TargetRoomId);

        if (room != nullptr)
        {
            vnFont::print(
                sw * 0.16f,
                navigationY,
                L"Destination: %s",
                room->DisplayName.c_str()
            );
        }

        break;
    }


    //========================================================
    // 正確な位置
    //========================================================
    case ObjectiveNavigationType::WorldPosition:
    {
        vnFont::print(
            sw * 0.16f,
            navigationY,
            L"Destination marked on map."
        );

        break;
    }
    }
}

void UIObjectiveOverlay::MoveUIHorizontal(int dir)
{
    (void)dir;
}

void UIObjectiveOverlay::MoveUIVertical(int dir)
{
    (void)dir;
}

void UIObjectiveOverlay::Submit()
{
}

void UIObjectiveOverlay::Cancel()
{
}

void UIObjectiveOverlay::SetRender(vnSprite* sprite,bool enabled)
{
    if (sprite == nullptr)
    {
        return;
    }

    sprite->setRenderEnable(enabled);
    sprite->setExecuteEnable(enabled);
}


void UIObjectiveOverlay::SetSpriteColor(
    vnSprite* sprite,
    float r,
    float g,
    float b,
    float a
)
{
    if (sprite == nullptr)
    {
        return;
    }

    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
    a = std::clamp(a, 0.0f, 1.0f);

    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        sprite->vtx[i].r = r;
        sprite->vtx[i].g = g;
        sprite->vtx[i].b = b;
        sprite->vtx[i].a = a;
    }
}