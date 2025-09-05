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
cd server

rm -rf deps/sp_link/build
rm -rf deps/sp_midi/build
rm -rf deps/tau5_discovery/build
rm -rf deps/exqlite/build
rm -rf priv/nifs
mkdir -p priv/nifs

rm -rf _build
rm -rf deps
rm -rf priv/static/assets/*


cd "${ROOT_DIR}"
rm -rf gui/build
rm -rf gui/build-release
rm -rf gui/build-release-node
rm -rf release/*
# Keep the release directory structure but remove contents
[ -d release ] && touch release/.gitkeep
rm -rf build

