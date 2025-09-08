@echo off
setlocal

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..\..
set GUI_DIR=%ROOT_DIR%\gui
set BUILD_DIR=%GUI_DIR%\build
set CONFIG=Release

REM Parse arguments
:parse_args
if "%1"=="" goto end_parse
if /i "%1"=="debug" (
    set CONFIG=Debug
    echo Building tau5-node in Debug mode...
)
if /i "%1"=="release" (
    set CONFIG=Release
    echo Building tau5-node in Release mode...
)
shift
goto parse_args
:end_parse

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo Building tau5-node CLI...

REM Configure with CMake
cmake -G "Visual Studio 17 2022" ..
if errorlevel 1 (
    cmake -G "Visual Studio 16 2019" ..
    if errorlevel 1 (
        echo Error: Failed to configure with CMake
        exit /b 1
    )
)

REM Build tau5-node-dev target
cmake --build . --config %CONFIG% --target tau5-node-dev
if errorlevel 1 (
    echo Error: Failed to build tau5-node-dev
    exit /b 1
)

echo tau5-node-dev build complete