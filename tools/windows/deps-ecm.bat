@echo off
rem ============================================================================
rem deps-ecm.bat - clone, build, and install Extra CMake Modules (ECM) into
rem build\prefix, where the Sonnet stack expects it.
rem
rem Run inside the devcmd.bat environment so cmake + the MSVC compiler are
rem on PATH:
rem   tools\windows\devcmd.bat tools\windows\deps-ecm.bat
rem
rem Environment overrides (all optional):
rem   ECM_TAG   ECM git tag  (default v6.9.0, matching the Sonnet stack)
rem ============================================================================
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
if not defined ECM_TAG set "ECM_TAG=v6.9.0"
set "SRC_DIR=%REPO_ROOT%\build\deps-src\ecm"
set "PREFIX=%REPO_ROOT%\build\prefix"

where git >nul 2>nul
if errorlevel 1 (
    echo ERROR: git not found on PATH.
    exit /b 1
)
where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: cmake not found. Run this script via devcmd.bat.
    exit /b 1
)

if not exist "%SRC_DIR%" (
    echo Cloning extra-cmake-modules %ECM_TAG% ...
    git clone --depth 1 --branch "%ECM_TAG%" ^
        https://invent.kde.org/frameworks/extra-cmake-modules.git "%SRC_DIR%"
    if errorlevel 1 goto :fail
)

echo Configuring ECM ...
cmake -S "%SRC_DIR%" -B "%SRC_DIR%\build" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%PREFIX%" ^
    -DBUILD_TESTING=OFF
if errorlevel 1 goto :fail

echo Building and installing ECM to %PREFIX% ...
cmake --build "%SRC_DIR%\build"
if errorlevel 1 goto :fail
cmake --install "%SRC_DIR%\build"
if errorlevel 1 goto :fail

echo ECM installed to %PREFIX%.
exit /b 0

:fail
echo.
echo FAILED (exit %errorlevel%).
exit /b 1
