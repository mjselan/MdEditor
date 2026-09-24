@echo off
rem ============================================================================
rem make-installer.bat - build the offline installer with Qt Installer Framework.
rem
rem Prerequisite: Installer\Windows\windeploy.bat has run successfully (staged data dir).
rem
rem Usage:  tools\windows\devcmd.bat Installer\Windows\make-installer.bat
rem Output: Installer\Windows\MarkdownEditor-<version>-offline.exe
rem         (version from MARKDOWNEDITOR_VERSION, else parsed from
rem         CMakeLists.txt, the single source of truth)
rem
rem Environment overrides (all optional):
rem   IFW_BIN                binarycreator.exe path or its bin directory
rem                          (default: auto-detected Qt Installer Framework 4.x,
rem                          preferring 4.11)
rem   QT_DIR                 Qt MSVC kit root, used to find a sibling Tools
rem                          install of the Installer Framework
rem   MARKDOWNEDITOR_VERSION override the version in the output filename
rem ============================================================================
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "PKG_DIR=%REPO_ROOT%\Installer\Windows\packages"
set "CONFIG_DIR=%REPO_ROOT%\Installer\Windows\config"
if not defined IFW_BIN set "IFW_BIN=C:\Qt\Tools\QtInstallerFramework\4.11\bin"
rem Accept either the binarycreator.exe path or its containing directory.
if exist "%IFW_BIN%\binarycreator.exe" set "IFW_BIN=%IFW_BIN%\binarycreator.exe"
if not exist "%IFW_BIN%" call :find_ifw
if not exist "%IFW_BIN%" (
    echo ERROR: Qt Installer Framework binarycreator was not found.
    echo        Install Qt Installer Framework 4.x or set IFW_BIN to its bin directory.
    goto :fail
)

set "VERSION="
if defined MARKDOWNEDITOR_VERSION set "VERSION=%MARKDOWNEDITOR_VERSION%"
if "%VERSION%"=="" (
    for /f "tokens=2" %%V in ('findstr /r /c:"^ *VERSION [0-9]" "%REPO_ROOT%\CMakeLists.txt"') do set "VERSION=%%V"
)
if "%VERSION%"=="" (
    echo ERROR: could not parse VERSION from CMakeLists.txt.
    goto :fail
)
set "OUT=%REPO_ROOT%\Installer\Windows\MarkdownEditor-%VERSION%-offline.exe"

if not exist "%PKG_DIR%\com.mdeditor.markdowneditor\data\markdowneditor.exe" (
    echo ERROR: staged data directory is empty or missing markdowneditor.exe.
    echo        Run first:  tools\windows\devcmd.bat Installer\Windows\windeploy.bat
    goto :fail
)

echo Creating offline installer...
"%IFW_BIN%" ^
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

rem --- Locate binarycreator.exe; result in IFW_BIN (empty = missing) ---------
:find_ifw
set "IFW_BIN="
rem 1. Sibling Tools of the Qt kit (online installer layout).
if defined QT_DIR (
    for %%T in ("%QT_DIR%\..\..\Tools\QtInstallerFramework" "%QT_DIR%\..\Tools\QtInstallerFramework") do (
        if not defined IFW_BIN call :pick_ifw "%%~T"
    )
)
rem 2. Default install location.
if not defined IFW_BIN call :pick_ifw "C:\Qt\Tools\QtInstallerFramework"
rem 3. Whatever is already on PATH (CI may have pre-installed it).
if not defined IFW_BIN (
    for /f "delims=" %%B in ('where binarycreator.exe 2^>nul') do (
        if not defined IFW_BIN set "IFW_BIN=%%B"
    )
)
exit /b 0

rem --- Pick binarycreator under a QtInstallerFramework root -------------------
rem %1 = root dir; prefer 4.11, else the numerically newest with binarycreator.
:pick_ifw
set "IFW_BIN="
if "%~1"=="" exit /b 1
if exist "%~1\4.11\bin\binarycreator.exe" (
    set "IFW_BIN=%~1\4.11\bin\binarycreator.exe"
    exit /b 0
)
set "IFW_BEST_MAJ=-1"
set "IFW_BEST_MIN=-1"
set "IFW_BEST_PAT=-1"
set "IFW_BEST_DIR="
for /f "delims=" %%D in ('dir /b /ad "%~1" 2^>nul') do (
    if exist "%~1\%%D\bin\binarycreator.exe" call :ifw_better "%%D"
)
if defined IFW_BEST_DIR set "IFW_BIN=%~1\%IFW_BEST_DIR%\bin\binarycreator.exe"
set "IFW_BEST_DIR="
exit /b 0

rem --- Remember a version dir when numerically newer than IFW_BEST_* -----------
:ifw_better
set "VMAJ="
set "VMIN="
set "VPAT="
for /f "tokens=1-3 delims=." %%A in ("%~1") do (
    set "VMAJ=%%A"
    set "VMIN=%%B"
    set "VPAT=%%C"
)
if "%VMIN%"=="" set "VMIN=0"
if "%VPAT%"=="" set "VPAT=0"
call :is_digits "%VMAJ%"
if not "%NUM_OK%"=="1" exit /b 0
call :is_digits "%VMIN%"
if not "%NUM_OK%"=="1" exit /b 0
call :is_digits "%VPAT%"
if not "%NUM_OK%"=="1" exit /b 0
if %VMAJ% GTR %IFW_BEST_MAJ% goto :ifw_take
if %VMAJ% LSS %IFW_BEST_MAJ% exit /b 0
if %VMIN% GTR %IFW_BEST_MIN% goto :ifw_take
if %VMIN% LSS %IFW_BEST_MIN% exit /b 0
if %VPAT% GTR %IFW_BEST_PAT% goto :ifw_take
exit /b 0
:ifw_take
set "IFW_BEST_MAJ=%VMAJ%"
set "IFW_BEST_MIN=%VMIN%"
set "IFW_BEST_PAT=%VPAT%"
set "IFW_BEST_DIR=%~1"
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
