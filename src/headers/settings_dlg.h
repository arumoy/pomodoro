// settings_dlg.h - SettingsDialog: the modal "Default Times" dialog, wrapped
// in a class with DWLP_USER-based this-routing.
#ifndef TOMATO_SETTINGS_DLG_H
#define TOMATO_SETTINGS_DLG_H

#include <windows.h>
#include <functional>

class Settings;

class SettingsDialog {
public:
    SettingsDialog(Settings& settings, std::function<void()> onApplied)
        : settings_(settings), onApplied_(std::move(onApplied)) {}

    // Show the modal dialog owned by `parent`. Returns true if the user
    // accepted (OK) and the settings were applied.
    bool show(HINSTANCE hInst, HWND parent);

private:
    // Static thunk -> routes to the instance via DWLP_USER.
    static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT msg, WPARAM wParam,
                                    LPARAM lParam);
    INT_PTR handle(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

    Settings&             settings_;
    std::function<void()> onApplied_;  // called after a successful save
};

#endif // TOMATO_SETTINGS_DLG_H
