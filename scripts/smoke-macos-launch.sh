#!/usr/bin/env bash
# Launch Rubidium briefly; fail if it exits early or logs a fatal init error.
# Usage: smoke-macos-launch.sh <path-to-Rubidium-binary> [seconds]

set -euo pipefail

BIN="${1:?Rubidium binary path required}"
SECONDS_WAIT="${2:-15}"
LOG="$(mktemp)"

cleanup() { rm -f "$LOG"; }
trap cleanup EXIT

"$BIN" >"$LOG" 2>&1 &
pid=$!
sleep "$SECONDS_WAIT"

if ! kill -0 "$pid" 2>/dev/null
then
   echo "::error::Rubidium exited within ${SECONDS_WAIT}s (crash or dyld failure)"
   echo "--- launch log ---"
   cat "$LOG" || true
   echo "--- end ---"
   exit 1
fi

kill -TERM "$pid" 2>/dev/null || true
sleep 1
kill -KILL "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true

echo "--- launch log ---"
cat "$LOG" || true
echo "--- end ---"

grep -q "Paths initialized" "$LOG" || {
   echo "::error::Did not reach Sneeze Paths initialized"
   exit 1
}

grep -q "Initialized (1 engine thread" "$LOG" || {
   echo "::error::Engine Initialize did not complete"
   exit 1
}

if grep -qiE "SDL_Init failed|SDL_CreateWindow failed|Failed to initialize AppFrame|Failed to initialize Sneeze|Failed to load fonts|curl_global_init failed" "$LOG"
then
   echo "::error::Fatal initialization error in launch log"
   exit 1
fi

echo "macOS launch smoke ok (still running after ${SECONDS_WAIT}s)"
