@echo off
REM Thin wrapper to call build-release.bat with --node-only flag

set SCRIPT_DIR=%~dp0
call "%SCRIPT_DIR%build-release.bat" --node-only %*