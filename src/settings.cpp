// settings.cpp - Registry-backed Settings implementation.
#include "settings.h"
#include "resource.h"

namespace {

// Standard per-user settings location on Windows: HKCU\Software\...
constexpr wchar_t kSubKey[]    = L"Software\\Tomato\\PomodoroTimer";
constexpr wchar_t kValWork[]   = L"DefaultWorkMinutes";
constexpr wchar_t kValRest[]   = L"DefaultRestMinutes";
constexpr wchar_t kValSound[]  = L"WorkSoundPath";

// Read a DWORD value. Returns true and writes *out when present.
bool regReadDword(const wchar_t* name, DWORD* out) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSubKey, 0, KEY_QUERY_VALUE, &hKey)
            != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0, data = 0, cb = sizeof(data);
    LONG rc = RegQueryValueExW(hKey, name, nullptr, &type, (LPBYTE)&data, &cb);
    RegCloseKey(hKey);
    if (rc == ERROR_SUCCESS && type == REG_DWORD) {
        *out = data;
        return true;
    }
    return false;
}

// Write a DWORD value (creating the key if needed).
bool regWriteDword(const wchar_t* name, DWORD value) {
    HKEY hKey;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSubKey, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &hKey, &disp) != ERROR_SUCCESS) {
        return false;
    }
    LONG rc = RegSetValueExW(hKey, name, 0, REG_DWORD,
                             reinterpret_cast<const BYTE*>(&value),
                             sizeof(value));
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

// Read a REG_SZ value into a std::wstring. Returns empty string if absent.
std::wstring regReadString(const wchar_t* name) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSubKey, 0, KEY_QUERY_VALUE, &hKey)
            != ERROR_SUCCESS) {
        return std::wstring();
    }
    DWORD type = 0, cb = 0;
    // First query the size.
    LONG rc = RegQueryValueExW(hKey, name, nullptr, &type, nullptr, &cb);
    if (rc != ERROR_SUCCESS || type != REG_SZ || cb == 0) {
        RegCloseKey(hKey);
        return std::wstring();
    }
    std::wstring value(cb / sizeof(wchar_t), L'\0');
    rc = RegQueryValueExW(hKey, name, nullptr, &type,
                          reinterpret_cast<LPBYTE>(&value[0]), &cb);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS) {
        return std::wstring();
    }
    // Trim at the first NUL (the stored value includes its terminator).
    size_t nul = value.find(L'\0');
    if (nul != std::wstring::npos) {
        value.resize(nul);
    }
    return value;
}

// Write a REG_SZ value (creating the key if needed).
bool regWriteString(const wchar_t* name, const std::wstring& value) {
    HKEY hKey;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSubKey, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &hKey, &disp) != ERROR_SUCCESS) {
        return false;
    }
    // Include the terminating NUL in the byte count.
    DWORD cb = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    LONG rc = RegSetValueExW(hKey, name, 0, REG_SZ,
                             reinterpret_cast<const BYTE*>(value.c_str()), cb);
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

} // namespace

void Settings::load() {
    DWORD v = 0;
    if (regReadDword(kValWork, &v) &&
        v >= MIN_DEFAULT_WORK_MIN && v <= MAX_DEFAULT_MIN) {
        defaultWorkMin_ = static_cast<int>(v);
    }
    if (regReadDword(kValRest, &v) && v <= MAX_DEFAULT_MIN) {  // rest min is 0
        defaultRestMin_ = static_cast<int>(v);
    }
    workSoundPath_ = regReadString(kValSound);
}

bool Settings::saveDefaultTimes() {
    bool ok = regWriteDword(kValWork, static_cast<DWORD>(defaultWorkMin_));
    ok = regWriteDword(kValRest, static_cast<DWORD>(defaultRestMin_)) && ok;
    return ok;
}

bool Settings::saveWorkSound() {
    return regWriteString(kValSound, workSoundPath_);
}
