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
if not exist "%BUILD_DIR%\bin\tau5-gui-dev.exe" (
    echo Error: tau5-gui-dev binary not found at %BUILD_DIR%\bin\tau5-gui-dev.exe
    echo.
    echo Please build the development GUI first by running:
    echo   .\bin\win\dev-build-gui.bat
    echo.
    echo This will compile the Qt browser component with development tools enabled.
    cd /d "%WORKING_DIR%"
    exit /b 1
)

rem Pass through all command-line arguments and respect environment variables
rem If no arguments provided, default to --devtools for backward compatibility
if "%~1"=="" (
    rem Default behavior when no arguments provided
    "%BUILD_DIR%\bin\tau5-gui-dev.exe" --dev-server-path "%SERVER_DIR%" --devtools
) else (
    rem Pass through all arguments as provided
    "%BUILD_DIR%\bin\tau5-gui-dev.exe" --dev-server-path "%SERVER_DIR%" %*
)

cd /d "%WORKING_DIR%"
endlocal