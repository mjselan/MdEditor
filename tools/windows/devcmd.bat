@echo off
rem ============================================================================
rem devcmd.bat - run a command inside the MSVC + Qt + Ninja environment.
rem
rem Environment overrides (all optional):
rem   QT_DIR    Qt MSVC kit root        (default C:\Qt\6.11.1\msvc2022_64)
rem   VS_DIR    Visual Studio root      (default ...\Microsoft Visual Studio\18\Community)
rem   VC_ARCH   vcvarsall argument      (default x64)
rem   NINJA_DIR Ninja binary directory  (default <QT_DIR>\..\..\Tools\Ninja)
rem
rem Usage:  tools\windows\devcmd.bat <command...>
rem Example: tools\windows\devcmd.bat cmake --preset debug
rem ============================================================================
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: devcmd.bat ^<command...^>
    exit /b 2
)

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

if not defined QT_DIR set "QT_DIR=C:\Qt\6.11.1\msvc2022_64"
if not defined VS_DIR set "VS_DIR=C:\Program Files\Microsoft Visual Studio\18\Community"
if not defined VC_ARCH set "VC_ARCH=x64"

if not exist "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo ERROR: vcvarsall.bat not found under "%VS_DIR%".
    echo        Set VS_DIR to your Visual Studio installation root.
    exit /b 1
)
call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" %VC_ARCH% >nul
if errorlevel 1 exit /b 1

if not exist "%QT_DIR%\bin\qmake6.exe" (
    echo ERROR: Qt kit not found under "%QT_DIR%".
    echo        Set QT_DIR to your Qt 6 MSVC kit root.
    exit /b 1
)
set "PATH=%QT_DIR%\bin;%PATH%"

if not defined NINJA_DIR (
    for %%I in ("%QT_DIR%\..\..\Tools\Ninja") do (
        if exist "%%~fI\ninja.exe" set "NINJA_DIR=%%~fI"
    )
    if not defined NINJA_DIR (
        if exist "C:\Qt\Tools\Ninja\ninja.exe" set "NINJA_DIR=C:\Qt\Tools\Ninja"
    )
)
if defined NINJA_DIR set "PATH=%NINJA_DIR%;%PATH%"

where cmake >nul 2>nul
if errorlevel 1 (
    for %%I in ("%QT_DIR%\..\..\Tools\CMake_64\bin") do (
        if exist "%%~fI\cmake.exe" set "PATH=%%~fI;%PATH%"
    )
)
where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: cmake not found on PATH and no bundled CMake under Tools.
    echo        Install CMake or add it to PATH.
    exit /b 1
)

call %*
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%
