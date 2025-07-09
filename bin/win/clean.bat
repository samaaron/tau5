@echo off
setlocal enabledelayedexpansion

:: Jump to main script, skipping function definitions
goto :main

:: ========== FUNCTIONS ==========

:cleanDir
:: Function to clean a directory
:: Parameter: %~1 = directory path
if exist "%~1" (
    echo   - Removing %~1
    rmdir "%~1" /s /q
) else (
    echo   - %~1 [not found]
)
goto :eof

:cleanAssets
:: Function to clean assets directory (files and subdirectories)
:: Parameter: %~1 = assets directory path
if exist "%~1" (
    echo   - Cleaning %~1
    :: Delete all files
    del "%~1\*.*" /s /q 2>nul
    :: Delete all subdirectories
    for /d %%d in ("%~1\*") do (
        echo     - Removing directory %%~nxd
        rmdir "%%d" /s /q
    )
) else (
    echo   - %~1 [not found]
)
goto :eof

:: ========== MAIN SCRIPT ==========

:main
set WORKING_DIR=%CD%
cd %~dp0
cd ..\..

echo Starting cleanup process...
echo.

:: Server directory cleanup
cd server 2>nul
if !errorlevel! equ 0 (
    echo Cleaning server directory...

    :: Clean dependencies
    call :cleanDir "deps\sp_link\build"
    call :cleanDir "deps\sp_link\deps\sp_link"
    call :cleanDir "deps\tau5_discovery\build"
    call :cleanDir "deps\exqlite\build"

    :: Clean and recreate priv\nifs
    call :cleanDir "priv\nifs"
    echo   - Creating priv\nifs
    mkdir priv\nifs 2>nul

    :: Clean build directories
    call :cleanDir "_build"
    call :cleanDir "deps"

    :: Clean static assets
    call :cleanAssets "priv\static\assets"

    cd ..
) else (
    echo Server directory not found, skipping server cleanup...
)

echo.
echo Cleaning root directories...

:: Clean root level directories
call :cleanDir "gui\build"
call :cleanDir "Release"
call :cleanDir "build"

cd %WORKING_DIR%

echo.
echo Cleanup process completed successfully.
endlocal