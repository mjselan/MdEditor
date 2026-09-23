@echo off
rem ============================================================================
rem windeploy.bat - stage the runnable app tree for the Qt Installer Framework.
rem
rem 1. Configures + builds the Release preset (devcmd.bat: MSVC + Qt + Ninja).
rem 2. Stages markdowneditor.exe and runs windeployqt6.exe next to it, which
rem    copies the required Qt DLLs, plugins and translations.
rem 3. Adds the MSVC C runtime DLLs (no vc_redist.exe dependency).
rem 4. Adds the optional Sonnet spell-check stack (client plugin + dictionaries)
rem    from build\prefix when it was built there.
rem
rem Usage:  devcmd.bat Installer\Windows\windeploy.bat
rem Output: Installer\Windows\packages\com.freebuff.markdowneditor\data
rem ============================================================================
setlocal EnableExtensions

rem --- Locate the repo root (two levels above this script) --------------------
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "PKG=%REPO_ROOT%\Installer\Windows\packages\com.freebuff.markdowneditor"
set "DATA=%PKG%\data"
set "QT_DIR=C:\Qt\6.11.1\msvc2022_64"
set "IFW_BIN=C:\Qt\Tools\QtInstallerFramework\4.11\bin"

echo [1/5] Building Release preset...
pushd "%REPO_ROOT%"
call build\devcmd.bat cmake --preset release
if errorlevel 1 goto :fail
call build\devcmd.bat cmake --build --preset release
if errorlevel 1 goto :fail
popd
if not exist "%REPO_ROOT%\build\release\markdowneditor.exe" (
    echo ERROR: build\release\markdowneditor.exe not found after build.
    goto :fail
)

echo [2/5] Cleaning and staging deploy directory...
if exist "%DATA%" rmdir /s /q "%DATA%"
mkdir "%DATA%" 2>nul
copy /y "%REPO_ROOT%\build\release\markdowneditor.exe" "%DATA%\" >nul
if errorlevel 1 goto :fail

echo [3/5] Running windeployqt6...
rem --no-translations: the UI is English-only.
rem --no-system-d3d-compiler --no-opengl-sw: not needed for a raster Widgets app.
rem --compiler-runtime is skipped on purpose: we copy the CRT from the local
rem MSVC redist below, which is more explicit than windeployqt's heuristic.
"%QT_DIR%\bin\windeployqt6.exe" ^
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
set "VC_REDIST_ROOT=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC"
set "CRT="
for /d %%D in ("%VC_REDIST_ROOT%\*") do (
    if exist "%%D\x64\Microsoft.VC143.CRT\vcruntime140.dll" set "CRT=%%D\x64\Microsoft.VC143.CRT"
    if exist "%%D\x64\Microsoft.VC145.CRT\vcruntime140.dll" set "CRT=%%D\x64\Microsoft.VC145.CRT"
)
if "%CRT%"=="" (
    echo ERROR: no Microsoft.VC14x.CRT redist directory found under %VC_REDIST_ROOT%.
    echo        Install the "MSVC v14x - VS 2022/2026 C++ x64/x86 build tools" component,
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
echo Next: devcmd.bat Installer\Windows\make-installer.bat
exit /b 0

:fail
popd 2>nul
echo.
echo FAILED (exit %errorlevel%).
exit /b 1
