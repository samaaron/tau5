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

echo "Building tau5-node CLI for macOS..."

# Check for architecture
# BUILD_NODE_ONLY=ON ensures tau5-node is built as standalone binary in build/bin/
if [[ $(uname -m) == 'arm64' ]]
then
  echo "Building for Apple Silicon (arm64)..."
  cmake -G "Unix Makefiles" -DCMAKE_OSX_ARCHITECTURES="arm64" -DCMAKE_BUILD_TYPE="$CONFIG" -DBUILD_NODE_ONLY=ON ..
else
  echo "Building for Intel (x86_64)..."
  cmake -G "Unix Makefiles" -DCMAKE_OSX_ARCHITECTURES="x86_64" -DCMAKE_BUILD_TYPE="$CONFIG" -DBUILD_NODE_ONLY=ON ..
fi

cmake --build . --target tau5-node

echo "tau5-node build complete"