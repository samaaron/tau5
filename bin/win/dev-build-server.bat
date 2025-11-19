@echo off
setlocal

set WORKING_DIR=%CD%
set ORIGINAL_MIX_ENV=%MIX_ENV%

where mix >nul 2>&1
if %errorlevel% neq 0 (
  echo mix command not found. Ensure Elixir is installed and mix is in PATH.
  set EXIT_CODE=1
  goto :cleanup
)

cd %~dp0\..\..\server

:: Install hex and rebar if needed
call mix local.hex --force
if %errorlevel% neq 0 (
  echo mix local.hex failed with exit code %errorlevel%
  set EXIT_CODE=%errorlevel%
  goto :cleanup
)

call mix local.rebar --force
if %errorlevel% neq 0 (
  echo mix local.rebar failed with exit code %errorlevel%
  set EXIT_CODE=%errorlevel%
  goto :cleanup
)

:: Build production release
echo Building production release...
set MIX_ENV=prod
call mix setup
if %errorlevel% neq 0 (
  echo mix setup for prod failed with exit code %errorlevel%
  set EXIT_CODE=%errorlevel%
  goto :cleanup
)

call mix supersonic.deploy
if %errorlevel% neq 0 (
  echo mix supersonic.deploy failed with exit code %errorlevel%
  set EXIT_CODE=%errorlevel%
  goto :cleanup
)

call mix assets.deploy
if %errorlevel% neq 0 (
  echo mix assets.deploy failed with exit code %errorlevel%
  set EXIT_CODE=%errorlevel%
  goto :cleanup
)

call mix release --overwrite
if %errorlevel% neq 0 (
  echo mix release failed with exit code %errorlevel%
  set EXIT_CODE=%errorlevel%
  goto :cleanup
)

:: Setup development environment
echo Setting up development environment...
set MIX_ENV=dev
call mix setup
if %errorlevel% neq 0 (
  echo mix setup for dev failed with exit code %errorlevel%
  set EXIT_CODE=%errorlevel%
  goto :cleanup
)

:: Success
set EXIT_CODE=0

:cleanup
:: Restore environment and directory
cd %WORKING_DIR%
set MIX_ENV=%ORIGINAL_MIX_ENV%

if %EXIT_CODE% equ 0 (
  echo ---
  echo     Tau5 server built successfully.
  echo ---
) else (
  echo ---
  echo     Build failed with exit code %EXIT_CODE%
  echo     Try running the clean.bat script and then building again.
  echo ---
)

endlocal
exit /b %EXIT_CODE%