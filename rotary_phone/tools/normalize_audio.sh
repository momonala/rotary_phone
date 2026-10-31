#!/usr/bin/env bash
set -euo pipefail

media="$(cd "$(dirname "$0")/../media" && pwd)"
src="${1:-$media/01}"
out="${2:-$media/01_normalized}"
call_lufs=-33.6
robot_lufs=-10.8
robot_tracks="012 013 014 097 098"

mkdir -p "$out"
for f in "$src"/*.mp3; do
  name=$(basename "$f")
  target=$call_lufs
  [[ " $robot_tracks " == *" ${name:0:3} "* ]] && target=$robot_lufs
  loudness=$(ffmpeg -nostats -hide_banner -i "$f" -af ebur128 -f null - 2>&1 \
    | awk '/Integrated loudness:/{getline; print $2}' | tail -1)
  gain=$(awk -v t="$target" -v l="$loudness" 'BEGIN { printf "%.2f", t - l }')
  ffmpeg -nostats -hide_banner -loglevel error -y -i "$f" \
    -af "volume=${gain}dB,alimiter=limit=0.89:level=false" \
    -ar 44100 -c:a libmp3lame -b:a 128k "$out/$name"
  printf "%-50s %7s LUFS -> %s (%+.1f dB)\n" "$name" "$loudness" "$target" "$gain"
done
