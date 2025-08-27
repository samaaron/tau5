@echo off
setlocal

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..\..
set GUI_DIR=%ROOT_DIR%\gui
set BUILD_DIR=%GUI_DIR%\build
set SERVER_DIR=%ROOT_DIR%\server

REM Set server path explicitly for development
set TAU5_SERVER_PATH=%SERVER_DIR%

REM Check which configuration exists
if exist "%BUILD_DIR%\Release\tau5-node.exe" (
    "%BUILD_DIR%\Release\tau5-node.exe" %*
) else if exist "%BUILD_DIR%\Debug\tau5-node.exe" (
    "%BUILD_DIR%\Debug\tau5-node.exe" %*
) else (
    echo Error: tau5-node.exe not found. Please build it first using build-node.bat
    exit /b 1
)