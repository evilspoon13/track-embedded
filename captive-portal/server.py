import json
import os
import subprocess
from pathlib import Path

from flask import Flask, request, jsonify, redirect, send_from_directory, render_template

import wifi

ROOT = Path(__file__).resolve().parent.parent
CONFIG_DIR = ROOT / "config"
GRAPHICS_PATH = CONFIG_DIR / "graphics.json"
DBC_PATH = CONFIG_DIR / "display.dbc"
UI_DIR = Path(__file__).resolve().parent / "ui"  # vite build output

app = Flask(
    __name__,
    template_folder=str(Path(__file__).resolve().parent / "templates"),
    static_folder=str(Path(__file__).resolve().parent / "static"),
    static_url_path="/static",
)

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

@app.route("/api/wifi/scan")
def api_wifi_scan():
    return jsonify(wifi.scan_networks())


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
    ok, msg = wifi.connect(ssid, password)
    return jsonify({"ok": ok, "message": msg})


# -- config API --

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


@app.route("/api/dbc", methods=["GET"])
@app.route("/api/config/dbc", methods=["GET"])
def api_get_dbc():
    try:
        return app.response_class(
            DBC_PATH.read_text(),
            mimetype="text/plain; charset=utf-8",
        )
    except FileNotFoundError:
        return jsonify({"error": "DBC file not found"}), 404


@app.route("/api/dbc", methods=["POST"])
@app.route("/api/config/dbc", methods=["POST"])
def api_set_dbc():
    body = request.get_data(as_text=True)
    if not body.strip():
        return jsonify({"ok": False, "error": "Empty DBC content"}), 400
    if not atomic_write(DBC_PATH, body):
        return jsonify({"ok": False, "error": "Failed to write DBC"}), 500
    atomic_write(Path("/tmp/display.dbc"), body)
    send_sighup("track-can-reader.service")
    return jsonify({"ok": True})
    

# -- main --

def main():
    port = int(os.environ.get("PORTAL_PORT", "80"))
    print(f"captive portal running on http://0.0.0.0:{port}")
    app.run(host="0.0.0.0", port=port)

if __name__ == "__main__":
    main()