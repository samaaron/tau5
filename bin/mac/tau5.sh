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

# Set server path explicitly for development
export TAU5_SERVER_PATH="${SERVER_DIR}"
./gui/build/Tau5.app/Contents/MacOS/Tau5 dev --enable-mcp --enable-repl --public-endpoint