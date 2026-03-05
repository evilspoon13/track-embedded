import subprocess
import re


# WiFi management using nmcli, this is separate from server.py to keep subprocess calls isolated
def scan_networks():
    try:
        result = subprocess.run(
            ["nmcli", "-t", "-f", "SSID,SIGNAL,SECURITY", "dev", "wifi", "list", "--rescan", "yes"],
            capture_output=True, text=True, timeout=15,
        )
        networks = []
        seen = set()
        for line in result.stdout.strip().splitlines():
            parts = line.split(":")
            if len(parts) >= 3:
                ssid = parts[0].strip()
                if not ssid or ssid in seen:
                    continue
                seen.add(ssid)
                networks.append({
                    "ssid": ssid,
                    "signal": int(parts[1]) if parts[1].isdigit() else 0,
                    "security": parts[2] or "Open",
                })
        networks.sort(key=lambda n: n["signal"], reverse=True)
        return networks
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return []


def connect(ssid, password=""):
    try:
        cmd = ["nmcli", "dev", "wifi", "connect", ssid]
        if password:
            cmd += ["password", password]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        if result.returncode == 0:
            return True, f"Connected to {ssid}"
        return False, result.stderr.strip() or "Connection failed"
    except subprocess.TimeoutExpired:
        return False, "Connection timed out"
    except FileNotFoundError:
        return False, "nmcli not found"


def get_status():
    try:
        result = subprocess.run(
            ["nmcli", "-t", "-f", "NAME,DEVICE,TYPE", "con", "show", "--active"],
            capture_output=True, text=True, timeout=5,
        )
        for line in result.stdout.strip().splitlines():
            parts = line.split(":")
            if len(parts) >= 3 and "wireless" in parts[2]:
                return {"connected": True, "ssid": parts[0], "device": parts[1]}
        return {"connected": False}
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return {"connected": False}


def get_ip():
    try:
        result = subprocess.run(
            ["nmcli", "-t", "-f", "IP4.ADDRESS", "dev", "show", "wlan0"],
            capture_output=True, text=True, timeout=5,
        )
        for line in result.stdout.strip().splitlines():
            m = re.search(r"(\d+\.\d+\.\d+\.\d+)", line)
            if m:
                return m.group(1)
    except (subprocess.TimeoutExpired, FileNotFoundError):
        pass
    return None
