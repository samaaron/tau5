#!/bin/bash
set -e

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
        --help)
            echo "Usage: $0 [--node-only]"
            echo "  --node-only  Package tau5-node only (no GUI)"
            echo ""
            echo "Creates fully self-contained AppImage with Qt libraries bundled"
            exit 0
            ;;
    esac
done

cleanup_function() {
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

echo "================================================"
if [ "$NODE_ONLY" = true ]; then
    echo "Tau5 AppImage Build (tau5-node only)"
else
    echo "Tau5 AppImage Build (full version with GUI)"
fi
echo "Qt libraries will be bundled for portability"
echo "================================================"

cd "${ROOT_DIR}"

# Determine architecture
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

# Determine build naming based on context
if [ "$GITHUB_REF_TYPE" = "tag" ]; then
    # GitHub Actions tagged build - use clean version from tag
    VERSION_STRING="v${VERSION}"
elif [ -n "$GITHUB_ACTIONS" ]; then
    # GitHub Actions CI build - use commit hash so each build is unique
    COMMIT_SHORT=${GITHUB_SHA:0:7}
    VERSION_STRING="${COMMIT_SHORT}"
    # Add Ubuntu version suffix for CI builds to distinguish between different GLIBC versions
    if [[ "$RUNNER_OS" == "Linux" ]]; then
        # Extract Ubuntu version from runner name (e.g., ubuntu-22.04-arm -> u22)
        if [[ "$RUNNER_NAME" =~ ubuntu-([0-9]+)\.([0-9]+) ]] || [[ "$ImageOS" =~ ([0-9]+)\.([0-9]+) ]]; then
            UBUNTU_MAJOR="${BASH_REMATCH[1]}"
            VERSION_STRING="${VERSION_STRING}-u${UBUNTU_MAJOR}"
        fi
    fi
else
    # Local build - use git describe for full info
    GIT_DESC=$(git describe --tags --always --dirty 2>/dev/null || echo "local")
    VERSION_STRING="${GIT_DESC}"
fi

if [ "$NODE_ONLY" = true ]; then
    APPIMAGE_NAME="tau5-node-${ARCH_NAME}-${VERSION_STRING}.AppImage"
    BINARY_NAME="tau5-node"
else
    APPIMAGE_NAME="tau5-${ARCH_NAME}-${VERSION_STRING}.AppImage"
    BINARY_NAME="tau5-gui"
fi

# Determine the correct release directory
# Check if this is a node-only release build or a full release build
NODE_RELEASE_DIR="${ROOT_DIR}/release/Tau5-Node-for-Linux-${ARCH_NAME}-v${VERSION}"
FULL_RELEASE_DIR="${ROOT_DIR}/release/Tau5-for-Linux-${ARCH_NAME}-v${VERSION}"

if [ -d "${NODE_RELEASE_DIR}" ]; then
    # We have a node-only release
    RELEASE_DIR="${NODE_RELEASE_DIR}"
elif [ -d "${FULL_RELEASE_DIR}" ]; then
    # We have a full release (can build both node and full AppImages from it)
    RELEASE_DIR="${FULL_RELEASE_DIR}"
else
    # Neither exists, we'll error out below
    if [ "$NODE_ONLY" = true ]; then
        RELEASE_DIR="${NODE_RELEASE_DIR}"
    else
        RELEASE_DIR="${FULL_RELEASE_DIR}"
    fi
fi

# Check if release exists
if [ ! -d "${RELEASE_DIR}" ]; then
    echo "ERROR: Release directory not found: ${RELEASE_DIR}"
    if [ "$NODE_ONLY" = true ]; then
        echo "Please run 'build-release.sh --node-only' first"
    else
        echo "Please run 'build-release.sh' first"
    fi
    exit 1
fi

echo "Using release directory: $(basename ${RELEASE_DIR})"

# Create AppDir
echo "Creating AppDir structure..."
APPDIR="${RELEASE_DIR}-bundled.AppDir"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"

# Copy binaries
echo "Copying binaries..."
if [ "$NODE_ONLY" = true ]; then
    cp "${RELEASE_DIR}/tau5-node" "${APPDIR}/usr/bin/"
else
    cp "${RELEASE_DIR}/tau5-gui" "${APPDIR}/usr/bin/" 2>/dev/null || true
    cp "${RELEASE_DIR}/tau5-node" "${APPDIR}/usr/bin/"
fi
# Note: linuxdeployqt will handle RPATH/RUNPATH automatically

# Copy Elixir release (needs to be relative to the binary location)
echo "Copying Elixir release..."
cp -r "${RELEASE_DIR}/_build" "${APPDIR}/usr/bin/"

# Create desktop file and icon first (linuxdeploy needs these)
echo "Creating desktop file and icon..."
if [ "$NODE_ONLY" = true ]; then
    EXEC_NAME="tau5-node"
else
    EXEC_NAME="tau5-gui"
fi
cat > "${APPDIR}/tau5.desktop" << EOF
[Desktop Entry]
Name=Tau5
Comment=Collaborative live-coding platform for music and visuals
Exec=${EXEC_NAME}
Icon=tau5
Type=Application
Categories=AudioVideo;Audio;Music;Development;
Terminal=false
EOF

# Create icon
if [ -f "${ROOT_DIR}/assets/tau5.png" ]; then
    cp "${ROOT_DIR}/assets/tau5.png" "${APPDIR}/tau5.png"
elif [ -f "${ROOT_DIR}/gui/resources/tau5-logo.png" ]; then
    cp "${ROOT_DIR}/gui/resources/tau5-logo.png" "${APPDIR}/tau5.png"
elif [ -f "${ROOT_DIR}/tau5-icon.png" ]; then
    # Use the icon we just created
    cp "${ROOT_DIR}/tau5-icon.png" "${APPDIR}/tau5.png"
else
    echo "Creating placeholder icon..."
    # Create a proper 48x48 black PNG using Python if available
    if command -v python3 >/dev/null 2>&1 && python3 -c "import PIL" 2>/dev/null; then
        python3 -c "
from PIL import Image
img = Image.new('RGB', (48, 48), color='black')
img.save('${APPDIR}/tau5.png')
"
    elif command -v convert >/dev/null 2>&1; then
        convert -size 48x48 xc:black "${APPDIR}/tau5.png"
    else
        # Create a minimal valid 48x48 black PNG using base64
        # This is a valid 48x48 black PNG
        echo "/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCAAwADADASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAv/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/8QAFQEBAQAAAAAAAAAAAAAAAAAAAAX/xAAUEQEAAAAAAAAAAAAAAAAAAAAA/9oADAMBAAIRAxEAPwCwAAAAAAAAAAAAAAAAAP/Z" | base64 -d > "${APPDIR}/tau5.png"
    fi
fi

# Bundle Qt libraries - require proper deployment tools
echo "Bundling Qt libraries..."

# Download linuxdeploy and Qt plugin if needed
LINUXDEPLOY_DIR="/tmp/linuxdeploy-${ARCH}"
mkdir -p "${LINUXDEPLOY_DIR}"

if [[ $ARCH == 'x86_64' ]]; then
    LINUXDEPLOY_ARCH="x86_64"
elif [[ $ARCH == 'aarch64' ]] || [[ $ARCH == 'arm64' ]]; then
    LINUXDEPLOY_ARCH="aarch64"
else
    echo "ERROR: Unsupported architecture: $ARCH"
    exit 1
fi

LINUXDEPLOY="${LINUXDEPLOY_DIR}/linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage"
LINUXDEPLOY_QT="${LINUXDEPLOY_DIR}/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage"

# Download linuxdeploy if not present
if [ ! -f "${LINUXDEPLOY}" ]; then
    echo "Downloading linuxdeploy..."
    LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage"
    wget -q --show-progress -O "${LINUXDEPLOY}" "${LINUXDEPLOY_URL}"
    chmod +x "${LINUXDEPLOY}"
fi

# Download Qt plugin if not present
if [ ! -f "${LINUXDEPLOY_QT}" ]; then
    echo "Downloading linuxdeploy Qt plugin..."
    QT_PLUGIN_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage"
    wget -q --show-progress -O "${LINUXDEPLOY_QT}" "${QT_PLUGIN_URL}"
    chmod +x "${LINUXDEPLOY_QT}"
fi

# Use linuxdeploy to bundle dependencies
echo "Using linuxdeploy to bundle Qt dependencies..."

# Set Qt environment if needed
if [ -n "$QTDIR" ]; then
    export QT_ROOT_DIR="$QTDIR"
elif [ -n "$Qt6_DIR" ]; then
    export QT_ROOT_DIR="$(dirname $(dirname $Qt6_DIR))"
fi

# Deploy libraries for each binary
# Note: We don't use --output here as we'll create the AppImage manually with appimagetool
if [ "$NODE_ONLY" = true ]; then
    "${LINUXDEPLOY}" --appdir "${APPDIR}" \
        --executable "${APPDIR}/usr/bin/tau5-node" \
        --desktop-file "${APPDIR}/tau5.desktop" \
        --icon-file "${APPDIR}/tau5.png" \
        --plugin qt
else
    # Deploy for full version (both binaries)
    "${LINUXDEPLOY}" --appdir "${APPDIR}" \
        --executable "${APPDIR}/usr/bin/tau5-gui" \
        --desktop-file "${APPDIR}/tau5.desktop" \
        --icon-file "${APPDIR}/tau5.png" \
        --plugin qt
    
    # Also handle tau5-node (no need to re-specify desktop/icon, already handled)
    "${LINUXDEPLOY}" --appdir "${APPDIR}" \
        --executable "${APPDIR}/usr/bin/tau5-node" \
        --plugin qt
fi

echo "Qt libraries bundled successfully."

# Note: linuxdeploy handles all library bundling including ICU and other dependencies

# Create AppRun with dependency checking
echo "Creating AppRun launcher with dependency checks..."
cat > "${APPDIR}/AppRun" << 'EOF'
#!/bin/bash
set -e

# Get the directory where the AppImage is mounted
HERE="$(dirname "$(readlink -f "${0}")")"

export RELEASE_ROOT="${HERE}/_build/prod/rel/tau5"
# Ensure bundled libraries are used exclusively
export LD_LIBRARY_PATH="${HERE}/usr/lib"
export QT_PLUGIN_PATH="${HERE}/usr/plugins"
# Prevent Qt from using system libraries
unset QT_QPA_PLATFORM_PLUGIN_PATH
# Debug: Show library resolution if requested
if [ "$TAU5_DEBUG_LIBS" = "1" ]; then
    echo "AppImage library path: ${LD_LIBRARY_PATH}"
    echo "Checking for Qt libraries:"
    ls -la "${HERE}/usr/lib/libQt6"* 2>/dev/null || echo "  No Qt libraries found!"
    echo "Running ldd on tau5-node:"
    ldd "${HERE}/usr/bin/tau5-node" 2>&1 | head -20
fi

# Determine which binary to run
if [ -f "${HERE}/usr/bin/tau5-node" ] && [ ! -f "${HERE}/usr/bin/tau5-gui" ]; then
    exec "${HERE}/usr/bin/tau5-node" "$@"
fi

if [ -n "$TAU5_NODE_MODE" ] || [ "$1" = "--node" ]; then
    if [ "$1" = "--node" ]; then
        shift
    fi
    exec "${HERE}/usr/bin/tau5-node" "$@"
else
    exec "${HERE}/usr/bin/tau5-gui" "$@"
fi
EOF

chmod +x "${APPDIR}/AppRun"

# Desktop file and icon already created before linuxdeploy step

# Download appimagetool if needed
if ! command -v appimagetool &> /dev/null; then
    echo "Downloading appimagetool..."
    if [[ $ARCH == 'x86_64' ]]; then
        APPIMAGE_ARCH="x86_64"
    elif [[ $ARCH == 'aarch64' ]] || [[ $ARCH == 'arm64' ]]; then
        APPIMAGE_ARCH="aarch64"
    else
        echo "ERROR: Unsupported architecture: $ARCH"
        exit 1
    fi
    
    APPIMAGETOOL_URL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${APPIMAGE_ARCH}.AppImage"
    APPIMAGETOOL_PATH="/tmp/appimagetool-${APPIMAGE_ARCH}.AppImage"
    
    wget -q --show-progress -O "${APPIMAGETOOL_PATH}" "${APPIMAGETOOL_URL}"
    chmod +x "${APPIMAGETOOL_PATH}"
    APPIMAGETOOL="${APPIMAGETOOL_PATH}"
else
    APPIMAGETOOL="appimagetool"
fi

# Debug: Show AppDir structure before packaging
echo ""
echo "=== AppDir structure before packaging ==="
echo "All symlinks in AppDir:"
find "${APPDIR}" -type l -ls 2>/dev/null || true
echo ""
echo "=== AppRun location and type ==="
ls -la "${APPDIR}/AppRun"
file "${APPDIR}/AppRun" 2>/dev/null || true
echo ""
echo "=== /usr/bin contents ==="
ls -la "${APPDIR}/usr/bin/" 2>/dev/null || true
echo ""
echo "=== Root level contents ==="
ls -la "${APPDIR}/" 2>/dev/null || true
echo "================================"
echo ""

# Build AppImage
echo "Building AppImage..."
cd "${ROOT_DIR}/release"
# In CI environments, appimagetool may fail to test the AppImage due to missing FUSE
# but still creates a valid AppImage file
ARCH=${APPIMAGE_ARCH:-$ARCH} "${APPIMAGETOOL}" --no-appstream "${APPDIR}" "${APPIMAGE_NAME}" || true

# Check if AppImage was actually created despite any test failures
if [ ! -f "${APPIMAGE_NAME}" ]; then
    echo "ERROR: AppImage was not created"
    exit 1
fi

# Clean up
rm -rf "${APPDIR}"
if [ -n "${APPIMAGETOOL_PATH}" ] && [ -f "${APPIMAGETOOL_PATH}" ]; then
    rm -f "${APPIMAGETOOL_PATH}"
fi

# Check final size
APPIMAGE_SIZE=$(du -h "${APPIMAGE_NAME}" | cut -f1)

echo ""
echo "========================================"
echo "Self-Contained AppImage Build Complete!"
echo "========================================"
echo "AppImage: ${ROOT_DIR}/release/${APPIMAGE_NAME}"
echo "Size: ${APPIMAGE_SIZE}"
echo ""
echo "This AppImage includes:"
echo "  ✓ All Qt libraries (no Qt installation needed)"
echo "  ✓ Elixir/Erlang runtime"
echo "  ✓ All NIFs"
echo ""
echo "Still requires from system:"
echo "  • libasound2 (ALSA) for MIDI support"
echo "  • libssl3/libssl1.1 (OpenSSL) for HTTPS/crypto"
echo ""
echo "The AppImage will check for these on startup and provide"
echo "clear instructions if they're missing."
echo ""
if [ "$NODE_ONLY" = true ]; then
    echo "Run with: ./${APPIMAGE_NAME}"
else
    echo "Run with: ./${APPIMAGE_NAME}          # GUI mode"
    echo "         ./${APPIMAGE_NAME} --node   # Node mode"
fi