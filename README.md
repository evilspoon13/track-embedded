# T.R.A.C.K. — Telemetry Rendering And Capture Kit

Real-time driver display and telemetry system for Formula SAE EV teams. Runs on a Raspberry Pi 4 with a CAN bus interface, rendering configurable dashboard widgets at 60 FPS on a 7" HDMI display.

The system is split across two repos that live side by side:

- **track-embedded/** — C++ processes (CAN reader, graphics engine, data logger), Flask captive portal, systemd services
- **track-web/** — React/Vite frontend + Express/TypeScript backend (cloud portal)

The same React UI serves both the Pi (via Flask, no auth, local config files) and the cloud (via Express, Firebase auth, Firestore). Build-time feature flags control which features are included in each build.

## Architecture

```
[CAN Bus] ─► [can-reader] ─► (shared memory) ─┬─► [graphics-engine] ─► HDMI Display
[GPIO]    ─► [gpio-reader] ─►                 ├─► [data-logger] ─► SD Card
[GPS]     ─► [gps-reader]  ─►                 ├─► [cloud-bridge] ─► Cloud (WSS /ws/pi)
                                               └─► [captive-portal] ─► Web UI

[User's phone/laptop]
      │
      ├─ On Pi's AP ──► Flask (port 80) ──► static React build ──► /api/* ──► local JSON + shared memory
      │
      └─ On internet ──► Express (cloud) ──► static React build ──► /api/* ──► Firestore
```

## Prerequisites

- Raspberry Pi 4 with Raspberry Pi OS
- MCP2515 CAN controller + TJA1050 transceiver (or vcan for testing)
- Node.js 18+ and npm (for building the UI)

## First-Time Pi Setup

Run once on a fresh Pi:

```bash
# 1. Clone both repos side by side
git clone <track-embedded-url>
git clone <track-web-url>

# 2. Install system dependencies, build raylib, build C++ binaries
cd track-embedded
./scripts/setup-pi.sh

# 3. Build the React UI for the Pi (no auth, no Firebase)
./scripts/build-ui.sh

# 4. Deploy everything to /opt/track/ and enable systemd services
sudo ./scripts/deploy.sh

# 5. Configure networking (see setup-pi.sh output for details)
sudo cp config/hostapd.conf /etc/hostapd/hostapd.conf
sudo cp config/dnsmasq.conf /etc/dnsmasq.d/track.conf

# 6. Add to /boot/config.txt:
#   dtoverlay=vc4-kms-v3d
#   gpu_mem=128
#   dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25

# 7. Reboot — all services start automatically
sudo reboot
```

## Updating After Code Changes

After pulling new changes on the Pi:

```bash
cd track-embedded

# Rebuild C++ (if embedded code changed)
make clean && make PLATFORM=DRM

# Rebuild UI (if frontend code changed)
./scripts/build-ui.sh

# Redeploy and restart
sudo ./scripts/deploy.sh
sudo systemctl restart track-setup track-can-interface track-can-reader track-gps-reader track-gpio track-graphics track-logger track-cloud-bridge track-portal
```

## Selecting The CAN Interface

Systemd now reads the active CAN settings from `/opt/track/config/track.env`.

Default:

```bash
CAN_IFACE=can0
CAN_MODE=can
CAN_BITRATE=500000
```

For virtual CAN:

```bash
CAN_IFACE=vcan0
CAN_MODE=vcan
CAN_BITRATE=500000
```

Apply the change with:

```bash
sudo systemctl daemon-reload
sudo systemctl restart track-can-interface track-can-reader track-graphics track-logger track-cloud-bridge
```

## Scripts

| Script | Purpose |
|---|---|
| `scripts/setup-pi.sh` | One-time setup: apt packages, raylib, ixwebsocket, venv, initial build |
| `scripts/build-ui.sh` | Build React UI with Pi feature flags, output to `captive-portal/ui/` |
| `scripts/deploy.sh` | Copy binaries, config, and portal to `/opt/track/`, enable systemd services |
| `scripts/run.sh` | Run all processes in foreground for local dev/testing (no systemd) |
| `scripts/setup-vcan.sh` | Set up virtual CAN interface for testing without hardware |
| `scripts/test-can.sh` | Send test CAN frames for development |

## Boot Order (systemd)

```
track-setup (tmpfs dirs)
    ├─► track-gpio (GPIO inputs → shared memory, DRM builds)
    ├─► track-gps-reader (NMEA → shared memory)
    └─► track-can-interface (set CAN bitrate)
            └─► track-can-reader (CAN → shared memory)
                    ├─► track-graphics (shared memory → display)
                    ├─► track-logger (shared memory → disk)
                    ├─► track-cloud-bridge (shared memory → cloud)
                    └─► track-portal (Flask on port 80)
```
