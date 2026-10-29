// main.cpp - Application entry point for the Tomato Pomodoro timer (Win32, x64).
//
// Object-oriented design (one class per responsibility):
//   gdi.h           Phase enum + GdiObject<T> RAII handle wrappers
//   Settings        registry-backed persistence          (settings.*)
//   WorkSound       ding chime + looping work sound       (sound.*)
//   PomodoroTimer   countdown state machine + ITimerView  (timer.*)
//   SettingsDialog  modal "Default Times" dialog           (settings_dlg.*)
//   MainWindow      owns the controls/timer/sound/settings (ui.*)
//   main.cpp        constructs MainWindow and runs the loop
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
#include "ui.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    // Modern visual styles for the common controls.
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    MainWindow window(hInstance);   // loads settings in its constructor
    if (!window.create(nCmdShow)) {
        return 1;
    }

    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
        if (bRet == -1) {
            break;  // GetMessage error
        }
        if (!IsDialogMessageW(window.handle(), &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
