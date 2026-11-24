// vn_framework3D_2026_Adv.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "framework.h"
#include "vn_framework3D_2026_Adv.h"

#include "framework/vn_environment.h"

#define MAX_LOADSTRING 100

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名

HWND hWnd = NULL;								// ウィンドウのハンドル

//クライアント領域のサイズ
int screen_width = SCREEN_WIDTH;
int screen_height = SCREEN_HEIGHT;

bool  g_isFullscreen = false;
RECT  g_windowRect = {};;     // ウィンドウ時の位置/サイズ
DWORD sWindowStyle;    // ウィンドウ時のスタイル

void ToggleFullscreen(HWND hWnd);

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
//INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

namespace
{
    bool gHideGameplayCursor = false;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: ここにコードを挿入してください。

    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_VNFRAMEWORK3D2026ADV, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // アプリケーション初期化の実行:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VNFRAMEWORK3D2026ADV));

    MSG msg;

    //ロケールの設定(日本)
    setlocale(LC_CTYPE, "JPN");

    //メインフレームの初期化
    vnMainFrame::initialize(hWnd, hInst, screen_width, screen_height);

    // メイン メッセージ ループ:
    while (true)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                goto END;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        //メインフレームの実行
        vnMainFrame::execute();
    }

END:

    //メインフレームの終了
    vnMainFrame::terminate();

    return (int)msg.wParam;
}



//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VNFRAMEWORK3D2026ADV));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL; //MAKEINTRESOURCEW(IDC_VNFRAMEWORK3D2026ADV);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します
//
//   コメント:
//
//        この関数で、グローバル変数でインスタンス ハンドルを保存し、
//        メイン プログラム ウィンドウを作成および表示します。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // グローバル変数にインスタンス ハンドルを格納する

   /*HWND*/ hWnd = CreateWindowW(szWindowClass, szTitle, (WS_OVERLAPPED | WS_SYSMENU),  //WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   //クライアント領域が希望のサイズになるようにウィンドウ全体のサイズを計算して再設定
   RECT rw, rc;
   ::GetWindowRect(hWnd, &rw); // ウィンドウ全体のサイズ
   ::GetClientRect(hWnd, &rc); // クライアント領域のサイズ

   int new_width = (rw.right - rw.left) - (rc.right - rc.left) + screen_width;
   int new_height = (rw.bottom - rw.top) - (rc.bottom - rc.top) + screen_height;

   ::SetWindowPos(hWnd, nullptr, 0, 0, new_width, new_height, SWP_NOMOVE | SWP_NOZORDER);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   //WM_PAINTを発生させないようにする
   ValidateRect(hWnd, 0);

   //ToggleFullscreen(hWnd);

   return TRUE;
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND  - アプリケーション メニューの処理
//  WM_PAINT    - メイン ウィンドウを描画する
//  WM_DESTROY  - 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
#if 0
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 選択されたメニューの解析:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: HDC を使用する描画コードをここに追加してください...
            EndPaint(hWnd, &ps);
        }
        break;
#endif
    case WM_SETCURSOR:
    {
        // ゲームが前面にありカーソルがゲーム画面内にあるときだけ隠す
        if (gHideGameplayCursor && LOWORD(lParam) == HTCLIENT && GetForegroundWindow() == hWnd)
        {
            SetCursor(nullptr);
            return TRUE;
        }

        // UI表示中やタイトルバーではWindowsにカーソルを設定させる
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }

    case WM_MOUSEMOVE:              //マウス座標の設定
        DirectInput::setMousePosition(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_KEYDOWN:				//キーが押された
        //if (wParam == VK_ESCAPE)	//ESCキー
        //{
        //    PostQuitMessage(0);		//アプリケーションを終了する
        //}
        if (wParam == VK_F11)
        {
            ToggleFullscreen(hWnd);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

#if 0
// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
#endif
void ToggleFullscreen(HWND hWnd)
{
    if (!g_isFullscreen)
    {
        // ----------------------------
        // フルスクリーンにする
        // ----------------------------

        // 現在のウィンドウ位置/サイズを保存（戻す用）
        GetWindowRect(hWnd, &g_windowRect);

        // 現在のスタイルを取得
        DWORD style = GetWindowLong(hWnd, GWL_STYLE);

        // 枠を消す（タイトルバー等を外す）
        SetWindowLong(hWnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);

        // モニタ領域を取得（プライマリモニタ）
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY), &mi);

        // モニタいっぱいに広げる（解像度変更はしない）
        SetWindowPos(
            hWnd,
            HWND_TOP,
            mi.rcMonitor.left,
            mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );

        g_isFullscreen = true;
    }
    else
    {
        // ----------------------------
        // ウィンドウに戻す
        // ----------------------------

        // 保存しておいたスタイルに戻す
        SetWindowLong(hWnd, GWL_STYLE, sWindowStyle);

        // 保存しておいた位置・サイズに戻す
        SetWindowPos(
            hWnd,
            NULL,
            g_windowRect.left,
            g_windowRect.top,
            g_windowRect.right - g_windowRect.left,
            g_windowRect.bottom - g_windowRect.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );

        g_isFullscreen = false;
    }
}

void SetGameplayCursorHidden(bool hidden)
{
    // render() から毎フレーム呼ばれるので状態が変わったときだけ処理する
    if (gHideGameplayCursor == hidden)
    {
        return;
    }

    gHideGameplayCursor = hidden;

    POINT cursorPos{};
    RECT clientRect{};

    // カーソルがゲームウィンドウのクライアント領域にあるときだけ即時更新する
    // これでマウスを動かさずにマップを開閉してもその場で表示が切り替わる
    if (hWnd != nullptr &&
        GetCursorPos(&cursorPos) &&
        WindowFromPoint(cursorPos) == hWnd &&
        ScreenToClient(hWnd, &cursorPos) &&
        GetClientRect(hWnd, &clientRect) &&
        PtInRect(&clientRect, cursorPos))
    {
        SendMessageW(
            hWnd,
            WM_SETCURSOR,
            reinterpret_cast<WPARAM>(hWnd),
            MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)
        );
    }
}