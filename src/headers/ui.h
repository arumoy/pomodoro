// ui.h - Main window: class registration, control creation, and WndProc.
#ifndef TOMATO_UI_H
#define TOMATO_UI_H

#include <windows.h>

// Create the main window's child controls and fonts (called on WM_CREATE).
void CreateControls(HWND hwnd);

// Main window procedure.
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // TOMATO_UI_H
