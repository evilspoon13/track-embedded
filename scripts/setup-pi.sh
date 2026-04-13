#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."

echo "=== T.R.A.C.K. Pi Setup ==="

# system packages
echo "Installing system packages..."
sudo apt update && sudo apt install -y \
    build-essential \
    g++ \
    git \
    cmake \
    python3-venv \
    can-utils \
    libzstd-dev \
    libssl-dev \
    zlib1g-dev \
    nlohmann-json3-dev \
    libasound2-dev \
    libdrm-dev \
    libgbm-dev \
    libegl-dev \
    libgles2-mesa-dev \
    libgpiod-dev \
    hostapd \
    dnsmasq

# raylib (built for DRM/framebuffer, not X11)
if [ -f /usr/local/include/raylib.h ]; then
    echo "raylib already installed, skipping."
else
    echo "Building raylib from source (DRM platform)..."
    RAYLIB_DIR=$(mktemp -d)
    (
        git clone --depth 1 https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
        mkdir -p "$RAYLIB_DIR/build" && cd "$RAYLIB_DIR/build"
        cmake -DPLATFORM=DRM -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/usr/local ..
        make -j$(nproc)
        sudo make install
    )
    rm -rf "$RAYLIB_DIR"
    echo "raylib installed (DRM)."
fi

# ixwebsocket
if [ -f /usr/local/include/ixwebsocket/IXWebSocket.h ]; then
    echo "ixwebsocket already installed, skipping."
else
    echo "Building ixwebsocket from source..."
    IX_DIR=$(mktemp -d)
    (
        git clone --depth 1 https://github.com/machinezone/IXWebSocket.git "$IX_DIR"
        mkdir -p "$IX_DIR/build" && cd "$IX_DIR/build"
        cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DUSE_TLS=1 ..
        make -j$(nproc)
        sudo make install
    )
    rm -rf "$IX_DIR"
    echo "ixwebsocket installed."
fi

sudo ldconfig

# captive portal python venv + flask
echo "Setting up captive portal virtualenv..."
python3 -m venv "$ROOT/captive-portal/venv"
"$ROOT/captive-portal/venv/bin/pip" install --upgrade pip
"$ROOT/captive-portal/venv/bin/pip" install -r "$ROOT/captive-portal/requirements.txt"

# runtime directories and config files
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

# build for DRM (headless Pi)
echo "Building track-embedded (PLATFORM=DRM)..."
cd "$ROOT"
make clean && make PLATFORM=DRM

echo ""
echo "=== Setup complete ==="
echo ""
echo "Next steps:"
echo "  1. Add to /boot/config.txt (if not already present):"
echo "       dtoverlay=vc4-kms-v3d"
echo "       gpu_mem=128"
echo "       dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25"
echo "  2. Configure hostapd: sudo cp $ROOT/config/hostapd.conf /etc/hostapd/hostapd.conf"
echo "  3. Configure dnsmasq: sudo cp $ROOT/config/dnsmasq.conf /etc/dnsmasq.d/track.conf"
echo "  4. Deploy:  sudo $ROOT/scripts/deploy.sh"
echo "  5. Reboot"
