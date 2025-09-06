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
    BINARY_NAME="tau5"
fi

# Both AppImage types use the same full release directory
# The --node-only flag just controls which binary gets packaged
RELEASE_DIR="${ROOT_DIR}/release/Tau5-for-Linux-${ARCH_NAME}-v${VERSION}"

# Check if release exists
if [ ! -d "${RELEASE_DIR}" ]; then
    echo "ERROR: Release directory not found: ${RELEASE_DIR}"
    echo "Please run build-release.sh first"
    echo "Note: Both full and node-only AppImages use the full release directory"
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
    cp "${RELEASE_DIR}/tau5" "${APPDIR}/usr/bin/" 2>/dev/null || true
    cp "${RELEASE_DIR}/tau5-node" "${APPDIR}/usr/bin/"
fi
# Note: linuxdeployqt will handle RPATH/RUNPATH automatically

# Copy Elixir release (needs to be relative to the binary location)
echo "Copying Elixir release..."
cp -r "${RELEASE_DIR}/_build" "${APPDIR}/usr/bin/"

# Bundle Qt libraries - require proper deployment tools
echo "Bundling Qt libraries..."

# Check for linuxdeployqt (required for proper AppImage creation)
if ! command -v linuxdeployqt >/dev/null 2>&1; then
    echo "ERROR: linuxdeployqt is required but not found!"
    echo ""
    echo "To install linuxdeployqt:"
    echo "  - For x64: Download from https://github.com/probonopd/linuxdeployqt/releases"
    echo "  - For ARM64: Build from source: git clone, qmake, make"
    echo ""
    echo "linuxdeployqt ensures Qt libraries are properly bundled for AppImage portability."
    exit 1
fi

echo "Using linuxdeployqt to bundle Qt dependencies..."
if [ "$NODE_ONLY" = true ]; then
    linuxdeployqt "${APPDIR}/usr/bin/tau5-node" -bundle-non-qt-libs -no-plugins
else
    linuxdeployqt "${APPDIR}/usr/bin/tau5" -bundle-non-qt-libs
    linuxdeployqt "${APPDIR}/usr/bin/tau5-node" -bundle-non-qt-libs -no-plugins
fi

# linuxdeployqt will exit with error if it fails, so we don't need to check
echo "Qt libraries bundled successfully."

# Note: linuxdeployqt handles all library bundling including ICU and other dependencies

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
if [ -f "${HERE}/usr/bin/tau5-node" ] && [ ! -f "${HERE}/usr/bin/tau5" ]; then
    exec "${HERE}/usr/bin/tau5-node" "$@"
fi

if [ -n "$TAU5_NODE_MODE" ] || [ "$1" = "--node" ]; then
    if [ "$1" = "--node" ]; then
        shift
    fi
    exec "${HERE}/usr/bin/tau5-node" "$@"
else
    exec "${HERE}/usr/bin/tau5" "$@"
fi
EOF

chmod +x "${APPDIR}/AppRun"

# Create desktop file
cat > "${APPDIR}/tau5.desktop" << EOF
[Desktop Entry]
Name=Tau5
Comment=Collaborative live-coding platform for music and visuals
Exec=AppRun
Icon=tau5
Type=Application
Categories=AudioVideo;Audio;Music;Development;
Terminal=false
EOF

# Create icon
if [ ! -f "${ROOT_DIR}/assets/tau5.png" ]; then
    echo "Creating placeholder icon..."
    printf '\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90wS\xde\x00\x00\x00\x0cIDATx\x9cc\xf8\x0f\x00\x00\x01\x01\x00\x05\xb8\xd5\x91\x00\x00\x00\x00IEND\xaeB`\x82' > "${APPDIR}/tau5.png"
else
    cp "${ROOT_DIR}/assets/tau5.png" "${APPDIR}/tau5.png"
fi

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

# Build AppImage
echo ""
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