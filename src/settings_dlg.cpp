// settings_dlg.cpp - "Default Times" settings dialog.
#include "settings_dlg.h"
#include "app.h"
#include "resource.h"
#include "settings.h"
#include <strsafe.h>

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

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam,
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
