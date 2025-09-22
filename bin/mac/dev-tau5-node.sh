#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/../.."
GUI_DIR="${ROOT_DIR}/gui"
BUILD_DIR="${GUI_DIR}/build"
SERVER_DIR="${ROOT_DIR}/server"

# Development build - check for standalone first (from build-node.sh)
# If not found, try the app bundle (from build-gui.sh)
if [ -f "${BUILD_DIR}/bin/tau5-node-dev" ]; then
    # Standalone tau5-node-dev from build-node.sh
    # Use --server-path flag instead of environment variable
    # Default to development mode with devtools for easier development
    "${BUILD_DIR}/bin/tau5-node-dev" --dev-server-path "${SERVER_DIR}" --devtools "$@"
elif [ -f "${BUILD_DIR}/bin/Tau5.app/Contents/MacOS/tau5-node-dev" ]; then
    # tau5-node-dev from within the app bundle (build-gui.sh)
    # Note: This is primarily for testing - normally you'd use tau5 itself
    "${BUILD_DIR}/bin/Tau5.app/Contents/MacOS/tau5-node-dev" --dev-server-path "${SERVER_DIR}" --devtools "$@"
else
    echo "Error: tau5-node-dev binary not found!"
    echo ""
    echo "Please build the development node first by running:"
    echo "  ./bin/mac/dev-build-node.sh"
    exit 1
fi