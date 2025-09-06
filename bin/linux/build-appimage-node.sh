#!/bin/bash
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."

cleanup_function() {
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

echo "================================================"
echo "Tau5 AppImage Build (tau5-node only)"
echo "Using simplified approach for non-Qt binary"
echo "================================================"

cd "${ROOT_DIR}"

# Determine architecture
ARCH=$(uname -m)
if [[ $ARCH == 'x86_64' ]]; then
    ARCH_NAME="x64"
    GO_ARCH="x86_64"
elif [[ $ARCH == 'aarch64' ]] || [[ $ARCH == 'arm64' ]]; then
    ARCH_NAME="ARM64"
    GO_ARCH="aarch64"
else
    ARCH_NAME=$ARCH
    GO_ARCH=$ARCH
fi

# Read version from project root VERSION file
VERSION=$(cat "${ROOT_DIR}/VERSION" 2>/dev/null || echo "0.0.0")

# Determine build naming based on context
if [ "$GITHUB_REF_TYPE" = "tag" ]; then
    VERSION_STRING="v${VERSION}"
elif [ -n "$GITHUB_ACTIONS" ]; then
    COMMIT_SHORT=${GITHUB_SHA:0:7}
    VERSION_STRING="${COMMIT_SHORT}"
else
    GIT_DESC=$(git describe --tags --always --dirty 2>/dev/null || echo "local")
    VERSION_STRING="${GIT_DESC}"
fi

APPIMAGE_NAME="tau5-node-${ARCH_NAME}-${VERSION_STRING}.AppImage"

# Try different release directory naming patterns
RELEASE_DIR="${ROOT_DIR}/release/Tau5-Node-for-Linux-${ARCH_NAME}-v${VERSION}"
if [ ! -d "${RELEASE_DIR}" ]; then
    # Fallback to general naming
    RELEASE_DIR="${ROOT_DIR}/release/Tau5-for-Linux-${ARCH_NAME}-v${VERSION}"
fi

# Check if release exists
if [ ! -d "${RELEASE_DIR}" ]; then
    echo "ERROR: Release directory not found: ${RELEASE_DIR}"
    echo "Please run build-release-node.sh first"
    exit 1
fi

echo "Using release directory: $(basename ${RELEASE_DIR})"

# Create AppDir
echo "Creating AppDir structure..."
APPDIR="${RELEASE_DIR}-node.AppDir"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
mkdir -p "${APPDIR}/usr/lib"

# Copy binaries
echo "Copying binaries..."
cp "${RELEASE_DIR}/tau5-node" "${APPDIR}/usr/bin/"

# Copy Elixir release (needs to be relative to the binary location)
echo "Copying Elixir release..."
cp -r "${RELEASE_DIR}/_build" "${APPDIR}/usr/bin/"

# Create desktop file
cat > "${APPDIR}/usr/share/applications/tau5-node.desktop" << EOF
[Desktop Entry]
Name=Tau5 Node
Comment=Tau5 headless server
Exec=tau5-node
Icon=tau5-node
Type=Application
Categories=AudioVideo;Audio;Music;Development;
Terminal=true
EOF

# Create/copy icon
if [ ! -f "${ROOT_DIR}/assets/tau5.png" ]; then
    echo "Creating placeholder icon..."
    printf '\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90wS\xde\x00\x00\x00\x0cIDATx\x9cc\xf8\x0f\x00\x00\x01\x01\x00\x05\xb8\xd5\x91\x00\x00\x00\x00IEND\xaeB`\x82' > "${APPDIR}/usr/share/icons/hicolor/256x256/apps/tau5-node.png"
else
    cp "${ROOT_DIR}/assets/tau5.png" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/tau5-node.png"
fi

# Create AppRun
echo "Creating AppRun launcher..."
cat > "${APPDIR}/AppRun" << 'EOF'
#!/bin/bash
set -e

# Get the directory where the AppImage is mounted
HERE="$(dirname "$(readlink -f "${0}")")"

export RELEASE_ROOT="${HERE}/usr/bin/_build/prod/rel/tau5"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"

exec "${HERE}/usr/bin/tau5-node" "$@"
EOF

chmod +x "${APPDIR}/AppRun"

# Copy essential libraries manually
echo "Copying essential libraries..."
# Find and copy libasound for MIDI support
if [ -f /usr/lib/x86_64-linux-gnu/libasound.so.2 ]; then
    cp /usr/lib/x86_64-linux-gnu/libasound.so.2* "${APPDIR}/usr/lib/" 2>/dev/null || true
elif [ -f /usr/lib/aarch64-linux-gnu/libasound.so.2 ]; then
    cp /usr/lib/aarch64-linux-gnu/libasound.so.2* "${APPDIR}/usr/lib/" 2>/dev/null || true
fi

# Download go-appimage tool if needed
APPIMAGETOOL_NAME="appimagetool-${GO_ARCH}.AppImage"
APPIMAGETOOL_PATH="/tmp/${APPIMAGETOOL_NAME}"

if [ ! -f "${APPIMAGETOOL_PATH}" ]; then
    echo "Downloading go-appimage appimagetool..."
    # Get the latest version number
    LATEST_RELEASE=$(wget -qO- https://api.github.com/repos/probonopd/go-appimage/releases/tags/continuous | grep -oP '"name":\s*"appimagetool-\K[0-9]+' | head -1)
    APPIMAGETOOL_URL="https://github.com/probonopd/go-appimage/releases/download/continuous/appimagetool-${LATEST_RELEASE}-${GO_ARCH}.AppImage"
    
    echo "Downloading from: ${APPIMAGETOOL_URL}"
    wget -q --show-progress -O "${APPIMAGETOOL_PATH}" "${APPIMAGETOOL_URL}"
    chmod +x "${APPIMAGETOOL_PATH}"
fi

# Build AppImage without deployment (to avoid Qt plugin detection)
echo ""
echo "Building AppImage..."
cd "${ROOT_DIR}/release"
# Use basename since we're already in the release directory
APPDIR_NAME="$(basename ${APPDIR})"
# Export environment variables and run appimagetool without deploy
export VERSION="${VERSION}"
export ARCH="${GO_ARCH}"
# Use the tool directly without deploy step
"${APPIMAGETOOL_PATH}" "${APPDIR_NAME}"

# The tool creates its own filename, so we need to rename it
CREATED_NAME="Tau5 Node-${VERSION}-${GO_ARCH}.AppImage"

if [ -f "${CREATED_NAME}" ] && [ "${CREATED_NAME}" != "${APPIMAGE_NAME}" ]; then
    mv "${CREATED_NAME}" "${APPIMAGE_NAME}"
fi

# Check if AppImage was created
if [ ! -f "${APPIMAGE_NAME}" ]; then
    echo "ERROR: AppImage was not created"
    exit 1
fi

# Clean up
rm -rf "${APPDIR}"

# Check final size
APPIMAGE_SIZE=$(du -h "${APPIMAGE_NAME}" | cut -f1)

echo ""
echo "========================================"
echo "Simplified AppImage Build Complete!"
echo "========================================"
echo "AppImage: ${ROOT_DIR}/release/${APPIMAGE_NAME}"
echo "Size: ${APPIMAGE_SIZE}"
echo ""
echo "This AppImage includes minimal libraries for tau5-node"
echo "It should work on most Linux distributions with FUSE support"
echo ""
echo "Run with: ./${APPIMAGE_NAME}"