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
"${SCRIPT_DIR}"/build-app.sh
"${SCRIPT_DIR}"/build-release.sh

cd "${ROOT_DIR}"/Release
