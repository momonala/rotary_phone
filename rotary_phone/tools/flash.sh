#!/usr/bin/env bash
set -euo pipefail

firmware="$(cd "$(dirname "$0")/../firmware" && pwd)"
sketch="$firmware/${1:-rotary_phone}"
fqbn=esp8266:esp8266:d1_mini
port=$(ls /dev/cu.usbserial-* 2>/dev/null | head -1 || true)
[ -n "$port" ] || { echo "No /dev/cu.usbserial-* port: plug in the D1 Mini." >&2; exit 1; }

ssid=$(security find-generic-password -s rotary_phone_wifi_ssid -w)
pass=$(security find-generic-password -s rotary_phone_wifi_pass -w)

arduino-cli compile --fqbn "$fqbn" \
  --build-property "build.extra_flags=-DWIFI_SSID=\"$ssid\" -DWIFI_PASS=\"$pass\"" "$sketch"
arduino-cli upload --fqbn "$fqbn" -p "$port" "$sketch"
