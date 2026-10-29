// ui.cpp - MainWindow implementation.
#include "ui.h"
#include "resource.h"
#include "settings_dlg.h"
#include <commdlg.h>   // GetOpenFileNameW
#include <strsafe.h>
#include <string>

namespace {
const wchar_t kClassName[] = L"PomodoroTimerWindow";
}

MainWindow::MainWindow(HINSTANCE hInst)
    : hInst_(hInst),
      timer_(nullptr, *this, sound_) {   // host set in create()
    settings_.load();
    sound_.setPath(settings_.workSoundPath());
}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------
bool MainWindow::create(int nCmdShow) {
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &MainWindow::wndProc;
    wc.hInstance     = hInst_;
    wc.lpszClassName = kClassName;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);

    // Light orange background (RAII brush; also used by WM_CTLCOLORSTATIC).
    bgBrush_.reset(CreateSolidBrush(APP_BG_COLOR));
    wc.hbrBackground = bgBrush_ ? bgBrush_.get()
                                : (HBRUSH)(COLOR_BTNFACE + 1);

    wc.hIcon = (HICON)LoadImageW(hInst_, MAKEINTRESOURCEW(IDI_APPICON),
                                 IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(hInst_, MAKEINTRESOURCEW(IDI_APPICON),
                                   IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (!wc.hIcon) {
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Error",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    RECT rc = {0, 0, 420, 300};
    DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX);
    AdjustWindowRect(&rc, style, TRUE);  // TRUE: window has a menu bar

    // Menu bar: Settings popup.
    HMENU hMenuBar = CreateMenu();
    HMENU hSettings = CreatePopupMenu();
    AppendMenuW(hSettings, MF_STRING, IDM_SETTINGS, L"Default Times...");
    AppendMenuW(hSettings, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSettings, MF_STRING, IDM_WORK_SOUND,
                L"Select Work Sound (.wav)...");
    AppendMenuW(hSettings, MF_STRING, IDM_CLEAR_SOUND, L"Clear Work Sound");
    AppendMenuW(hSettings, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSettings, MF_STRING, IDM_EXIT, L"Exit");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hSettings, L"Settings");

    // Pass `this` through CreateWindow so WM_NCCREATE can stash it.
    hwnd_ = CreateWindowExW(
        0, kClassName, L"Pomodoro Timer", style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, hMenuBar, hInst_, this);

    if (!hwnd_) {
        MessageBoxW(nullptr, L"Failed to create window.", L"Error",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    timer_.setHost(hwnd_);
    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
    return true;
}

void MainWindow::createControls() {
    HWND hWorkLbl = CreateWindowExW(0, L"STATIC", L"Work:",
                    WS_CHILD | WS_VISIBLE | SS_RIGHT,
                    20, 24, 90, 22, hwnd_, nullptr, hInst_, nullptr);
    HWND hRestLbl = CreateWindowExW(0, L"STATIC", L"Rest:",
                    WS_CHILD | WS_VISIBLE | SS_RIGHT,
                    20, 58, 90, 22, hwnd_, nullptr, hInst_, nullptr);

    hWorkEdit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_LEFT,
        120, 22, 70, 24, hwnd_, (HMENU)(INT_PTR)IDC_WORK_EDIT, hInst_, nullptr);
    SendMessageW(hWorkEdit_, EM_SETLIMITTEXT, 3, 0);

    hRestEdit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_LEFT,
        120, 56, 70, 24, hwnd_, (HMENU)(INT_PTR)IDC_REST_EDIT, hInst_, nullptr);
    SendMessageW(hRestEdit_, EM_SETLIMITTEXT, 3, 0);

    hWorkUnit_ = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        200, 22, 110, 120, hwnd_, (HMENU)(INT_PTR)IDC_WORK_UNIT, hInst_, nullptr);
    SendMessageW(hWorkUnit_, CB_ADDSTRING, 0, (LPARAM)L"Minutes");
    SendMessageW(hWorkUnit_, CB_ADDSTRING, 0, (LPARAM)L"Seconds");
    SendMessageW(hWorkUnit_, CB_SETCURSEL, UNIT_MINUTES, 0);

    hRestUnit_ = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        200, 56, 110, 120, hwnd_, (HMENU)(INT_PTR)IDC_REST_UNIT, hInst_, nullptr);
    SendMessageW(hRestUnit_, CB_ADDSTRING, 0, (LPARAM)L"Minutes");
    SendMessageW(hRestUnit_, CB_ADDSTRING, 0, (LPARAM)L"Seconds");
    SendMessageW(hRestUnit_, CB_SETCURSEL, UNIT_MINUTES, 0);

    hPhaseLabel_ = CreateWindowExW(
        0, L"STATIC", L"READY", WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 100, 380, 28, hwnd_, (HMENU)(INT_PTR)IDC_PHASE_LABEL, hInst_, nullptr);

    hTimeLabel_ = CreateWindowExW(
        0, L"STATIC", L"00:00", WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 128, 380, 64, hwnd_, (HMENU)(INT_PTR)IDC_TIME_LABEL, hInst_, nullptr);

    hStartBtn_ = CreateWindowExW(
        0, L"BUTTON", L"Start",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        100, 206, 100, 34, hwnd_, (HMENU)(INT_PTR)IDC_START_BTN, hInst_, nullptr);

    hStopBtn_ = CreateWindowExW(
        0, L"BUTTON", L"Stop",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        220, 206, 100, 34, hwnd_, (HMENU)(INT_PTR)IDC_STOP_BTN, hInst_, nullptr);

    // Fonts - Garamond throughout (serif family hint FF_ROMAN).
    timeFont_.reset(CreateFontW(52, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_ROMAN, APP_FONT_NAME));
    phaseFont_.reset(CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_ROMAN, APP_FONT_NAME));
    uiFont_.reset(CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_ROMAN, APP_FONT_NAME));

    if (timeFont_)  SendMessageW(hTimeLabel_, WM_SETFONT, (WPARAM)timeFont_.get(), TRUE);
    if (phaseFont_) SendMessageW(hPhaseLabel_, WM_SETFONT, (WPARAM)phaseFont_.get(), TRUE);

    HFONT hUi = uiFont_ ? uiFont_.get() : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessageW(hWorkEdit_, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(hRestEdit_, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(hWorkUnit_, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(hRestUnit_, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(hStartBtn_, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(hStopBtn_,  WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(hWorkLbl, WM_SETFONT, (WPARAM)hUi, TRUE);
    SendMessageW(hRestLbl, WM_SETFONT, (WPARAM)hUi, TRUE);

    applyDefaultTimesToFields();
    setControlsRunning(false);
}

// ---------------------------------------------------------------------------
// ITimerView
// ---------------------------------------------------------------------------
void MainWindow::showTime(int remainingSeconds) {
    wchar_t buf[16];
    StringCchPrintfW(buf, ARRAYSIZE(buf), L"%02d:%02d",
                     remainingSeconds / 60, remainingSeconds % 60);
    SetWindowTextW(hTimeLabel_, buf);
}

void MainWindow::showPhase(Phase phase) {
    SetWindowTextW(hPhaseLabel_, phase == Phase::Work ? L"WORK" : L"REST");
}

void MainWindow::showStopped() {
    SetWindowTextW(hPhaseLabel_, L"STOPPED");
    SetWindowTextW(hTimeLabel_, L"00:00");
}

void MainWindow::setControlsRunning(bool running) {
    EnableWindow(hStartBtn_, !running);
    EnableWindow(hStopBtn_,  running);
    EnableWindow(hWorkEdit_, !running);
    EnableWindow(hRestEdit_, !running);
    EnableWindow(hWorkUnit_, !running);
    EnableWindow(hRestUnit_, !running);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
std::optional<int> MainWindow::readDuration(HWND edit, HWND unit,
                                            int minSeconds) const {
    wchar_t buf[16] = {0};
    int len = GetWindowTextW(edit, buf, ARRAYSIZE(buf));
    if (len <= 0) return std::nullopt;
    for (int i = 0; i < len; ++i) {
        if (buf[i] < L'0' || buf[i] > L'9') return std::nullopt;
    }
    long value = 0;
    for (int i = 0; i < len; ++i) {
        value = value * 10 + (buf[i] - L'0');
        if (value > MAX_FIELD_VALUE) return std::nullopt;
    }
    LRESULT sel = SendMessageW(unit, CB_GETCURSEL, 0, 0);
    long seconds = (sel == UNIT_SECONDS) ? value : value * 60;
    if (seconds < minSeconds || seconds > MAX_TOTAL_SECONDS) return std::nullopt;
    return static_cast<int>(seconds);
}

void MainWindow::onStart() {
    std::optional<int> work = readDuration(hWorkEdit_, hWorkUnit_, MIN_WORK_SECONDS);
    if (!work) {
        MessageBoxW(hwnd_,
                    L"Please enter a valid Work duration.\n\n"
                    L"Minutes: 1 to 600.  Seconds: 1 to 999 "
                    L"(total max 600 minutes).",
                    L"Invalid input", MB_OK | MB_ICONWARNING);
        return;
    }
    std::optional<int> rest = readDuration(hRestEdit_, hRestUnit_, MIN_REST_SECONDS);
    if (!rest) {
        MessageBoxW(hwnd_,
                    L"Please enter a valid Rest duration.\n\n"
                    L"Use 0 to skip the rest phase. "
                    L"Minutes: 0 to 600.  Seconds: 0 to 999 "
                    L"(total max 600 minutes).",
                    L"Invalid input", MB_OK | MB_ICONWARNING);
        return;
    }
    timer_.start(*work, *rest);
}

void MainWindow::applyDefaultTimesToFields() {
    wchar_t num[16];
    StringCchPrintfW(num, ARRAYSIZE(num), L"%d", settings_.defaultWorkMin());
    SetWindowTextW(hWorkEdit_, num);
    SendMessageW(hWorkUnit_, CB_SETCURSEL, UNIT_MINUTES, 0);
    StringCchPrintfW(num, ARRAYSIZE(num), L"%d", settings_.defaultRestMin());
    SetWindowTextW(hRestEdit_, num);
    SendMessageW(hRestUnit_, CB_SETCURSEL, UNIT_MINUTES, 0);
}

void MainWindow::openSettingsDialog() {
    SettingsDialog dlg(settings_, [this]() {
        if (!timer_.running()) {
            applyDefaultTimesToFields();
        }
    });
    dlg.show(hInst_, hwnd_);
}

void MainWindow::onChooseWorkSound() {
    wchar_t path[MAX_PATH];
    StringCchCopyW(path, ARRAYSIZE(path), settings_.workSoundPath().c_str());

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd_;
    ofn.lpstrFilter = L"WAV audio (*.wav)\0*.wav\0All files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = ARRAYSIZE(path);
    ofn.lpstrTitle  = L"Select a .wav to loop during WORK";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrDefExt = L"wav";

    if (!GetOpenFileNameW(&ofn)) {
        return;  // cancelled
    }

    settings_.setWorkSoundPath(path);
    sound_.setPath(settings_.workSoundPath());
    if (!settings_.saveWorkSound()) {
        MessageBoxW(hwnd_,
                    L"The work-sound path could not be saved to the registry.",
                    L"Warning", MB_OK | MB_ICONWARNING);
    }
    // Restart the loop so a new file takes effect if we are mid-WORK.
    sound_.stop();
    if (timer_.running() && timer_.phase() == Phase::Work) {
        sound_.start();
    }
}

void MainWindow::onClearWorkSound() {
    settings_.setWorkSoundPath(L"");
    sound_.setPath(L"");
    settings_.saveWorkSound();
    sound_.stop();
    MessageBoxW(hwnd_, L"Work sound cleared.", L"Settings",
                MB_OK | MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
// Win32 plumbing
// ---------------------------------------------------------------------------
LRESULT CALLBACK MainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->handle(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::handle(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        createControls();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_START_BTN:  onStart();               return 0;
        case IDC_STOP_BTN:   timer_.stop();           return 0;
        case IDM_SETTINGS:   openSettingsDialog();    return 0;
        case IDM_EXIT:       DestroyWindow(hwnd_);    return 0;
        case IDM_WORK_SOUND: onChooseWorkSound();     return 0;
        case IDM_CLEAR_SOUND:onClearWorkSound();      return 0;
        }
        break;

    case WM_TIMER:
        if (wParam == TIMER_ID) {
            timer_.tick();
        }
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)bgBrush_.get();
    }

    case WM_DESTROY:
        timer_.stop();     // kills the WM_TIMER and stops audio
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
