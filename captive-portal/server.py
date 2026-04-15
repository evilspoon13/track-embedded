"""
server.py           Captive Portal Web Server

@author     Cameron Stone '26 <cameron28202@gmail.com>

@copyright  Texas A&M University
"""

import json
import os
import secrets
import subprocess
import struct
import time
import uuid
from datetime import datetime
from pathlib import Path

import cantools
from flask import Flask, request, jsonify, redirect, send_from_directory, render_template
from flask_sock import Sock

import wifi
from shm_reader import create_telemetry_reader, create_gps_reader

ROOT = Path(__file__).resolve().parent.parent
CONFIG_DIR = ROOT / "config"
GRAPHICS_PATH = CONFIG_DIR / "graphics.json"
DBC_PATH = CONFIG_DIR / "display.dbc"
DEVICE_PATH = CONFIG_DIR / "device.json"
UI_DIR = Path(__file__).resolve().parent / "ui"  # vite build output
LOG_DIR = Path("/tmp/track-logs")
LOG_ENTRY_SIZE = 24
DEFAULT_LOG_LIMIT = 100
MAX_LOG_LIMIT = 500

app = Flask(
    __name__,
    template_folder=str(Path(__file__).resolve().parent / "templates"),
    static_folder=str(Path(__file__).resolve().parent / "static"),
    static_url_path="/static",
)
sock = Sock(app)

# -- captive portal detection --

CAPTIVE_PORTAL_CHECKS = [
    "/hotspot-detect.html",       # Apple
    "/library/test/success.html", # Apple
    "/generate_204",              # Android
    "/gen_204",                   # Android
    "/connecttest.txt",           # Windows
    "/ncsi.txt",                  # Windows
    "/redirect",                  # Firefox
    "/canonical.html",            # Firefox
    "/success.txt",               # various
]

@app.before_request
def captive_portal_redirect():
    if request.path in CAPTIVE_PORTAL_CHECKS:
        return redirect("/")


# -- helpers --

def send_sighup(service_name):
    try:
        result = subprocess.run(
            ["systemctl", "kill", "-s", "SIGHUP", service_name],
            capture_output=True, text=True, timeout=5,
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False
    
def send_sigusr1(service_name):
    try:
        result = subprocess.run(
            ["systemctl", "kill", "-s", "SIGUSR1", service_name],
            capture_output=True, text=True, timeout=5,
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False
    
def send_sigusr2(service_name):
    try:
        result = subprocess.run(
            ["systemctl", "kill", "-s", "SIGUSR2", service_name],
            capture_output=True, text=True, timeout=5,
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False



def atomic_write(path, content):
    tmp = Path(str(path) + ".tmp")
    try:
        tmp.write_text(content)
        tmp.rename(path)
        return True
    except OSError:
        tmp.unlink(missing_ok=True)
        return False


# -- pages --

@app.route("/")
def index():
    status = wifi.get_status()
    if status["connected"]:
        ip = wifi.get_ip() or "unknown"
        wifi_status = f'Connected to <strong>{status["ssid"]}</strong> ({ip})'
    else:
        wifi_status = "Not connected"
    return render_template("index.html", wifi_status=wifi_status)


@app.route("/wifi")
def wifi_page():
    return render_template("wifi.html")


# -- SPA (vite build output) --

@app.route("/app/")
@app.route("/app/<path:filename>")
def serve_spa(filename=""):
    if filename and (UI_DIR / filename).is_file():
        return send_from_directory(UI_DIR, filename)
    return send_from_directory(UI_DIR, "index.html")


# -- wifi API --

@app.route("/api/wifi/status")
def api_wifi_status():
    return jsonify(wifi.get_status())


@app.route("/api/wifi/connect", methods=["POST"])
def api_wifi_connect():
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"ok": False, "error": "Invalid JSON"}), 400
    ssid = data.get("ssid", "").strip()
    password = data.get("password", "")
    if not ssid:
        return jsonify({"ok": False, "error": "SSID required"}), 400

    # Connect after a delay so the response reaches the client before AP goes down
    def deferred_connect():
        import time
        time.sleep(2)
        wifi.connect(ssid, password)

    import threading
    threading.Thread(target=deferred_connect, daemon=True).start()
    return jsonify({"ok": True, "message": f"Connecting to {ssid}..."})


# -- config helpers --


def read_graphics_config():
    try:
        return json.loads(GRAPHICS_PATH.read_text())
    except (FileNotFoundError, json.JSONDecodeError):
        return {"screens": []}


def write_graphics_config(config):
    if not atomic_write(GRAPHICS_PATH, json.dumps(config, indent=2)):
        return False
    send_sighup("track-graphics.service")
    send_sigusr1("track-cloud-bridge.service") # trigger send to cloud
    return True


# -- DBC helpers --

def signal_type_from_cantools(sig):
    """Map a cantools signal to the type string the frontend expects."""
    if sig.is_float:
        return "double" if sig.length > 32 else "float"
    if sig.is_signed:
        if sig.length <= 8:
            return "int8"
        if sig.length <= 16:
            return "int16"
        return "int32"
    if sig.length <= 8:
        return "uint8"
    if sig.length <= 16:
        return "uint16"
    return "uint32"


def parse_dbc_to_config(raw):
    """Parse raw DBC text into the { frames } JSON the frontend expects."""
    db = cantools.database.load_string(raw)
    frames = {}
    for msg in db.messages:
        can_id_hex = hex(msg.frame_id)
        signals = []
        for sig in msg.signals:
            signals.append({
                "name": sig.name,
                "start_byte": sig.start,
                "length": sig.length,
                "type": signal_type_from_cantools(sig),
                "scale": sig.scale,
                "offset": sig.offset,
            })
        frames[can_id_hex] = {"can_id_label": msg.name, "signals": signals}
    return {"frames": frames}


def config_to_dbc(config):
    """Convert { frames } JSON back to a raw DBC string."""
    from cantools.database.conversion import IdentityConversion, LinearConversion

    db = cantools.database.Database()
    for can_id_hex, frame_def in config.get("frames", {}).items():
        frame_id = int(can_id_hex, 16)
        signals = []
        for sig_def in frame_def.get("signals", []):
            t = sig_def.get("type", "uint8")
            is_signed = t.startswith("int")
            is_float = t in ("float", "double")
            scale = sig_def.get("scale", 1)
            offset = sig_def.get("offset", 0)
            if is_float:
                conversion = IdentityConversion(is_float=True)
            elif scale != 1 or offset != 0:
                conversion = LinearConversion(scale=scale, offset=offset, is_float=False)
            else:
                conversion = None
            signals.append(cantools.database.Signal(
                name=sig_def["name"],
                start=sig_def["start_byte"],
                length=sig_def["length"],
                is_signed=is_signed,
                conversion=conversion,
            ))
        dlc = max((s.start + s.length for s in signals), default=0)
        dlc = (dlc + 7) // 8  # bits to bytes
        db.messages.append(cantools.database.Message(
            frame_id=frame_id,
            name=frame_def.get("can_id_label", f"MSG_{can_id_hex}"),
            length=max(dlc, 1),
            signals=signals,
        ))
    return db.as_dbc_string()


def read_dbc_frame_labels():
    try:
        raw = DBC_PATH.read_text()
        config = parse_dbc_to_config(raw)
    except (FileNotFoundError, OSError, json.JSONDecodeError):
        return {}
    except Exception:
        return {}
    return {
        int(can_id_hex, 16): frame["can_id_label"]
        for can_id_hex, frame in config.get("frames", {}).items()
    }


def _ts_to_date(ts_ms):
    # match cloud: server-local YYYY-MM-DD
    return datetime.fromtimestamp(ts_ms / 1000).strftime("%Y-%m-%d")


def iter_local_log_entries():
    if not LOG_DIR.exists():
        return

    for path in sorted(LOG_DIR.glob("*.bin"), reverse=True):
        session = path.stem
        try:
            with path.open("rb") as f:
                while True:
                    chunk = f.read(LOG_ENTRY_SIZE)
                    if len(chunk) < LOG_ENTRY_SIZE:
                        break
                    ts, can_id, _pad, value = struct.unpack("<qIId", chunk)
                    yield {
                        "ts": ts,
                        "can_id": can_id,
                        "value": value,
                        "session": session,
                    }
        except OSError:
            continue


def widget_to_api(w):
    """Convert stored widget (hex string can_id) to API format (integer can_id)."""
    w = dict(w)
    data = dict(w.get("data", {}))
    can_id_str = data.get("can_id", "0x0")
    data["can_id"] = int(can_id_str, 16) if isinstance(can_id_str, str) else can_id_str
    w["data"] = data
    return w


def widget_from_api(w):
    """Convert API widget (integer can_id) to storage format (hex string can_id)."""
    w = dict(w)
    data = dict(w.get("data", {}))
    can_id = data.get("can_id", 0)
    data["can_id"] = hex(can_id) if isinstance(can_id, int) else can_id
    w["data"] = data
    return w


# -- config API (full file) --

@app.route("/api/graphics", methods=["GET"])
@app.route("/api/config/graphics", methods=["GET"])
def api_get_graphics():
    try:
        return app.response_class(
            GRAPHICS_PATH.read_text(),
            mimetype="application/json",
        )
    except (FileNotFoundError, json.JSONDecodeError) as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/graphics", methods=["POST"])
@app.route("/api/config/graphics", methods=["POST"])
def api_set_graphics():
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"ok": False, "error": "Invalid JSON"}), 400
    if "screens" not in data:
        return jsonify({"ok": False, "error": "Missing 'screens' key"}), 400
    if not atomic_write(GRAPHICS_PATH, json.dumps(data, indent=2)):
        return jsonify({"ok": False, "error": "Failed to write config"}), 500
    send_sighup("track-graphics.service")
    return jsonify({"ok": True})


# -- per-screen CRUD (matches Express API) --

@app.route("/api/graphics/screens", methods=["GET"])
def api_get_screen_names():
    config = read_graphics_config()
    names = [s["name"] for s in config.get("screens", [])]
    return jsonify({"screens": names})


@app.route("/api/graphics/screens/<screen_id>", methods=["GET"])
def api_get_screen(screen_id):
    config = read_graphics_config()
    for s in config.get("screens", []):
        if s["name"] == screen_id:
            return jsonify({
                "name": s["name"],
                "widgets": [widget_to_api(w) for w in s.get("widgets", [])],
            })
    return jsonify({"msg": "Screen not found"}), 404


@app.route("/api/graphics/screens/<screen_id>", methods=["POST"])
def api_update_screen(screen_id):
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"success": False, "msg": "Invalid JSON"}), 400

    config = read_graphics_config()
    new_screen = {
        "name": data.get("name", screen_id),
        "widgets": [widget_from_api(w) for w in data.get("widgets", [])],
    }

    idx = next((i for i, s in enumerate(config["screens"]) if s["name"] == screen_id), -1)
    if idx >= 0:
        config["screens"][idx] = new_screen
    else:
        config["screens"].append(new_screen)

    if not write_graphics_config(config):
        return jsonify({"success": False, "msg": "Failed to write config"}), 500
    return jsonify({"success": True})


@app.route("/api/graphics/screens/<screen_id>", methods=["DELETE"])
def api_delete_screen(screen_id):
    config = read_graphics_config()
    idx = next((i for i, s in enumerate(config["screens"]) if s["name"] == screen_id), -1)
    if idx == -1:
        return jsonify({"success": False}), 404
    config["screens"].pop(idx)
    if not write_graphics_config(config):
        return jsonify({"success": False, "msg": "Failed to write config"}), 500
    return jsonify({"success": True})


# -- DBC API (matches cloud backend) --

@app.route("/api/dbc", methods=["GET"])
def api_get_dbc():
    """Return parsed DBC as JSON { frames: { "0x...": { can_id_label, signals } } }."""
    try:
        raw = DBC_PATH.read_text()
    except FileNotFoundError:
        return jsonify({"msg": "Not Found"}), 404
    try:
        config = parse_dbc_to_config(raw)
    except Exception as e:
        return jsonify({"msg": str(e)}), 500
    return jsonify(config)


@app.route("/api/dbc", methods=["POST"])
def api_update_dbc():
    """Accept { frames: { ... } } JSON, convert to DBC, and save."""
    data = request.get_json(silent=True)
    if not data or "frames" not in data:
        return jsonify({"msg": "Missing 'frames' key"}), 400
    try:
        raw = config_to_dbc(data)
    except Exception as e:
        return jsonify({"msg": str(e)}), 500
    if not atomic_write(DBC_PATH, raw):
        return jsonify({"msg": "Failed to write DBC"}), 500
    send_sighup("track-can-reader.service")
    send_sigusr2("track-cloud-bridge.service")
    return jsonify({"msg": "Wrote DBC"})


@app.route("/api/dbc/upload", methods=["POST"])
def api_upload_dbc():
    """Accept { raw: "..." } raw DBC string, save it, return parsed frames."""
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"msg": "Missing raw DBC string in body.raw"}), 400
    raw = data.get("raw", "")
    if not raw or not isinstance(raw, str):
        return jsonify({"msg": "Missing raw DBC string in body.raw"}), 400
    try:
        config = parse_dbc_to_config(raw)
    except Exception:
        return jsonify({"msg": "Invalid DBC file — could not be parsed."}), 400
    if not atomic_write(DBC_PATH, raw):
        return jsonify({"msg": "Failed to write DBC"}), 500
    send_sighup("track-can-reader.service")
    send_sigusr2("track-cloud-bridge.service")
    return jsonify(config)


# -- logs API (matches cloud backend shape) --

@app.route("/api/logs", methods=["GET"])
def api_get_logs():
    limit_raw = request.args.get("limit", "")
    try:
        limit = min(int(limit_raw), MAX_LOG_LIMIT) if limit_raw else DEFAULT_LOG_LIMIT
    except ValueError:
        limit = DEFAULT_LOG_LIMIT

    before_raw = request.args.get("before")
    try:
        before_ts = int(before_raw) if before_raw is not None else None
    except ValueError:
        before_ts = None

    date_filter = request.args.get("date")

    frame_labels = read_dbc_frame_labels()

    filtered_entries = []
    for entry in iter_local_log_entries() or ():
        if before_ts is not None and entry["ts"] >= before_ts:
            continue
        if date_filter and _ts_to_date(entry["ts"]) != date_filter:
            continue
        filtered_entries.append({
            **entry,
            "frame_name": frame_labels.get(entry["can_id"]),
        })

    filtered_entries.sort(key=lambda entry: entry["ts"], reverse=True)
    page = filtered_entries[:limit + 1]
    has_more = len(page) > limit
    page = page[:limit]
    next_cursor = page[-1]["ts"] if has_more and page else None

    return jsonify({
        "entries": page,
        "nextCursor": next_cursor,
    })


@app.route("/api/logs/days", methods=["GET"])
def api_get_log_days():
    counts = {}
    for entry in iter_local_log_entries() or ():
        date = _ts_to_date(entry["ts"])
        counts[date] = counts.get(date, 0) + 1
    days = [{"date": d, "count": c} for d, c in counts.items()]
    days.sort(key=lambda x: x["date"], reverse=True)
    return jsonify({"days": days})


# -- GPS snapshot --

@app.route("/api/gps", methods=["GET"])
def api_get_gps():
    gps = get_gps_reader()
    if not gps:
        return jsonify({"available": False}), 503
    pos = gps.current_pos()
    start = max(0, pos - 1)
    data, _ = gps.consume(start)
    if not data:
        return jsonify({"available": True, "has_fix": False})
    return jsonify({
        "available": True,
        "has_fix": bool(data.get("has_fix")),
        "lat": data["latitude"],
        "lon": data["longitude"],
        "speed_kmh": data["speed_kmh"],
        "heading": data["heading"],
        "gps_ts": data["timestamp_ms"],
        "ts": int(time.time() * 1000),
    })


# -- live telemetry WebSocket --

_tel_reader = None
_gps_reader = None

def get_telemetry_reader():
    global _tel_reader
    if _tel_reader is None:
        _tel_reader = create_telemetry_reader()
    return _tel_reader

def get_gps_reader():
    global _gps_reader
    if _gps_reader is None:
        _gps_reader = create_gps_reader()
    return _gps_reader


@sock.route("/ws/client")
def telemetry_ws(ws):
    tel = get_telemetry_reader()
    gps = get_gps_reader()
    if not tel and not gps:
        ws.close(1011, "Shared memory not available")
        return
    tel_pos = tel.current_pos() if tel else 0
    gps_pos = gps.current_pos() if gps else 0
    try:
        while True:
            if tel:
                signals, tel_pos = tel.consume(tel_pos)
                if signals:
                    ws.send(json.dumps({"type": "Telemetry", "payload": {"signals": signals}}))
            if gps:
                gps_data, gps_pos = gps.consume(gps_pos)
                if gps_data:
                    ws.send(json.dumps({
                        "type": "gps",
                        "payload": {
                            "lat": gps_data["latitude"],
                            "lon": gps_data["longitude"],
                            "speed_kmh": gps_data["speed_kmh"],
                            "heading": gps_data["heading"],
                            "ts": int(time.time() * 1000),
                            "gps_ts": gps_data["timestamp_ms"],
                        },
                    }))
            time.sleep(0.05)
    except Exception:
        return


# -- device identity --

def read_device_config():
    """Read device.json, creating it with a new UUID + secret if missing."""
    if DEVICE_PATH.exists():
        try:
            return json.loads(DEVICE_PATH.read_text())
        except (json.JSONDecodeError, OSError):
            pass
    config = {
        "device_id": str(uuid.uuid4()),
        "device_secret": secrets.token_hex(32),
        "team_members": [],
    }
    atomic_write(DEVICE_PATH, json.dumps(config, indent=2))
    return config


def write_device_config(config):
    return atomic_write(DEVICE_PATH, json.dumps(config, indent=2))


@app.route("/api/device", methods=["GET"])
def api_get_device():
    config = read_device_config()
    return jsonify({
        "device_id": config["device_id"],
        "team_members": config.get("team_members", []),
    })


@app.route("/api/device/team-members", methods=["POST"])
def api_set_team_members():
    data = request.get_json(silent=True)
    if not data or "team_members" not in data:
        return jsonify({"ok": False, "error": "Missing 'team_members' key"}), 400
    members = data["team_members"]
    if not isinstance(members, list) or not all(isinstance(m, str) for m in members):
        return jsonify({"ok": False, "error": "'team_members' must be a list of strings"}), 400

    config = read_device_config()
    config["team_members"] = [m.strip().lower() for m in members if m.strip()]
    if not write_device_config(config):
        return jsonify({"ok": False, "error": "Failed to write device.json"}), 500

    # Signal cloud-bridge to re-register with updated team members
    send_sighup("track-cloud-bridge.service")
    return jsonify({"ok": True, "team_members": config["team_members"]})


# -- main --

def main():
    read_device_config()  # generate device.json on first boot
    port = int(os.environ.get("PORTAL_PORT", "80"))
    print(f"captive portal running on http://0.0.0.0:{port}")
    app.run(host="0.0.0.0", port=port)

if __name__ == "__main__":
    main()
