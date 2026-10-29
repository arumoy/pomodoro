// settings_dlg.cpp - SettingsDialog implementation.
#include "settings_dlg.h"
#include "settings.h"
#include "resource.h"

namespace {

// Parse a dialog edit control as an integer in [lo, hi]. Returns true on
// success. Rejects non-digits and out-of-range values.
bool getDlgInt(HWND hDlg, int ctlId, int lo, int hi, int* out) {
    wchar_t buf[16] = {0};
    if (GetDlgItemTextW(hDlg, ctlId, buf, ARRAYSIZE(buf)) == 0) {
        return false;
    }
    for (const wchar_t* p = buf; *p; ++p) {
        if (*p < L'0' || *p > L'9') return false;
    }
    long v = 0;
    for (const wchar_t* p = buf; *p; ++p) {
        v = v * 10 + (*p - L'0');
        if (v > hi) return false;
    }
    if (v < lo || v > hi) return false;
    *out = static_cast<int>(v);
    return true;
}

} // namespace

bool SettingsDialog::show(HINSTANCE hInst, HWND parent) {
    INT_PTR r = DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_SETTINGS), parent,
                                &SettingsDialog::dlgProc,
                                reinterpret_cast<LPARAM>(this));
    return r == IDOK;
}

INT_PTR CALLBACK SettingsDialog::dlgProc(HWND hDlg, UINT msg, WPARAM wParam,
                                         LPARAM lParam) {
    SettingsDialog* self = nullptr;
    if (msg == WM_INITDIALOG) {
        // lParam is the `this` we passed to DialogBoxParam; stash it.
        self = reinterpret_cast<SettingsDialog*>(lParam);
        SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<SettingsDialog*>(
            GetWindowLongPtrW(hDlg, DWLP_USER));
    }
    return self ? self->handle(hDlg, msg, wParam, lParam) : FALSE;
}

INT_PTR SettingsDialog::handle(HWND hDlg, UINT msg, WPARAM wParam,
                               LPARAM /*lParam*/) {
    switch (msg) {
    case WM_INITDIALOG:
        SendDlgItemMessageW(hDlg, IDC_SET_WORK, EM_SETLIMITTEXT, 3, 0);
        SendDlgItemMessageW(hDlg, IDC_SET_REST, EM_SETLIMITTEXT, 3, 0);
        SetDlgItemInt(hDlg, IDC_SET_WORK, (UINT)settings_.defaultWorkMin(), FALSE);
        SetDlgItemInt(hDlg, IDC_SET_REST, (UINT)settings_.defaultRestMin(), FALSE);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            int work = 0, rest = 0;
            if (!getDlgInt(hDlg, IDC_SET_WORK, MIN_DEFAULT_WORK_MIN,
                           MAX_DEFAULT_MIN, &work)) {
                MessageBoxW(hDlg,
                            L"Default Work minutes must be a whole number "
                            L"from 1 to 600.",
                            L"Invalid setting", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            if (!getDlgInt(hDlg, IDC_SET_REST, MIN_DEFAULT_REST_MIN,
                           MAX_DEFAULT_MIN, &rest)) {
                MessageBoxW(hDlg,
                            L"Default Rest minutes must be a whole number "
                            L"from 0 to 600.",
                            L"Invalid setting", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            settings_.setDefaultWorkMin(work);
            settings_.setDefaultRestMin(rest);
            if (!settings_.saveDefaultTimes()) {
                MessageBoxW(hDlg,
                            L"The settings could not be saved to the registry.",
                            L"Warning", MB_OK | MB_ICONWARNING);
            }
            if (onApplied_) {
                onApplied_();  // let the window refresh its fields
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
