#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/../.."
GUI_DIR="${ROOT_DIR}/gui"
BUILD_DIR="${GUI_DIR}/build"
SERVER_DIR="${ROOT_DIR}/server"

# Set server path explicitly for development
export TAU5_SERVER_PATH="${SERVER_DIR}"

# Pass all arguments to tau5-node
"${BUILD_DIR}/tau5-node" "$@"