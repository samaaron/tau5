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

install_debian_packages() {
    echo "Installing minimal packages for tau5-node on Debian/Ubuntu..."
    
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        qt6-base-dev \
        libgl1-mesa-dev \
        autoconf \
        m4 \
        libncurses-dev \
        libssl-dev \
        libssh-dev \
        unixodbc-dev \
        xsltproc \
        fop \
        libxml2-utils \
        libxml2-dev \
        libxslt1-dev \
        curl \
        wget \
        inotify-tools \
        golang-go
    
    if apt-cache show libwxgtk3.2-dev &>/dev/null; then
        sudo apt-get install -y libwxgtk3.2-dev
    elif apt-cache show libwxgtk3.0-gtk3-dev &>/dev/null; then
        sudo apt-get install -y libwxgtk3.0-gtk3-dev
    fi
    
    if [[ $(uname -m) == "arm"* ]] || [[ $(uname -m) == "aarch64" ]]; then
        sudo apt-get install -y libatomic1
    fi
}

install_rhel_packages() {
    echo "Installing minimal packages for tau5-node on RHEL/Fedora/CentOS..."
    
    if command_exists dnf; then
        sudo dnf install -y \
            gcc \
            gcc-c++ \
            cmake \
            git \
            qt6-qtbase-devel \
            mesa-libGL-devel \
            erlang \
            erlang-devel \
            elixir \
            inotify-tools \
            golang
            
        if [[ $(uname -m) == "arm"* ]] || [[ $(uname -m) == "aarch64" ]]; then
            sudo dnf install -y libatomic
        fi
    elif command_exists yum; then
        sudo yum install -y \
            gcc \
            gcc-c++ \
            cmake \
            git \
            qt6-qtbase-devel \
            mesa-libGL-devel \
            erlang \
            erlang-devel \
            elixir \
            inotify-tools \
            golang
            
        if [[ $(uname -m) == "arm"* ]] || [[ $(uname -m) == "aarch64" ]]; then
            sudo yum install -y libatomic
        fi
    else
        echo "ERROR: Neither dnf nor yum found. Cannot install packages."
        exit 1
    fi
}

install_arch_packages() {
    echo "Installing minimal packages for tau5-node on Arch Linux..."
    
    sudo pacman -Syu --noconfirm
    sudo pacman -S --needed --noconfirm \
        base-devel \
        cmake \
        git \
        qt6-base \
        mesa \
        erlang \
        elixir \
        inotify-tools \
        go
}

install_opensuse_packages() {
    echo "Installing minimal packages for tau5-node on openSUSE..."
    
    sudo zypper refresh
    sudo zypper install -y \
        gcc \
        gcc-c++ \
        cmake \
        git \
        qt6-base-devel \
        Mesa-libGL-devel \
        erlang \
        elixir \
        inotify-tools \
        go
}

echo "Tau5 Node Dependency Installer"
echo "==============================="

# Detect distribution
detect_distro

# Install packages based on distribution
case "$DISTRO" in
    ubuntu|debian|linuxmint|pop|raspbian)
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
        echo "- Qt6 Core and Network libraries (qt6-base-dev)"
        echo "- CMake"
        echo "- C++ compiler (gcc or clang)"
        echo "- OpenGL development libraries (for Qt dependencies)"
        echo "- Erlang/OTP 27+"
        echo "- Elixir 1.18+"
        echo "- Go (for MCP servers)"
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
    
    echo "Checking for required Qt modules..."
    pkg-config --exists Qt6Core 2>/dev/null && echo "  ✓ Qt6Core found" || echo "  ✗ Qt6Core not found"
    pkg-config --exists Qt6Network 2>/dev/null && echo "  ✓ Qt6Network found" || echo "  ✗ Qt6Network not found"
else
    echo "WARNING: Qt6 not found in PATH. You may need to set QT_INSTALL_LOCATION environment variable."
    echo "Example: export QT_INSTALL_LOCATION=/path/to/qt6"
fi

# Check Erlang/Elixir
if command_exists erl; then
    ERL_VERSION=$(erl -eval 'erlang:display(erlang:system_info(otp_release)), halt().' -noshell | tr -d '"')
    echo "Erlang/OTP version $ERL_VERSION found"
    
    if [ "$ERL_VERSION" -ge 27 ] 2>/dev/null; then
        echo "  ✓ Erlang version meets requirements (27+)"
    else
        echo "  ⚠ WARNING: Erlang version $ERL_VERSION may be too old. Version 27+ recommended."
    fi
else
    echo "ERROR: Erlang not found. Please install Erlang manually."
    echo "Visit: https://www.erlang.org/downloads"
    exit 1
fi

if command_exists elixir; then
    ELIXIR_VERSION=$(elixir --version | grep Elixir | awk '{print $2}')
    echo "Elixir version $ELIXIR_VERSION found"
else
    echo "ERROR: Elixir not found. Please install Elixir manually."
    echo "Visit: https://elixir-lang.org/install.html"
    exit 1
fi

if command_exists go; then
    GO_VERSION=$(go version | awk '{print $3}')
    echo "Go version $GO_VERSION found"
else
    echo "WARNING: Go not found. MCP servers won't be built."
    echo "Install Go from: https://golang.org/dl/"
fi

echo ""
echo "Node dependencies installed successfully!"
echo ""
echo "Next steps:"
echo "1. If Qt6 was not found in PATH, set QT_INSTALL_LOCATION environment variable"
echo "2. Run ./bin/linux/build-server.sh to build the Elixir server"
echo "3. Run ./bin/linux/build-node.sh to build tau5-node"
echo ""
echo "To install full GUI dependencies, run ./bin/linux/install-gui-deps.sh"