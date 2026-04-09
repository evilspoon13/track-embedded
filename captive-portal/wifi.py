"""
wifi.py             WiFi Management Utilities

@author     Cameron Stone '26 <cameron28202@gmail.com>

@copyright  Texas A&M University
"""

import subprocess
import re


# WiFi management using nmcli, this is separate from server.py to keep subprocess calls isolated
def _stop_ap():
    """Stop AP services and return wlan0 to NetworkManager."""
    import pathlib, time
    subprocess.run(["systemctl", "stop", "hostapd", "dnsmasq"],
                   capture_output=True, timeout=10)
    subprocess.run(["ip", "addr", "flush", "dev", "wlan0"],
                   capture_output=True, timeout=5)
    # Remove the unmanage override so NM takes wlan0 back
    conf = pathlib.Path("/etc/NetworkManager/conf.d/unmanage-wlan0.conf")
    if conf.exists():
        conf.unlink()
    subprocess.run(["systemctl", "restart", "NetworkManager"],
                   capture_output=True, timeout=10)
    time.sleep(3)
    subprocess.run(["nmcli", "dev", "wifi", "rescan"],
                   capture_output=True, timeout=10)
    time.sleep(2)


def connect(ssid, password=""):
    try:
        _stop_ap()
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
