#!/bin/bash
set -e # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"

cleanup_function() {
    # Restore working directory as it was prior to this script running on exit
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

echo "Tau5 Central Server Dependency Installer (Ubuntu/Debian)"
echo "========================================================="
echo ""

# Check if running with sudo/root
if [ "$EUID" -ne 0 ]; then 
    echo "This script needs to install system packages."
    echo "Please run with sudo: sudo $0"
    exit 1
fi

# Update package list
echo "Updating package list..."
apt-get update

# Install minimal dependencies for central server
echo "Installing build dependencies..."
apt-get install -y \
    build-essential \
    autoconf \
    m4 \
    libncurses-dev \
    libssl-dev \
    libssh-dev \
    unixodbc-dev \
    xsltproc \
    curl \
    wget \
    git \
    inotify-tools

# Install Go for ASDF 0.16+ (optional but fixes warnings)
echo "Installing Go (for ASDF 0.16+)..."
apt-get install -y golang-go

# Install nginx and certbot for web serving
echo "Installing nginx and certbot..."
apt-get install -y \
    nginx \
    certbot \
    python3-certbot-nginx

# Install useful server management tools
echo "Installing server tools..."
apt-get install -y \
    htop \
    ufw \
    fail2ban \
    unattended-upgrades

echo ""
echo "---"
echo "    All central server dependencies installed successfully!"
echo "    "
echo "    Next steps:"
echo "    1. Run ./install-latest-elixir.sh (as regular user, not root)"
echo "    2. Clone/pull the Tau5 repository"
echo "    3. Run ./central-deploy.sh prod to build"
echo "    4. Run ./central-gen-systemd-service.sh to create service"
echo "    5. Configure nginx for your domain"
echo "---"