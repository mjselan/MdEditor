@echo off
rem ============================================================================
rem deps-sonnet.bat - build KF6 Sonnet (Core + Ui) against build\prefix
rem (ECM + static hunspell) and install it there. Run deps-ecm.bat and
rem hunspell-cl.bat first.
rem
rem Run inside the devcmd.bat environment so cmake + the MSVC compiler and
rem the Qt kit are on PATH:
rem   tools\windows\devcmd.bat tools\windows\deps-sonnet.bat
rem
rem Environment overrides (all optional):
rem   SONNET_TAG   Sonnet git tag  (default v6.9.0)
rem   QT_DIR       Qt MSVC kit root (default C:\Qt\6.11.1\msvc2022_64)
rem ============================================================================
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
if not defined SONNET_TAG set "SONNET_TAG=v6.9.0"
if not defined QT_DIR set "QT_DIR=C:\Qt\6.11.1\msvc2022_64"
set "SRC_DIR=%REPO_ROOT%\build\deps-src\sonnet"
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
if not exist "%PREFIX%\share\KF6\ECM\ECMConfig.cmake" (
    if not exist "%PREFIX%\lib\cmake\ECM\ECMConfig.cmake" (
        echo ERROR: ECM not found under "%PREFIX%".
        echo        Run first:  devcmd.bat tools\windows\deps-ecm.bat
        goto :fail
    )
)

if not exist "%SRC_DIR%" (
    echo Cloning Sonnet %SONNET_TAG% ...
    git clone --depth 1 --branch "%SONNET_TAG%" ^
        https://invent.kde.org/frameworks/sonnet.git "%SRC_DIR%"
    if errorlevel 1 goto :fail
)

echo Configuring Sonnet ...
cmake -S "%SRC_DIR%" -B "%SRC_DIR%\build" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%PREFIX%" ^
    -DCMAKE_PREFIX_PATH="%PREFIX%;%QT_DIR%" ^
    -DBUILD_TESTING=OFF
if errorlevel 1 goto :fail

echo Building and installing Sonnet to %PREFIX% ...
cmake --build "%SRC_DIR%\build"
if errorlevel 1 goto :fail
cmake --install "%SRC_DIR%\build"
if errorlevel 1 goto :fail

echo Sonnet installed to %PREFIX%.
exit /b 0

:fail
echo.
echo FAILED (exit %errorlevel%).
exit /b 1
