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

echo "Booting Tau5 Server on macOS..."

if [ -z "${TAU5_BOOT_LOG_PATH}" ]; then
    TAU5_BOOT_LOG_PATH='../log/server_stdouterr.log'
fi

if [ "$TAU5_ENV" = "prod" ]
then
  # Ensure prod env has been setup with:
  # export MIX_ENV=prod
  # mix setup
  # mix assets.deploy
  # mix release --overwrite --no-deps-check

  export MIX_ENV=prod
  _build/prod/rel/tau5/bin/tau5 start > $TAU5_BOOT_LOG_PATH 2>&1
elif [ "$TAU5_ENV" = "dev" ]
then
  # Ensure prod env has been setup with:
  # export MIX_ENV=dev
  # mix setup.dev

  export MIX_ENV=dev
  mix assets.setup
  mix assets.build
  mix phx.server > $TAU5_BOOT_LOG_PATH 2>&1
elif [ "$TAU5_ENV" = "test" ]
then
  export MIX_ENV=test
  export TAU5_MIDI_ENABLED=false
  export TAU5_LINK_ENABLED=false
  mix phx.server > $TAU5_BOOT_LOG_PATH 2>&1
else
  echo "Unknown TAU5_ENV ${TAU5_ENV} - expecting one of prod, dev or test."
fi
