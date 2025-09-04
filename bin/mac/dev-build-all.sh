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

echo "================================================"
echo "Building all Tau5 components (DEVELOPMENT MODE)"
echo "================================================"

# Build all development components
"${SCRIPT_DIR}"/dev-build-server.sh
"${SCRIPT_DIR}"/dev-build-gui.sh
"${SCRIPT_DIR}"/dev-build-node.sh
"${SCRIPT_DIR}"/dev-build-mcp-server.sh

echo ""
echo "================================================"
echo "Development build complete!"
echo "================================================"
echo "Note: This builds development versions only."
echo "For release builds, use: build-release.sh"


