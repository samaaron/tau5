@echo off
setlocal

set SCRIPT_DIR=%~dp0
set WORKING_DIR=%CD%
set ROOT_DIR=%SCRIPT_DIR%..\..

cd /d "%ROOT_DIR%"
gui\build\Release\tau5-devtools-mcp.exe %*

cd /d "%WORKING_DIR%"
endlocal