#!/bin/bash
set -e # Quit script on error
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
CONFIG=Release

# Parse arguments
for arg in "$@"; do
    case $arg in
        Debug|debug)
            CONFIG=Debug
            echo "Building tau5-node in Debug mode..."
            ;;
        Release|release)
            CONFIG=Release
            echo "Building tau5-node in Release mode..."
            ;;
    esac
done

cleanup_function() {
    # Restore working directory as it was prior to this script running on exit
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

GUI_DIR="${ROOT_DIR}/gui"
BUILD_DIR="${GUI_DIR}/build"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "Building tau5-node CLI..."

if [[ $(uname -m) == 'arm64' ]] || [[ $(uname -m) == 'aarch64' ]] || [ "$TAU5_BUILD_TARGET" == 'arm64' ]
then
  echo "Detected ARM architecture"
  cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DBUILD_NODE_ONLY=ON \
    -DBUILD_DEBUG_PANE=OFF \
    -DBUILD_MCP_SERVER=OFF \
    ..
else
  cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DBUILD_NODE_ONLY=ON \
    -DBUILD_DEBUG_PANE=OFF \
    -DBUILD_MCP_SERVER=OFF \
    ..
fi

cmake --build . --target tau5-node-dev

echo "tau5-node-dev build complete"