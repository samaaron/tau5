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

APP_DIR="${ROOT_DIR}/app"
BUILD_DIR="${APP_DIR}/build"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

config="Debug"

if [[ $(uname -m) == 'arm64' ]] || [ "$TAU5_BUILD_TARGET" == 'arm64' ]
then
  cmake -G "Unix Makefiles" -DCMAKE_OSC_ARCHITECTURES="ARM64" -DCMAKE_BUILD_TYPE="$config" ..
else
  cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="$config" ..
fi

cmake --build .