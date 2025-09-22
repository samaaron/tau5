@echo off
setlocal

set SCRIPT_DIR=%~dp0
set WORKING_DIR=%CD%
set ROOT_DIR=%SCRIPT_DIR%..\..
set GUI_DIR=%ROOT_DIR%\gui
set BUILD_DIR=%GUI_DIR%\build
set SERVER_DIR=%ROOT_DIR%\server

cd /d "%ROOT_DIR%"

rem Check if executable exists
if not exist "%BUILD_DIR%\bin\tau5-node-dev.exe" (
    echo Error: tau5-node-dev binary not found at %BUILD_DIR%\bin\tau5-node-dev.exe
    echo.
    echo Please build the development node first by running:
    echo   .\bin\win\dev-build-node.bat
    echo.
    echo This will compile the headless node component with development tools enabled.
    cd /d "%WORKING_DIR%"
    exit /b 1
)

rem Default to full development mode with all tools enabled
rem Use --server-path flag for development
"%BUILD_DIR%\bin\tau5-node-dev.exe" --dev-server-path "%SERVER_DIR%" --devtools %*

cd /d "%WORKING_DIR%"
endlocal