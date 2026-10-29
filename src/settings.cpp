// settings.cpp - Registry-backed settings storage.
#include "settings.h"
#include "app.h"
#include "resource.h"
#include <strsafe.h>

// Standard per-user settings location on Windows: HKCU\Software\...
#define SETTINGS_SUBKEY L"Software\\Tomato\\PomodoroTimer"

// Read a DWORD value from the settings key. Returns TRUE and writes *out when
// the value exists; otherwise leaves *out untouched and returns FALSE.
BOOL RegReadDword(const wchar_t *valueName, DWORD *out) {
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
BOOL RegWriteDword(const wchar_t *valueName, DWORD value) {
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
BOOL RegReadString(const wchar_t *valueName, wchar_t *out, DWORD cchOut) {
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
BOOL RegWriteString(const wchar_t *valueName, const wchar_t *value) {
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
void LoadSettings(void) {
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
BOOL SaveSettings(void) {
    BOOL ok = RegWriteDword(VAL_WORK_MIN, (DWORD)g_defaultWorkMin);
    ok = RegWriteDword(VAL_REST_MIN, (DWORD)g_defaultRestMin) && ok;
    return ok;
}
