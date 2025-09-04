@echo off
setlocal enabledelayedexpansion

set WORKING_DIR=%CD%
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..\..

echo ================================================
echo Tau5 Windows Release Build
echo ================================================

:: Clean and prepare directories
cd /d "%ROOT_DIR%"
echo Cleaning previous builds...
if exist release rmdir /s /q release
if exist gui\build-release rmdir /s /q gui\build-release
if exist server\_build\prod rmdir /s /q server\_build\prod

:: Build server in production mode
echo.
echo Building Elixir server (production release)...
cd /d "%ROOT_DIR%\server"

:: Setup will get deps, build assets, and compile NIFs
set MIX_ENV=prod
call mix setup
if %errorlevel% neq 0 (
    echo mix setup failed with exit code %errorlevel%
    cd /d "%WORKING_DIR%"
    exit /b %errorlevel%
)

:: Deploy assets for production (minified)
call mix assets.deploy
if %errorlevel% neq 0 (
    echo mix assets.deploy failed with exit code %errorlevel%
    cd /d "%WORKING_DIR%"
    exit /b %errorlevel%
)

:: Create release
call mix release --overwrite
if %errorlevel% neq 0 (
    echo mix release failed with exit code %errorlevel%
    cd /d "%WORKING_DIR%"
    exit /b %errorlevel%
)

:: Build GUI with release configuration
echo.
echo Building GUI components with release paths...
cd /d "%ROOT_DIR%\gui"
if not exist build-release mkdir build-release
cd build-release

:: Configure CMake for release build with proper server path
:: For Windows release: binaries in root, server in _build\prod\rel\tau5
cmake -G "Visual Studio 17 2022" ^
      -A x64 ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DTAU5_RELEASE_BUILD=ON ^
      -DTAU5_INSTALL_SERVER_PATH="_build\prod\rel\tau5" ^
      -DBUILD_DEBUG_PANE=OFF ^
      ..
if %errorlevel% neq 0 (
    echo CMake configuration failed with exit code %errorlevel%
    cd /d "%WORKING_DIR%"
    exit /b %errorlevel%
)

:: Build the project
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo CMake build failed with exit code %errorlevel%
    cd /d "%WORKING_DIR%"
    exit /b %errorlevel%
)

:: Create release package structure
echo.
echo Creating release package...
cd /d "%ROOT_DIR%"
if not exist release mkdir release

:: Copy GUI binaries
xcopy /E /I /Y gui\build-release\bin\Release\* release\
if %errorlevel% neq 0 (
    echo Failed to copy GUI binaries
    cd /d "%WORKING_DIR%"
    exit /b %errorlevel%
)

:: Copy server release
xcopy /E /I /Y server\_build\prod\rel\tau5 release\_build\prod\rel\tau5\
if %errorlevel% neq 0 (
    echo Failed to copy server release
    cd /d "%WORKING_DIR%"
    exit /b %errorlevel%
)

:: Deploy Qt dependencies
cd /d "%ROOT_DIR%\release"
%QT_INSTALL_LOCATION%\bin\windeployqt.exe tau5.exe
if %errorlevel% neq 0 (
    echo windeployqt failed
    cd /d "%WORKING_DIR%"
    exit /b %errorlevel%
)

echo.
echo ================================================
echo Release build completed successfully!
echo Location: %ROOT_DIR%\release
echo ================================================

cd /d "%WORKING_DIR%"
exit /b 0