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

# Default to full development mode with all tools enabled
# Use --server-path flag for development
# The binary should be in bin/ on Linux
"${BINARY_PATH}" --server-path "${SERVER_DIR}" --devtools "$@"