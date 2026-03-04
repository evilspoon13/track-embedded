#!/bin/bash
set -e

echo "=== T.R.A.C.K. Pi Setup ==="

# system packages
echo "Installing system packages..."
sudo apt update && sudo apt install -y \
  build-essential \
  g++ \
  cmake \
  can-utils \
  libx11-dev \
  libxrandr-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxi-dev \
  libzstd-dev \
  libssl-dev \
  zlib1g-dev \
  nlohmann-json3-dev \
  libasound2-dev \
  libgl1-mesa-dev

# raylib
if [ -f /usr/local/include/raylib.h ]; then
    echo "raylib already installed, skipping."
else
    echo "Building raylib from source..."
    RAYLIB_DIR=$(mktemp -d)
    git clone --depth 1 https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
    mkdir -p "$RAYLIB_DIR/build" && cd "$RAYLIB_DIR/build"
    cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/usr/local ..
    make -j$(nproc)
    sudo make install
    rm -rf "$RAYLIB_DIR"
    echo "raylib installed."
fi

# ixwebsocket
if [ -f /usr/local/include/ixwebsocket/IXWebSocket.h ]; then
    echo "ixwebsocket already installed, skipping."
else
    echo "Building ixwebsocket from source..."
    IX_DIR=$(mktemp -d)
    git clone --depth 1 https://github.com/machinezone/IXWebSocket.git "$IX_DIR"
    mkdir -p "$IX_DIR/build" && cd "$IX_DIR/build"
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DUSE_TLS=1 ..
    make -j$(nproc)
    sudo make install
    rm -rf "$IX_DIR"
    echo "ixwebsocket installed."
fi

sudo ldconfig

# runtime directories and config files
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."

if [ ! -f /tmp/display.dbc ] && [ -f "$ROOT/config/display.dbc" ]; then
    cp "$ROOT/config/display.dbc" /tmp/display.dbc
    echo "Copied display.dbc to /tmp"
fi

mkdir -p /tmp/track-logs

# virtual CAN for testing (skip if real CAN is available)
if ! ip link show can0 &>/dev/null; then
    echo "No real CAN interface found, setting up vcan0 for testing..."
    sudo modprobe vcan
    sudo ip link add dev vcan0 type vcan 2>/dev/null || true
    sudo ip link set up vcan0
    echo "vcan0 is up. Generate test traffic with: cangen vcan0"
fi

# build
echo "Building track-embedded..."
cd "$ROOT"
make clean && make

echo ""
echo "=== Setup complete ==="
echo "Run with: ./scripts/run.sh"
