// app.cpp - Definitions (storage) for the shared globals declared in app.h.
#include "app.h"
#include "resource.h"

AppState g_app = {0};

HFONT  g_hTimeFont  = NULL;
HFONT  g_hPhaseFont = NULL;
HFONT  g_hUiFont    = NULL;
HBRUSH g_hBgBrush   = NULL;

int     g_defaultWorkMin = DEFAULT_WORK_MIN;
int     g_defaultRestMin = DEFAULT_REST_MIN;
wchar_t g_workSoundPath[MAX_PATH] = L"";
BOOL    g_workSoundPlaying = FALSE;
