#!/bin/bash
# test-can.sh — send cycling CAN values to exercise all display widgets
#
# Usage: ./test-can.sh [interface] [delay_seconds]
#   interface     CAN interface to use (default: vcan0)
#   delay_seconds sleep between frames  (default: 0.05 = 20 Hz)
#
# CAN messages sent:
#   0x100  BMS_1  : state_of_charge (%), voltage (V)
#   0x300  BMS_2  : state_of_charge (%), voltage (V)   — 50% out of phase
#   0x200  Motor  : speed (mph), current (A), torque (Nm), power (kW)
#
# Signal encoding (little-endian, raw = (physical - offset) / scale):
#   BMS  state_of_charge : bits  0-15, unsigned, scale=0.1,  offset=0    → raw = pct * 10
#   BMS  voltage         : bits 16-31, signed,   scale=0.01, offset=-40  → raw = (V + 40) * 100
#   Motor speed          : bits  0-15, unsigned, scale=0.1,  offset=0    → raw = mph * 10
#   Motor current        : bits 16-31, signed,   scale=0.1,  offset=0    → raw = A * 10
#   Motor torque         : bits 32-47, unsigned, scale=0.1,  offset=0    → raw = Nm * 10
#   Motor power          : bits 48-63, unsigned, scale=0.01, offset=0    → raw = kW * 100

IFACE="${1:-vcan0}"
DELAY="${2:-0.05}"

# --- encoding helpers ---

# u16_le <raw>  →  4-char little-endian hex (LSB first)
u16_le() {
    printf "%02X%02X" $(($1 & 0xFF)) $((($1 >> 8) & 0xFF))
}

# s16_le <raw>  →  4-char little-endian hex, two's-complement for negatives
s16_le() {
    local r=$1
    [ $r -lt 0 ] && r=$((r + 65536))
    printf "%02X%02X" $((r & 0xFF)) $(((r >> 8) & 0xFF))
}

# --- frame builders ---

# bms_frame <soc_pct> <volts>
bms_frame() {
    printf "%s%s00000000" \
        "$(u16_le $(($1 * 10)))" \
        "$(s16_le $((($2 + 40) * 100)))"
}

# motor_frame <mph> <amps> <nm> <kw>
motor_frame() {
    printf "%s%s%s%s" \
        "$(u16_le $(($1 * 10)))" \
        "$(s16_le $(($2 * 10)))" \
        "$(u16_le $(($3 * 10)))" \
        "$(u16_le $(($4 * 100)))"
}

# --- main loop ---

echo "Sending test CAN data on $IFACE — Ctrl+C to stop"
echo "  0x100  BMS 1  : voltage + state_of_charge"
echo "  0x300  BMS 2  : voltage + state_of_charge  (50% out of phase)"
echo "  0x200  Motor  : speed, current, torque, power  (25% out of phase)"
echo ""

t=0
while true; do
    # Triangular wave helper: ramp 0→99→0 over 200 steps, offset by N steps
    tri() {
        local h=$((($t + $1) % 200))
        [ $h -lt 100 ] && echo $h || echo $((199 - h))
    }

    r0=$(tri 0)   # BMS 1 phase
    r1=$(tri 100) # BMS 2 phase  (half cycle behind)
    r2=$(tri 50)  # Motor phase  (quarter cycle behind)

    # BMS 1: voltage 0→280 V, SoC 0→99 %
    cansend "$IFACE" "100#$(bms_frame $r0 $((r0 * 280 / 99)))"

    # BMS 2: same ranges, different phase
    cansend "$IFACE" "300#$(bms_frame $r1 $((r1 * 280 / 99)))"

    # Motor: speed 0→99 mph, current 0→299 A, torque 0→198 Nm, power 0→79 kW
    cansend "$IFACE" "200#$(motor_frame $r2 $((r2 * 3)) $((r2 * 2)) $((r2 * 79 / 99)))"

    t=$((t + 1))
    sleep "$DELAY"
done
