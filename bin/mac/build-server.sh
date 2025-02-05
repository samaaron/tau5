#!/bin/bash
set -e # Quit script on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"

cleanup_function() {
    # Restore working directory as it was prior to this script running on exit
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

cd "${SCRIPT_DIR}"
cd ../../server

MIX_ENV=prod mix local.hex --force
MIX_ENV=prod mix local.rebar --force
MIX_ENV=prod mix setup
MIX_ENV=prod mix assets.deploy
MIX_ENV=prod mix release --overwrite --no-deps-check

MIX_ENV=dev mix deps.get
MIX_ENV=dev mix compile

