#!/bin/bash
set -e # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
SERVER_DIR="${ROOT_DIR}/server"
ORIGINAL_MIX_ENV="${MIX_ENV}"

cleanup_function() {
    # Restore working directory and environment as they were prior to this script
    cd "${WORKING_DIR}"
    export MIX_ENV="${ORIGINAL_MIX_ENV}"
}
trap cleanup_function EXIT

cd "$SERVER_DIR"

if [ "$1" == "prod" ]; then
    # Production mode - use release
    RELEASE_PATH="${SERVER_DIR}/_build/prod/rel/tau5/bin/tau5"
    
    if [ ! -f "$RELEASE_PATH" ]; then
        echo "Production release not found!"
        echo "Run ${SCRIPT_DIR}/central-deploy.sh prod first"
        exit 1
    fi
    
    # Check for existing beam processes on the port
    PORT=${PORT:-4000}
    if lsof -i :$PORT > /dev/null 2>&1; then
        echo "WARNING: Port $PORT is already in use!"
        echo "Existing process:"
        lsof -i :$PORT | grep LISTEN || true
        echo ""
        echo "You may have a manually started instance running."
        echo "To kill it: pkill -f 'beam.*tau5' or kill the PID shown above"
        exit 1
    fi
    
    # Check for SECRET_KEY_BASE
    if [ -z "$SECRET_KEY_BASE" ]; then
        # Try to load from ~/.tau5_secret if it exists
        if [ -f "$HOME/.tau5_secret" ]; then
            SECRET_KEY_BASE=$(cat "$HOME/.tau5_secret")
            echo "Loaded SECRET_KEY_BASE from ~/.tau5_secret"
        else
            echo "SECRET_KEY_BASE not set!"
            echo "Generate one with: mix phx.gen.secret > ~/.tau5_secret"
            echo "Or run: SECRET_KEY_BASE=<your-secret> $0 prod"
            exit 1
        fi
    fi
    
    echo "Starting central server in PRODUCTION mode"
    echo "Server will be available at: http://${PHX_HOST:-localhost}:${PORT}"
    
    TAU5_MODE=central \
    PHX_SERVER=true \
    PHX_HOST=${PHX_HOST:-localhost} \
    PORT=$PORT \
    SECRET_KEY_BASE=$SECRET_KEY_BASE \
    exec "$RELEASE_PATH" start
else
    # Check if mix is available
    if ! command -v mix &> /dev/null; then
        echo "mix command not found. Ensure Elixir is installed and mix is in PATH."
        exit 1
    fi
    
    # Development mode
    echo "Starting central server in DEVELOPMENT mode"
    echo "Server will be available at: http://localhost:4000"
    echo "Press Ctrl+C to stop"
    
    TAU5_MODE=central iex -S mix phx.server
fi