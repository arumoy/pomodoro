// worksound_menu.cpp - Work-sound selection menu handlers.
#include "worksound_menu.h"
#include "app.h"
#include "settings.h"
#include "sound.h"
#include <commdlg.h>    // GetOpenFileNameW
#include <strsafe.h>

// Open a file dialog to choose a .wav for the work-phase loop. On success,
// stores the path in the global and persists it, and (re)starts the loop if
// the timer is currently in the WORK phase.
void ChooseWorkSound(HWND hwnd) {
    wchar_t path[MAX_PATH];
    // Seed with the current path so the dialog opens near it.
    StringCchCopyW(path, ARRAYSIZE(path), g_workSoundPath);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter = L"WAV audio (*.wav)\0*.wav\0All files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = ARRAYSIZE(path);
    ofn.lpstrTitle  = L"Select a .wav to loop during WORK";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrDefExt = L"wav";

    if (!GetOpenFileNameW(&ofn)) {
        return;  // user cancelled (or an error); leave settings unchanged
    }

    StringCchCopyW(g_workSoundPath, ARRAYSIZE(g_workSoundPath), path);
    if (!RegWriteString(VAL_WORK_SOUND, g_workSoundPath)) {
        MessageBoxW(hwnd,
                    L"The work-sound path could not be saved to the registry.",
                    L"Warning", MB_OK | MB_ICONWARNING);
    }

    // If we are mid-WORK, restart the loop so the new file takes effect now.
    StopWorkSound();
    UpdateWorkSound();
}

// Clear the configured work sound and stop any current playback.
void ClearWorkSound(HWND hwnd) {
    g_workSoundPath[0] = L'\0';
    RegWriteString(VAL_WORK_SOUND, L"");
    StopWorkSound();
    MessageBoxW(hwnd, L"Work sound cleared.", L"Settings",
                MB_OK | MB_ICONINFORMATION);
}
