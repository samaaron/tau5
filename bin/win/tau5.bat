@echo off
setlocal

set SCRIPT_DIR=%~dp0
set WORKING_DIR=%CD%
set ROOT_DIR=%SCRIPT_DIR%..\..

cd /d "%ROOT_DIR%"

gui\build\Release\Tau5.exe dev --enable-mcp --enable-repl

cd /d "%WORKING_DIR%"
endlocal