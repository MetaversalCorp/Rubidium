#!/usr/bin/env bash
# Verify Rubidium.app bundle dylibs: universal slices + no CI/Homebrew load paths.
# Usage: verify-macos-bundle-dylibs.sh <path-to-Rubidium.app>

set -euo pipefail

APP="${1:?Rubidium.app path required}"
MACOS="$APP/Contents/MacOS"
BIN="$MACOS/Rubidium"

[ -f "$BIN" ] || { echo "::error::missing $BIN"; exit 1; }

echo "CFBundleExecutable=$(plutil -extract CFBundleExecutable raw "$APP/Contents/Info.plist")"

ARCHS="$(lipo -info "$BIN")"
echo "$ARCHS"
echo "$ARCHS" | grep -q arm64
echo "$ARCHS" | grep -q x86_64

for lib in libwasmtime.dylib libanari.0.dylib libanari_library_halogen.dylib libMoltenVK.dylib
do
   f="$MACOS/$lib"
   [ -f "$f" ] || { echo "::error::Missing $lib in bundle"; exit 1; }
   echo "=== lipo $lib ==="
   lipo -info "$f" | tee /dev/stderr | grep -q arm64
   lipo -info "$f" | grep -q x86_64
done

# Check each universal slice separately — fat binaries can be clean on arm64
# but still reference CI paths on x86_64.
check_no_foreign_paths() {
   local f="$1"
   echo "=== otool -L $(basename "$f") ==="
   otool -L "$f" | head -50
   local arch
   for arch in arm64 x86_64
   do
      if otool -L -arch "$arch" "$f" 2>/dev/null | grep $'^\t' | grep -qE '/Users/runner|/opt/homebrew|/usr/local/Cellar'
      then
         echo "::error::$(basename "$f") ($arch) LC_LOAD_DYLIB still references CI/Homebrew paths"
         otool -L -arch "$arch" "$f" | grep $'^\t' | grep -E '/Users/runner|/opt/homebrew|/usr/local/Cellar' || true
         return 1
      fi
   done
}

for f in "$BIN" "$MACOS"/*.dylib
do
   check_no_foreign_paths "$f"
done

echo "Bundle dylib verification ok"
