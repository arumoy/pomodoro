// pomodoro.cpp - A native Win32 Pomodoro timer for Windows x64
//
// Features:
//   - Numeric input fields for work length and rest length, each with a
//     Minutes/Seconds unit dropdown
//   - Start and Stop buttons
//   - A countdown display that switches between Work and Rest phases
//   - Plays a ding-bell chime at the end of each phase
//   - Rest of zero skips the rest phase entirely
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
#include <strsafe.h>
#include <mmsystem.h>  // Beep via the kernel; see PlayDingTune

// ---------------------------------------------------------------------------
// Control identifiers
// ---------------------------------------------------------------------------
#define IDC_WORK_EDIT   1001
#define IDC_REST_EDIT   1002
#define IDC_START_BTN   1003
#define IDC_STOP_BTN    1004
#define IDC_PHASE_LABEL 1005
#define IDC_TIME_LABEL  1006
#define IDC_WORK_UNIT   1007
#define IDC_REST_UNIT   1008

// Resource IDs (must match pomodoro.rc).
#define IDI_APPICON     101

// Unit combo-box item indices.
#define UNIT_MINUTES    0
#define UNIT_SECONDS    1

#define TIMER_ID        1
#define TIMER_INTERVAL  1000  // 1 second ticks

// Input bounds (defense-in-depth validation on the numeric fields), expressed
// in seconds. Work must be at least 1 second; rest may be 0 (skip rest).
// The numeric entry itself is capped at 3 digits (<= 999) per unit.
#define MIN_WORK_SECONDS 1
#define MIN_REST_SECONDS 0
#define MAX_TOTAL_SECONDS 36000  // 600 minutes
#define MAX_FIELD_VALUE   999    // max raw number typed into a field

// ---------------------------------------------------------------------------
// Application state
// ---------------------------------------------------------------------------
typedef enum { PHASE_WORK, PHASE_REST } Phase;

typedef struct {
    HWND hWorkEdit;
    HWND hRestEdit;
    HWND hWorkUnit;
    HWND hRestUnit;
    HWND hStartBtn;
    HWND hStopBtn;
    HWND hPhaseLabel;
    HWND hTimeLabel;

    BOOL  running;
    Phase phase;
    int   workSeconds;      // configured work duration in seconds
    int   restSeconds;      // configured rest duration in seconds (0 = skip)
    int   remainingSeconds; // seconds left in the current phase
} AppState;

// Light orange window background.
#define APP_BG_COLOR    RGB(255, 224, 178)
#define APP_FONT_NAME   L"Garamond"

static AppState g_app = {0};
static HFONT    g_hTimeFont  = NULL;
static HFONT    g_hPhaseFont = NULL;
static HFONT    g_hUiFont    = NULL;   // Garamond for inputs/buttons/labels
static HBRUSH   g_hBgBrush   = NULL;   // light orange background brush

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Read a field's numeric text plus its unit combo box and convert to a total
// number of seconds. Validates that the raw number is all digits and within
// [0, MAX_FIELD_VALUE], and that the resulting seconds are within
// [minSeconds, MAX_TOTAL_SECONDS]. Returns TRUE on success, writing *out.
// Rejects invalid input outright rather than silently clamping.
static BOOL ReadDurationField(HWND hEdit, HWND hUnit, int minSeconds,
                              int *out) {
    wchar_t buf[16] = {0};
    int len = GetWindowTextW(hEdit, buf, ARRAYSIZE(buf));
    if (len <= 0) {
        return FALSE;
    }

    // Ensure every character is a decimal digit. The edit control uses
    // ES_NUMBER, but we validate again here (defense in depth).
    for (int i = 0; i < len; ++i) {
        if (buf[i] < L'0' || buf[i] > L'9') {
            return FALSE;
        }
    }

    // Parse with overflow awareness.
    long value = 0;
    for (int i = 0; i < len; ++i) {
        value = value * 10 + (buf[i] - L'0');
        if (value > MAX_FIELD_VALUE) {
            return FALSE;  // too large, reject
        }
    }

    // Determine the unit from the combo box; default to minutes if the query
    // fails for any reason.
    LRESULT sel = SendMessageW(hUnit, CB_GETCURSEL, 0, 0);
    long seconds;
    if (sel == UNIT_SECONDS) {
        seconds = value;
    } else {
        seconds = value * 60;  // minutes -> seconds
    }

    if (seconds < minSeconds || seconds > MAX_TOTAL_SECONDS) {
        return FALSE;
    }

    *out = (int)seconds;
    return TRUE;
}

// Worker thread that plays a short "ding bell" ascending chime so the UI
// thread (and its 1-second timer) is never blocked by the synchronous Beep
// calls. Frequencies/durations in Hz/ms form a simple pleasant arpeggio.
static DWORD WINAPI DingTuneProc(LPVOID param) {
    (void)param;
    // Ascending chime: E6, G6, C7, then a held E7 - a cheerful "ding" bell.
    static const struct { DWORD freq; DWORD ms; } kNotes[] = {
        { 1319, 120 },  // E6
        { 1568, 120 },  // G6
        { 2093, 120 },  // C7
        { 2637, 320 },  // E7 (held)
    };
    for (int i = 0; i < (int)ARRAYSIZE(kNotes); ++i) {
        Beep(kNotes[i].freq, kNotes[i].ms);
    }
    return 0;
}

// Fire the ding-bell tune without blocking the caller.
static void PlayDingTune(void) {
    HANDLE h = CreateThread(NULL, 0, DingTuneProc, NULL, 0, NULL);
    if (h) {
        CloseHandle(h);  // fire-and-forget; thread self-terminates
    } else {
        MessageBeep(MB_ICONASTERISK);  // fallback if the thread can't start
    }
}

// Update the countdown label (MM:SS) and the phase label.
static void UpdateDisplay(void) {
    int minutes = g_app.remainingSeconds / 60;
    int seconds = g_app.remainingSeconds % 60;

    wchar_t timeBuf[16];
    StringCchPrintfW(timeBuf, ARRAYSIZE(timeBuf), L"%02d:%02d", minutes, seconds);
    SetWindowTextW(g_app.hTimeLabel, timeBuf);

    SetWindowTextW(g_app.hPhaseLabel,
                   g_app.phase == PHASE_WORK ? L"WORK" : L"REST");
}

// Enable/disable controls based on running state.
static void SetRunningUI(BOOL running) {
    EnableWindow(g_app.hStartBtn, !running);
    EnableWindow(g_app.hStopBtn,  running);
    EnableWindow(g_app.hWorkEdit, !running);
    EnableWindow(g_app.hRestEdit, !running);
    EnableWindow(g_app.hWorkUnit, !running);
    EnableWindow(g_app.hRestUnit, !running);
}

static void StartTimer(HWND hwnd) {
    int work = 0, rest = 0;
    if (!ReadDurationField(g_app.hWorkEdit, g_app.hWorkUnit,
                           MIN_WORK_SECONDS, &work)) {
        MessageBoxW(hwnd,
                    L"Please enter a valid Work duration.\n\n"
                    L"Minutes: 1 to 600.  Seconds: 1 to 999 "
                    L"(total max 600 minutes).",
                    L"Invalid input", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!ReadDurationField(g_app.hRestEdit, g_app.hRestUnit,
                           MIN_REST_SECONDS, &rest)) {
        MessageBoxW(hwnd,
                    L"Please enter a valid Rest duration.\n\n"
                    L"Use 0 to skip the rest phase. "
                    L"Minutes: 0 to 600.  Seconds: 0 to 999 "
                    L"(total max 600 minutes).",
                    L"Invalid input", MB_OK | MB_ICONWARNING);
        return;
    }

    g_app.workSeconds      = work;
    g_app.restSeconds      = rest;
    g_app.phase            = PHASE_WORK;
    g_app.remainingSeconds = work;
    g_app.running          = TRUE;

    UpdateDisplay();
    SetRunningUI(TRUE);

    if (!SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL)) {
        MessageBoxW(hwnd, L"Failed to start the timer.", L"Error",
                    MB_OK | MB_ICONERROR);
        g_app.running = FALSE;
        SetRunningUI(FALSE);
    }
}

static void StopTimer(HWND hwnd) {
    if (g_app.running) {
        KillTimer(hwnd, TIMER_ID);
        g_app.running = FALSE;
    }
    SetRunningUI(FALSE);
    SetWindowTextW(g_app.hPhaseLabel, L"STOPPED");
    SetWindowTextW(g_app.hTimeLabel,  L"00:00");
}

// Advance one second. Handles phase transitions when a phase hits zero.
static void OnTick(HWND hwnd) {
    if (!g_app.running) {
        return;
    }

    if (g_app.remainingSeconds > 0) {
        g_app.remainingSeconds--;
    }

    if (g_app.remainingSeconds <= 0) {
        // Phase complete: ring the bell, flash the window, then switch phases.
        FlashWindow(hwnd, TRUE);
        PlayDingTune();

        if (g_app.phase == PHASE_WORK) {
            if (g_app.restSeconds > 0) {
                // Begin the rest phase.
                g_app.phase            = PHASE_REST;
                g_app.remainingSeconds = g_app.restSeconds;
            } else {
                // Zero rest: skip rest and start the next work phase.
                g_app.phase            = PHASE_WORK;
                g_app.remainingSeconds = g_app.workSeconds;
            }
        } else {
            // Rest finished: back to work.
            g_app.phase            = PHASE_WORK;
            g_app.remainingSeconds = g_app.workSeconds;
        }
    }

    UpdateDisplay();
}

// ---------------------------------------------------------------------------
// Control creation
// ---------------------------------------------------------------------------
static void CreateControls(HWND hwnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

    // Labels for the input fields
    HWND hWorkLbl = CreateWindowExW(0, L"STATIC", L"Work:",
                    WS_CHILD | WS_VISIBLE | SS_RIGHT,
                    20, 24, 90, 22, hwnd, NULL, hInst, NULL);
    HWND hRestLbl = CreateWindowExW(0, L"STATIC", L"Rest:",
                    WS_CHILD | WS_VISIBLE | SS_RIGHT,
                    20, 58, 90, 22, hwnd, NULL, hInst, NULL);

    // Numeric edit fields. ES_NUMBER restricts to digits at the UI layer.
    g_app.hWorkEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"25",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_LEFT,
        120, 22, 70, 24, hwnd, (HMENU)(INT_PTR)IDC_WORK_EDIT, hInst, NULL);
    SendMessageW(g_app.hWorkEdit, EM_SETLIMITTEXT, 3, 0);

    g_app.hRestEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"5",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_LEFT,
        120, 56, 70, 24, hwnd, (HMENU)(INT_PTR)IDC_REST_EDIT, hInst, NULL);
    SendMessageW(g_app.hRestEdit, EM_SETLIMITTEXT, 3, 0);

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
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
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
        if (g_hTimeFont)  DeleteObject(g_hTimeFont);
        if (g_hPhaseFont) DeleteObject(g_hPhaseFont);
        if (g_hUiFont)    DeleteObject(g_hUiFont);
        if (g_hBgBrush)   DeleteObject(g_hBgBrush);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

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
    AdjustWindowRect(&rc, style, FALSE);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Pomodoro Timer",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL);

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
