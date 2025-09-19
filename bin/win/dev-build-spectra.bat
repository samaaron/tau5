set WORKING_DIR=%CD%
set CONFIG=%1
cd %~dp0
if /I "%CONFIG%" == "" (set CONFIG=Release)

@echo "Building MCP DevTools server..."
cd ..\..\gui

@echo "Creating build directory..."
mkdir build > nul
cd build

cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%CONFIG% -DBUILD_MCP_SERVER=ON ..\

cmake --build . --config %CONFIG% --target tau5-spectra

cd %WORKING_DIR%