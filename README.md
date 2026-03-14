# T.R.A.C.K. — Telemetry Rendering And Capture Kit

A low-cost, configurable real-time driver display and telemetry logging system for Formula SAE Electric. Built on embedded Linux (Raspberry Pi 4), targeting 60 FPS rendering with <50 ms CAN-to-screen latency.

## Team

| Member | Role |
|---|---|
| Jack Williams | Team Lead, CAN Development |
| Cameron Stone | CAN Development |
| Justin Busker | Graphics Engine |
| Alonso Peralta Espinoza | Web App Frontend |
| Brayden Bailey | Web App Backend |
| Campbell Wright | Hardware / PCB Design |

Texas A&M University — CSCE 483

## Architecture

```
[CAN Bus] ─► [can-reader] ─► (shared memory) ─┬─► [graphics-engine] ─► HDMI Display
                                                └─► [data-logger] ─► SD Card

[web-app backend] ◄─► [web-app frontend]
       │
       ▼
  config JSON ──► SIGHUP ──► processes reload
```

Multi-process pipeline with shared memory IPC. Each process is isolated — a crash in one does not affect the others.

## Repository Structure

```
track-embedded/
├── can-reader/          # CAN frame ingestion via SocketCAN
├── graphics-engine/     # Real-time display rendering
├── data-logger/         # Telemetry logging and compression
├── common/              # Shared C++ headers (queue, config, IPC)
├── web-server/
│   ├── backend/         # TypeScript + Express — config API
│   └── frontend/        # TypeScript + React — configuration UI
├── config/              # JSON configuration files
├── scripts/             # Build and tooling scripts
└── systemd/             # Service files for deployment
```

## Modules

### can-reader
Reads CAN frames via SocketCAN, decodes signals per configuration, and publishes to shared memory. Supports hot-reload via SIGHUP.

### graphics-engine
Consumes telemetry from shared memory and renders the driver display at 60 FPS. Supports configurable widget layouts and multiple screens.

### data-logger
Consumes telemetry from shared memory, batches writes, and logs to disk with compression. Exports in standard formats for post-run analysis.

### common
Shared C++ headers: broadcast queue, shared memory helpers, telemetry message types, and configuration parsing.

### web-app
- **Backend** (TypeScript / Express): Config API — manages channel mappings, widget layouts, and alert thresholds. Writes config and signals processes to reload.
- **Frontend** (TypeScript / React): Configuration UI — drag-and-drop layout editor, CAN channel setup, alert configuration. Accessible from any device on the network.

## Hardware

- Raspberry Pi 4
- MCP2515 CAN controller + TJA1050 transceiver
- Custom PCB (power regulation, button inputs, status LEDs)
- 7" HDMI display

## Performance Targets

| Metric | Target |
|---|---|
| Rendering | 60 FPS stable |
| CAN-to-screen latency | < 50 ms |
| Continuous runtime | 30+ minutes without degradation |
| Unit cost | < $500 |

## Build

```bash
./scripts/build.sh              # Build all C++ modules
./scripts/clean.sh              # Clean all builds
./scripts/gen-compile-commands.sh  # Generate compile_commands.json for clangd
```

## Deployment

Services deploy to `/opt/track/` and are managed via systemd. Graphics and logger depend on can-reader being up first.

## License

Internal project — Texas A&M Formula SAE Electric
