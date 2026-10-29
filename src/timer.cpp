// timer.cpp - Core countdown logic and input validation.
#include "timer.h"
#include "app.h"
#include "resource.h"
#include "sound.h"
#include <strsafe.h>

// Read a field's numeric text plus its unit combo box and convert to a total
// number of seconds. Validates that the raw number is all digits and within
// [0, MAX_FIELD_VALUE], and that the resulting seconds are within
// [minSeconds, MAX_TOTAL_SECONDS]. Returns TRUE on success, writing *out.
// Rejects invalid input outright rather than silently clamping.
BOOL ReadDurationField(HWND hEdit, HWND hUnit, int minSeconds, int *out) {
    wchar_t buf[16] = {0};
    int len = GetWindowTextW(hEdit, buf, ARRAYSIZE(buf));
    if (len <= 0) {
        return FALSE;
    }

    // Ensure every character is a decimal digit. The edit control uses
    // ES_NUMBER, but we validate again here (defense in depth).
    for (int i = 0; i < len; ++i) {
        if (buf[i] < L'0' || buf[i] > L'9') {
            return FALSE;
        }
    }

    // Parse with overflow awareness.
    long value = 0;
    for (int i = 0; i < len; ++i) {
        value = value * 10 + (buf[i] - L'0');
        if (value > MAX_FIELD_VALUE) {
            return FALSE;  // too large, reject
        }
    }

    // Determine the unit from the combo box; default to minutes if the query
    // fails for any reason.
    LRESULT sel = SendMessageW(hUnit, CB_GETCURSEL, 0, 0);
    long seconds;
    if (sel == UNIT_SECONDS) {
        seconds = value;
    } else {
        seconds = value * 60;  // minutes -> seconds
    }

    if (seconds < minSeconds || seconds > MAX_TOTAL_SECONDS) {
        return FALSE;
    }

    *out = (int)seconds;
    return TRUE;
}

// Update the countdown label (MM:SS) and the phase label.
void UpdateDisplay(void) {
    int minutes = g_app.remainingSeconds / 60;
    int seconds = g_app.remainingSeconds % 60;

    wchar_t timeBuf[16];
    StringCchPrintfW(timeBuf, ARRAYSIZE(timeBuf), L"%02d:%02d", minutes, seconds);
    SetWindowTextW(g_app.hTimeLabel, timeBuf);

    SetWindowTextW(g_app.hPhaseLabel,
                   g_app.phase == PHASE_WORK ? L"WORK" : L"REST");
}

// Enable/disable controls based on running state.
void SetRunningUI(BOOL running) {
    EnableWindow(g_app.hStartBtn, !running);
    EnableWindow(g_app.hStopBtn,  running);
    EnableWindow(g_app.hWorkEdit, !running);
    EnableWindow(g_app.hRestEdit, !running);
    EnableWindow(g_app.hWorkUnit, !running);
    EnableWindow(g_app.hRestUnit, !running);
}

void StartTimer(HWND hwnd) {
    int work = 0, rest = 0;
    if (!ReadDurationField(g_app.hWorkEdit, g_app.hWorkUnit,
                           MIN_WORK_SECONDS, &work)) {
        MessageBoxW(hwnd,
                    L"Please enter a valid Work duration.\n\n"
                    L"Minutes: 1 to 600.  Seconds: 1 to 999 "
                    L"(total max 600 minutes).",
                    L"Invalid input", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!ReadDurationField(g_app.hRestEdit, g_app.hRestUnit,
                           MIN_REST_SECONDS, &rest)) {
        MessageBoxW(hwnd,
                    L"Please enter a valid Rest duration.\n\n"
                    L"Use 0 to skip the rest phase. "
                    L"Minutes: 0 to 600.  Seconds: 0 to 999 "
                    L"(total max 600 minutes).",
                    L"Invalid input", MB_OK | MB_ICONWARNING);
        return;
    }

    g_app.workSeconds      = work;
    g_app.restSeconds      = rest;
    g_app.phase            = PHASE_WORK;
    g_app.remainingSeconds = work;
    g_app.running          = TRUE;

    UpdateDisplay();
    SetRunningUI(TRUE);

    if (!SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, NULL)) {
        MessageBoxW(hwnd, L"Failed to start the timer.", L"Error",
                    MB_OK | MB_ICONERROR);
        g_app.running = FALSE;
        SetRunningUI(FALSE);
        return;
    }

    // We are now running in the WORK phase: begin the looping work sound.
    UpdateWorkSound();
}

void StopTimer(HWND hwnd) {
    if (g_app.running) {
        KillTimer(hwnd, TIMER_ID);
        g_app.running = FALSE;
    }
    StopWorkSound();   // silence the looping work sound
    SetRunningUI(FALSE);
    SetWindowTextW(g_app.hPhaseLabel, L"STOPPED");
    SetWindowTextW(g_app.hTimeLabel,  L"00:00");
}

// Advance one second. Handles phase transitions when a phase hits zero.
void OnTick(HWND hwnd) {
    if (!g_app.running) {
        return;
    }

    if (g_app.remainingSeconds > 0) {
        g_app.remainingSeconds--;
    }

    if (g_app.remainingSeconds <= 0) {
        // Phase complete: ring the bell, flash the window, then switch phases.
        FlashWindow(hwnd, TRUE);
        PlayDingTune();

        if (g_app.phase == PHASE_WORK) {
            if (g_app.restSeconds > 0) {
                // Begin the rest phase.
                g_app.phase            = PHASE_REST;
                g_app.remainingSeconds = g_app.restSeconds;
            } else {
                // Zero rest: skip rest and start the next work phase.
                g_app.phase            = PHASE_WORK;
                g_app.remainingSeconds = g_app.workSeconds;
            }
        } else {
            // Rest finished: back to work.
            g_app.phase            = PHASE_WORK;
            g_app.remainingSeconds = g_app.workSeconds;
        }

        // The phase may have changed: start the work sound when entering WORK,
        // stop it when entering REST.
        UpdateWorkSound();
    }

    UpdateDisplay();
}
