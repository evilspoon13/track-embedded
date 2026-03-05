import json
import os
import subprocess
import sys
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlparse

import wifi

ROOT = Path(__file__).resolve().parent.parent
CONFIG_DIR = ROOT / "config"
GRAPHICS_PATH = CONFIG_DIR / "graphics.json"
DBC_PATH = CONFIG_DIR / "display.dbc"
TEMPLATES_DIR = Path(__file__).resolve().parent / "templates"
STATIC_DIR = Path(__file__).resolve().parent / "static"
UI_DIR = Path(__file__).resolve().parent / "ui"  # vite build output

# paths that devices use to detect captive portals
CAPTIVE_PORTAL_CHECKS = {
    "/hotspot-detect.html",      # Apple
    "/library/test/success.html",  # Apple
    "/generate_204",             # Android
    "/gen_204",                  # Android
    "/connecttest.txt",          # Windows
    "/ncsi.txt",                 # Windows
    "/redirect",                 # Firefox
    "/canonical.html",           # Firefox
    "/success.txt",              # various
}


def render_template(name, **ctx):
    path = TEMPLATES_DIR / name
    html = path.read_text()
    for key, value in ctx.items():
        html = html.replace("{{" + key + "}}", str(value))
    return html


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


class PortalHandler(BaseHTTPRequestHandler):

    def log_message(self, fmt, *args):
        sys.stderr.write(f"{args[0]} {args[1]} {args[2]}\n")

    # helpers to avoid repeating send header code
    def respond_html(self, html, status=200):
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(html.encode())))
        self.end_headers()
        self.wfile.write(html.encode())

    def respond_json(self, data, status=200):
        body = json.dumps(data)
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body.encode())))
        self.end_headers()
        self.wfile.write(body.encode())

    def respond_redirect(self, location):
        self.send_response(302)
        self.send_header("Location", location)
        self.end_headers()

    def respond_404(self):
        self.respond_html("<h1>404 Not Found</h1>", 404)

    def read_body(self):
        length = int(self.headers.get("Content-Length", 0))
        return self.rfile.read(length) if length else b""

    # -- GET routes --
    # override basehttprequesthandler to route based on path
    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"

        if path in CAPTIVE_PORTAL_CHECKS:
            self.respond_redirect("/")
            return

        routes = {
            "/":                  self.get_index,
            "/wifi":              self.get_wifi,
            "/api/wifi/scan":     self.api_wifi_scan,
            "/api/wifi/status":   self.api_wifi_status,
            "/api/config/graphics": self.api_get_graphics,
            "/api/config/dbc":    self.api_get_dbc,
            "/api/graphics":      self.api_get_graphics,
            "/api/dbc":           self.api_get_dbc,
        }

        handler = routes.get(path)
        if handler:
            handler()
        elif path.startswith("/static/"):
            self.serve_static(path)
        elif path.startswith("/app"):
            self.serve_spa(path)
        else:
            self.respond_404()

    def do_POST(self):
        path = urlparse(self.path).path.rstrip("/")

        routes = {
            "/api/wifi/connect":    self.api_wifi_connect,
            "/api/config/graphics": self.api_set_graphics,
            "/api/config/dbc":      self.api_set_dbc,
            "/api/graphics":        self.api_set_graphics,
            "/api/dbc":             self.api_set_dbc,
        }

        handler = routes.get(path)
        if handler:
            handler()
        else:
            self.respond_404()

    # -- pages --
    # read an HTML template file, substitute placeholder, return it. html files do heavy lifting
    def get_index(self):
        status = wifi.get_status()
        if status["connected"]:
            ip = wifi.get_ip() or "unknown"
            wifi_status = f'Connected to <strong>{status["ssid"]}</strong> ({ip})'
        else:
            wifi_status = "Not connected"
        self.respond_html(render_template("index.html", wifi_status=wifi_status))

    def get_wifi(self):
        self.respond_html(render_template("wifi.html"))


    # -- static files --

    def serve_static(self, path):
        rel = path.removeprefix("/static/")
        file_path = STATIC_DIR / rel
        if not file_path.is_file() or not file_path.resolve().is_relative_to(STATIC_DIR):
            self.respond_404()
            return
        content = file_path.read_bytes()
        ext = file_path.suffix
        mime = {".css": "text/css", ".js": "application/javascript",
                ".png": "image/png", ".svg": "image/svg+xml"}.get(ext, "application/octet-stream")
        self.send_response(200)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    # -- SPA (vite build output) --

    def serve_spa(self, path):
        pass
        #todo

    # -- wifi API --

    def api_wifi_scan(self):
        self.respond_json(wifi.scan_networks())

    def api_wifi_status(self):
        self.respond_json(wifi.get_status())

    def api_wifi_connect(self):
        try:
            data = json.loads(self.read_body())
        except (json.JSONDecodeError, ValueError):
            self.respond_json({"ok": False, "error": "Invalid JSON"}, 400)
            return
        ssid = data.get("ssid", "").strip()
        password = data.get("password", "")
        if not ssid:
            self.respond_json({"ok": False, "error": "SSID required"}, 400)
            return
        ok, msg = wifi.connect(ssid, password)
        self.respond_json({"ok": ok, "message": msg})

    # -- config API --

    def api_get_graphics(self):
        try:
            self.respond_json(json.loads(GRAPHICS_PATH.read_text()))
        except (FileNotFoundError, json.JSONDecodeError) as e:
            self.respond_json({"error": str(e)}, 500)

    def api_get_dbc(self):
        try:
            content = DBC_PATH.read_text()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(content.encode())))
            self.end_headers()
            self.wfile.write(content.encode())
        except FileNotFoundError:
            self.respond_json({"error": "DBC file not found"}, 404)

    def api_set_graphics(self):
        try:
            data = json.loads(self.read_body())
        except (json.JSONDecodeError, ValueError):
            self.respond_json({"ok": False, "error": "Invalid JSON"}, 400)
            return
        # Validate minimal structure
        if "screens" not in data:
            self.respond_json({"ok": False, "error": "Missing 'screens' key"}, 400)
            return
        if not atomic_write(GRAPHICS_PATH, json.dumps(data, indent=2)):
            self.respond_json({"ok": False, "error": "Failed to write config"}, 500)
            return
        send_sighup("fsae-graphics.service")
        self.respond_json({"ok": True})

    def api_set_dbc(self):
        body = self.read_body().decode("utf-8", errors="replace")
        if not body.strip():
            self.respond_json({"ok": False, "error": "Empty DBC content"}, 400)
            return
        if not atomic_write(DBC_PATH, body):
            self.respond_json({"ok": False, "error": "Failed to write DBC"}, 500)
            return
        # also copy to /tmp so can-reader picks it up
        atomic_write(Path("/tmp/display.dbc"), body)
        send_sighup("fsae-can-reader.service")
        self.respond_json({"ok": True})


def main():
    port = int(os.environ.get("PORTAL_PORT", "80"))
    server = HTTPServer(("0.0.0.0", port), PortalHandler)
    print(f"captive portal running on http://0.0.0.0:{port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()
    print("Portal stopped")


if __name__ == "__main__":
    main()