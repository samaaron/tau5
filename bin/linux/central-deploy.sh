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

# Check for clean flag
if [ "$1" == "clean" ] || [ "$2" == "clean" ]; then
    echo "Performing clean build (removing _build directory)..."
    rm -rf _build
fi

# Check if we're in production environment
if [ "$1" == "prod" ] || [ "$2" == "prod" ] || [ "$MIX_ENV" == "prod" ]; then
    echo "Building central server production release..."
    
    # Clean if requested
    if [ "$1" == "clean" ] || [ "$2" == "clean" ]; then
        rm -rf _build/prod
    fi
    
    MIX_ENV=prod mix deps.get --only prod
    MIX_ENV=prod mix compile
    MIX_ENV=prod mix supersonic.deploy
    MIX_ENV=prod mix assets.deploy
    MIX_ENV=prod mix release --overwrite
    
    echo "---"
    echo "    Central server production release built successfully."
    echo "    "
    echo "    To run the central server:"
    echo "    ${SCRIPT_DIR}/central-start.sh prod"
    echo "    "
    echo "    Or with systemd:"
    echo "    ${SCRIPT_DIR}/central-gen-systemd-service.sh"
    echo "    sudo systemctl start tau5"
    echo "    "
    echo "    Usage: $0 [prod] [clean]"
    echo "    Examples:"
    echo "      $0 prod       # Build production release"
    echo "      $0 prod clean # Clean build production release"
    echo "---"
else
    echo "Setting up central server development environment..."
    
    MIX_ENV=dev mix setup
    
    echo "---"
    echo "    Central server development environment ready."
    echo "    "
    echo "    To run in dev mode:"
    echo "    TAU5_MODE=central iex -S mix phx.server"
    echo "---"
fi