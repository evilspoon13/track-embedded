import mmap
import os
import struct

# -- Telemetry queue (CAN signals) --
TEL_MSG_SIZE = 80
TEL_MSG_FMT = "<I64s4xd"  # uint32, char[64], 4-byte pad, double
TEL_BUFFER_COUNT = 4096
TEL_WRITE_IDX_OFFSET = TEL_MSG_SIZE * TEL_BUFFER_COUNT
TEL_SHM_SIZE = TEL_WRITE_IDX_OFFSET + 8
TEL_SHM_PATH = "/dev/shm/track_telemetry"

# -- GPS queue --
GPS_MSG_SIZE = 48
GPS_MSG_FMT = "<4dq?7x"  # lat, lon, speed_kmh, heading, timestamp_ms, has_fix, pad
GPS_BUFFER_COUNT = 256
GPS_WRITE_IDX_OFFSET = GPS_MSG_SIZE * GPS_BUFFER_COUNT
GPS_SHM_SIZE = GPS_WRITE_IDX_OFFSET + 8
GPS_SHM_PATH = "/dev/shm/track_gps"


class TelemetryReader:
    def __init__(self, buf: mmap.mmap):
        self._buf = buf

    def current_pos(self) -> int:
        return struct.unpack_from("<Q", self._buf, TEL_WRITE_IDX_OFFSET)[0]

    def consume(self, pos: int) -> tuple[dict[str, float], int]:
        head = self.current_pos()
        if pos == head:
            return {}, pos

        if head - pos > TEL_BUFFER_COUNT:
            pos = head - TEL_BUFFER_COUNT

        signals: dict[str, float] = {}
        while pos < head:
            idx = pos & (TEL_BUFFER_COUNT - 1)
            offset = idx * TEL_MSG_SIZE
            can_id, name_bytes, value = struct.unpack_from(TEL_MSG_FMT, self._buf, offset)
            name = name_bytes.split(b"\x00", 1)[0].decode("utf-8", errors="replace")
            signals[f"{can_id}:{name}"] = value
            pos += 1

        return signals, pos


class GpsReader:
    def __init__(self, buf: mmap.mmap):
        self._buf = buf

    def current_pos(self) -> int:
        return struct.unpack_from("<Q", self._buf, GPS_WRITE_IDX_OFFSET)[0]

    def consume(self, pos: int) -> tuple[dict | None, int]:
        head = self.current_pos()
        if pos == head:
            return None, pos

        if head - pos > GPS_BUFFER_COUNT:
            pos = head - GPS_BUFFER_COUNT

        latest = None
        while pos < head:
            idx = pos & (GPS_BUFFER_COUNT - 1)
            offset = idx * GPS_MSG_SIZE
            lat, lon, speed, heading, ts, fix = struct.unpack_from(
                GPS_MSG_FMT, self._buf, offset
            )
            latest = {
                "latitude": lat,
                "longitude": lon,
                "speed_kmh": speed,
                "heading": heading,
                "timestamp_ms": ts,
                "has_fix": fix,
            }
            pos += 1

        return latest, pos


def create_telemetry_reader() -> TelemetryReader | None:
    return _open_reader(TEL_SHM_PATH, TEL_SHM_SIZE, TelemetryReader)


def create_gps_reader() -> GpsReader | None:
    return _open_reader(GPS_SHM_PATH, GPS_SHM_SIZE, GpsReader)


def _open_reader(shm_path: str, shm_size: int, cls):
    if not os.path.exists(shm_path):
        return None
    fd = None
    try:
        fd = os.open(shm_path, os.O_RDONLY)
        buf = mmap.mmap(fd, shm_size, access=mmap.ACCESS_READ)
        return cls(buf)
    except OSError:
        return None
    finally:
        if fd is not None:
            os.close(fd)
