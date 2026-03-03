#!/bin/bash
set -e

IFACE="${1:-vcan0}"

sudo modprobe vcan
sudo ip link add dev "$IFACE" type vcan 2>/dev/null && echo "Created $IFACE" || echo "$IFACE already exists"
sudo ip link set up "$IFACE"

echo "$IFACE is up"

# copy dbc config if missing
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ ! -f /tmp/display.dbc ] && [ -f "$SCRIPT_DIR/../config/display.dbc" ]; then
    cp "$SCRIPT_DIR/../config/display.dbc" /tmp/display.dbc
    echo "Copied display.dbc to /tmp"
fi

echo "Ready. Generate test traffic with: cangen $IFACE"
