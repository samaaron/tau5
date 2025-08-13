@echo off
setlocal

set SCRIPT_DIR=%~dp0
set WORKING_DIR=%CD%
set ROOT_DIR=%SCRIPT_DIR%..\..

cd /d "%ROOT_DIR%"

REM Enable MCP mode for development
set TAU5_ENABLE_DEV_MCP=1
gui\build\Release\Tau5.exe dev

cd /d "%WORKING_DIR%"
endlocal