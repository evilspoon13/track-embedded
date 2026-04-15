#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."

echo "=== Deploying T.R.A.C.K. to /opt/track/ ==="

# create directory structure
sudo mkdir -p /opt/track/config
sudo mkdir -p /opt/track/captive-portal
sudo mkdir -p /opt/track/graphics-engine/assets
sudo mkdir -p /opt/track/scripts

# copy binaries
echo "Copying binaries..."
sudo cp "$ROOT/can-reader/can-reader"         /opt/track/can-reader
sudo cp "$ROOT/data-logger/data-logger"       /opt/track/data-logger
sudo cp "$ROOT/graphics-engine/graphics-engine" /opt/track/graphics-engine/graphics-engine
sudo cp "$ROOT/cloud-bridge/cloud-bridge"     /opt/track/cloud-bridge
sudo cp "$ROOT/gps-reader/gps-reader"         /opt/track/gps-reader
if [ -f "$ROOT/gpio-reader/gpio-reader" ]; then
    sudo cp "$ROOT/gpio-reader/gpio-reader"   /opt/track/gpio-reader
else
    echo "  Skipping gpio-reader (not built — requires PLATFORM=DRM)"
fi

# copy graphics engine assets (fonts etc)
if [ -d "$ROOT/graphics-engine/assets" ]; then
    sudo cp -r "$ROOT/graphics-engine/assets/"* /opt/track/graphics-engine/assets/
fi

# copy config files
echo "Copying config..."
for cfg in graphics.json display.dbc track.env; do
    if [ ! -f "/opt/track/config/$cfg" ]; then
        sudo cp "$ROOT/config/$cfg" "/opt/track/config/$cfg"
    else
        echo "  Skipping $cfg (already exists)"
    fi
done

# copy captive portal
echo "Copying captive portal..."
sudo cp "$ROOT/captive-portal/server.py"  /opt/track/captive-portal/
sudo cp "$ROOT/captive-portal/wifi.py"    /opt/track/captive-portal/
sudo cp "$ROOT/captive-portal/shm_reader.py" /opt/track/captive-portal/
sudo cp "$ROOT/captive-portal/requirements.txt" /opt/track/captive-portal/
sudo cp -r "$ROOT/captive-portal/templates" /opt/track/captive-portal/
sudo cp -r "$ROOT/captive-portal/static"   /opt/track/captive-portal/

# copy SPA build if it exists
if [ -d "$ROOT/captive-portal/ui" ]; then
    sudo cp -r "$ROOT/captive-portal/ui" /opt/track/captive-portal/
fi

# set up venv and install/update dependencies
if [ ! -d /opt/track/captive-portal/venv ]; then
    echo "Creating captive portal venv..."
    sudo python3 -m venv /opt/track/captive-portal/venv
fi
echo "Installing captive portal dependencies..."
sudo /opt/track/captive-portal/venv/bin/pip install --upgrade pip
sudo /opt/track/captive-portal/venv/bin/pip install -r /opt/track/captive-portal/requirements.txt

# copy operator scripts (invoked by gpio-reader, etc.)
echo "Copying scripts..."
for s in wifi-mode.sh ap-mode.sh; do
    sudo cp "$SCRIPT_DIR/$s" "/opt/track/scripts/$s"
    sudo chmod +x "/opt/track/scripts/$s"
done

# install and enable systemd services
echo "Installing systemd services..."
sudo cp "$ROOT/systemd/"*.service /etc/systemd/system/
sudo systemctl daemon-reload

sudo systemctl enable \
    track-setup \
    track-can-interface \
    track-can-reader \
    track-graphics \
    track-logger \
    track-cloud-bridge \
    track-gps-reader \
    track-gpio \
    track-portal

echo ""
echo "=== Deploy complete ==="
echo "Services enabled. They will start on next boot."
echo "To start now: sudo systemctl start track-setup track-can-interface track-can-reader track-gps-reader track-graphics track-logger track-portal track-cloud-bridge"
