// timer.h - PomodoroTimer: core countdown state machine and its view interface.
#ifndef TOMATO_TIMER_H
#define TOMATO_TIMER_H

#include <windows.h>
#include "gdi.h"     // Phase

class WorkSound;

// The timer drives the display through this interface so it does not depend on
// the concrete window class (dependency inversion). MainWindow implements it.
class ITimerView {
public:
    virtual ~ITimerView() = default;
    virtual void showTime(int remainingSeconds) = 0;  // update MM:SS label
    virtual void showPhase(Phase phase) = 0;          // "WORK"/"REST"
    virtual void showStopped() = 0;                   // idle display
    virtual void setControlsRunning(bool running) = 0;// enable/disable inputs
};

class PomodoroTimer {
public:
    // The timer posts WM_TIMER to `host`, drives `view`, and controls `sound`.
    PomodoroTimer(HWND host, ITimerView& view, WorkSound& sound)
        : host_(host), view_(view), sound_(sound) {}

    // The host window is created after the timer is constructed; set it then.
    void setHost(HWND host) { host_ = host; }

    // Begin a work/rest cycle with the given durations (in seconds).
    void start(int workSeconds, int restSeconds);

    // Stop and reset.
    void stop();

    // Advance one second (called from the host's WM_TIMER handler).
    void tick();

    bool running() const { return running_; }
    Phase phase() const { return phase_; }

private:
    void enterPhase(Phase phase, int seconds);

    HWND        host_;
    ITimerView& view_;
    WorkSound&  sound_;

    bool  running_ = false;
    Phase phase_   = Phase::Work;
    int   workSeconds_ = 0;
    int   restSeconds_ = 0;
    int   remainingSeconds_ = 0;
};

#endif // TOMATO_TIMER_H
