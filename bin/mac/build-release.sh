#!/bin/bash
set -e # Quit script on error
set -x # Echo commands
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."

cleanup_function() {
    # Restore working directory as it was prior to this script running on exit
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

cd "${ROOT_DIR}"
rm -rf Release
mkdir -p Release
cd Release
cp -R ../app/build/Tau5.app .
cd Tau5.app/Contents/Resources
cp -R "${ROOT_DIR}/server/_build" .