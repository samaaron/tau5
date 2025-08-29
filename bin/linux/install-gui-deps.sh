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

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to detect the Linux distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        DISTRO_VERSION=$VERSION_ID
    elif [ -f /etc/debian_version ]; then
        DISTRO="debian"
    elif [ -f /etc/redhat-release ]; then
        DISTRO="rhel"
    else
        DISTRO="unknown"
    fi
    
    echo "Detected distribution: $DISTRO"
}

install_debian_gui_packages() {
    echo "Installing GUI-specific packages for Debian/Ubuntu..."
    
    sudo apt-get update
    sudo apt-get install -y \
        qt6-webengine-dev \
        qt6-websockets-dev \
        qt6-svg-dev \
        qt6-tools-dev \
        qt6-tools-dev-tools \
        libglu1-mesa-dev \
        libasound2-dev \
        libpng-dev
}

install_rhel_gui_packages() {
    echo "Installing GUI-specific packages for RHEL/Fedora/CentOS..."
    
    if command_exists dnf; then
        sudo dnf install -y \
            qt6-qtwebengine-devel \
            qt6-qtwebsockets-devel \
            qt6-qtsvg-devel \
            qt6-qttools-devel \
            mesa-libGLU-devel \
            alsa-lib-devel
    elif command_exists yum; then
        sudo yum install -y \
            qt6-qtwebengine-devel \
            qt6-qtwebsockets-devel \
            qt6-qtsvg-devel \
            qt6-qttools-devel \
            mesa-libGLU-devel \
            alsa-lib-devel
    else
        echo "ERROR: Neither dnf nor yum found. Cannot install packages."
        exit 1
    fi
}

install_arch_gui_packages() {
    echo "Installing GUI-specific packages for Arch Linux..."
    
    sudo pacman -S --needed --noconfirm \
        qt6-webengine \
        qt6-websockets \
        qt6-svg \
        qt6-tools \
        glu \
        alsa-lib
}

install_opensuse_gui_packages() {
    echo "Installing GUI-specific packages for openSUSE..."
    
    sudo zypper install -y \
        qt6-webengine-devel \
        qt6-websockets-devel \
        qt6-svg-devel \
        qt6-tools \
        glu-devel \
        alsa-devel
}

echo "Tau5 GUI Dependency Installer"
echo "=============================="

echo "Step 1: Installing base/node dependencies..."
echo "============================================="
"${SCRIPT_DIR}/install-node-deps.sh"

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install node dependencies"
    exit 1
fi

echo ""
echo "Step 2: Installing GUI-specific dependencies..."
echo "================================================"

# Detect distribution
detect_distro

# Install GUI-specific packages based on distribution
case "$DISTRO" in
    ubuntu|debian|linuxmint|pop|raspbian)
        install_debian_gui_packages
        ;;
    fedora|rhel|centos|rocky|almalinux)
        install_rhel_gui_packages
        ;;
    arch|manjaro|endeavouros)
        install_arch_gui_packages
        ;;
    opensuse*|suse*)
        install_opensuse_gui_packages
        ;;
    *)
        echo "ERROR: Unsupported distribution: $DISTRO"
        echo "Please install the following GUI dependencies manually:"
        echo "- Qt6 WebEngine"
        echo "- Qt6 WebSockets"
        echo "- Qt6 SVG and Widgets"
        echo "- Qt6 Tools"
        echo "- GLU (OpenGL Utility Library)"
        echo "- ALSA development libraries"
        exit 1
        ;;
esac

echo ""
echo "Verifying GUI-specific Qt modules..."

pkg-config --exists Qt6WebEngineWidgets 2>/dev/null && echo "  ✓ Qt6WebEngine found" || echo "  ✗ Qt6WebEngine not found"
pkg-config --exists Qt6WebSockets 2>/dev/null && echo "  ✓ Qt6WebSockets found" || echo "  ✗ Qt6WebSockets not found" 
pkg-config --exists Qt6Svg 2>/dev/null && echo "  ✓ Qt6Svg found" || echo "  ✗ Qt6Svg not found"
pkg-config --exists Qt6Widgets 2>/dev/null && echo "  ✓ Qt6Widgets found" || echo "  ✗ Qt6Widgets not found"
pkg-config --exists alsa 2>/dev/null && echo "  ✓ ALSA libraries found" || echo "  ⚠ ALSA libraries not found"

echo ""
echo "GUI dependencies installed successfully!"
echo ""
echo "Next steps:"
echo "1. Run ./bin/linux/build-all.sh to build everything"
echo "   OR"
echo "2. Build components individually:"
echo "   - ./bin/linux/build-server.sh"
echo "   - ./bin/linux/build-gui.sh"
echo "   - ./bin/linux/build-node.sh"