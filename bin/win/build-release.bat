@echo off
setlocal enabledelayedexpansion

set WORKING_DIR=%CD%
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..\..
set NODE_ONLY=false

:: Parse arguments
:parse_args
if "%1"=="" goto end_parse
if /i "%1"=="--node-only" (
    set NODE_ONLY=true
)
shift
goto parse_args
:end_parse

echo ================================================
if "%NODE_ONLY%"=="true" (
    echo Tau5 Windows Release Build ^(tau5-node only^)
    echo For headless systems without Qt GUI dependencies
) else (
    echo Tau5 Windows Release Build
)
echo ================================================

:: Clean and prepare directories
cd /d "%ROOT_DIR%"
echo Cleaning previous builds...
if "%NODE_ONLY%"=="true" (
    if exist release-node rmdir /s /q release-node
    if exist gui\build-release-node rmdir /s /q gui\build-release-node
) else (
    if exist release rmdir /s /q release
    if exist gui\build-release rmdir /s /q gui\build-release
)
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
if "%NODE_ONLY%"=="true" (
    echo Building tau5-node with release paths...
    cd /d "%ROOT_DIR%\gui"
    if not exist build-release-node mkdir build-release-node
    cd build-release-node
    
    :: Configure CMake for node-only release build
    cmake -G "Visual Studio 17 2022" ^
          -A x64 ^
          -DCMAKE_BUILD_TYPE=Release ^
          -DTAU5_RELEASE_BUILD=ON ^
          -DTAU5_INSTALL_SERVER_PATH="_build/prod/rel/tau5" ^
          -DBUILD_NODE_ONLY=ON ^
          -DBUILD_DEBUG_PANE=OFF ^
          -DBUILD_MCP_SERVER=OFF ^
          ..
    if %errorlevel% neq 0 (
        :: Try with Visual Studio 2019 if 2022 is not available
        cmake -G "Visual Studio 16 2019" ^
              -A x64 ^
              -DCMAKE_BUILD_TYPE=Release ^
              -DTAU5_RELEASE_BUILD=ON ^
              -DTAU5_INSTALL_SERVER_PATH="_build/prod/rel/tau5" ^
              -DBUILD_NODE_ONLY=ON ^
              -DBUILD_DEBUG_PANE=OFF ^
              -DBUILD_MCP_SERVER=OFF ^
              ..
        if %errorlevel% neq 0 (
            echo CMake configuration failed with exit code %errorlevel%
            cd /d "%WORKING_DIR%"
            exit /b %errorlevel%
        )
    )
    
    :: Build tau5-node only
    cmake --build . --config Release --target tau5-node
    if %errorlevel% neq 0 (
        echo CMake build failed with exit code %errorlevel%
        cd /d "%WORKING_DIR%"
        exit /b %errorlevel%
    )
) else (
    echo Building GUI components with release paths...
    cd /d "%ROOT_DIR%\gui"
    if not exist build-release mkdir build-release
    cd build-release
    
    :: Configure CMake for release build with proper server path
    :: For Windows release: binaries in root, server in _build/prod/rel/tau5
    cmake -G "Visual Studio 17 2022" ^
          -A x64 ^
          -DCMAKE_BUILD_TYPE=Release ^
          -DTAU5_RELEASE_BUILD=ON ^
          -DTAU5_INSTALL_SERVER_PATH="_build/prod/rel/tau5" ^
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
)

:: Create release package structure
echo.
echo Creating release package...
cd /d "%ROOT_DIR%"

if "%NODE_ONLY%"=="true" (
    if not exist release-node mkdir release-node
    cd release-node
    if not exist Tau5-Node-Windows mkdir Tau5-Node-Windows
    cd Tau5-Node-Windows
    
    :: Copy tau5-node binary (it should be in bin\Release for CMake builds)
    echo Copying tau5-node binary...
    copy "%ROOT_DIR%\gui\build-release-node\bin\Release\tau5-node.exe" . /Y
    if %errorlevel% neq 0 (
        :: Try without Release subdirectory
        copy "%ROOT_DIR%\gui\build-release-node\bin\tau5-node.exe" . /Y
        if %errorlevel% neq 0 (
            echo Failed to copy tau5-node.exe
            cd /d "%WORKING_DIR%"
            exit /b %errorlevel%
        )
    )
    
    :: Copy production server release
    echo Copying server release...
    if not exist _build\prod\rel mkdir _build\prod\rel
    if not exist "%ROOT_DIR%\server\_build\prod\rel\tau5" (
        echo ERROR: Server release not found at %ROOT_DIR%\server\_build\prod\rel\tau5
        echo Make sure the Elixir release build succeeded
        cd /d "%WORKING_DIR%"
        exit /b 1
    )
    xcopy /E /I /Y "%ROOT_DIR%\server\_build\prod\rel\tau5" "_build\prod\rel\tau5\"
    if %errorlevel% neq 0 (
        echo Failed to copy server release
        cd /d "%WORKING_DIR%"
        exit /b %errorlevel%
    )
) else (
    if not exist release mkdir release
    
    :: Copy GUI binaries (they're directly in bin\ for CMake builds)
    xcopy /E /I /Y gui\build-release\bin\* release\
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
    
    :: Deploy Qt dependencies if windeployqt is available
    cd /d "%ROOT_DIR%\release"
    if exist "%QT_INSTALL_LOCATION%\bin\windeployqt.exe" (
        "%QT_INSTALL_LOCATION%\bin\windeployqt.exe" tau5.exe
    ) else if exist "%Qt6_DIR%\bin\windeployqt.exe" (
        "%Qt6_DIR%\bin\windeployqt.exe" tau5.exe
    ) else (
        echo WARNING: windeployqt not found, Qt dependencies may not be deployed
    )
    if %errorlevel% neq 0 (
        echo windeployqt failed
        cd /d "%WORKING_DIR%"
        exit /b %errorlevel%
    )
)

echo.
echo ================================================
if "%NODE_ONLY%"=="true" (
    echo tau5-node Windows release build completed successfully!
    echo ================================================
    echo Release package: %ROOT_DIR%\release-node\Tau5-Node-Windows\
    echo.
    echo The release directory is self-contained and ready for distribution.
    echo This build is suitable for headless systems without Qt dependencies.
    echo Users can run Tau5 with:
    echo   tau5-node.exe
) else (
    echo Release build completed successfully!
    echo Location: %ROOT_DIR%\release
    echo ================================================
)

cd /d "%WORKING_DIR%"
exit /b 0