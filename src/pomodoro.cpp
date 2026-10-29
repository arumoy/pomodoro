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
#include <mmsystem.h>   // Beep + PlaySound (looping work sound)
#include <commdlg.h>    // GetOpenFileNameW (choose the .wav)

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
#define IDD_SETTINGS    201   // Settings dialog template
#define IDC_SET_WORK    202   // dialog: default work minutes edit
#define IDC_SET_REST    203   // dialog: default rest minutes edit

// Menu command IDs.
#define IDM_SETTINGS    301   // Settings -> Default Times...
#define IDM_EXIT        302   // Settings -> Exit
#define IDM_WORK_SOUND  303   // Settings -> Select Work Sound (.wav)...
#define IDM_CLEAR_SOUND 304   // Settings -> Clear Work Sound

// Persistent-settings bounds (minutes).
#define MIN_DEFAULT_WORK_MIN 1
#define MIN_DEFAULT_REST_MIN 0
#define MAX_DEFAULT_MIN      600
#define DEFAULT_WORK_MIN     25   // factory default if nothing is stored
#define DEFAULT_REST_MIN     5

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

// Persisted default time values (in minutes). Loaded at startup from the
// registry and applied to the input fields; editable via Settings.
static int g_defaultWorkMin = DEFAULT_WORK_MIN;
static int g_defaultRestMin = DEFAULT_REST_MIN;

// Path to a user-chosen .wav that loops while the timer is in the WORK phase.
// Empty string means "no work sound". Persisted in the registry.
static wchar_t g_workSoundPath[MAX_PATH] = L"";
static BOOL    g_workSoundPlaying = FALSE;  // is the loop currently playing?

// Standard per-user settings location on Windows: HKCU\Software\...
#define SETTINGS_SUBKEY  L"Software\\Tomato\\PomodoroTimer"
#define VAL_WORK_MIN     L"DefaultWorkMinutes"
#define VAL_REST_MIN     L"DefaultRestMinutes"
#define VAL_WORK_SOUND   L"WorkSoundPath"

// Read a DWORD value from the settings key. Returns TRUE and writes *out when
// the value exists; otherwise leaves *out untouched and returns FALSE.
static BOOL RegReadDword(const wchar_t *valueName, DWORD *out) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_SUBKEY, 0, KEY_QUERY_VALUE,
                      &hKey) != ERROR_SUCCESS) {
        return FALSE;
    }
    DWORD type = 0, data = 0, cb = sizeof(data);
    LONG rc = RegQueryValueExW(hKey, valueName, NULL, &type,
                               (LPBYTE)&data, &cb);
    RegCloseKey(hKey);
    if (rc == ERROR_SUCCESS && type == REG_DWORD) {
        *out = data;
        return TRUE;
    }
    return FALSE;
}

// Write a DWORD value into the settings key (creating the key if needed).
static BOOL RegWriteDword(const wchar_t *valueName, DWORD value) {
    HKEY hKey;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, SETTINGS_SUBKEY, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                        &hKey, &disp) != ERROR_SUCCESS) {
        return FALSE;
    }
    LONG rc = RegSetValueExW(hKey, valueName, 0, REG_DWORD,
                             (const BYTE *)&value, sizeof(value));
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

// Read a string (REG_SZ) value into a caller-supplied buffer. Returns TRUE on
// success with a NUL-terminated result; FALSE if the value is missing or the
// type is unexpected. On failure, writes an empty string.
static BOOL RegReadString(const wchar_t *valueName, wchar_t *out, DWORD cchOut) {
    if (cchOut == 0) return FALSE;
    out[0] = L'\0';
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_SUBKEY, 0, KEY_QUERY_VALUE,
                      &hKey) != ERROR_SUCCESS) {
        return FALSE;
    }
    DWORD type = 0;
    DWORD cb = cchOut * sizeof(wchar_t);  // bytes available
    LONG rc = RegQueryValueExW(hKey, valueName, NULL, &type,
                               (LPBYTE)out, &cb);
    RegCloseKey(hKey);
    if (rc == ERROR_SUCCESS && type == REG_SZ) {
        // Ensure NUL termination regardless of how the value was stored.
        DWORD cch = cb / sizeof(wchar_t);
        if (cch >= cchOut) cch = cchOut - 1;
        out[cch] = L'\0';
        return TRUE;
    }
    out[0] = L'\0';
    return FALSE;
}

// Write a string (REG_SZ) value into the settings key (creating it if needed).
static BOOL RegWriteString(const wchar_t *valueName, const wchar_t *value) {
    HKEY hKey;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, SETTINGS_SUBKEY, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                        &hKey, &disp) != ERROR_SUCCESS) {
        return FALSE;
    }
    size_t len = 0;
    StringCchLengthW(value, MAX_PATH, &len);
    DWORD cb = (DWORD)((len + 1) * sizeof(wchar_t));  // include NUL
    LONG rc = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                             (const BYTE *)value, cb);
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

// Load persisted defaults into the globals, clamping to valid bounds. Missing
// values fall back to the factory defaults already in the globals.
static void LoadSettings(void) {
    DWORD v;
    if (RegReadDword(VAL_WORK_MIN, &v)) {
        if (v >= MIN_DEFAULT_WORK_MIN && v <= MAX_DEFAULT_MIN) {
            g_defaultWorkMin = (int)v;
        }
    }
    if (RegReadDword(VAL_REST_MIN, &v)) {
        if (v <= MAX_DEFAULT_MIN) {  // rest min is 0
            g_defaultRestMin = (int)v;
        }
    }
    // Work sound path (optional). Empty/missing -> no sound.
    RegReadString(VAL_WORK_SOUND, g_workSoundPath, ARRAYSIZE(g_workSoundPath));
}

// Persist the current default globals to the registry.
static BOOL SaveSettings(void) {
    BOOL ok = RegWriteDword(VAL_WORK_MIN, (DWORD)g_defaultWorkMin);
    ok = RegWriteDword(VAL_REST_MIN, (DWORD)g_defaultRestMin) && ok;
    return ok;
}

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

// Stop any currently looping work sound.
static void StopWorkSound(void) {
    if (g_workSoundPlaying) {
        PlaySoundW(NULL, NULL, 0);   // halt async playback
        g_workSoundPlaying = FALSE;
    }
}

// Start looping the configured work sound (async). No-op if no path is set.
static void StartWorkSound(void) {
    if (g_workSoundPath[0] == L'\0') {
        return;  // nothing configured
    }
    // SND_LOOP requires SND_ASYNC; loops until PlaySound(NULL, ...) is called.
    if (PlaySoundW(g_workSoundPath, NULL,
                   SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT)) {
        g_workSoundPlaying = TRUE;
    } else {
        g_workSoundPlaying = FALSE;  // file missing/unreadable: stay silent
    }
}

// Reconcile the work-sound playback with the current timer state: it should
// loop only while the timer is running AND in the WORK phase. Called at every
// state change (start, stop, phase transition).
static void UpdateWorkSound(void) {
    BOOL shouldPlay = (g_app.running && g_app.phase == PHASE_WORK &&
                       g_workSoundPath[0] != L'\0');
    if (shouldPlay && !g_workSoundPlaying) {
        StartWorkSound();
    } else if (!shouldPlay && g_workSoundPlaying) {
        StopWorkSound();
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
        return;
    }

    // We are now running in the WORK phase: begin the looping work sound.
    UpdateWorkSound();
}

static void StopTimer(HWND hwnd) {
    if (g_app.running) {
        KillTimer(hwnd, TIMER_ID);
        g_app.running = FALSE;
    }
    StopWorkSound();   // silence the looping work sound
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

        // The phase may have changed: start the work sound when entering WORK,
        // stop it when entering REST.
        UpdateWorkSound();
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
// Settings dialog
// ---------------------------------------------------------------------------

// Parse a dialog edit control as an integer in [lo, hi]. Returns TRUE on
// success. Rejects non-digits and out-of-range values.
static BOOL GetDlgInt(HWND hDlg, int ctlId, int lo, int hi, int *out) {
    wchar_t buf[16] = {0};
    if (GetDlgItemTextW(hDlg, ctlId, buf, ARRAYSIZE(buf)) == 0) {
        return FALSE;
    }
    for (const wchar_t *p = buf; *p; ++p) {
        if (*p < L'0' || *p > L'9') {
            return FALSE;
        }
    }
    long v = 0;
    for (const wchar_t *p = buf; *p; ++p) {
        v = v * 10 + (*p - L'0');
        if (v > hi) return FALSE;
    }
    if (v < lo || v > hi) return FALSE;
    *out = (int)v;
    return TRUE;
}

static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam,
                                        LPARAM lParam) {
    (void)lParam;
    switch (msg) {
    case WM_INITDIALOG: {
        // Only digits, up to 3 chars, in each field; seed with current values.
        SendDlgItemMessageW(hDlg, IDC_SET_WORK, EM_SETLIMITTEXT, 3, 0);
        SendDlgItemMessageW(hDlg, IDC_SET_REST, EM_SETLIMITTEXT, 3, 0);
        SetDlgItemInt(hDlg, IDC_SET_WORK, (UINT)g_defaultWorkMin, FALSE);
        SetDlgItemInt(hDlg, IDC_SET_REST, (UINT)g_defaultRestMin, FALSE);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            int work = 0, rest = 0;
            if (!GetDlgInt(hDlg, IDC_SET_WORK, MIN_DEFAULT_WORK_MIN,
                           MAX_DEFAULT_MIN, &work)) {
                MessageBoxW(hDlg,
                            L"Default Work minutes must be a whole number "
                            L"from 1 to 600.",
                            L"Invalid setting", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            if (!GetDlgInt(hDlg, IDC_SET_REST, MIN_DEFAULT_REST_MIN,
                           MAX_DEFAULT_MIN, &rest)) {
                MessageBoxW(hDlg,
                            L"Default Rest minutes must be a whole number "
                            L"from 0 to 600.",
                            L"Invalid setting", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            g_defaultWorkMin = work;
            g_defaultRestMin = rest;
            if (!SaveSettings()) {
                MessageBoxW(hDlg,
                            L"The settings could not be saved to the registry.",
                            L"Warning", MB_OK | MB_ICONWARNING);
            }

            // Reflect the new defaults in the main window if idle.
            if (!g_app.running) {
                wchar_t num[16];
                StringCchPrintfW(num, ARRAYSIZE(num), L"%d", g_defaultWorkMin);
                SetWindowTextW(g_app.hWorkEdit, num);
                SendMessageW(g_app.hWorkUnit, CB_SETCURSEL, UNIT_MINUTES, 0);
                StringCchPrintfW(num, ARRAYSIZE(num), L"%d", g_defaultRestMin);
                SetWindowTextW(g_app.hRestEdit, num);
                SendMessageW(g_app.hRestUnit, CB_SETCURSEL, UNIT_MINUTES, 0);
            }

            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}


// ---------------------------------------------------------------------------
// Work-sound selection
// ---------------------------------------------------------------------------

// Open a file dialog to choose a .wav for the work-phase loop. On success,
// stores the path in the global and persists it, and (re)starts the loop if
// the timer is currently in the WORK phase.
static void ChooseWorkSound(HWND hwnd) {
    wchar_t path[MAX_PATH];
    // Seed with the current path so the dialog opens near it.
    StringCchCopyW(path, ARRAYSIZE(path), g_workSoundPath);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter = L"WAV audio (*.wav)\0*.wav\0All files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = ARRAYSIZE(path);
    ofn.lpstrTitle  = L"Select a .wav to loop during WORK";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrDefExt = L"wav";

    if (!GetOpenFileNameW(&ofn)) {
        return;  // user cancelled (or an error); leave settings unchanged
    }

    StringCchCopyW(g_workSoundPath, ARRAYSIZE(g_workSoundPath), path);
    if (!RegWriteString(VAL_WORK_SOUND, g_workSoundPath)) {
        MessageBoxW(hwnd,
                    L"The work-sound path could not be saved to the registry.",
                    L"Warning", MB_OK | MB_ICONWARNING);
    }

    // If we are mid-WORK, restart the loop so the new file takes effect now.
    StopWorkSound();
    UpdateWorkSound();
}

// Clear the configured work sound and stop any current playback.
static void ClearWorkSound(HWND hwnd) {
    g_workSoundPath[0] = L'\0';
    RegWriteString(VAL_WORK_SOUND, L"");
    StopWorkSound();
    MessageBoxW(hwnd, L"Work sound cleared.", L"Settings",
                MB_OK | MB_ICONINFORMATION);
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

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
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
    // "Default Times..." command and "Exit".
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
