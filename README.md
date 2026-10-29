# Tomato — a native Win32 Pomodoro Timer

A small, dependency-free Pomodoro timer for **Windows x64**, written in C++ against
the Win32 API and compiled with the Microsoft Visual C++ (MSVC) toolchain.

![App](resources/tomato.png)

## Features

- Work and Rest duration fields, each with a **Minutes / Seconds** unit dropdown
- **Start** and **Stop** buttons
- Large `MM:SS` countdown that automatically alternates between **WORK** and **REST**
- A **ding-bell chime** plays at the end of every phase
- A Rest value of **0** skips the rest phase entirely (work → work → …)
- Garamond UI font and a light-orange theme
- Custom tomato application icon (multi-resolution `.ico`)
- A **Settings** menu to set the default Work/Rest minutes, persisted per-user
  in the Windows registry (`HKCU\Software\Tomato\PomodoroTimer`)

No external libraries, package managers, or runtimes are required — only the
Windows SDK and MSVC that ship with Visual Studio Build Tools.

---

## Project layout

```
Tomato/
├── build.bat                 Build orchestrator (run from the project root)
├── README.md                 This file
├── src/
│   ├── pomodoro.cpp          Application source
│   ├── pomodoro.rc           Resource script (icon + manifest)
│   ├── pomodoro.manifest     Visual styles (Common Controls v6) + DPI awareness
│   └── make_icon.ps1         Regenerates resources/tomato.ico from the PNG
├── resources/
│   ├── tomato.png            512×512 source artwork
│   └── tomato.ico            Generated multi-size icon (16–256 px)
└── build/                    (created by the build)
    ├── pomodoro.exe          Final x64 binary
    └── obj/                  Intermediate files (.obj, .res)
```

> The `build/` directory is generated output. If you use version control you
> probably want to add it (and any generated `.ico`) to `.gitignore`.

---

## Prerequisites

You need a Windows machine and the **MSVC C++ build toolchain**. You do **not**
need the full Visual Studio IDE — the free **Build Tools** are enough.

### 1. Install Visual Studio Build Tools (C++)

1. Download **Build Tools for Visual Studio** from
   <https://visualstudio.microsoft.com/downloads/> (under *Tools for Visual Studio*),
   or install via winget:

   ```powershell
   winget install --id Microsoft.VisualStudio.2022.BuildTools
   ```

2. In the installer, select the **“Desktop development with C++”** workload.
   At minimum this must include:
   - **MSVC v143 (or newer) — VS C++ x64/x86 build tools**
   - **Windows 10 or Windows 11 SDK**

   These provide `cl.exe` (compiler), `link.exe` (linker), `rc.exe` (resource
   compiler), and the Win32 headers/libraries this project links against
   (`user32.lib`, `gdi32.lib`, `comctl32.lib`, `advapi32.lib`).

> This project was built and verified against **Visual Studio Build Tools 2026**
> with **MSVC 14.50.x** targeting x64, but any reasonably recent MSVC (v143+)
> and Windows 10/11 SDK will work.

### 2. PowerShell (already included with Windows)

Used only to (re)generate the icon via `src/make_icon.ps1`. The committed
`resources/tomato.ico` means you normally do not need to run it.

---

## Build from scratch

The build script finds your Visual Studio installation automatically (via
`vswhere.exe`), initializes the x64 developer environment, compiles the
resources, then compiles and links the executable. You do **not** need to open
a “Developer Command Prompt” first.

From the **project root**:

```bat
build.bat
```

Or from PowerShell:

```powershell
cmd /c build.bat
```

On success you will see:

```
[INFO] Using VS at: C:\Program Files (x86)\Microsoft Visual Studio\...\BuildTools
[INFO] Compiling resources...
[INFO] Compiling and linking build\pomodoro.exe...
[INFO] Build succeeded: build\pomodoro.exe
```

The finished program is **`build\pomodoro.exe`**.

### Run it

```bat
build\pomodoro.exe
```

(Double-clicking it in Explorer works too.)

---

## How the build works

`build.bat` performs these steps:

1. **Locate the toolchain.** It runs
   `%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe`
   to find the latest install that has the
   `Microsoft.VisualStudio.Component.VC.Tools.x86.x64` component.
2. **Initialize the x64 environment** by calling that install's
   `VC\Auxiliary\Build\vcvars64.bat` (puts `cl`, `link`, `rc`, and the SDK on `PATH`).
3. **Compile resources:** `rc /fo build\obj\pomodoro.res src\pomodoro.rc`
   (embeds the icon and the application manifest).
4. **Compile and link:**

   ```
   cl /nologo /W4 /EHsc /O2 /GS /sdl /DUNICODE /D_UNICODE ^
      /Fo"build\obj\\" ^
      src\pomodoro.cpp build\obj\pomodoro.res ^
      /link /subsystem:windows ^
      user32.lib gdi32.lib comctl32.lib advapi32.lib ^
      /out:"build\pomodoro.exe"
   ```

   | Flag | Purpose |
   |------|---------|
   | `/W4` | High warning level |
   | `/EHsc` | Standard C++ exception handling |
   | `/O2` | Optimize for speed |
   | `/GS` `/sdl` | Buffer-overrun and additional security checks |
   | `/DUNICODE /D_UNICODE` | Unicode (wide-char) build |
   | `/subsystem:windows` | GUI app (entry point is `wWinMain`, no console) |

---

## Regenerating the icon (optional)

`resources/tomato.ico` is committed, so this is only needed if you change
`resources/tomato.png`. From the project root:

```powershell
powershell -ExecutionPolicy Bypass -File src\make_icon.ps1
```

This resamples the PNG to 16/32/48/64/128/256-px and packs them into a single
multi-resolution `.ico`. Then re-run `build.bat` to embed the new icon.

---

## Clean build

Delete the generated output and rebuild:

```powershell
Remove-Item -Recurse -Force build
build.bat
```

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `vswhere.exe not found` | Visual Studio / Build Tools isn't installed. Install the **Desktop development with C++** workload (see Prerequisites). |
| `Could not find a VS installation with the VC x64 toolset` | The C++ x64 build tools component is missing. Re-run the VS Installer → **Modify** → add **MSVC … VS C++ x64/x86 build tools**. |
| `'cl' is not recognized` when running `cl` yourself | Don't call `cl` directly from a plain prompt. Use `build.bat`, which sets up the environment, or open a **“x64 Native Tools Command Prompt for VS”**. |
| `fatal error RC1015: cannot open ... windows.h` or missing SDK | Install a **Windows 10/11 SDK** via the VS Installer. |
| Icon or title bar shows a generic icon | Ensure `resources/tomato.ico` exists (run `make_icon.ps1`) and rebuild. |
| App shows old-style (non-themed) controls | The manifest in `src/pomodoro.manifest` must be embedded — this happens automatically through `pomodoro.rc`; just rebuild. |

---

## Notes

- Target architecture is **x64**. The build initializes `vcvars64.bat`; to target
  another architecture you would adjust the script accordingly.
- The Garamond UI font falls back to the system default if Garamond is not
  installed, so the app still runs on machines without it.
- Input is validated: Work must be ≥ 1 second, Rest ≥ 0 seconds, and the total
  of either phase is capped at 600 minutes.
