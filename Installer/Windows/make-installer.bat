@echo off
rem ============================================================================
rem make-installer.bat - build the offline installer with Qt Installer Framework.
rem
rem Prerequisite: Installer\Windows\windeploy.bat has run successfully (staged data dir).
rem
rem Usage:  devcmd.bat Installer\Windows\make-installer.bat
rem Output: Installer\Windows\MarkdownEditor-<version>-offline.exe
rem         (version parsed from CMakeLists.txt, the single source of truth)
rem ============================================================================
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "PKG_DIR=%REPO_ROOT%\Installer\Windows\packages"
set "CONFIG_DIR=%REPO_ROOT%\Installer\Windows\config"
set "IFW_BIN=C:\Qt\Tools\QtInstallerFramework\4.11\bin"

set "VERSION="
for /f "tokens=2" %%V in ('findstr /r /c:"^ *VERSION [0-9]" "%REPO_ROOT%\CMakeLists.txt"') do set "VERSION=%%V"
if "%VERSION%"=="" (
    echo ERROR: could not parse VERSION from CMakeLists.txt.
    goto :fail
)
set "OUT=%REPO_ROOT%\Installer\Windows\MarkdownEditor-%VERSION%-offline.exe"

if not exist "%PKG_DIR%\com.mdeditor.markdowneditor\data\markdowneditor.exe" (
    echo ERROR: staged data directory is empty or missing markdowneditor.exe.
    echo        Run first:  devcmd.bat Installer\Windows\windeploy.bat
    goto :fail
)

echo Creating offline installer...
"%IFW_BIN%\binarycreator.exe" ^
    --config "%CONFIG_DIR%\config.xml" ^
    --packages "%PKG_DIR%" ^
    --offline-only ^
    "%OUT%"
if errorlevel 1 goto :fail

if not exist "%OUT%" (
    echo ERROR: binarycreator reported success but %OUT% is missing.
    goto :fail
)

for %%F in ("%OUT%") do echo Created %%~fF ^(%%~zF bytes^)
echo Done.
exit /b 0

:fail
echo.
echo FAILED (exit %errorlevel%).
exit /b 1
