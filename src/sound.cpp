// sound.cpp - WorkSound implementation (end-of-phase chime + looping work sound).
#include "sound.h"
#include <mmsystem.h>   // Beep + PlaySound

namespace {

// Worker thread that plays a short "ding bell" ascending chime so the UI
// thread (and its 1-second timer) is never blocked by the synchronous Beep
// calls. Frequencies/durations in Hz/ms form a simple pleasant arpeggio.
DWORD WINAPI dingTuneProc(LPVOID) {
    // Ascending chime: E6, G6, C7, then a held E7 - a cheerful "ding" bell.
    struct Note { DWORD freq; DWORD ms; };
    static const Note kNotes[] = {
        { 1319, 120 },  // E6
        { 1568, 120 },  // G6
        { 2093, 120 },  // C7
        { 2637, 320 },  // E7 (held)
    };
    for (const Note& n : kNotes) {
        Beep(n.freq, n.ms);
    }
    return 0;
}

} // namespace

WorkSound::~WorkSound() {
    stop();
}

void WorkSound::playDing() {
    HANDLE h = CreateThread(nullptr, 0, dingTuneProc, nullptr, 0, nullptr);
    if (h) {
        CloseHandle(h);  // fire-and-forget; thread self-terminates
    } else {
        MessageBeep(MB_ICONASTERISK);  // fallback if the thread can't start
    }
}

void WorkSound::stop() {
    if (playing_) {
        PlaySoundW(nullptr, nullptr, 0);   // halt async playback
        playing_ = false;
    }
}

void WorkSound::start() {
    if (path_.empty()) {
        return;  // nothing configured
    }
    // SND_LOOP requires SND_ASYNC; loops until PlaySound(nullptr, ...) is called.
    if (PlaySoundW(path_.c_str(), nullptr,
                   SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT)) {
        playing_ = true;
    } else {
        playing_ = false;  // file missing/unreadable: stay silent
    }
}
