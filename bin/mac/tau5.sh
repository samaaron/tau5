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

# Enable MCP mode for development
export TAU5_ENABLE_DEV_MCP=1
# Enable Elixir REPL console for development
export TAU5_ENABLE_DEV_REPL=1
./gui/build/Tau5.app/Contents/MacOS/Tau5 dev