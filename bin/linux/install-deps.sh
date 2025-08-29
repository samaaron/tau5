#!/bin/bash
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "NOTE: install-deps.sh is deprecated."
echo "Please use:"
echo "  - install-gui-deps.sh for full GUI build"
echo "  - install-node-deps.sh for headless/node only"
echo ""
echo "Running install-gui-deps.sh for backward compatibility..."
echo ""

exec "${SCRIPT_DIR}/install-gui-deps.sh" "$@"