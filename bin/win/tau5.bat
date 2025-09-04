@echo off
setlocal

set SCRIPT_DIR=%~dp0
set WORKING_DIR=%CD%
set ROOT_DIR=%SCRIPT_DIR%..\..

cd /d "%ROOT_DIR%"

rem Set server path for dev mode
set TAU5_SERVER_PATH=%ROOT_DIR%\server

rem Quick development setup with all dev tools enabled
gui\build\bin\Tau5.exe --devtools

cd /d "%WORKING_DIR%"
endlocal