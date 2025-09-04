@echo off
set WORKING_DIR=%CD%
setlocal

cd /d %~dp0

set CONFIG=Release
set BUILD_DEBUG_PANE=ON

:parse_args
if "%1"=="" goto end_parse
if /I "%1"=="Debug" (set CONFIG=Debug& shift& goto parse_args)
if /I "%1"=="Release" (set CONFIG=Release& shift& goto parse_args)
if /I "%1"=="--no-debug-pane" (set BUILD_DEBUG_PANE=OFF& shift& goto parse_args)
shift
goto parse_args
:end_parse

@echo "Generating project files..."
if "%BUILD_DEBUG_PANE%"=="OFF" (
    @echo "Building without debug pane..."
)

cd ..\..\gui

@echo "Creating build directory..."
if not exist build mkdir build
cd build

cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%CONFIG% -DBUILD_DEBUG_PANE=%BUILD_DEBUG_PANE% -DBUILD_NODE_ONLY=OFF ..\

cmake --build . --config %CONFIG% --target tau5

cd /d %WORKING_DIR%

endlocal