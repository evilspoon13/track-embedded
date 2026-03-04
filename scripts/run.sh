#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."

# always sync dbc config so can-reader picks up any changes
if [ -f "$ROOT/config/display.dbc" ]; then
    cp "$ROOT/config/display.dbc" /tmp/display.dbc
    echo "Copied display.dbc to /tmp"
fi

CAN_IFACE="${1:-can0}"
CONFIG_PATH="${2:-$ROOT/config/graphics.json}"
PIDS=()

cleanup() {
    echo "Stopping all processes..."
    for pid in "${PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    wait
    echo "Done."
}
trap cleanup EXIT INT TERM

# 1. can-reader first - it creates the shared memory queue
echo "Starting can-reader..."
"$ROOT/can-reader/can-reader" "$CAN_IFACE" &
PIDS+=($!)
sleep 0.5

# 2. data-logger - reads from queue, writes logs
echo "Starting data-logger..."
"$ROOT/data-logger/data-logger" &
PIDS+=($!)

# 3. graphics-engine - reads from queue, renders display
echo "Starting graphics-engine with config: $CONFIG_PATH"
"$ROOT/graphics-engine/graphics-engine" "$CONFIG_PATH" &
PIDS+=($!)

# 4. cloud-bridge - reads from queue, streams to cloud
echo "Starting cloud-bridge..."
"$ROOT/cloud-bridge/cloud-bridge" &
PIDS+=($!)

echo "All processes running. Press Ctrl+C to stop."
wait
