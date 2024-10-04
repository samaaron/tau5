set WORKING_DIR=%CD%
set CONFIG=%1
cd %~dp0

call build-server.bat
call build-app.bat

cd %WORKING_DIR%