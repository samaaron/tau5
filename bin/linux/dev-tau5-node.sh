#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/../.."
GUI_DIR="${ROOT_DIR}/gui"
BUILD_DIR="${GUI_DIR}/build"
SERVER_DIR="${ROOT_DIR}/server"

# Check if the binary exists
BINARY_PATH="${BUILD_DIR}/bin/tau5-node-dev"
if [ ! -f "${BINARY_PATH}" ]; then
    echo "Error: tau5-node-dev binary not found at ${BINARY_PATH}"
    echo ""
    echo "Please build the development node first by running:"
    echo "  ./bin/linux/dev-build-node.sh"
    exit 1
fi

# Pass through all command-line arguments and respect environment variables
# If no arguments provided, default to --devtools for backward compatibility
if [ $# -eq 0 ]; then
    "${BINARY_PATH}" --dev-server-path "${SERVER_DIR}" --devtools
else
    "${BINARY_PATH}" --dev-server-path "${SERVER_DIR}" "$@"
fi