#pragma once

#include "../input/IUIControllable.h"

class vnScene;
class vnSprite;

class UIAtmosphericEscapeOptions final : public IUIControllable
{
public:
    // 1つの入力で複数項目が変化した場合にも、各変更を取りこぼさない。
    enum Change : unsigned int { Audio = 1, Post = 2, Shadows = 4, Camera = 8 };

    void Initialize(vnScene* scene);
    void Terminate(vnScene* scene);
    void Show();
    void Hide();
    bool IsVisible() const { return mVisible; }

    // UIInputHandler::HandleInputの後に呼ぶ。
    void Update(int padId);
    void Draw() const;
    bool WantsClose() const { return mWantsClose; }
    unsigned int ConsumeChanges();

    void MoveUIHorizontal(int dir) override;
    void MoveUIVertical(int dir) override;
    void Submit() override;
    void Cancel() override;

private:
    void MovePage(int dir);
    void SelectRow(int index);
    void SetPlatesVisible(bool visible);

    vnSprite* mBackground = nullptr;
    vnSprite* mTabFocus = nullptr;
    vnSprite* mRowFocus = nullptr;
    bool mVisible = false;
    bool mWantsClose = false;
    unsigned int mChanges = 0;
    int mPage = 0;
    int mRow = 0;
};
