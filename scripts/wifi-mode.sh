#!/bin/bash
# Switch Pi to WiFi client mode (disables AP)
set -e

echo "=== Switching to WiFi mode ==="

sudo systemctl stop hostapd dnsmasq 2>/dev/null || true
sudo ip addr flush dev wlan0 2>/dev/null || true

# Ensure NetworkManager manages wlan0
sudo rm -f /etc/NetworkManager/conf.d/unmanage-wlan0.conf
sudo systemctl restart NetworkManager

echo "Waiting for NetworkManager..."
sleep 3

sudo nmcli device wifi rescan 2>/dev/null || true
sleep 2

echo ""
echo "Available networks:"
sudo nmcli device wifi list

echo ""
echo "Connect with: sudo nmcli device wifi connect \"NETWORK_NAME\""
echo "Then check IP: hostname -I"
