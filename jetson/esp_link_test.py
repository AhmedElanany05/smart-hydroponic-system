#!/usr/bin/env python3
"""Checks the Wi-Fi link between the Jetson and the tower ESP32.

Run on the Jetson:   python3 esp_link_test.py
It reads the ESP32 status, says hello, sends a test plant-check result and then
keeps sending a heartbeat every 30 s (Ctrl+C to stop). Only standard Python is used.
"""
import getpass
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

ADDRESSES = ["http://tower-esp32.local", "http://192.168.4.1"]   # home Wi-Fi name, then the ESP32's own Wi-Fi


def get(base, path, **params):
    url = base + path + ("?" + urllib.parse.urlencode(params) if params else "")
    with urllib.request.urlopen(url, timeout=5) as r:
        return json.loads(r.read().decode())


def find_esp():
    for base in ADDRESSES:
        try:
            return base, get(base, "/status")
        except Exception as e:
            print(f"  {base}: no answer ({e.__class__.__name__})")
    return None, None


def main():
    print("1) Looking for the ESP32 ...")
    base, status = find_esp()
    if not base:
        print("ESP32 not found. Join the same Wi-Fi as the ESP32 (home Wi-Fi, or the ESP32's own 'Tower' Wi-Fi) and try again.")
        sys.exit(1)
    print(f"   found at {base}")
    print(f"   time {status['time']}, pH {status['ph']}, TDS {status['tds_ppm']} ppm, "
          f"air {status['air_temp_c']} C, battery {status['battery_v']} V, Jetson power: {status['jetson']}")

    key = os.environ.get("TOWER_KEY") or getpass.getpass("2) Control key (API_KEY from secrets.h): ")

    print("3) Saying hello ...")
    try:
        print("   reply:", get(base, "/jetson/hello", key=key))
    except urllib.error.HTTPError as e:
        print("   refused:", e.code, "- wrong key?")
        sys.exit(1)

    print("4) Sending a test result (it shows on the ESP32 page under 'Last plant check') ...")
    print("   reply:", get(base, "/jetson/result", key=key, label="link test", conf="1.00"))

    print("5) Heartbeat every 30 s - Ctrl+C to stop")
    try:
        while True:
            reply = get(base, "/jetson/alive", key=key)
            note = "  <- the ESP32 asks the Jetson to shut down" if reply.get("shutdown") else ""
            print(time.strftime("   %H:%M:%S"), reply, note)
            time.sleep(30)
    except KeyboardInterrupt:
        print("\nstopped - link test finished")


if __name__ == "__main__":
    main()
