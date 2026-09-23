@echo off
rem ============================================================================
rem hunspell-cl.bat - build a static hunspell with MSVC and install it into
rem build\prefix for the Sonnet stack.
rem
rem Run inside the devcmd.bat environment so cmake + the MSVC compiler are
rem on PATH:
rem   tools\windows\devcmd.bat tools\windows\hunspell-cl.bat
rem
rem Environment overrides (all optional):
rem   HUNSPELL_TAG   hunspell git tag  (default v1.7.2)
rem ============================================================================
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
if not defined HUNSPELL_TAG set "HUNSPELL_TAG=v1.7.2"
set "SRC_DIR=%REPO_ROOT%\build\deps-src\hunspell"
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
    echo Cloning hunspell %HUNSPELL_TAG% ...
    git clone --depth 1 --branch "%HUNSPELL_TAG%" ^
        https://github.com/hunspell/hunspell.git "%SRC_DIR%"
    if errorlevel 1 goto :fail
)

echo Configuring static hunspell ...
cmake -S "%SRC_DIR%" -B "%SRC_DIR%\build" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%PREFIX%" ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DBUILD_TESTING=OFF
if errorlevel 1 goto :fail

echo Building and installing hunspell to %PREFIX% ...
cmake --build "%SRC_DIR%\build"
if errorlevel 1 goto :fail
cmake --install "%SRC_DIR%\build"
if errorlevel 1 goto :fail

echo hunspell installed to %PREFIX%.
exit /b 0

:fail
echo.
echo FAILED (exit %errorlevel%).
exit /b 1
