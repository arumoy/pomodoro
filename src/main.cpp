// main.cpp - Application entry point for the Tomato Pomodoro timer (Win32, x64).
//
// Features:
//   - Work and Rest duration fields, each with a Minutes/Seconds unit dropdown
//   - Start and Stop buttons
//   - A countdown that automatically alternates between WORK and REST
//   - A ding-bell chime at the end of each phase
//   - Rest of zero skips the rest phase entirely
//   - Garamond UI font and a light-orange theme
//   - Settings menu: default times (persisted) and a looping work .wav
//
// The implementation is split across modules:
//   resource.h        shared IDs/constants
//   app.h/.cpp        shared state and globals
//   settings.*        registry load/save
//   sound.*           ding chime + looping work sound
//   timer.*           core countdown logic
//   settings_dlg.*    "Default Times" dialog
//   worksound_menu.*  work-sound menu handlers
//   ui.*              window class, controls, WndProc
//   main.cpp          this entry point
//
// Build with MSVC: see build.bat

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include "app.h"
#include "resource.h"
#include "settings.h"
#include "ui.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    // Load persisted default time values (HKCU\Software\Tomato\PomodoroTimer)
    // before any controls are created so the fields show the saved defaults.
    LoadSettings();

    // Initialize common controls for modern visual styles.
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    const wchar_t CLASS_NAME[] = L"PomodoroTimerWindow";

    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);

    // Light orange window background. Created once and reused for both the
    // class background and the WM_CTLCOLORSTATIC label backgrounds; freed in
    // WM_DESTROY.
    g_hBgBrush = CreateSolidBrush(APP_BG_COLOR);
    wc.hbrBackground = g_hBgBrush ? g_hBgBrush
                                  : (HBRUSH)(COLOR_BTNFACE + 1);

    // Load our tomato icon from resources. LoadImage with the correct metrics
    // picks the best-matching size from the multi-resolution .ico.
    wc.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON),
                                 IMAGE_ICON,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON),
                                 LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON),
                                   IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON),
                                   LR_DEFAULTCOLOR);
    if (!wc.hIcon) {
        wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);  // fallback
    }

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Failed to register window class.", L"Error",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    // Fixed-size client area: compute window size from desired client rect.
    RECT rc = {0, 0, 420, 300};
    DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX);
    AdjustWindowRect(&rc, style, TRUE);  // TRUE: window has a menu bar

    // Build the menu bar: a top-level "Settings" popup containing the
    // "Default Times..." command, the work-sound items, and "Exit".
    HMENU hMenuBar = CreateMenu();
    HMENU hSettings = CreatePopupMenu();
    AppendMenuW(hSettings, MF_STRING, IDM_SETTINGS, L"Default Times...");
    AppendMenuW(hSettings, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSettings, MF_STRING, IDM_WORK_SOUND,
                L"Select Work Sound (.wav)...");
    AppendMenuW(hSettings, MF_STRING, IDM_CLEAR_SOUND, L"Clear Work Sound");
    AppendMenuW(hSettings, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSettings, MF_STRING, IDM_EXIT, L"Exit");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hSettings, L"Settings");

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Pomodoro Timer",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, hMenuBar, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create window.", L"Error",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) {
            break;  // GetMessage error
        }
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return (int)msg.wParam;
}
