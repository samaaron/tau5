#!/bin/bash
WORKING_DIR=$(pwd)
# Default to Release, but allow Debug as first argument
CONFIG=Release
if [ "$1" = "Debug" ] || [ "$1" = "debug" ]; then
    CONFIG=Debug
fi
cd "$(dirname "$0")"

echo "Building MCP DevTools server..."
cd ../../gui

echo "Creating build directory..."
mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=$CONFIG -DBUILD_MCP_SERVER=ON ../

cmake --build . --config $CONFIG --target tau5-dev-gui-mcp-server

cd "$WORKING_DIR"