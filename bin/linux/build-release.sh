#!/bin/bash
set -e # Quit script on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
NODE_ONLY=false

# Parse arguments
for arg in "$@"; do
    case $arg in
        --node-only)
            NODE_ONLY=true
            ;;
    esac
done

cleanup_function() {
    # Restore working directory as it was prior to this script running on exit
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

echo "================================================"
if [ "$NODE_ONLY" = true ]; then
    echo "Tau5 Linux Release Build (tau5-node only)"
    echo "For headless systems without Qt GUI dependencies"
else
    echo "Tau5 Linux Release Build"
fi
echo "================================================"

# Clean and prepare directories
cd "${ROOT_DIR}"
echo "Cleaning previous builds..."
if [ "$NODE_ONLY" = true ]; then
    rm -rf gui/build-release-node  # Use separate build directory for node-only release
else
    rm -rf gui/build-release  # Use separate build directory for release
fi
rm -rf server/_build/prod

# Build server in production mode
echo ""
echo "Building Elixir server (production release)..."
cd "${ROOT_DIR}/server"
# Setup will get deps, build assets, and compile NIFs
MIX_ENV=prod mix setup
# Deploy assets for production (minified)
MIX_ENV=prod mix assets.deploy
# Create release
MIX_ENV=prod mix release --overwrite

# Build GUI with release configuration
echo ""
if [ "$NODE_ONLY" = true ]; then
    echo "Building tau5-node with release paths..."
    cd "${ROOT_DIR}/gui"
    mkdir -p build-release-node  # Separate directory from dev builds
    cd build-release-node

    if [[ $(uname -m) == 'arm64' ]] || [[ $(uname -m) == 'aarch64' ]] || [ "$TAU5_BUILD_TARGET" == 'arm64' ]
    then
        echo "Detected ARM architecture"
        cmake -G "Unix Makefiles" \
              -DCMAKE_BUILD_TYPE=Release \
              -DTAU5_RELEASE_BUILD=ON \
              -DTAU5_SERVER_PATH="_build/prod/rel/tau5" \
              -DBUILD_NODE_ONLY=ON \
              -DBUILD_DEBUG_PANE=OFF \
              -DBUILD_MCP_SERVER=OFF \
              ..
    else
        cmake -G "Unix Makefiles" \
              -DCMAKE_BUILD_TYPE=Release \
              -DTAU5_RELEASE_BUILD=ON \
              -DTAU5_SERVER_PATH="_build/prod/rel/tau5" \
              -DBUILD_NODE_ONLY=ON \
              -DBUILD_DEBUG_PANE=OFF \
              -DBUILD_MCP_SERVER=OFF \
              ..
    fi

    # Build tau5-node only
    echo "Building tau5-node..."
    cmake --build . --target tau5-node
else
    echo "Building GUI components with release paths..."
    cd "${ROOT_DIR}/gui"
    mkdir -p build-release  # Separate directory from dev builds
    cd build-release

    # Configure CMake for release build with proper server path
    cmake -G "Unix Makefiles" \
          -DCMAKE_BUILD_TYPE=Release \
          -DTAU5_RELEASE_BUILD=ON \
          -DTAU5_SERVER_PATH="_build/prod/rel/tau5" \
          -DBUILD_DEBUG_PANE=OFF \
          ..

    echo "Building GUI and tau5-node..."
    cmake --build . --target tau5-gui
    cmake --build . --target tau5-node
fi

echo ""
echo "Assembling release package..."
cd "${ROOT_DIR}"

ARCH=$(uname -m)
if [[ $ARCH == 'x86_64' ]]; then
    ARCH_NAME="x64"
elif [[ $ARCH == 'aarch64' ]] || [[ $ARCH == 'arm64' ]]; then
    ARCH_NAME="ARM64"
else
    ARCH_NAME=$ARCH
fi

# Read version from project root VERSION file
VERSION=$(cat "${ROOT_DIR}/VERSION" 2>/dev/null || echo "0.0.0")

if [ "$NODE_ONLY" = true ]; then
    RELEASE_DIR_NAME="Tau5-Node-for-Linux-${ARCH_NAME}-v${VERSION}"
    mkdir -p "release/${RELEASE_DIR_NAME}"
    cd "release/${RELEASE_DIR_NAME}"

    # Copy tau5-node binary to release root
    echo "Copying tau5-node binary..."
    cp "${ROOT_DIR}/gui/build-release-node/bin/tau5-node" .

    # Make binary executable
    chmod +x tau5-node
else
    RELEASE_DIR_NAME="Tau5-for-Linux-${ARCH_NAME}-v${VERSION}"
    mkdir -p "release/${RELEASE_DIR_NAME}"
    cd "release/${RELEASE_DIR_NAME}"

    # Copy binaries to release root
    echo "Copying binaries..."
    cp "${ROOT_DIR}/gui/build-release/bin/tau5-gui" .
    cp "${ROOT_DIR}/gui/build-release/bin/tau5-node" .

    # Make binaries executable
    chmod +x tau5-gui tau5-node
fi

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

echo ""
echo "========================================"
if [ "$NODE_ONLY" = true ]; then
    echo "tau5-node release build completed successfully!"
    echo "========================================"
    echo "Release package: ${ROOT_DIR}/release/${RELEASE_DIR_NAME}/"
    echo ""
    echo "The release directory is self-contained and ready for distribution."
    echo "This build is suitable for headless systems without Qt dependencies."
    echo "Users can run Tau5 with:"
    echo "  ./tau5-node"
else
    echo "Release build completed successfully!"
    echo "========================================"
    echo "Release package: ${ROOT_DIR}/release/${RELEASE_DIR_NAME}/"
    echo ""
    echo "The release directory is self-contained and ready for distribution."
    echo "Users can run Tau5 with:"
    echo "  ./tau5"
    echo "  ./tau5-node"
fi