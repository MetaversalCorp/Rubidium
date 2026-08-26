#!/usr/bin/env bash
# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Downloads GitHub Actions artifacts from a Build workflow run, stages release
# packages via scripts/layout-cdn-artifacts.sh, registers each in pkg/manifest.json.
#
# Usage:
#   MANIFEST_CDN_URL=https://cdn.example.com/rubidium/ \
#     scripts/register-manifest-ci.sh <build_run_id>
#
# Expects repo root as cwd; requires gh, openssl dev libs only at compile time for GM.

set -euo pipefail

if [[ $# -lt 1 ]] || [[ -z "${1:-}" ]]
then
   echo "Usage: MANIFEST_CDN_URL=<base/> $0 <build_run_id>" >&2
   exit 1
fi

RUN_ID="$1"
REPO="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY not set}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

VERSION="$(tr -d ' \r\n' < VERSION)"

GM="$ROOT/gm-build/GenerateManifest"
if [[ ! -x "$GM" ]]
then
   echo "GenerateManifest not found at $GM — build pkg/ CMake project first." >&2
   exit 1
fi

ARTIFACT_ROOT="$ROOT/_manifest-artifacts"
rm -rf "$ARTIFACT_ROOT"
mkdir -p "$ARTIFACT_ROOT"

for SPEC in rubidium-linux-x64 rubidium-macos-universal-dmg
do
   mkdir -p "$ARTIFACT_ROOT/$SPEC"
   if gh run download "$RUN_ID" --repo "$REPO" -n "$SPEC" -D "$ARTIFACT_ROOT/$SPEC"
   then
      echo "  artifact ok: $SPEC"
   else
      echo "  (skip) artifact missing on run: $SPEC"
   fi
done

STAGING="$ROOT/_manifest-staging"
rm -rf "$STAGING"
bash "$ROOT/scripts/layout-cdn-artifacts.sh" "$ARTIFACT_ROOT" "$STAGING" "$VERSION"

REL="$STAGING/rubidium/releases/$VERSION"
bash "$ROOT/scripts/register-manifest-staged.sh" "$REL" "$ROOT"
