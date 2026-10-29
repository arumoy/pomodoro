// sound.cpp - End-of-phase chime and looping work sound.
#include "sound.h"
#include "app.h"
#include <mmsystem.h>   // Beep + PlaySound

// Worker thread that plays a short "ding bell" ascending chime so the UI
// thread (and its 1-second timer) is never blocked by the synchronous Beep
// calls. Frequencies/durations in Hz/ms form a simple pleasant arpeggio.
static DWORD WINAPI DingTuneProc(LPVOID param) {
    (void)param;
    // Ascending chime: E6, G6, C7, then a held E7 - a cheerful "ding" bell.
    static const struct { DWORD freq; DWORD ms; } kNotes[] = {
        { 1319, 120 },  // E6
        { 1568, 120 },  // G6
        { 2093, 120 },  // C7
        { 2637, 320 },  // E7 (held)
    };
    for (int i = 0; i < (int)ARRAYSIZE(kNotes); ++i) {
        Beep(kNotes[i].freq, kNotes[i].ms);
    }
    return 0;
}

// Fire the ding-bell tune without blocking the caller.
void PlayDingTune(void) {
    HANDLE h = CreateThread(NULL, 0, DingTuneProc, NULL, 0, NULL);
    if (h) {
        CloseHandle(h);  // fire-and-forget; thread self-terminates
    } else {
        MessageBeep(MB_ICONASTERISK);  // fallback if the thread can't start
    }
}

// Stop any currently looping work sound.
void StopWorkSound(void) {
    if (g_workSoundPlaying) {
        PlaySoundW(NULL, NULL, 0);   // halt async playback
        g_workSoundPlaying = FALSE;
    }
}

// Start looping the configured work sound (async). No-op if no path is set.
void StartWorkSound(void) {
    if (g_workSoundPath[0] == L'\0') {
        return;  // nothing configured
    }
    // SND_LOOP requires SND_ASYNC; loops until PlaySound(NULL, ...) is called.
    if (PlaySoundW(g_workSoundPath, NULL,
                   SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT)) {
        g_workSoundPlaying = TRUE;
    } else {
        g_workSoundPlaying = FALSE;  // file missing/unreadable: stay silent
    }
}

// Reconcile the work-sound playback with the current timer state.
void UpdateWorkSound(void) {
    BOOL shouldPlay = (g_app.running && g_app.phase == PHASE_WORK &&
                       g_workSoundPath[0] != L'\0');
    if (shouldPlay && !g_workSoundPlaying) {
        StartWorkSound();
    } else if (!shouldPlay && g_workSoundPlaying) {
        StopWorkSound();
    }
}
