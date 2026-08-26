#!/usr/bin/env bash
# Sign Rubidium.app for Developer ID + hardened runtime (notarization-ready).
# One codesign --deep pass signs every nested Mach-O (faster than per-file signing).
# Usage: sign-macos-app.sh <path-to-Rubidium.app> <signing-identity> [keychain-path]
#
# Env:
#   RUBIDIUM_NOTARIZE_MACOS=true|false — add --timestamp when true (VERSION push
#   or manual Notarize checked). Omit timestamp on sign-only test builds.

set -euo pipefail

APP="${1:?Rubidium.app path required}"
IDENTITY="${2:?codesign identity required}"
KEYCHAIN="${3:-${RUBIDIUM_SIGNING_KEYCHAIN:-}}"

if [ ! -d "$APP" ]
then
   echo "::error::Not a bundle: $APP"
   exit 1
fi

MACOS="$APP/Contents/MacOS"
if [ ! -d "$MACOS" ]
then
   echo "::error::Missing Contents/MacOS in $APP"
   exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=../branding/read-product.sh
. "$REPO_ROOT/branding/read-product.sh"
ENTITLEMENTS="${RUBIDIUM_MACOS_ENTITLEMENTS:-$REPO_ROOT/pkg/macos/Rubidium.entitlements}"
BUNDLE_COSIGN_TIMEOUT="${CODESIGN_BUNDLE_TIMEOUT_SEC:-900}"
NOTARIZE="${RUBIDIUM_NOTARIZE_MACOS:-false}"

export TMPDIR="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"

_keychain_args=()
if [ -n "$KEYCHAIN" ]
then
   _keychain_args=(--keychain "$KEYCHAIN")
   if [ -n "${RUBIDIUM_SIGNING_KEYCHAIN_PASS:-}" ]
   then
      security unlock-keychain -p "$RUBIDIUM_SIGNING_KEYCHAIN_PASS" "$KEYCHAIN" 2>/dev/null || true
      security set-key-partition-list -S apple-tool:,apple: -s \
         -k "$RUBIDIUM_SIGNING_KEYCHAIN_PASS" "$KEYCHAIN" 2>/dev/null || true
   fi
fi

_timestamp_args=()
if [ "$NOTARIZE" = "true" ] || [ "$NOTARIZE" = "1" ]
then
   _timestamp_args=(--timestamp)
fi

_codesign_args=(codesign --force --deep --options runtime)
if [ -f "$ENTITLEMENTS" ]
then
   _codesign_args+=(--entitlements "$ENTITLEMENTS")
else
   echo "::warning::Missing entitlements plist at $ENTITLEMENTS — Wasmtime JIT will crash under hardened runtime."
fi
if [ "${#_timestamp_args[@]}" -gt 0 ]
then
   _codesign_args+=("${_timestamp_args[@]}")
fi
if [ "${#_keychain_args[@]}" -gt 0 ]
then
   _codesign_args+=("${_keychain_args[@]}")
fi
_codesign_args+=(--sign "$IDENTITY" "$APP")

_n_mach_o=0
for _path in "$MACOS"/*.dylib "$MACOS/$PRODUCT_NAME" "$MACOS/$PRODUCT_NAME_SETUP"
do
   [ -f "$_path" ] || continue
   if file -b "$_path" | grep -q 'Mach-O'
   then
      _n_mach_o=$((_n_mach_o + 1))
   fi
done

echo "Preparing $(basename "$APP") for Developer ID signing (${_n_mach_o} nested Mach-O)..."
xattr -cr "$APP" 2>/dev/null || true
codesign --remove-signature "$APP" 2>/dev/null || true

echo "Signing app bundle (--deep$([ ${#_timestamp_args[@]} -gt 0 ] && echo ', +timestamp' || echo ', no timestamp'))..."
if ! bash "$SCRIPT_DIR/codesign-with-timeout.sh" "$BUNDLE_COSIGN_TIMEOUT" \
   "${_codesign_args[@]}"
then
   echo "::error::codesign failed on app bundle: $APP"
   exit 1
fi

codesign --verify --deep --verbose=2 "$APP"
echo "codesign verify ok"
