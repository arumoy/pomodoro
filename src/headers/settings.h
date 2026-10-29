// settings.h - Settings: per-user persistence in the Windows registry
// (HKCU\Software\Tomato\PomodoroTimer).
#ifndef TOMATO_SETTINGS_H
#define TOMATO_SETTINGS_H

#include <windows.h>
#include <string>

class Settings {
public:
    // Load persisted values from the registry (clamped to valid bounds);
    // missing values keep their factory defaults.
    void load();

    // Persist the default work/rest minutes. Returns true on success.
    bool saveDefaultTimes();

    // Persist the work-sound path. Returns true on success.
    bool saveWorkSound();

    // Accessors.
    int defaultWorkMin() const { return defaultWorkMin_; }
    int defaultRestMin() const { return defaultRestMin_; }
    void setDefaultWorkMin(int m) { defaultWorkMin_ = m; }
    void setDefaultRestMin(int m) { defaultRestMin_ = m; }

    const std::wstring& workSoundPath() const { return workSoundPath_; }
    void setWorkSoundPath(std::wstring path) { workSoundPath_ = std::move(path); }
    bool hasWorkSound() const { return !workSoundPath_.empty(); }

private:
    int          defaultWorkMin_ = 25;  // factory defaults (see resource.h)
    int          defaultRestMin_ = 5;
    std::wstring workSoundPath_;         // empty = no work sound
};

#endif // TOMATO_SETTINGS_H
