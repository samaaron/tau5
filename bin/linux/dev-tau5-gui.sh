#!/bin/bash
set -e # Quit script on error
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."

cleanup_function() {
    # Restore working directory as it was prior to this script running on exit
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

cd "${ROOT_DIR}"

# Check if the binary exists
BINARY_PATH="./gui/build/bin/tau5-gui-dev"
if [ ! -f "${BINARY_PATH}" ]; then
    echo "Error: tau5-gui-dev binary not found at ${BINARY_PATH}"
    echo ""
    echo "Please build the development GUI first by running:"
    echo "  ./bin/linux/dev-build-gui.sh"
fi

# Pass through all command-line arguments and respect environment variables
# If no arguments provided, default to --devtools for backward compatibility
if [ $# -eq 0 ]; then
    # Default behavior when no arguments provided
    exec "${BINARY_PATH}" --server-path "${ROOT_DIR}/server" --devtools
else
    # Pass through all arguments as provided
    exec "${BINARY_PATH}" --server-path "${ROOT_DIR}/server" "$@"
fi
