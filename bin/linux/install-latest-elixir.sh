#!/bin/bash
set -e # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"

cleanup_function() {
    # Restore working directory as it was prior to this script
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

echo "Installing latest Elixir and Erlang via ASDF..."
echo ""
echo "NOTE: Run ./install-deps.sh first to install build dependencies"
echo ""

# Install or update ASDF
if [ ! -d "$HOME/.asdf" ]; then
    echo "Installing latest ASDF..."
    git clone https://github.com/asdf-vm/asdf.git ~/.asdf
    cd ~/.asdf
    git checkout "$(git describe --tags --abbrev=0)"
    cd -
    
    # Add to bashrc if not already there
    if ! grep -q "asdf.sh" ~/.bashrc; then
        echo '. "$HOME/.asdf/asdf.sh"' >> ~/.bashrc
    fi
    
    # Create completions directory and file to avoid warnings
    mkdir -p "$HOME/.asdf/completions"
    touch "$HOME/.asdf/completions/asdf.bash"
else
    echo "Updating ASDF to latest version..."
    cd ~/.asdf
    git fetch
    git checkout "$(git describe --tags --abbrev=0)"
    cd - > /dev/null
    
    # Ensure completions directory exists
    mkdir -p "$HOME/.asdf/completions"
    touch "$HOME/.asdf/completions/asdf.bash"
fi

# Source ASDF
source "$HOME/.asdf/asdf.sh"

# Add plugins if not already added
echo "Adding ASDF plugins..."
asdf plugin-add erlang https://github.com/asdf-vm/asdf-erlang.git 2>/dev/null || true
asdf plugin-add elixir https://github.com/asdf-vm/asdf-elixir.git 2>/dev/null || true

# Update plugins to get latest versions
echo "Updating plugins..."
asdf plugin-update erlang
asdf plugin-update elixir

# Get latest stable versions
echo "Fetching latest versions..."
LATEST_ERLANG=$(asdf list-all erlang | grep -E "^[0-9]+\.[0-9]+(\.[0-9]+)?$" | tail -1)
LATEST_ELIXIR=$(asdf list-all elixir | grep -E "^[0-9]+\.[0-9]+\.[0-9]+-otp-[0-9]+$" | tail -1)

echo "Latest Erlang: $LATEST_ERLANG"
echo "Latest Elixir: $LATEST_ELIXIR"

# Install Erlang
echo ""
echo "Installing Erlang $LATEST_ERLANG"
echo "This will compile from source and take 10-15 minutes..."
asdf install erlang $LATEST_ERLANG
asdf global erlang $LATEST_ERLANG

# Install Elixir
echo ""
echo "Installing Elixir $LATEST_ELIXIR..."
asdf install elixir $LATEST_ELIXIR
asdf global elixir $LATEST_ELIXIR

# Install Hex and Rebar
echo "Installing Hex and Rebar..."
source "$HOME/.asdf/asdf.sh"
mix local.hex --force
mix local.rebar --force

echo "---"
echo "    Installation complete!"
echo "    "
echo "    Erlang version: $(erl -eval 'erlang:display(erlang:system_info(version)), halt().' -noshell)"
echo "    Elixir version: $(elixir --version | head -n1)"
echo "    "
echo "    Please run: source ~/.bashrc"
echo "    Or log out and back in to ensure PATH is updated"
echo "---"