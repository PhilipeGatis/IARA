#!/usr/bin/env bash
# TEMPORARY: build and push firmware + LittleFS over OTA to the board while it
# is in AP fallback mode (join the "AquariumController" network first).
#
#   scripts/ota_ap.sh            # firmware + filesystem
#   scripts/ota_ap.sh fw         # firmware only
#   scripts/ota_ap.sh fs         # filesystem (React UI) only
#   HOST=iara.local scripts/ota_ap.sh   # any other address
#
# Commit before running: a dirty tree reports "<prev>-dirty" as the version.
set -euo pipefail

HOST="${HOST:-192.168.4.1}"
WHAT="${1:-all}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/.pio/build/esp32dev"
cd "$ROOT"

wait_up() {
  echo "==> Waiting for http://$HOST to come back..."
  for _ in $(seq 1 60); do
    if curl -s -m 3 "http://$HOST/api/status" >/dev/null; then
      echo "    up."; return 0
    fi
    sleep 2
  done
  echo "    did not come back in 120 s (it may have joined the home WiFi instead)."
  return 1
}

push() { # $1 = file, $2 = filename the device sees
  echo "==> Uploading $2 to $HOST ($(du -h "$1" | cut -f1))"
  curl --fail -m 180 -X POST -H 'X-IARA-Request: 1' \
    -F "update=@$1;filename=$2" "http://$HOST/api/ota"
  echo
}

curl -s -m 5 "http://$HOST/api/status" >/dev/null \
  || { echo "Board not reachable at $HOST. Join the AP network first."; exit 1; }

# Firmware first: the frontend build rewrites data/assets (gzip is not
# byte-for-byte reproducible), which would stamp the firmware "-dirty".
if [[ "$WHAT" == all || "$WHAT" == fw ]]; then
  echo "==> Building firmware"
  pio run -e esp32dev
fi

if [[ "$WHAT" == all || "$WHAT" == fs ]]; then
  echo "==> Building frontend"
  (cd frontend && npm run build)
  echo "==> Building LittleFS image"
  if ! pio run -e esp32dev -t buildfs; then
    # tool-mklittlefs is x86_64; pio sometimes fails with "Bad CPU type".
    arch -x86_64 ~/.platformio/packages/tool-mklittlefs/mklittlefs \
      -c data -p 256 -b 4096 -s 983040 "$BUILD/littlefs.bin"
  fi
fi

if [[ "$WHAT" == all || "$WHAT" == fs ]]; then
  push "$BUILD/littlefs.bin" littlefs.bin
  wait_up
fi

if [[ "$WHAT" == all || "$WHAT" == fw ]]; then
  push "$BUILD/firmware.bin" firmware.bin
  wait_up || true
fi

echo "==> Done."
