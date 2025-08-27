#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/../.."
GUI_DIR="${ROOT_DIR}/gui"
BUILD_DIR="${GUI_DIR}/build"
SERVER_DIR="${ROOT_DIR}/server"

# Check if we have a Release build with tau5-node in the app bundle
if [ -f "${ROOT_DIR}/Release/Tau5.app/Contents/MacOS/tau5-node" ]; then
    # Release build - server is bundled in Resources
    export TAU5_SERVER_PATH="${ROOT_DIR}/Release/Tau5.app/Contents/Resources"
    "${ROOT_DIR}/Release/Tau5.app/Contents/MacOS/tau5-node" "$@"
elif [ -f "${BUILD_DIR}/tau5-node" ]; then
    # Development build - server is in the server directory
    export TAU5_SERVER_PATH="${SERVER_DIR}"
    "${BUILD_DIR}/tau5-node" "$@"
else
    echo "Error: tau5-node executable not found!"
    echo "Please build tau5-node first using: ${SCRIPT_DIR}/build-node.sh"
    exit 1
fi