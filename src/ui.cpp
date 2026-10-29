// ui.cpp - Main window: control creation and the window procedure.
#include "ui.h"
#include "app.h"
#include "resource.h"
#include "timer.h"
#include "sound.h"
#include "settings_dlg.h"
#include "worksound_menu.h"
#include <strsafe.h>

// ---------------------------------------------------------------------------
// Control creation
// ---------------------------------------------------------------------------
void CreateControls(HWND hwnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

    // Labels for the input fields
    HWND hWorkLbl = CreateWindowExW(0, L"STATIC", L"Work:",
                    WS_CHILD | WS_VISIBLE | SS_RIGHT,
                    20, 24, 90, 22, hwnd, NULL, hInst, NULL);
    HWND hRestLbl = CreateWindowExW(0, L"STATIC", L"Rest:",
                    WS_CHILD | WS_VISIBLE | SS_RIGHT,
                    20, 58, 90, 22, hwnd, NULL, hInst, NULL);

    // Numeric edit fields. ES_NUMBER restricts to digits at the UI layer.
    // Initial text is set below from the persisted default settings.
    g_app.hWorkEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_LEFT,
        120, 22, 70, 24, hwnd, (HMENU)(INT_PTR)IDC_WORK_EDIT, hInst, NULL);
    SendMessageW(g_app.hWorkEdit, EM_SETLIMITTEXT, 3, 0);

    g_app.hRestEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_LEFT,
        120, 56, 70, 24, hwnd, (HMENU)(INT_PTR)IDC_REST_EDIT, hInst, NULL);
    SendMessageW(g_app.hRestEdit, EM_SETLIMITTEXT, 3, 0);

    // Apply the persisted default time values (in minutes).
    {
        wchar_t num[16];
        StringCchPrintfW(num, ARRAYSIZE(num), L"%d", g_defaultWorkMin);
        SetWindowTextW(g_app.hWorkEdit, num);
        StringCchPrintfW(num, ARRAYSIZE(num), L"%d", g_defaultRestMin);
        SetWindowTextW(g_app.hRestEdit, num);
    }

    // Unit dropdowns (Minutes / Seconds) for each field. Use a drop-down list
    // (CBS_DROPDOWNLIST) so the user can only pick from the two options.
    // The list height argument is the full control height including the
    // dropped-down list area.
    g_app.hWorkUnit = CreateWindowExW(
        0, L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        200, 22, 110, 120, hwnd, (HMENU)(INT_PTR)IDC_WORK_UNIT, hInst, NULL);
    SendMessageW(g_app.hWorkUnit, CB_ADDSTRING, 0, (LPARAM)L"Minutes");
    SendMessageW(g_app.hWorkUnit, CB_ADDSTRING, 0, (LPARAM)L"Seconds");
    SendMessageW(g_app.hWorkUnit, CB_SETCURSEL, UNIT_MINUTES, 0);

    g_app.hRestUnit = CreateWindowExW(
        0, L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        200, 56, 110, 120, hwnd, (HMENU)(INT_PTR)IDC_REST_UNIT, hInst, NULL);
    SendMessageW(g_app.hRestUnit, CB_ADDSTRING, 0, (LPARAM)L"Minutes");
    SendMessageW(g_app.hRestUnit, CB_ADDSTRING, 0, (LPARAM)L"Seconds");
    SendMessageW(g_app.hRestUnit, CB_SETCURSEL, UNIT_MINUTES, 0);

    // Phase label
    g_app.hPhaseLabel = CreateWindowExW(
        0, L"STATIC", L"READY",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 100, 380, 28, hwnd, (HMENU)(INT_PTR)IDC_PHASE_LABEL, hInst, NULL);

    // Big countdown label
    g_app.hTimeLabel = CreateWindowExW(
        0, L"STATIC", L"00:00",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 128, 380, 64, hwnd, (HMENU)(INT_PTR)IDC_TIME_LABEL, hInst, NULL);

    // Start / Stop buttons
    g_app.hStartBtn = CreateWindowExW(
        0, L"BUTTON", L"Start",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        100, 206, 100, 34, hwnd, (HMENU)(INT_PTR)IDC_START_BTN, hInst, NULL);

    g_app.hStopBtn = CreateWindowExW(
        0, L"BUTTON", L"Stop",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        220, 206, 100, 34, hwnd, (HMENU)(INT_PTR)IDC_STOP_BTN, hInst, NULL);

    // Fonts - Garamond throughout (serif family hint FF_ROMAN).
    g_hTimeFont = CreateFontW(52, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_ROMAN, APP_FONT_NAME);
    g_hPhaseFont = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_TT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_ROMAN, APP_FONT_NAME);
    // Shared UI font for edit fields, combos and buttons.
    g_hUiFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_TT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_ROMAN, APP_FONT_NAME);
    if (g_hTimeFont) {
        SendMessageW(g_app.hTimeLabel, WM_SETFONT, (WPARAM)g_hTimeFont, TRUE);
    }
    if (g_hPhaseFont) {
        SendMessageW(g_app.hPhaseLabel, WM_SETFONT, (WPARAM)g_hPhaseFont, TRUE);
    }

    // Apply the Garamond UI font to the remaining controls. Fall back to the
    // stock GUI font if Garamond could not be created.
    HFONT hUi = g_hUiFont ? g_hUiFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessageW(g_app.hWorkEdit, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(g_app.hRestEdit, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(g_app.hWorkUnit, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(g_app.hRestUnit, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(g_app.hStartBtn, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(g_app.hStopBtn,  WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(hWorkLbl, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(hRestLbl, WM_SETFONT, (WPARAM)hUi, TRUE);

    SetRunningUI(FALSE);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_START_BTN:
            StartTimer(hwnd);
            return 0;
        case IDC_STOP_BTN:
            StopTimer(hwnd);
            return 0;
        case IDM_SETTINGS: {
            HINSTANCE hInst =
                (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
            DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_SETTINGS), hwnd,
                       SettingsDlgProc);
            return 0;
        }
        case IDM_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case IDM_WORK_SOUND:
            ChooseWorkSound(hwnd);
            return 0;
        case IDM_CLEAR_SOUND:
            ClearWorkSound(hwnd);
            return 0;
        }
        break;

    case WM_TIMER:
        if (wParam == TIMER_ID) {
            OnTick(hwnd);
        }
        return 0;

    case WM_CTLCOLORSTATIC: {
        // Paint static labels transparently over the light orange background.
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)g_hBgBrush;
    }

    case WM_DESTROY:
        if (g_app.running) {
            KillTimer(hwnd, TIMER_ID);
        }
        StopWorkSound();   // stop any looping audio before exit
        if (g_hTimeFont)  DeleteObject(g_hTimeFont);
        if (g_hPhaseFont) DeleteObject(g_hPhaseFont);
        if (g_hUiFont)    DeleteObject(g_hUiFont);
        if (g_hBgBrush)   DeleteObject(g_hBgBrush);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
