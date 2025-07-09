set WORKING_DIR=%CD%
set CONFIG=%1
cd %~dp0
if /I "%CONFIG%" == "" (set CONFIG=Release)

@echo "Generating project files..."
cd ..\..\app

@echo "Creating build directory..."
mkdir build > nul
cd build

cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%CONFIG% ..\

cmake --build . --config %CONFIG%

cd %WORKING_DIR%