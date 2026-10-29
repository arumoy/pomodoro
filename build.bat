@echo off
REM ===========================================================================
REM build.bat - Build the Pomodoro Timer for Windows x64 using MSVC.
REM
REM Project layout:
REM   src\        source, resource script, manifest, icon generator
REM   resources\  tomato.png (source art) and tomato.ico (generated)
REM   build\      final binary (pomodoro.exe)
REM   build\obj\  intermediate/generated files (.obj, .res)
REM
REM Locates the Visual Studio Build Tools via vswhere, initializes the x64
REM developer environment, compiles the resource script, then compiles and
REM links build\pomodoro.exe.
REM ===========================================================================
setlocal enableextensions

REM Always operate relative to this script's directory (the project root).
pushd "%~dp0"

set "SRCDIR=src"
set "OBJDIR=build\obj"
set "OUTDIR=build"
set "SRC=%SRCDIR%\pomodoro.cpp"
set "RC=%SRCDIR%\pomodoro.rc"
set "RES=%OBJDIR%\pomodoro.res"
set "OUT=%OUTDIR%\pomodoro.exe"

REM --- Ensure output directories exist -------------------------------------
if not exist "%OBJDIR%" mkdir "%OBJDIR%"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM --- Locate vcvars64.bat via vswhere -------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found. Is Visual Studio / Build Tools installed?
    popd & exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -products * -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VSINSTALL=%%i"
)

if not defined VSINSTALL (
    echo [ERROR] Could not find a VS installation with the VC x64 toolset.
    popd & exit /b 1
)

set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found at "%VCVARS%".
    popd & exit /b 1
)

echo [INFO] Using VS at: %VSINSTALL%
call "%VCVARS%" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize the x64 build environment.
    popd & exit /b 1
)

REM --- Compile the resource script -----------------------------------------
REM The icon path inside the .rc is relative to the .rc file (src\), so it
REM references ..\resources\tomato.ico.
echo [INFO] Compiling resources...
rc /nologo /fo "%RES%" "%RC%"
if errorlevel 1 (
    echo [ERROR] Resource compilation failed.
    popd & exit /b 1
)

REM --- Compile and link ----------------------------------------------------
REM   /W4        high warning level
REM   /EHsc      standard C++ exception handling
REM   /O2        optimize for speed
REM   /GS        buffer security checks (stack canaries)
REM   /sdl       additional security checks
REM   /Fo        intermediate object directory (trailing backslash)
REM   /subsystem:windows  GUI app (uses wWinMain, no console)
echo [INFO] Compiling and linking %OUT%...
cl /nologo /W4 /EHsc /O2 /GS /sdl /DUNICODE /D_UNICODE ^
   /Fo"%OBJDIR%\\" ^
   "%SRC%" "%RES%" ^
   /link /subsystem:windows ^
   user32.lib gdi32.lib comctl32.lib advapi32.lib winmm.lib comdlg32.lib ^
   /out:"%OUT%"
if errorlevel 1 (
    echo [ERROR] Build failed.
    popd & exit /b 1
)

echo [INFO] Build succeeded: %OUT%
popd
endlocal
