#!/bin/bash
set -e # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
SERVER_DIR="${ROOT_DIR}/server"

cleanup_function() {
    # Restore working directory as it was prior to this script
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

# Check if mix is available
if ! command -v mix &> /dev/null; then
    echo "mix command not found. Ensure Elixir is installed and mix is in PATH."
    exit 1
fi

# Get current user
CURRENT_USER=${SUDO_USER:-$USER}
CURRENT_GROUP=$(id -gn $CURRENT_USER)

# Generate secret if not provided
if [ -z "$SECRET_KEY_BASE" ]; then
    echo "Generating SECRET_KEY_BASE..."
    cd "$SERVER_DIR"
    SECRET_KEY_BASE=$(mix phx.gen.secret)
    echo "Generated new secret key"
fi

# Create service file content
SERVICE_FILE="/tmp/tau5.service"
cat > "$SERVICE_FILE" << EOF
[Unit]
Description=Tau5 Central Server
After=network.target

[Service]
Type=simple
User=$CURRENT_USER
Group=$CURRENT_GROUP
WorkingDirectory=$(realpath "$SERVER_DIR")
Environment="MIX_ENV=prod"
Environment="TAU5_MODE=central"
Environment="PHX_HOST=tau5.live"
Environment="PORT=4000"
Environment="PHX_SERVER=true"
Environment="SECRET_KEY_BASE=$SECRET_KEY_BASE"
ExecStart=$(realpath "$SERVER_DIR")/_build/prod/rel/tau5/bin/tau5 start
ExecStop=$(realpath "$SERVER_DIR")/_build/prod/rel/tau5/bin/tau5 stop
KillMode=mixed
KillSignal=SIGTERM
TimeoutStopSec=10
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

echo "---"
echo "    Systemd service file generated at: ${SERVICE_FILE}"
echo "    "
echo "    To install the service, run:"
echo "      sudo cp $SERVICE_FILE /etc/systemd/system/tau5.service"
echo "      sudo systemctl daemon-reload"
echo "      sudo systemctl enable tau5"
echo "      sudo systemctl start tau5"
echo "    "
echo "    To check service status:"
echo "      sudo systemctl status tau5"
echo "    "
echo "    To view logs:"
echo "      sudo journalctl -u tau5 -f"
echo "    "
echo "    IMPORTANT: Save this SECRET_KEY_BASE securely:"
echo "    $SECRET_KEY_BASE"
echo "---"