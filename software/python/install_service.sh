#!/bin/bash
set -e

VENV_NAME="greymatter"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

USERNAME=$(whoami)
GROUPNAME=$(id -gn)
USER_HOME=$(eval echo ~"$USERNAME")
VENV_PIP="$USER_HOME/.virtualenvs/$VENV_NAME/bin/pip"

# Check virtualenv exists
if [ ! -f "$VENV_PIP" ]; then
    echo "Virtualenv '$VENV_NAME' not found. Create it first:"
    echo "  mkvirtualenv $VENV_NAME"
    exit 1
fi

echo "Installing greymatter into virtualenv '$VENV_NAME'"
"$VENV_PIP" install -e "$SCRIPT_DIR"'[server]'

echo "Installing systemd service for $USERNAME"
sed "s|%USERNAME%|$USERNAME|g; s|%GROUPNAME%|$GROUPNAME|g; s|%USER_HOME%|$USER_HOME|g" \
    "$SCRIPT_DIR/greymatter_server.service.template" > /tmp/greymatter_server.service

sudo mv /tmp/greymatter_server.service /etc/systemd/system/greymatter_server.service
sudo systemctl daemon-reload
sudo systemctl enable greymatter_server
sudo systemctl start greymatter_server
sudo systemctl status greymatter_server
