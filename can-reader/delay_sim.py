#!/usr/bin/env python3
"""
delay_sim.py    Delay-probe responder (simulation node)

Runs on a second Raspberry Pi connected to the TRACK node over CAN.
On receipt of a probe frame (ID 0x7E0) from TRACK, replies with a frame
(ID 0x7E1) carrying T2 and T3 as big-endian u32 microseconds, so TRACK
can compute one-way delay via:

    one_way_delay = ((T4 - T1) - (T3 - T2)) / 2

Usage:
    sudo python3 delay_sim.py [can_iface]   # default: can0
"""

import os
import socket
import struct
import sys
import time

DELAY_REQ_ID = 0x7E0
DELAY_RSP_ID = 0x7E1

CAN_FRAME_FMT = "=IB3x8s"  # can_id, dlc, pad, data


def now_us() -> int:
    return time.clock_gettime_ns(time.CLOCK_MONOTONIC) // 1000 & 0xFFFFFFFF


def open_can(iface: str) -> socket.socket:
    s = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((iface,))
    return s


def main() -> int:
    iface = sys.argv[1] if len(sys.argv) > 1 else "can0"
    s = open_can(iface)
    print(f"delay_sim listening on {iface}, req=0x{DELAY_REQ_ID:X} rsp=0x{DELAY_RSP_ID:X}")

    while True:
        raw = s.recv(16)
        can_id, dlc, data = struct.unpack(CAN_FRAME_FMT, raw)
        can_id &= socket.CAN_EFF_MASK

        if can_id != DELAY_REQ_ID:
            continue

        T2 = now_us()
        T3 = now_us()
        payload = struct.pack(">II", T2, T3)
        frame = struct.pack(CAN_FRAME_FMT, DELAY_RSP_ID, 8, payload)
        s.send(frame)
        print(f"replied T2={T2} T3={T3}")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        pass
