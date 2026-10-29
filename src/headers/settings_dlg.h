// settings_dlg.h - The "Default Times" settings dialog
// (Settings -> Default Times...).
#ifndef TOMATO_SETTINGS_DLG_H
#define TOMATO_SETTINGS_DLG_H

#include <windows.h>

// Modal dialog procedure for IDD_SETTINGS. Invoke via DialogBoxW.
INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam,
                                 LPARAM lParam);

#endif // TOMATO_SETTINGS_DLG_H
