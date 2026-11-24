#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "UICommon.h"

extern HWND hWnd;

void UI_GetMouseVirtual(float& outX, float& outY)
{
    // 画面上のカーソル位置 → クライアント座標へ
    POINT pt;
    ::GetCursorPos(&pt);
    ::ScreenToClient(hWnd, &pt);

    // 実クライアントサイズ
    RECT rc;
    ::GetClientRect(hWnd, &rc);
    const float clientW = (float)(rc.right - rc.left);
    const float clientH = (float)(rc.bottom - rc.top);

    // 仮想UI解像度
    const float virtualW = (float)vnMainFrame::screenWidth;
    const float virtualH = (float)vnMainFrame::screenHeight;

    // 実→仮想へスケール
    const float sx = (clientW > 1.0f) ? (virtualW / clientW) : 1.0f;
    const float sy = (clientH > 1.0f) ? (virtualH / clientH) : 1.0f;

    outX = (float)pt.x * sx;
    outY = (float)pt.y * sy;
}

bool UI_IsHover(const UIRect& r)
{
    float mx, my;
    UI_GetMouseVirtual(mx, my);

    return (mx >= r.x && mx <= r.x + r.w &&
        my >= r.y && my <= r.y + r.h);
}

bool UI_IsClicked(const UIRect& r)
{
    return UI_IsHover(r) && vnMouse::trgL();
}
