// app.h - Shared application state and globals used across translation units.
#ifndef TOMATO_APP_H
#define TOMATO_APP_H

#include <windows.h>

// ---------------------------------------------------------------------------
// Timer phases
// ---------------------------------------------------------------------------
typedef enum { PHASE_WORK, PHASE_REST } Phase;

// ---------------------------------------------------------------------------
// Main-window control handles + live timer state.
// ---------------------------------------------------------------------------
typedef struct {
    HWND hWorkEdit;
    HWND hRestEdit;
    HWND hWorkUnit;
    HWND hRestUnit;
    HWND hStartBtn;
    HWND hStopBtn;
    HWND hPhaseLabel;
    HWND hTimeLabel;

    BOOL  running;
    Phase phase;
    int   workSeconds;      // configured work duration in seconds
    int   restSeconds;      // configured rest duration in seconds (0 = skip)
    int   remainingSeconds; // seconds left in the current phase
} AppState;

// ---------------------------------------------------------------------------
// Global shared state (defined in app.cpp).
// ---------------------------------------------------------------------------
extern AppState g_app;

// GDI objects owned by the UI module (defined in app.cpp, created in ui.cpp).
extern HFONT  g_hTimeFont;   // large countdown font
extern HFONT  g_hPhaseFont;  // phase-label font
extern HFONT  g_hUiFont;     // Garamond for inputs/buttons/labels
extern HBRUSH g_hBgBrush;    // light orange background brush

// Persisted settings (defined in app.cpp, loaded in settings.cpp).
extern int     g_defaultWorkMin;         // default work minutes
extern int     g_defaultRestMin;         // default rest minutes
extern wchar_t g_workSoundPath[MAX_PATH];// looping work .wav ("" = none)
extern BOOL    g_workSoundPlaying;       // is the loop currently playing?

#endif // TOMATO_APP_H
