#!/usr/bin/env bash
# Build a DragNDrop-style DMG (Rubidium.app + Applications symlink) without mounting.
# Usage: create-macos-dmg.sh <Rubidium.app> <output.dmg> [volume-name]

set -euo pipefail

APP="${1:?path to Rubidium.app required}"
OUTPUT="${2:?output .dmg path required}"
VOLUME_NAME="${3:-Rubidium}"

if [ ! -d "$APP" ]
then
   echo "::error::Not a bundle: $APP"
   exit 1
fi

STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT

ditto "$APP" "$STAGING/$(basename "$APP")"
ln -s /Applications "$STAGING/Applications"

mkdir -p "$(dirname "$OUTPUT")"
rm -f "$OUTPUT"

hdiutil create \
   -volname "$VOLUME_NAME" \
   -srcfolder "$STAGING" \
   -ov \
   -format UDZO \
   -imagekey zlib-level=9 \
   "$OUTPUT"

echo "Created DMG: $OUTPUT"
