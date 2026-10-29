// settings.h - Persist/restore user settings in the Windows registry
// (HKCU\Software\Tomato\PomodoroTimer).
#ifndef TOMATO_SETTINGS_H
#define TOMATO_SETTINGS_H

#include <windows.h>

// Registry value names (public so other modules can read/write their own).
#define VAL_WORK_MIN   L"DefaultWorkMinutes"
#define VAL_REST_MIN   L"DefaultRestMinutes"
#define VAL_WORK_SOUND L"WorkSoundPath"

// Low-level typed registry helpers (operate on the app's settings key).
BOOL RegReadDword(const wchar_t *valueName, DWORD *out);
BOOL RegWriteDword(const wchar_t *valueName, DWORD value);
BOOL RegReadString(const wchar_t *valueName, wchar_t *out, DWORD cchOut);
BOOL RegWriteString(const wchar_t *valueName, const wchar_t *value);

// Load persisted settings into the globals (clamping to valid bounds); missing
// values keep their factory defaults.
void LoadSettings(void);

// Persist the default work/rest minute globals. Returns TRUE on success.
BOOL SaveSettings(void);

#endif // TOMATO_SETTINGS_H
