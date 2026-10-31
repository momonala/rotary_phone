#!/usr/bin/env bash
set -euo pipefail

payload="${1:-ring}"
ssh pi-cloud mosquitto_pub -t home/rotary_phone -m "$payload"
