// ui.h - MainWindow: owns the controls, timer, sound, and settings, and
// implements ITimerView so the timer can drive the display. Routes the Win32
// window procedure to member functions via GWLP_USERDATA.
#ifndef TOMATO_UI_H
#define TOMATO_UI_H

#include <windows.h>
#include <optional>
#include "gdi.h"
#include "settings.h"
#include "sound.h"
#include "timer.h"

class MainWindow : public ITimerView {
public:
    explicit MainWindow(HINSTANCE hInst);

    // Register the class, create the window + menu. Returns false on failure.
    bool create(int nCmdShow);

    HWND handle() const { return hwnd_; }

    // ITimerView ------------------------------------------------------------
    void showTime(int remainingSeconds) override;
    void showPhase(Phase phase) override;
    void showStopped() override;
    void setControlsRunning(bool running) override;

private:
    // Win32 plumbing: static thunk -> member handler via GWLP_USERDATA.
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wParam, LPARAM lParam);

    // Setup / actions.
    void createControls();
    void onStart();
    void onChooseWorkSound();
    void onClearWorkSound();
    void openSettingsDialog();
    void applyDefaultTimesToFields();

    // Read a field + unit combo into seconds; std::nullopt if invalid.
    std::optional<int> readDuration(HWND edit, HWND unit, int minSeconds) const;

    HINSTANCE hInst_ = nullptr;
    HWND      hwnd_  = nullptr;

    // Child controls.
    HWND hWorkEdit_ = nullptr, hRestEdit_ = nullptr;
    HWND hWorkUnit_ = nullptr, hRestUnit_ = nullptr;
    HWND hStartBtn_ = nullptr, hStopBtn_  = nullptr;
    HWND hPhaseLabel_ = nullptr, hTimeLabel_ = nullptr;

    // GDI resources (RAII).
    GdiFont  timeFont_, phaseFont_, uiFont_;
    GdiBrush bgBrush_;

    // Owned subsystems.
    Settings      settings_;
    WorkSound     sound_;
    PomodoroTimer timer_;
};

#endif // TOMATO_UI_H
