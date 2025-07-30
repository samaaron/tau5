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

# Function to install packages on Debian/Ubuntu
install_debian_packages() {
    echo "Installing packages for Debian/Ubuntu..."
    
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        qt6-base-dev \
        qt6-webengine-dev \
        qt6-websockets-dev \
        qt6-svg-dev \
        qt6-tools-dev \
        qt6-tools-dev-tools \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libasound2-dev \
        erlang \
        erlang-dev \
        elixir
}

# Function to install packages on RHEL/Fedora/CentOS
install_rhel_packages() {
    echo "Installing packages for RHEL/Fedora/CentOS..."
    
    if command_exists dnf; then
        sudo dnf install -y \
            gcc \
            gcc-c++ \
            cmake \
            git \
            qt6-qtbase-devel \
            qt6-qtwebengine-devel \
            qt6-qtwebsockets-devel \
            qt6-qtsvg-devel \
            qt6-qttools-devel \
            mesa-libGL-devel \
            mesa-libGLU-devel \
            alsa-lib-devel \
            erlang \
            erlang-devel \
            elixir
    elif command_exists yum; then
        sudo yum install -y \
            gcc \
            gcc-c++ \
            cmake \
            git \
            qt6-qtbase-devel \
            qt6-qtwebengine-devel \
            qt6-qtwebsockets-devel \
            qt6-qtsvg-devel \
            qt6-qttools-devel \
            mesa-libGL-devel \
            mesa-libGLU-devel \
            alsa-lib-devel \
            erlang \
            erlang-devel \
            elixir
    else
        echo "ERROR: Neither dnf nor yum found. Cannot install packages."
        exit 1
    fi
}

# Function to install packages on Arch Linux
install_arch_packages() {
    echo "Installing packages for Arch Linux..."
    
    sudo pacman -Syu --noconfirm
    sudo pacman -S --needed --noconfirm \
        base-devel \
        cmake \
        git \
        qt6-base \
        qt6-webengine \
        qt6-websockets \
        qt6-svg \
        qt6-tools \
        mesa \
        glu \
        alsa-lib \
        erlang \
        elixir
}

# Function to install packages on openSUSE
install_opensuse_packages() {
    echo "Installing packages for openSUSE..."
    
    sudo zypper refresh
    sudo zypper install -y \
        gcc \
        gcc-c++ \
        cmake \
        git \
        qt6-base-devel \
        qt6-webengine-devel \
        qt6-websockets-devel \
        qt6-svg-devel \
        qt6-tools \
        Mesa-libGL-devel \
        glu-devel \
        alsa-devel \
        erlang \
        elixir
}

# Main installation
echo "Tau5 Linux Dependency Installer"
echo "================================"

# Detect distribution
detect_distro

# Install packages based on distribution
case "$DISTRO" in
    ubuntu|debian|linuxmint|pop)
        install_debian_packages
        ;;
    fedora|rhel|centos|rocky|almalinux)
        install_rhel_packages
        ;;
    arch|manjaro|endeavouros)
        install_arch_packages
        ;;
    opensuse*|suse*)
        install_opensuse_packages
        ;;
    *)
        echo "ERROR: Unsupported distribution: $DISTRO"
        echo "Please install the following dependencies manually:"
        echo "- Qt6 (Core, Widgets, WebEngine, WebSockets, Svg)"
        echo "- CMake"
        echo "- C++ compiler (gcc or clang)"
        echo "- OpenGL development libraries (GL and GLU)"
        echo "- ALSA development libraries"
        echo "- Erlang/OTP 27+"
        echo "- Elixir 1.18+"
        exit 1
        ;;
esac

# Verify installations
echo ""
echo "Verifying installations..."

# Check for essential commands
required_commands=("cmake" "gcc" "g++" "git")
missing_commands=()

for cmd in "${required_commands[@]}"; do
    if ! command_exists "$cmd"; then
        missing_commands+=("$cmd")
    fi
done

if [ ${#missing_commands[@]} -ne 0 ]; then
    echo "ERROR: The following required commands are missing: ${missing_commands[*]}"
    exit 1
fi

# Check Qt6
if command_exists qmake6; then
    QT_VERSION=$(qmake6 -query QT_VERSION)
    echo "Qt6 version $QT_VERSION found"
else
    echo "WARNING: Qt6 not found in PATH. You may need to set QT_INSTALL_LOCATION environment variable."
    echo "Example: export QT_INSTALL_LOCATION=/path/to/qt6"
fi

# Check Erlang/Elixir
if command_exists erl; then
    ERL_VERSION=$(erl -eval 'erlang:display(erlang:system_info(otp_release)), halt().' -noshell | tr -d '"')
    echo "Erlang/OTP version $ERL_VERSION found"
else
    echo "ERROR: Erlang not found. Please install Erlang manually."
    exit 1
fi

if command_exists elixir; then
    ELIXIR_VERSION=$(elixir --version | grep Elixir | awk '{print $2}')
    echo "Elixir version $ELIXIR_VERSION found"
else
    echo "ERROR: Elixir not found. Please install Elixir manually."
    exit 1
fi

echo ""
echo "All dependencies have been installed successfully!"
echo ""
echo "Next steps:"
echo "1. If Qt6 was not found in PATH, set QT_INSTALL_LOCATION environment variable"
echo "2. Run ./build-all.sh to build Tau5"
echo ""

# Check for ARM architecture and print additional info
if [[ $(uname -m) == "arm"* ]] || [[ $(uname -m) == "aarch64" ]]; then
    echo "ARM architecture detected. You may need to install additional atomic libraries if build fails."
    echo "On Debian/Ubuntu: sudo apt-get install libatomic1"
    echo "On RHEL/Fedora: sudo dnf install libatomic"
fi