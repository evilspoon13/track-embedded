import mmap
import os
import struct

MSG_SIZE = 80
MSG_FMT = "<I64s4xd"  # uint32, char[64], 4-byte pad, double
BUFFER_COUNT = 4096
BUFFER_BYTES = MSG_SIZE * BUFFER_COUNT
WRITE_IDX_OFFSET = BUFFER_BYTES
SHM_SIZE = BUFFER_BYTES + 8
SHM_PATH = "/dev/shm/track_telemetry"

class TelemetryReader:
    def __init__(self, buf: mmap.mmap):
        self._buf = buf

    def current_pos(self) -> int:
        return struct.unpack_from("<Q", self._buf, WRITE_IDX_OFFSET)[0]

    def consume(self, pos: int) -> tuple[dict[str, float], int]:
        # Matches C++ reader impl -- loop until we catch up with writer idx
        head = self.current_pos()
        if pos == head:
            return {}, pos

        if head - pos > BUFFER_COUNT:
            pos = head - BUFFER_COUNT

        signals: dict[str, float] = {}
        while pos < head:
            idx = pos & (BUFFER_COUNT - 1)
            offset = idx * MSG_SIZE
            can_id, name_bytes, value = struct.unpack_from(MSG_FMT, self._buf, offset)
            name = name_bytes.split(b"\x00", 1)[0].decode("utf-8", errors="replace")
            signals[f"{can_id}:{name}"] = value
            pos += 1

        return signals, pos


def create_reader() -> TelemetryReader | None:
    #Factory: returns None if the shared memory segment doesn't exist
    if not os.path.exists(SHM_PATH):
        return None
    fd = None
    try:
        fd = os.open(SHM_PATH, os.O_RDONLY)
        buf = mmap.mmap(fd, SHM_SIZE, access=mmap.ACCESS_READ)
        return TelemetryReader(buf)
    except OSError:
        return None
    finally:
        if fd is not None:
            os.close(fd)
