#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/../.."
GUI_DIR="${ROOT_DIR}/gui"
BUILD_DIR="${GUI_DIR}/build"
SERVER_DIR="${ROOT_DIR}/server"

# Development build - check in app bundle first, then standalone in bin directory
if [ -f "${BUILD_DIR}/bin/Tau5.app/Contents/MacOS/tau5-node" ]; then
    export TAU5_SERVER_PATH="${SERVER_DIR}"
    "${BUILD_DIR}/bin/Tau5.app/Contents/MacOS/tau5-node" "$@"
elif [ -f "${BUILD_DIR}/bin/tau5-node" ]; then
    # Standalone location for build-node.sh builds
    export TAU5_SERVER_PATH="${SERVER_DIR}"
    "${BUILD_DIR}/bin/tau5-node" "$@"
else
    echo "Error: tau5-node executable not found!"
    echo "Please build tau5-node first using: ${SCRIPT_DIR}/build-gui.sh or ${SCRIPT_DIR}/build-node.sh"
    exit 1
fi