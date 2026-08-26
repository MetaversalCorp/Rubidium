#!/usr/bin/env bash
# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Assembles the CDN folder layout (matches pkg/manifest.json URL paths from
# generate_manifest: .../releases/<ver>/... and ci-windows.ps1 upload targets).
#   rubidium/manifest.json
#   rubidium/download/   — "latest" stubs (Rubidium.dmg, Rubidium.tar.gz, Rubidium.apk, …)
#   rubidium/releases/<version>/ — versioned installers
#
# Buru SFTP: if your server already has Release/Download with capitals, rename to
# lowercase or map URLs so manifest URLs stay releases/download (case-sensitive hosts).
#
# Usage:
#   layout-cdn-artifacts.sh <artifact_root> <output_parent> <version> [manifest_path]
#
# <artifact_root> — parent of per-artifact folders from `gh run download`, e.g.:
#     artifact_root/rubidium-linux-x64/...
#     artifact_root/rubidium-macos-universal-dmg/*.dmg
# <output_parent> — receives a new `rubidium/` directory under it.
# <version>       — semantic version from VERSION (e.g. 0.0.3), no v prefix.
# [manifest_path] — optional path to manifest.json to copy to rubidium/manifest.json.
#
# Optional environment (Jenkins / manual Windows packages, not from GitHub Actions):
#   WIN_INSTALLER_SRC — path to Rubidium-<ver>-windows-x64.exe (NSIS)
#   WIN_SETUP_SRC     — path to RubidiumSetup.exe for download/RubidiumSetup.exe
#
# Expected GitHub artifact folder names (.github/workflows/build.yml):
#   rubidium-linux-x64, rubidium-macos-universal-dmg, rubidium-android-apk, rubidium-quest-apk

set -euo pipefail

if [[ $# -lt 3 ]]
then
   echo "Usage: $0 <artifact_root> <output_parent> <version> [manifest_path]" >&2
   exit 1
fi

ARTIFACT_ROOT="$1"
OUTPUT_PARENT="$2"
VERSION="$3"
MANIFEST_SRC="${4:-}"

OUT="${OUTPUT_PARENT}/rubidium"
REL="${OUT}/releases/${VERSION}"
DL="${OUT}/download"

mkdir -p "$REL" "$DL"

if [[ -n "$MANIFEST_SRC" ]] && [[ -f "$MANIFEST_SRC" ]]
then
   cp -f "$MANIFEST_SRC" "${OUT}/manifest.json"
fi

# --- Optional: Windows installer (Jenkins / manual) ---------------------------------

if [[ -n "${WIN_INSTALLER_SRC:-}" ]] && [[ -f "$WIN_INSTALLER_SRC" ]]
then
   WIN_NAME="Rubidium-${VERSION}-windows-x64.exe"
   cp -f "$WIN_INSTALLER_SRC" "${REL}/${WIN_NAME}"
   if [[ -n "${WIN_SETUP_SRC:-}" ]] && [[ -f "${WIN_SETUP_SRC}" ]]
   then
      cp -f "${WIN_SETUP_SRC}" "${DL}/RubidiumSetup.exe"
   fi
fi

# --- macOS DMG ----------------------------------------------------------------------

MAC_DMG_DIR="${ARTIFACT_ROOT}/rubidium-macos-universal-dmg"
DMG=""
if [[ -d "$MAC_DMG_DIR" ]]
then
   DMG=$(find "$MAC_DMG_DIR" -type f \( -name 'Rubidium-*-macos-universal.dmg' -o -name 'Rubidium-*-macos-arm64.dmg' -o -name 'Rubidium-*.dmg' \) -print 2>/dev/null | head -1 || true)
fi
if [[ -z "$DMG" ]]
then
   DMG=$(find "$ARTIFACT_ROOT" -type f \( -name 'Rubidium-*-macos-universal.dmg' -o -name 'Rubidium-*-macos-arm64.dmg' -o -name 'Rubidium-*.dmg' \) -print 2>/dev/null | head -1 || true)
fi
if [[ -n "$DMG" ]]
then
   DMG_BASE="$(basename "$DMG")"
   cp -f "$DMG" "${REL}/${DMG_BASE}"
   cp -f "$DMG" "${DL}/Rubidium.dmg"
fi

# --- Linux TGZ (CPack-style name; CI uploads loose binaries) ------------------------

LINUX_BASE="${ARTIFACT_ROOT}/rubidium-linux-x64"
LINUX_ART=""
if [[ -d "$LINUX_BASE" ]]
then
   LINUX_ART=$(find "$LINUX_BASE" -type f -name Rubidium -print 2>/dev/null | head -1 || true)
fi
if [[ -n "$LINUX_ART" ]] && [[ -f "$LINUX_ART" ]]
then
   LINUX_DIR="$(dirname "$LINUX_ART")"
   TGZ_NAME="Rubidium-${VERSION}-linux-x64.tar.gz"
   PACK=$(mktemp -d)
   mkdir -p "${PACK}/Rubidium/bin"
   # CI uploads the full install/release/bin/ tree (Rubidium + .so + fonts).
   # Match CPack layout: Rubidium/bin/<runtime bundle>, not the bare binary alone.
   cp -a "${LINUX_DIR}/." "${PACK}/Rubidium/bin/"
   tar -czf "${REL}/${TGZ_NAME}" -C "$PACK" Rubidium
   cp -f "${REL}/${TGZ_NAME}" "${DL}/Rubidium.tar.gz"
   rm -rf "$PACK"
fi

# --- Android APK --------------------------------------------------------------------

ANDROID_DIR="${ARTIFACT_ROOT}/rubidium-android-apk"
ANDROID_APK=""
if [[ -d "$ANDROID_DIR" ]]
then
   ANDROID_APK=$(find "$ANDROID_DIR" -type f -name 'rubidium-android-arm64.apk' -print 2>/dev/null | head -1 || true)
fi
if [[ -z "$ANDROID_APK" ]]
then
   ANDROID_APK=$(find "$ARTIFACT_ROOT" -type f -name 'rubidium-android-arm64.apk' -print 2>/dev/null | head -1 || true)
fi
if [[ -n "$ANDROID_APK" ]]
then
   APK_NAME="Rubidium-${VERSION}-android-arm64.apk"
   cp -f "$ANDROID_APK" "${REL}/${APK_NAME}"
   cp -f "$ANDROID_APK" "${DL}/Rubidium.apk"
fi

# --- Quest APK ----------------------------------------------------------------------

QUEST_DIR="${ARTIFACT_ROOT}/rubidium-quest-apk"
QUEST_APK=""
if [[ -d "$QUEST_DIR" ]]
then
   QUEST_APK=$(find "$QUEST_DIR" -type f -name 'rubidium-quest.apk' -print 2>/dev/null | head -1 || true)
fi
if [[ -z "$QUEST_APK" ]]
then
   QUEST_APK=$(find "$ARTIFACT_ROOT" -type f -name 'rubidium-quest.apk' -print 2>/dev/null | head -1 || true)
fi
if [[ -n "$QUEST_APK" ]]
then
   QAPK_NAME="Rubidium-${VERSION}-quest-arm64.apk"
   cp -f "$QUEST_APK" "${REL}/${QAPK_NAME}"
fi

echo "CDN staging layout at ${OUT}:"
find "$OUT" -type f -print 2>/dev/null | sort || true
