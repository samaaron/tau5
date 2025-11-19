#!/bin/bash
set -e # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ORIGINAL_MIX_ENV="${MIX_ENV}"

cleanup_function() {
    # Restore working directory and environment as they were prior to this script
    cd "${WORKING_DIR}"
    export MIX_ENV="${ORIGINAL_MIX_ENV}"
}
trap cleanup_function EXIT

# Check if mix is available
if ! command -v mix &> /dev/null; then
    echo "mix command not found. Ensure Elixir is installed and mix is in PATH."
    exit 1
fi

cd "${SCRIPT_DIR}"
cd ../../server

# Install hex and rebar if needed
mix local.hex --force
mix local.rebar --force

# Build production release
echo "Building production release..."
MIX_ENV=prod mix setup
MIX_ENV=prod mix supersonic.deploy
MIX_ENV=prod mix assets.deploy
MIX_ENV=prod mix release --overwrite

# Setup development environment
echo "Setting up development environment..."
MIX_ENV=dev mix setup

echo "---"
echo "    Tau5 server built successfully."
echo "---"