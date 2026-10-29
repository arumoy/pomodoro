// timer.h - Core countdown logic: start/stop, per-second tick, phase
// transitions, the display refresh, and input-field validation.
#ifndef TOMATO_TIMER_H
#define TOMATO_TIMER_H

#include <windows.h>

// Read a field's numeric text plus its unit combo box and convert to seconds.
// Validates digits-only and range; returns TRUE on success, writing *out.
BOOL ReadDurationField(HWND hEdit, HWND hUnit, int minSeconds, int *out);

// Refresh the MM:SS countdown label and the phase label from g_app.
void UpdateDisplay(void);

// Enable/disable the inputs and Start/Stop buttons for the given run state.
void SetRunningUI(BOOL running);

// Start the timer (validates the fields first). Begins in the WORK phase.
void StartTimer(HWND hwnd);

// Stop the timer and reset the display.
void StopTimer(HWND hwnd);

// Advance one second and handle phase transitions (called on WM_TIMER).
void OnTick(HWND hwnd);

#endif // TOMATO_TIMER_H
