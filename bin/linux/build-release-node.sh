#!/bin/bash
# Thin wrapper to call build-release.sh with --node-only flag

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
exec "${SCRIPT_DIR}/build-release.sh" --node-only "$@"