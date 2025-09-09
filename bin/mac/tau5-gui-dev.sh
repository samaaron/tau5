#!/bin/bash
set -e # Quit script on error
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
SERVER_DIR="${ROOT_DIR}/server"

cleanup_function() {
    # Restore working directory as it was prior to this script running on exit
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

cd "${ROOT_DIR}"

# Check if the binary exists
BINARY_PATH="./gui/build/bin/Tau5.app/Contents/MacOS/Tau5"
if [ ! -f "${BINARY_PATH}" ]; then
    echo "Error: Tau5 binary not found at ${BINARY_PATH}"
    echo ""
    echo "Please build the development GUI first by running:"
    echo "  ./bin/mac/dev-build-gui.sh"
    exit 1
fi

# Quick development setup with all dev tools enabled
# Use --server-path flag instead of environment variable
"${BINARY_PATH}" --server-path "${SERVER_DIR}" --devtools