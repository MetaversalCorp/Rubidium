#!/usr/bin/env bash
# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Registers staged CDN release packages into pkg/manifest.json via GenerateManifest,
# then promotes each registered platform to the stable channel (current.stable).
#
# Usage:
#   MANIFEST_CDN_URL=<base/> \
#     scripts/register-manifest-staged.sh <releases_version_dir> <repo_root>
#
# <releases_version_dir> — directory containing Rubidium-<ver>-*.tar.gz|.dmg|.apk
#   (e.g. cdn-staging/rubidium/releases/0.0.4 after layout-cdn-artifacts.sh).
# <repo_root> — repo root (pkg/manifest.json, gm-build/GenerateManifest).
# VERSION is read from <repo_root>/VERSION.

set -euo pipefail

if [[ $# -lt 2 ]] || [[ -z "${1:-}" ]] || [[ -z "${2:-}" ]]
then
   echo "Usage: MANIFEST_CDN_URL=<base/> $0 <releases_version_dir> <repo_root>" >&2
   exit 1
fi

RELEASE_DIR_RAW="$1"
if [[ ! -d "$RELEASE_DIR_RAW" ]]
then
   echo "No staged releases directory — nothing to register."
   exit 0
fi

RELEASE_DIR="$(cd "$RELEASE_DIR_RAW" && pwd)"
ROOT="$(cd "$2" && pwd)"
CDN_URL="${MANIFEST_CDN_URL:-https://cdn.rp1.com/rubidium/}"

VERSION="$(tr -d ' \r\n' < "${ROOT}/VERSION")"

GM="${ROOT}/gm-build/GenerateManifest"
if [[ ! -x "$GM" ]]
then
   echo "GenerateManifest not found at $GM — build pkg/ CMake project first." >&2
   exit 1
fi

if [[ "${CDN_URL}" != */ ]]
then
   CDN_URL="${CDN_URL}/"
fi

CHANNEL="${MANIFEST_CHANNEL:-stable}"
MANIFEST="${ROOT}/pkg/manifest.json"
NOTES_FILE="${ROOT}/RELEASE"
PLATFORMS=()

if [[ ! -f "$NOTES_FILE" ]]
then
   echo "Release notes file not found: $NOTES_FILE" >&2
   exit 1
fi

shopt -s nullglob
for FPATH in "$RELEASE_DIR"/*
do
   [[ -f "$FPATH" ]] || continue
   BASE="$(basename "$FPATH")"
   PLAT=""
   case "$BASE" in
      *-linux-x64.tar.gz)        PLAT="linux-x64" ;;
      *-macos-universal.dmg)     PLAT="macos-universal" ;;
      *-macos-arm64.dmg)         PLAT="macos-arm64" ;;
      *-android-arm64.apk)       PLAT="android-arm64" ;;
      *-quest-arm64.apk)         PLAT="quest-arm64" ;;
      *)                         echo "  skip unknown file: $BASE"; continue ;;
   esac

   echo "  register $PLAT <- $BASE"
   "$GM" register "$FPATH" "$VERSION" "$PLAT" "$CDN_URL" "$MANIFEST" "$NOTES_FILE"
   PLATFORMS+=("$PLAT")
done

if [[ "${#PLATFORMS[@]}" -eq 0 ]]
then
   echo "No recognized packages in $RELEASE_DIR — manifest not updated." >&2
   echo "Expected Rubidium-<ver>-linux-x64.tar.gz, *-macos-universal.dmg, *-android-arm64.apk, *-quest-arm64.apk" >&2
   exit 1
fi

echo "Registered ${#PLATFORMS[@]} platform(s) for version $VERSION."

if [[ "${SKIP_PROMOTE:-}" == "1" ]]
then
   echo "SKIP_PROMOTE=1 — current.${CHANNEL} not updated."
   exit 0
fi

for PLAT in "${PLATFORMS[@]}"
do
   echo "  promote $PLAT -> ${CHANNEL} @ $VERSION"
   "$GM" promote "$VERSION" "$PLAT" "$CHANNEL" "$MANIFEST"
done

echo "Promoted ${#PLATFORMS[@]} platform(s) to current.${CHANNEL}."
