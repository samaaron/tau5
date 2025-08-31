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

# Set server path for dev mode
export TAU5_SERVER_PATH="${ROOT_DIR}/server"

./gui/build/tau5 dev --enable-mcp --enable-repl --public-endpoint
