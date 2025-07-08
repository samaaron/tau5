@echo off
set BUILD_WORKING_DIR=%CD%
set CONFIG=%1

cd %~dp0

:: Call build-server.bat
call build-server.bat
if %errorlevel% neq 0 (
    echo build-server.bat failed with exit code %errorlevel%
    cd %BUILD_WORKING_DIR%
    exit /b %errorlevel%
)

:: Call build-app.bat
call build-app.bat
if %errorlevel% neq 0 (
    echo build-app.bat failed with exit code %errorlevel%
    cd %BUILD_WORKING_DIR%
    exit /b %errorlevel%
)

@echo "Completed Building Tau5"
cd %BUILD_WORKING_DIR%
