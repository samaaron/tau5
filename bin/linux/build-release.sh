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
echo "Tau5 Linux Release Build"
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
# For Linux release: binaries in root, server in _build/prod/rel/tau5
cmake -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DTAU5_RELEASE_BUILD=ON \
      -DTAU5_INSTALL_SERVER_PATH="_build/prod/rel/tau5" \
      -DBUILD_DEBUG_PANE=OFF \
      ..

# Build both tau5 and tau5-node
echo "Building GUI and tau5-node..."
cmake --build . --target tau5
cmake --build . --target tau5-node

# Create release directory structure
echo ""
echo "Assembling release package..."
cd "${ROOT_DIR}"
mkdir -p release/Tau5-Linux
cd release/Tau5-Linux

# Copy binaries to release root
echo "Copying binaries..."
cp "${ROOT_DIR}/gui/build-release/bin/tau5" .
cp "${ROOT_DIR}/gui/build-release/bin/tau5-node" .

# Make binaries executable
chmod +x tau5 tau5-node

# Copy production server release
echo "Copying server release..."
mkdir -p _build/prod/rel
if [ ! -d "${ROOT_DIR}/server/_build/prod/rel/tau5" ]; then
    echo "ERROR: Server release not found at ${ROOT_DIR}/server/_build/prod/rel/tau5"
    echo "Make sure the Elixir release build succeeded"
    exit 1
fi
cp -R "${ROOT_DIR}/server/_build/prod/rel/tau5" _build/prod/rel/
if [ ! -d "_build/prod/rel/tau5" ]; then
    echo "ERROR: Failed to copy server release"
    exit 1
fi

# Test the release build
echo ""
echo "Testing release build with health checks..."
cd "${ROOT_DIR}/release/Tau5-Linux"

# Run health checks - essential for CI/CD pipelines
./tau5 --check
if [ $? -ne 0 ]; then
    echo "ERROR: Release build health check failed for tau5!"
    exit 1
fi

./tau5-node --check
if [ $? -ne 0 ]; then
    echo "ERROR: Release build health check failed for tau5-node!"
    exit 1
fi

echo "✓ All health checks passed"

echo ""
echo "========================================"
echo "Release build completed successfully!"
echo "========================================"
echo "Release package: ${ROOT_DIR}/release/Tau5-Linux/"
echo ""
echo "The release directory is self-contained and ready for distribution."
echo "Users can run Tau5 with:"
echo "  ./tau5"
echo "  ./tau5-node"