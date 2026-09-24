@echo off
rem ============================================================================
rem windeploy.bat - stage the runnable app tree for the Qt Installer Framework.
rem
rem 1. Configures + builds the Release preset (devcmd.bat: MSVC + Qt + Ninja).
rem 2. Stages markdowneditor.exe and runs windeployqt next to it, which
rem    copies the required Qt DLLs, plugins and translations.
rem 3. Adds the MSVC C runtime DLLs (no vc_redist.exe dependency).
rem 4. Adds the optional Sonnet spell-check stack (client plugin + dictionaries)
rem    from build\prefix when it was built there.
rem
rem Usage:  tools\windows\devcmd.bat Installer\Windows\windeploy.bat
rem Output: Installer\Windows\packages\com.mdeditor.markdowneditor\data
rem
rem Environment overrides (all optional):
rem   QT_DIR                   Qt MSVC kit root (default C:\Qt\6.11.1\msvc2022_64)
rem   VC_REDIST_ROOT           MSVC redist root holding versioned subdirs
rem                            (default: auto-detected from the active MSVC
rem                            environment or the Visual Studio install)
rem   MARKDOWNEDITOR_SKIP_BUILD=1  reuse the existing build\release tree
rem                            instead of rebuilding the Release preset.
rem
rem When run inside an already-initialized MSVC environment (VSCMD_VER is set,
rem e.g. GitHub Actions, which exports the vcvars environment to each step),
rem cmake is invoked directly; otherwise the build goes through
rem tools\windows\devcmd.bat.
rem ============================================================================
setlocal EnableExtensions

rem --- Locate the repo root (two levels above this script) --------------------
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "PKG=%REPO_ROOT%\Installer\Windows\packages\com.mdeditor.markdowneditor"
set "DATA=%PKG%\data"
if not defined QT_DIR set "QT_DIR=C:\Qt\6.11.1\msvc2022_64"
set "IFW_BIN=C:\Qt\Tools\QtInstallerFramework\4.11\bin"

echo [1/5] Building Release preset...
if defined MARKDOWNEDITOR_SKIP_BUILD (
    echo       MARKDOWNEDITOR_SKIP_BUILD=1 - reusing the existing build tree.
) else if defined VSCMD_VER (
    echo       Using the active MSVC environment (VSCMD_VER=%VSCMD_VER%).
    pushd "%REPO_ROOT%"
    call cmake --preset release
    if errorlevel 1 goto :fail
    call cmake --build --preset release
    if errorlevel 1 goto :fail
    popd
) else (
    pushd "%REPO_ROOT%"
    call tools\windows\devcmd.bat cmake --preset release
    if errorlevel 1 goto :fail
    call tools\windows\devcmd.bat cmake --build --preset release
    if errorlevel 1 goto :fail
    popd
)
if not exist "%REPO_ROOT%\build\release\markdowneditor.exe" (
    echo ERROR: build\release\markdowneditor.exe not found after build.
    goto :fail
)

echo [2/5] Cleaning and staging deploy directory...
if exist "%DATA%" rmdir /s /q "%DATA%"
mkdir "%DATA%" 2>nul
copy /y "%REPO_ROOT%\build\release\markdowneditor.exe" "%DATA%\" >nul
if errorlevel 1 goto :fail

echo [3/5] Running windeployqt...
rem --no-translations: the UI is English-only.
rem --no-system-d3d-compiler --no-opengl-sw: not needed for a raster Widgets app.
rem --compiler-runtime is skipped on purpose: we copy the CRT from the local
rem MSVC redist below, which is more explicit than windeployqt's heuristic.
set "WINDEPLOYQT=%QT_DIR%\bin\windeployqt6.exe"
if not exist "%WINDEPLOYQT%" set "WINDEPLOYQT=%QT_DIR%\bin\windeployqt.exe"
if not exist "%WINDEPLOYQT%" (
    echo ERROR: neither windeployqt6.exe nor windeployqt.exe found under "%QT_DIR%\bin".
    echo        Set QT_DIR to your Qt 6 MSVC kit root.
    goto :fail
)
"%WINDEPLOYQT%" ^
    --dir "%DATA%" ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    "%DATA%\markdowneditor.exe"
if errorlevel 1 goto :fail
rem windeployqt drops vc_redist.x64.exe unless told otherwise; the explicit CRT
rem DLLs staged below supersede it, so delete the redundant bootstrapper.
if exist "%DATA%\vc_redist.x64.exe" del "%DATA%\vc_redist.x64.exe"

echo [4/5] Adding MSVC C runtime DLLs...
call :find_crt
if "%CRT%"=="" (
    echo ERROR: no Microsoft.VC14x.CRT redist directory found.
    echo        Install the "MSVC v14x - VS 2022 C++ x64/x86 build tools" component,
    echo        or set VC_REDIST_ROOT to your VS VC\Redist\MSVC directory,
    echo        or copy vcruntime140.dll + msvcp140.dll manually into the data dir.
    goto :fail
)
copy /y "%CRT%\vcruntime140.dll" "%DATA%\" >nul || goto :fail
copy /y "%CRT%\vcruntime140_1.dll" "%DATA%\" >nul 2>&1
copy /y "%CRT%\msvcp140.dll" "%DATA%\" >nul || goto :fail

echo [5/5] Adding Sonnet spell-check stack...
if exist "%REPO_ROOT%\build\prefix\lib\plugins\kf6\sonnet\sonnet_hunspell.dll" (
    rem SonnetCore/Ui runtime DLLs - the app links them directly, so they must
    rem sit next to the exe exactly like the Qt6*.dll set.
    copy /y "%REPO_ROOT%\build\prefix\bin\KF6SonnetCore.dll" "%DATA%\" >nul || goto :fail
    copy /y "%REPO_ROOT%\build\prefix\bin\KF6SonnetUi.dll" "%DATA%\" >nul || goto :fail
    rem Speller client plugins are loaded at runtime from <exeDir>/kf6/sonnet.
    rem hunspell = primary backend; ispellchecker = native Windows fallback.
    mkdir "%DATA%\kf6\sonnet" 2>nul
    copy /y "%REPO_ROOT%\build\prefix\lib\plugins\kf6\sonnet\sonnet_hunspell.dll" "%DATA%\kf6\sonnet\" >nul || goto :fail
    if exist "%REPO_ROOT%\build\prefix\lib\plugins\kf6\sonnet\sonnet_ispellchecker.dll" copy /y "%REPO_ROOT%\build\prefix\lib\plugins\kf6\sonnet\sonnet_ispellchecker.dll" "%DATA%\kf6\sonnet\" >nul
    if exist "%REPO_ROOT%\build\prefix\bin\data\hunspell" (
        mkdir "%DATA%\data\hunspell" 2>nul
        copy /y "%REPO_ROOT%\build\prefix\bin\data\hunspell\*.dic" "%DATA%\data\hunspell\" >nul
        copy /y "%REPO_ROOT%\build\prefix\bin\data\hunspell\*.aff" "%DATA%\data\hunspell\" >nul
    )
    echo       Sonnet: plugin + dictionaries staged.
) else (
    echo       build\prefix has no Sonnet stack - staging without spell check.
)

echo.
echo Deploy tree staged at:
echo   %DATA%
echo   (DLLs, plugins, translation-free Qt runtime, CRT, dictionaries)
echo.
echo Next: tools\windows\devcmd.bat Installer\Windows\make-installer.bat
exit /b 0

:fail
popd 2>nul
echo.
echo FAILED (exit %errorlevel%).
exit /b 1

rem --- Locate the MSVC redist CRT directory; result in CRT (empty = missing) --
:find_crt
set "CRT="
rem 1. Explicit override: a VC\Redist\MSVC root holding versioned subdirs.
if defined VC_REDIST_ROOT (
    call :pick_crt "%VC_REDIST_ROOT%"
    if defined CRT exit /b 0
    echo WARNING: VC_REDIST_ROOT has no usable CRT: %VC_REDIST_ROOT%
)
rem 2. Active MSVC environment (vcvars / msvc-dev-cmd) already knows its root.
if defined VCToolsRedistDir (
    call :pick_crt "%VCToolsRedistDir%..\.."
    if defined CRT exit /b 0
)
rem 3. Newest Visual Studio reported by vswhere.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "delims=" %%V in ('"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul') do (
        if not defined CRT (
            if exist "%%V\VC\Redist\MSVC" call :pick_crt "%%V\VC\Redist\MSVC"
        )
    )
)
if defined CRT exit /b 0
rem 4. Well-known install locations (VS 18 and 2022 editions, Build Tools).
for %%R in (
    "%ProgramFiles%\Microsoft Visual Studio\18\Enterprise"
    "%ProgramFiles%\Microsoft Visual Studio\18\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\18\Community"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
) do (
    if not defined CRT (
        if exist "%%~R\VC\Redist\MSVC" call :pick_crt "%%~R\VC\Redist\MSVC"
    )
)
exit /b 0

rem --- Pick the newest CRT under a VC\Redist\MSVC root; result in CRT ---------
:pick_crt
rem %1 = CRT dir itself, a versioned Redist subdir, or the Redist\MSVC root.
rem Version dirs compare numerically (14.44 beats 14.9); within the winning
rem dir the VC145 CRT is preferred over VC143.
set "CRT="
if "%~1"=="" exit /b 1
if exist "%~1\vcruntime140.dll" (
    set "CRT=%~1"
    exit /b 0
)
if exist "%~1\x64\Microsoft.VC145.CRT\vcruntime140.dll" (
    set "CRT=%~1\x64\Microsoft.VC145.CRT"
    exit /b 0
)
if exist "%~1\x64\Microsoft.VC143.CRT\vcruntime140.dll" (
    set "CRT=%~1\x64\Microsoft.VC143.CRT"
    exit /b 0
)
set "CRT_BEST_MAJ=-1"
set "CRT_BEST_MIN=-1"
set "CRT_BEST_PAT=-1"
set "CRT_BEST_DIR="
for /f "delims=" %%D in ('dir /b /ad "%~1" 2^>nul') do call :crt_better "%~1" "%%D"
if defined CRT_BEST_DIR (
    if exist "%CRT_BEST_DIR%\x64\Microsoft.VC145.CRT\vcruntime140.dll" set "CRT=%CRT_BEST_DIR%\x64\Microsoft.VC145.CRT"
)
if not defined CRT (
    if defined CRT_BEST_DIR (
        if exist "%CRT_BEST_DIR%\x64\Microsoft.VC143.CRT\vcruntime140.dll" set "CRT=%CRT_BEST_DIR%\x64\Microsoft.VC143.CRT"
    )
)
set "CRT_BEST_DIR="
if defined CRT exit /b 0
exit /b 1

rem --- Remember a Redist subdir when it holds a newer CRT than CRT_BEST_* -----
:crt_better
if not exist "%~1\%~2\x64\Microsoft.VC145.CRT\vcruntime140.dll" (
    if not exist "%~1\%~2\x64\Microsoft.VC143.CRT\vcruntime140.dll" exit /b 0
)
set "CMAJ="
set "CMIN="
set "CPAT="
for /f "tokens=1-3 delims=." %%A in ("%~2") do (
    set "CMAJ=%%A"
    set "CMIN=%%B"
    set "CPAT=%%C"
)
if "%CMIN%"=="" set "CMIN=0"
if "%CPAT%"=="" set "CPAT=0"
call :is_digits "%CMAJ%"
if not "%NUM_OK%"=="1" exit /b 0
call :is_digits "%CMIN%"
if not "%NUM_OK%"=="1" exit /b 0
call :is_digits "%CPAT%"
if not "%NUM_OK%"=="1" exit /b 0
if %CMAJ% GTR %CRT_BEST_MAJ% goto :crt_take
if %CMAJ% LSS %CRT_BEST_MAJ% exit /b 0
if %CMIN% GTR %CRT_BEST_MIN% goto :crt_take
if %CMIN% LSS %CRT_BEST_MIN% exit /b 0
if %CPAT% GTR %CRT_BEST_PAT% goto :crt_take
exit /b 0
:crt_take
set "CRT_BEST_MAJ=%CMAJ%"
set "CRT_BEST_MIN=%CMIN%"
set "CRT_BEST_PAT=%CPAT%"
set "CRT_BEST_DIR=%~1\%~2"
exit /b 0

rem --- Set NUM_OK=1 when %1 consists only of ASCII digits --------------------
:is_digits
set "NUM_OK="
if "%~1"=="" exit /b 0
set "NUM_TMP=%~1"
for %%N in (0 1 2 3 4 5 6 7 8 9) do call :strip_digit %%N
if "%NUM_TMP%"=="" set "NUM_OK=1"
exit /b 0

:strip_digit
if "%NUM_TMP%"=="" exit /b 0
call set "NUM_TMP=%%NUM_TMP:%1=%%"
exit /b 0
