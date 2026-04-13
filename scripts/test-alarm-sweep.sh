#!/usr/bin/env bash
# Sweeps BMS_1.state_of_charge (CAN 0x100, bytes 0-1 LE, scale 0.1)
# from 0..100 and back on vcan0 so you can watch a gauge/bar climb
# through its thresholds and trigger the alarm border.
#
# Usage: ./test-alarm-sweep.sh [iface] [step] [delay_sec]
#   iface     CAN interface (default: vcan0)
#   step      display-unit step per tick (default: 2)
#   delay_sec seconds between frames (default: 0.1)

set -euo pipefail

IFACE="${1:-vcan0}"
STEP="${2:-2}"
DELAY="${3:-0.1}"

if ! ip link show "$IFACE" >/dev/null 2>&1; then
  echo "error: $IFACE not found. Bring it up first, e.g.:"
  echo "  sudo modprobe vcan && sudo ip link add dev $IFACE type vcan && sudo ip link set up $IFACE"
  exit 1
fi

trap 'echo; echo "stopped."; exit 0' INT

# Build an 8-byte payload with raw uint16 LE in bytes 0-1, zeros elsewhere.
# raw = display_value / 0.1  (state_of_charge scale).
send_soc() {
  local display="$1"
  local raw lo hi
  raw=$(awk -v v="$display" 'BEGIN{ printf "%d", v*10 + 0.5 }')
  lo=$(( raw & 0xFF ))
  hi=$(( (raw >> 8) & 0xFF ))
  cansend "$IFACE" 100#$(printf '%02X%02X000000000000' "$lo" "$hi")
}

echo "sweeping 0x100 state_of_charge on $IFACE (Ctrl-C to stop)"
while true; do
  for ((v=0; v<=100; v+=STEP));   do send_soc "$v"; sleep "$DELAY"; done
  for ((v=100; v>=0; v-=STEP));   do send_soc "$v"; sleep "$DELAY"; done
done
