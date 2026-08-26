#!/usr/bin/env bash
# Run codesign with a wall-clock timeout (GHA has no GNU timeout by default).
# Usage: codesign-with-timeout.sh <seconds> codesign [args...]

set -euo pipefail

_date() {
   date -u '+%Y-%m-%dT%H:%M:%SZ'
}

TIMEOUT_SEC="${1:?timeout seconds required}"
shift

if [ "$#" -lt 1 ]
then
   echo "::error::codesign-with-timeout: missing codesign command"
   exit 1
fi

_label="codesign $*"
echo "[$(_date)] starting (${TIMEOUT_SEC}s max): ${_label}"

"$@" &
_pid=$!
_killer="$(mktemp)"
trap 'rm -f "$_killer"; kill "$_pid" 2>/dev/null || true' EXIT

(
   sleep "$TIMEOUT_SEC"
   if kill -0 "$_pid" 2>/dev/null
   then
      echo "::error::timed out after ${TIMEOUT_SEC}s: ${_label}"
      kill -9 "$_pid" 2>/dev/null || true
   fi
) &
echo $! > "$_killer"

set +e
wait "$_pid"
_rc=$?
set -e

kill "$(cat "$_killer")" 2>/dev/null || true
rm -f "$_killer"
trap - EXIT

if [ "$_rc" -eq 0 ]
then
   echo "[$(_date)] finished: ${_label}"
fi

exit "$_rc"
