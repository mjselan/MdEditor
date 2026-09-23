@echo off
rem ============================================================================
rem deps-dicts.bat - download en_US hunspell dictionaries into the Windows
rem prefix layout (build\prefix\bin\data\hunspell) expected by windeploy.bat.
rem Dictionaries come from the LibreOffice dictionaries collection.
rem No compiler needed; requires curl.exe (bundled with Windows 10+).
rem
rem Usage:  tools\windows\deps-dicts.bat
rem ============================================================================
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "DICT_DIR=%REPO_ROOT%\build\prefix\bin\data\hunspell"
set "DICT_BASE=https://raw.githubusercontent.com/LibreOffice/dictionaries/master/en"

where curl >nul 2>nul
if errorlevel 1 (
    echo ERROR: curl not found on PATH.
    exit /b 1
)

if not exist "%DICT_DIR%" mkdir "%DICT_DIR%"
for %%F in (en_US.dic en_US.aff) do (
    echo Downloading %%F ...
    curl -LfsS -o "%DICT_DIR%\%%F" "%DICT_BASE%/%%F"
    if errorlevel 1 goto :fail
)

echo Dictionaries installed to %DICT_DIR%.
exit /b 0

:fail
echo.
echo FAILED (exit %errorlevel%).
exit /b 1
