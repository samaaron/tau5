@echo off
setlocal

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..\..
set GUI_DIR=%ROOT_DIR%\gui
set BUILD_DIR=%GUI_DIR%\build

REM Check which configuration exists
if exist "%BUILD_DIR%\Release\tau5-node.exe" (
    "%BUILD_DIR%\Release\tau5-node.exe" %*
) else if exist "%BUILD_DIR%\Debug\tau5-node.exe" (
    "%BUILD_DIR%\Debug\tau5-node.exe" %*
) else (
    echo Error: tau5-node.exe not found. Please build it first using build-cli.bat
    exit /b 1
)