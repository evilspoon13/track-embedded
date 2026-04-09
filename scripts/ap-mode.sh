#!/bin/bash
# Switch Pi to Access Point mode (disables WiFi client)
set -e

echo "=== Switching to AP mode ==="

# Disconnect from any WiFi network
sudo nmcli device disconnect wlan0 2>/dev/null || true

# Tell NetworkManager to leave wlan0 alone
sudo mkdir -p /etc/NetworkManager/conf.d
sudo tee /etc/NetworkManager/conf.d/unmanage-wlan0.conf > /dev/null <<EOF
[keyfile]
unmanaged-devices=interface-name:wlan0
EOF
sudo systemctl restart NetworkManager

sleep 2

# Set up static IP for AP
sudo ip addr flush dev wlan0
sudo ip addr add 192.168.4.1/24 dev wlan0
sudo ip link set wlan0 up

# Start AP services
sudo systemctl start hostapd
sudo systemctl start dnsmasq

echo ""
echo "AP mode active!"
echo "  SSID: TRACK-Setup"
echo "  IP:   192.168.4.1"
echo "  Portal: http://192.168.4.1/app/"
