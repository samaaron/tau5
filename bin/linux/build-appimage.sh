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
            echo "Creates fully self-contained AppImage using go-appimage tools"
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
echo "Using go-appimage for maximum compatibility"
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

if [ "$NODE_ONLY" = true ]; then
    APPIMAGE_NAME="tau5-node-${ARCH_NAME}-${VERSION_STRING}.AppImage"
    BINARY_NAME="tau5-node"
else
    APPIMAGE_NAME="tau5-${ARCH_NAME}-${VERSION_STRING}.AppImage"
    BINARY_NAME="tau5"
fi

# Try different release directory naming patterns
if [ "$NODE_ONLY" = true ]; then
    # Try node-specific naming first
    RELEASE_DIR="${ROOT_DIR}/release/Tau5-Node-for-Linux-${ARCH_NAME}-v${VERSION}"
    if [ ! -d "${RELEASE_DIR}" ]; then
        # Fallback to general naming
        RELEASE_DIR="${ROOT_DIR}/release/Tau5-for-Linux-${ARCH_NAME}-v${VERSION}"
    fi
else
    RELEASE_DIR="${ROOT_DIR}/release/Tau5-for-Linux-${ARCH_NAME}-v${VERSION}"
fi

# Check if release exists
if [ ! -d "${RELEASE_DIR}" ]; then
    echo "ERROR: Release directory not found: ${RELEASE_DIR}"
    echo "Please run build-release.sh or build-release-node.sh first"
    exit 1
fi

echo "Using release directory: $(basename ${RELEASE_DIR})"

# Create AppDir
echo "Creating AppDir structure..."
APPDIR="${RELEASE_DIR}-go.AppDir"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

# Copy binaries
echo "Copying binaries..."
if [ "$NODE_ONLY" = true ]; then
    cp "${RELEASE_DIR}/tau5-node" "${APPDIR}/usr/bin/"
else
    cp "${RELEASE_DIR}/tau5" "${APPDIR}/usr/bin/" 2>/dev/null || true
    cp "${RELEASE_DIR}/tau5-node" "${APPDIR}/usr/bin/"
fi

# Copy Elixir release (needs to be relative to the binary location)
echo "Copying Elixir release..."
cp -r "${RELEASE_DIR}/_build" "${APPDIR}/usr/bin/"

# Create desktop file
cat > "${APPDIR}/usr/share/applications/tau5.desktop" << EOF
[Desktop Entry]
Name=Tau5
Comment=Collaborative live-coding platform for music and visuals
Exec=${BINARY_NAME}
Icon=tau5
Type=Application
Categories=AudioVideo;Audio;Music;Development;
Terminal=false
EOF

# Create/copy icon
if [ ! -f "${ROOT_DIR}/assets/tau5.png" ]; then
    echo "Creating placeholder icon..."
    printf '\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90wS\xde\x00\x00\x00\x0cIDATx\x9cc\xf8\x0f\x00\x00\x01\x01\x00\x05\xb8\xd5\x91\x00\x00\x00\x00IEND\xaeB`\x82' > "${APPDIR}/usr/share/icons/hicolor/256x256/apps/tau5.png"
else
    cp "${ROOT_DIR}/assets/tau5.png" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/tau5.png"
fi

# Create AppRun
echo "Creating AppRun launcher..."
cat > "${APPDIR}/AppRun" << 'EOF'
#!/bin/bash
set -e

# Get the directory where the AppImage is mounted
HERE="$(dirname "$(readlink -f "${0}")")"

export RELEASE_ROOT="${HERE}/usr/bin/_build/prod/rel/tau5"

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

# Deploy Qt and other dependencies
echo ""
echo "Deploying libraries and dependencies..."
# Use standard deploy for both node and GUI versions
# The tool will automatically detect and bundle required dependencies
"${APPIMAGETOOL_PATH}" deploy "${APPDIR}/usr/share/applications/tau5.desktop"

# Build AppImage
echo ""
echo "Building AppImage..."
cd "${ROOT_DIR}/release"
# Use basename since we're already in the release directory
APPDIR_NAME="$(basename ${APPDIR})"
# Export environment variables and run appimagetool
export VERSION="${VERSION}"
export ARCH="${GO_ARCH}"
"${APPIMAGETOOL_PATH}" "${APPDIR_NAME}"

# The tool creates its own filename, so we need to rename it
if [ "$NODE_ONLY" = true ]; then
    CREATED_NAME="Tau5-${VERSION}-${GO_ARCH}.AppImage"
else
    CREATED_NAME="Tau5-${VERSION}-${GO_ARCH}.AppImage"
fi

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
echo "Go-AppImage Build Complete!"
echo "========================================"
echo "AppImage: ${ROOT_DIR}/release/${APPIMAGE_NAME}"
echo "Size: ${APPIMAGE_SIZE}"
echo ""
echo "This AppImage includes ALL libraries for maximum compatibility"
echo "It should work on any Linux distribution with FUSE support"
echo ""
if [ "$NODE_ONLY" = true ]; then
    echo "Run with: ./${APPIMAGE_NAME}"
else
    echo "Run with: ./${APPIMAGE_NAME}          # GUI mode"
    echo "         ./${APPIMAGE_NAME} --node   # Node mode"
fi