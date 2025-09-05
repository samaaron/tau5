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

# Pass through all command-line arguments and respect environment variables
# If no arguments provided, default to --devtools for backward compatibility
if [ $# -eq 0 ]; then
    # Default behavior when no arguments provided
    exec ./gui/build/bin/tau5 --server-path "${ROOT_DIR}/server" --devtools
else
    # Pass through all arguments as provided
    exec ./gui/build/bin/tau5 --server-path "${ROOT_DIR}/server" "$@"
fi
