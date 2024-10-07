@echo off
set WORKING_DIR=%CD%

cd %~dp0\..\..\server

:: Set MIX_ENV to prod
SET MIX_ENV=prod

:: Run mix local.hex
cmd /c mix local.hex --force
if %errorlevel% neq 0 exit /b %errorlevel%

:: Run mix local.rebar
cmd /c mix local.rebar --force
if %errorlevel% neq 0 exit /b %errorlevel%

:: Run mix setup
cmd /c mix setup
if %errorlevel% neq 0 exit /b %errorlevel%

:: Run mix assets.deploy
cmd /c mix assets.deploy
if %errorlevel% neq 0 exit /b %errorlevel%

:: Run mix release
cmd /c mix release --overwrite --no-deps-check
if %errorlevel% neq 0 exit /b %errorlevel%

:: Set MIX_ENV back to dev
SET MIX_ENV=dev

:: Run mix deps.get
cmd /c mix deps.get
if %errorlevel% neq 0 exit /b %errorlevel%

:: Run mix compile
cmd /c mix compile
if %errorlevel% neq 0 exit /b %errorlevel%

cd %WORKING_DIR%
