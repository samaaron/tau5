@echo off
set WORKING_DIR=%CD%
set CONFIG=%1

cd %~dp0

:: Call build-server.bat
call build-server.bat
if %errorlevel% neq 0 (
    echo build-server.bat failed with exit code %errorlevel%
    exit /b %errorlevel%
)

:: Call build-app.bat
call build-app.bat
if %errorlevel% neq 0 (
    echo build-app.bat failed with exit code %errorlevel%
    exit /b %errorlevel%
)

cd %WORKING_DIR%
