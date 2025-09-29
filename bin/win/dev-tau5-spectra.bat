@echo off
setlocal

set SCRIPT_DIR=%~dp0
set WORKING_DIR=%CD%
set ROOT_DIR=%SCRIPT_DIR%..\..

cd /d "%ROOT_DIR%"

REM Check for --debug flag in arguments
set HAS_DEBUG=0
for %%a in (%*) do (
    if "%%a"=="--debug" set HAS_DEBUG=1
)

if %HAS_DEBUG%==1 (
    echo Starting Tau5 Spectra in DEBUG mode...
    echo Log file will be created in: %ROOT_DIR%\tau5-mcp-debug.log
    echo.
)

gui\build\bin\tau5-spectra.exe %*

cd /d "%WORKING_DIR%"
endlocal