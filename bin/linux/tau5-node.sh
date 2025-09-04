#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/../.."
GUI_DIR="${ROOT_DIR}/gui"
BUILD_DIR="${GUI_DIR}/build"
SERVER_DIR="${ROOT_DIR}/server"

# Use --server-path flag for development
# The binary should be in bin/ on Linux
"${BUILD_DIR}/bin/tau5-node" --server-path "${SERVER_DIR}" "$@"