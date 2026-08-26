#!/usr/bin/env bash
# Build a universal (arm64 + x86_64) libMoltenVK.dylib for bundling inside Rubidium.app.
# CI calls this before cmake configure; local macOS builds can use the same script or
# rely on Homebrew via CMake when MOLTENVK_DYLIB is not set.
#
# Usage:
#   ./scripts/build-moltenvk-universal.sh [output-dir]
#
# Writes: <output-dir>/libMoltenVK.dylib

set -euo pipefail

OUT_DIR="${1:-$(cd "$(dirname "$0")/.." && pwd)/moltenvk-universal}"
MOLTENVK_TAG="${MOLTENVK_TAG:-v1.2.10}"
MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-12.0}"
OUT_DYLIB="$OUT_DIR/libMoltenVK.dylib"

if [ -f "$OUT_DYLIB" ]
then
   echo "MoltenVK: reusing $OUT_DYLIB"
   lipo -info "$OUT_DYLIB"
   lipo -info "$OUT_DYLIB" | grep -q arm64
   lipo -info "$OUT_DYLIB" | grep -q x86_64
   exit 0
fi

mkdir -p "$OUT_DIR"
SCRATCH="$OUT_DIR/src"
rm -rf "$SCRATCH"

echo "MoltenVK: cloning $MOLTENVK_TAG into $SCRATCH"
git clone --depth 1 --branch "$MOLTENVK_TAG" https://github.com/KhronosGroup/MoltenVK.git "$SCRATCH"

cd "$SCRATCH"
echo "MoltenVK: fetching dependencies (macOS)"
./fetchDependencies --macos

echo "MoltenVK: building universal macOS Release"
xcodebuild build -quiet \
   -project MoltenVKPackaging.xcodeproj \
   -scheme "MoltenVK Package (macOS only)" \
   -configuration Release \
   ARCHS="arm64 x86_64" \
   VALID_ARCHS="arm64 x86_64" \
   ONLY_ACTIVE_ARCH=NO \
   MACOSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET"

BUILT="$(find Package -path '*/dylib/macOS/libMoltenVK.dylib' -type f 2>/dev/null | head -1)"
if [ -z "$BUILT" ] || [ ! -f "$BUILT" ]
then
   echo "::error::libMoltenVK.dylib not found under Package/ after xcodebuild"
   find Package -name 'libMoltenVK.dylib' 2>/dev/null || true
   exit 1
fi

cp "$BUILT" "$OUT_DYLIB"
echo "MoltenVK: installed $OUT_DYLIB"
lipo -info "$OUT_DYLIB"
lipo -info "$OUT_DYLIB" | grep -q arm64
lipo -info "$OUT_DYLIB" | grep -q x86_64
