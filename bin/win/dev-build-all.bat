@echo off
set BUILD_WORKING_DIR=%CD%
set CONFIG=%1

cd %~dp0

:: Call dev-build-server.bat
call dev-build-server.bat
if %errorlevel% neq 0 (
    echo dev-build-server.bat failed with exit code %errorlevel%
    cd %BUILD_WORKING_DIR%
    exit /b %errorlevel%
)

:: Call dev-build-gui.bat
call dev-build-gui.bat
if %errorlevel% neq 0 (
    echo dev-build-gui.bat failed with exit code %errorlevel%
    cd %BUILD_WORKING_DIR%
    exit /b %errorlevel%
)

:: Call dev-build-node.bat
call dev-build-node.bat
if %errorlevel% neq 0 (
    echo dev-build-node.bat failed with exit code %errorlevel%
    cd %BUILD_WORKING_DIR%
    exit /b %errorlevel%
)

:: Build Spectra (MCP server for Chrome DevTools)
call dev-build-spectra.bat
if %errorlevel% neq 0 (
    echo dev-build-spectra.bat failed with exit code %errorlevel%
    cd %BUILD_WORKING_DIR%
    exit /b %errorlevel%
)

@echo "Completed Building Tau5"
cd %BUILD_WORKING_DIR%
