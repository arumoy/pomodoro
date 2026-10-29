// worksound_menu.h - Handlers for the work-sound menu items
// (Settings -> Select Work Sound... / Clear Work Sound).
#ifndef TOMATO_WORKSOUND_MENU_H
#define TOMATO_WORKSOUND_MENU_H

#include <windows.h>

// Prompt for a .wav, persist it, and (re)start the loop if mid-WORK.
void ChooseWorkSound(HWND hwnd);

// Clear the configured work sound and stop any current playback.
void ClearWorkSound(HWND hwnd);

#endif // TOMATO_WORKSOUND_MENU_H
