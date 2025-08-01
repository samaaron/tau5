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

"${SCRIPT_DIR}"/build-server.sh
"${SCRIPT_DIR}"/build-gui.sh
"${SCRIPT_DIR}"/build-mcp.sh
"${SCRIPT_DIR}"/build-release.sh


