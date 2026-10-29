// resource.h - Identifiers and constants shared between the resource script
// (pomodoro.rc) and the application code.
#ifndef TOMATO_RESOURCE_H
#define TOMATO_RESOURCE_H

// ---------------------------------------------------------------------------
// Control identifiers (child windows on the main window)
// ---------------------------------------------------------------------------
#define IDC_WORK_EDIT   1001
#define IDC_REST_EDIT   1002
#define IDC_START_BTN   1003
#define IDC_STOP_BTN    1004
#define IDC_PHASE_LABEL 1005
#define IDC_TIME_LABEL  1006
#define IDC_WORK_UNIT   1007
#define IDC_REST_UNIT   1008

// ---------------------------------------------------------------------------
// Resource IDs (must match pomodoro.rc)
// ---------------------------------------------------------------------------
#define IDI_APPICON     101
#define IDD_SETTINGS    201   // Settings dialog template
#define IDC_SET_WORK    202   // dialog: default work minutes edit
#define IDC_SET_REST    203   // dialog: default rest minutes edit

// ---------------------------------------------------------------------------
// Menu command IDs
// ---------------------------------------------------------------------------
#define IDM_SETTINGS    301   // Settings -> Default Times...
#define IDM_EXIT        302   // Settings -> Exit
#define IDM_WORK_SOUND  303   // Settings -> Select Work Sound (.wav)...
#define IDM_CLEAR_SOUND 304   // Settings -> Clear Work Sound

// ---------------------------------------------------------------------------
// Persistent-settings bounds (minutes)
// ---------------------------------------------------------------------------
#define MIN_DEFAULT_WORK_MIN 1
#define MIN_DEFAULT_REST_MIN 0
#define MAX_DEFAULT_MIN      600
#define DEFAULT_WORK_MIN     25   // factory default if nothing is stored
#define DEFAULT_REST_MIN     5

// ---------------------------------------------------------------------------
// Unit combo-box item indices
// ---------------------------------------------------------------------------
#define UNIT_MINUTES    0
#define UNIT_SECONDS    1

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------
#define TIMER_ID        1
#define TIMER_INTERVAL  1000  // 1 second ticks

// ---------------------------------------------------------------------------
// Input bounds (defense-in-depth validation on the numeric fields), seconds.
// Work must be at least 1 second; rest may be 0 (skip rest). The numeric entry
// itself is capped at 3 digits (<= 999) per unit.
// ---------------------------------------------------------------------------
#define MIN_WORK_SECONDS 1
#define MIN_REST_SECONDS 0
#define MAX_TOTAL_SECONDS 36000  // 600 minutes
#define MAX_FIELD_VALUE   999    // max raw number typed into a field

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------
#define APP_BG_COLOR    RGB(255, 224, 178)   // light orange window background
#define APP_FONT_NAME   L"Garamond"

#endif // TOMATO_RESOURCE_H
