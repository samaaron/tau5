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
if not exist "%BUILD_DIR%\bin\tau5.exe" (
    echo Error: tau5.exe not found. Please build it first using dev-build-gui.bat
    cd /d "%WORKING_DIR%"
    exit /b 1
)

rem Pass through all command-line arguments and respect environment variables
rem If no arguments provided, default to --devtools for backward compatibility
if "%~1"=="" (
    rem Default behavior when no arguments provided
    "%BUILD_DIR%\bin\tau5.exe" --server-path "%SERVER_DIR%" --devtools
) else (
    rem Pass through all arguments as provided
    "%BUILD_DIR%\bin\tau5.exe" --server-path "%SERVER_DIR%" %*
)

cd /d "%WORKING_DIR%"
endlocal