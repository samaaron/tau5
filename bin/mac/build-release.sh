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
echo "Tau5 macOS Release Build"
echo "================================================"

# Clean and prepare directories
cd "${ROOT_DIR}"
echo "Cleaning previous builds..."
rm -rf release
rm -rf gui/build-release  # Use separate build directory for release
rm -rf server/_build/prod

# Build server in production mode
echo ""
echo "Building Elixir server (production release)..."
cd "${ROOT_DIR}/server"
MIX_ENV=prod mix deps.get --only prod
MIX_ENV=prod mix compile
MIX_ENV=prod mix release --overwrite

# Build GUI with release configuration
echo ""
echo "Building GUI components with release paths..."
cd "${ROOT_DIR}/gui"
mkdir -p build-release  # Separate directory from dev builds
cd build-release

# Configure CMake for release build with proper server path
# In macOS app bundle: Tau5.app/Contents/Resources/_build/prod/rel/tau5
if [[ $(uname -m) == 'arm64' ]]; then
    echo "Configuring for Apple Silicon (arm64) release..."
    cmake -G "Unix Makefiles" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_ARCHITECTURES="arm64" \
          -DTAU5_RELEASE_BUILD=ON \
          -DTAU5_INSTALL_SERVER_PATH="../Resources/_build/prod/rel/tau5" \
          -DBUILD_DEBUG_PANE=OFF \
          ..
else
    echo "Configuring for Intel (x86_64) release..."
    cmake -G "Unix Makefiles" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_ARCHITECTURES="x86_64" \
          -DTAU5_RELEASE_BUILD=ON \
          -DTAU5_INSTALL_SERVER_PATH="../Resources/_build/prod/rel/tau5" \
          -DBUILD_DEBUG_PANE=OFF \
          ..
fi

# Build both tau5 and tau5-node
echo "Building GUI and tau5-node..."
cmake --build . --target tau5
cmake --build . --target tau5-node

# Create release directory structure
echo ""
echo "Assembling release package..."
cd "${ROOT_DIR}"
mkdir -p release
cd release

# Copy the app bundle
cp -R ../gui/build-release/bin/*.app .
APP_NAME=$(find . -name "*.app" -type d | head -n 1)

# tau5-node should already be in the app bundle from the build
# Verify it's there
if [ ! -f "${APP_NAME}/Contents/MacOS/tau5-node" ]; then
    echo "ERROR: tau5-node not found in app bundle!"
    exit 1
fi

# Copy production server release into the app bundle
echo "Copying server release into app bundle..."
cd "${APP_NAME}/Contents/Resources"
mkdir -p _build/prod/rel
cp -R "${ROOT_DIR}/server/_build/prod/rel/tau5" _build/prod/rel/

# Test the release build
echo ""
echo "Testing release build with --check..."
cd "${ROOT_DIR}/release"

# The --check flag now understands release builds
"${APP_NAME}/Contents/MacOS/Tau5" --check
if [ $? -ne 0 ]; then
    echo "ERROR: Release build health check failed!"
    exit 1
fi

"${APP_NAME}/Contents/MacOS/tau5-node" --check
if [ $? -ne 0 ]; then
    echo "ERROR: tau5-node health check failed!"
    exit 1
fi

echo "✓ All health checks passed"

echo ""
echo "========================================"
echo "Release build completed successfully!"
echo "========================================"
echo "Release package: ${ROOT_DIR}/release/${APP_NAME}"
echo ""
echo "The app bundle is self-contained and ready for distribution."
echo ""
echo "Note: To fix library paths and symlinks for distribution, run:"
echo "  ${SCRIPT_DIR}/fix-release-paths.sh"