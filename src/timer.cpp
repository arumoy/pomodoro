// timer.cpp - PomodoroTimer state machine.
#include "timer.h"
#include "sound.h"
#include "resource.h"

void PomodoroTimer::start(int workSeconds, int restSeconds) {
    workSeconds_ = workSeconds;
    restSeconds_ = restSeconds;
    running_ = true;
    enterPhase(Phase::Work, workSeconds_);

    view_.setControlsRunning(true);

    if (!SetTimer(host_, TIMER_ID, TIMER_INTERVAL, nullptr)) {
        MessageBoxW(host_, L"Failed to start the timer.", L"Error",
                    MB_OK | MB_ICONERROR);
        running_ = false;
        view_.setControlsRunning(false);
        return;
    }

    // Now running in WORK: begin the looping work sound if one is set.
    sound_.start();
}

void PomodoroTimer::stop() {
    if (running_) {
        KillTimer(host_, TIMER_ID);
        running_ = false;
    }
    sound_.stop();
    view_.setControlsRunning(false);
    view_.showStopped();
}

void PomodoroTimer::enterPhase(Phase phase, int seconds) {
    phase_ = phase;
    remainingSeconds_ = seconds;
    view_.showPhase(phase_);
    view_.showTime(remainingSeconds_);

    // The work sound loops only during WORK; reconcile on every phase entry.
    if (running_) {
        if (phase_ == Phase::Work) {
            sound_.start();
        } else {
            sound_.stop();
        }
    }
}

void PomodoroTimer::tick() {
    if (!running_) {
        return;
    }

    if (remainingSeconds_ > 0) {
        remainingSeconds_--;
    }

    if (remainingSeconds_ <= 0) {
        // Phase complete: ring the bell, flash the window, then switch phases.
        FlashWindow(host_, TRUE);
        WorkSound::playDing();

        if (phase_ == Phase::Work) {
            if (restSeconds_ > 0) {
                enterPhase(Phase::Rest, restSeconds_);
            } else {
                // Zero rest: skip rest and start the next work phase.
                enterPhase(Phase::Work, workSeconds_);
            }
        } else {
            enterPhase(Phase::Work, workSeconds_);  // rest done -> work
        }
    } else {
        view_.showTime(remainingSeconds_);
    }
}
