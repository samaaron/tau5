@echo off

set WORKING_DIR=%CD%
set ORIGINAL_MIX_ENV=%MIX_ENV%

where mix >nul 2>&1
if %errorlevel% neq 0 (
  echo mix command not found. Ensure Elixir is installed and mix is in PATH.
  goto :abort 1
)

cd %~dp0\..\..\server

:: Set MIX_ENV to prod
SET MIX_ENV=prod

:: Run mix local.hex
cmd /c mix local.hex --force
if %errorlevel% neq 0 (
  echo mix local.hex failed with exit code %errorlevel%
  goto :abort %errorlevel%
)

:: Run mix local.rebar
cmd /c mix local.rebar --force
if %errorlevel% neq 0 (
  echo mix local.rebar failed with exit code %errorlevel%
  goto :abort %errorlevel%
)


:: Run mix setup
cmd /c mix setup
if %errorlevel% neq 0 (
  echo mix setup failed with exit code %errorlevel%
  goto :abort %errorlevel%
)

:: Run mix assets.deploy
cmd /c mix assets.deploy
if %errorlevel% neq 0 (
  echo mix assets.deploy failed with exit code %errorlevel%
  goto :abort %errorlevel%
)

:: Run mix release
cmd /c mix release --overwrite --no-deps-check
if %errorlevel% neq 0 (
  echo mix release failed with exit code %errorlevel%
  goto :abort %errorlevel%
)

:: Set MIX_ENV back to dev
SET MIX_ENV=dev

:: Run mix deps.get
cmd /c mix deps.get
if %errorlevel% neq 0 (
  echo mix deps.get failed with exit code %errorlevel%
  goto :abort %errorlevel%
)

:: Run mix compile
cmd /c mix compile
if %errorlevel% neq 0 (
  echo mix compile failed with exit code %errorlevel%
  goto :abort %errorlevel%
)

cd %WORKING_DIR%
set MIX_ENV=%ORIGINAL_MIX_ENV%
echo ---
echo     Tau5 server for Windows built successfully.
echo ---
exit /b 0

:abort
:: Change back to the original directory, reset environment variables
:: and exit with the error code.
cd %WORKING_DIR%
set MIX_ENV=%ORIGINAL_MIX_ENV%
echo ---
echo     Try running the clean.bat script and then building again.
echo ---
exit /b 1