#!/bin/bash
WORKING_DIR=$(pwd)
CONFIG=${1:-Release}
cd "$(dirname "$0")"

echo "Building MCP DevTools server..."
cd ../../gui

echo "Creating build directory..."
mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=$CONFIG -DBUILD_MCP_SERVER=ON ../

cmake --build . --config $CONFIG --target tau5-gui-dev-mcp

cd "$WORKING_DIR"