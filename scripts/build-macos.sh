#!/usr/bin/env bash
# macOS universal build. Mac host required.
#
# Default: compile + link Rubidium only. Plain `cmake --build` against the
# Rubidium build tree. No dep checks, no configure step. Fails naturally if
# the tree or the dep libraries aren't there yet.
#
# The Rubidium src tree is a SINGLE multi-config tree at
#   builds/macos-<arch>/build/
# that emits Debug or Release into
#   builds/macos-<arch>/install/{debug,release}/bin/
# depending on the --config flag (which drives `cmake --build --config`).
# Uses the Ninja Multi-Config generator so one build tree carries both
# configurations and --config just selects at build time. (To open in Xcode
# IDE instead, override by passing `-G Xcode` via extra args and configuring
# manually; the default path here is CLI/Ninja for parity with Linux.)
#
# The DEPS trees stay per-config (deps/builds/macos-<arch>/{debug,release}/)
# and both must be built on disk before you can build a config whose deps
# don't exist yet.
#
# Flags switch the script into deps mode or deps+Rubidium mode:
#
#   --deps         Build SDL3 into deps/builds/macos-<arch>/<config>/libs/.
#   --fresh        Reconfigure the Rubidium tree from scratch (cmake -S src --fresh).
#                  Does NOT build -- just regenerates the project files.
#                  Wipes CMakeCache.txt + CMakeFiles/ so stale cached values
#                  can't linger. Deps tree is never touched.
#                  Requires CMake >= 3.24.
#                  Compose with --rebuild to reconfigure AND build:
#                  --fresh --rebuild => fresh configure + clean + build.
#   --all          Build deps, then configure + build Rubidium.
#   --only <dep>   Build a single dep (implies deps-targeting).
#   --list         Show dep stamp cache.
#   --rebuild      Modifier: force a full rebuild of whatever target(s) are
#                  selected by the other flags, regardless of prior build state.
#                  NEVER crosses the src <-> deps wall on its own. Matrix:
#                    --rebuild                  scrub + rebuild Rubidium only
#                    --rebuild --deps           scrub + rebuild all deps
#                    --rebuild --only <dep>     scrub + rebuild one dep
#                    --rebuild --all            scrub + rebuild deps, then Rubidium
#                  Source clones in deps/repos/ are never scrubbed.
#
# HARD RULE: the deps folder (deps/builds/<platform>/<config>/) may only be
# modified when --deps, --only, or --all is present on the command line. An
# Rubidium-only invocation (anything else, including --fresh or --rebuild alone)
# cannot touch a single bit inside deps/.
#
# The deps tree (deps/CMakeLists.txt) and the Rubidium tree (src/CMakeLists.txt)
# are two completely independent CMake projects. They share nothing. This
# script is the only glue: in --all mode it builds deps, then configures +
# builds Rubidium in a separate CMake invocation.
#
# Debug and Release live in fully separate DEPS trees under
# deps/builds/macos-<arch>/{debug,release}/ but share a single Rubidium build
# tree at builds/macos-<arch>/build/ and distinct install trees at
# builds/macos-<arch>/install/{debug,release}/.
# The platform slug is macos-universal (arm64 + x86_64 fat binaries).
# Host CPU only affects build speed, not the output arch set.
#
# Usage:
#   ./scripts/build-macos.sh                      # Rubidium (Release)
#   ./scripts/build-macos.sh --config Debug       # Rubidium (Debug)
#   ./scripts/build-macos.sh --fresh              # Reconfigure only (no build)
#   ./scripts/build-macos.sh --deps               # Deps only
#   ./scripts/build-macos.sh --all                # Deps, then Rubidium
#   ./scripts/build-macos.sh --only sdl3          # Rebuild one dep

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RUBIDIUM_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SNEEZE_DIR="${SNEEZE_DIR:-$(cd "$RUBIDIUM_DIR/../Sneeze" && pwd)}"

# --sync and --verify apply to Sneeze's third-party deps (sneeze-sdk, rmap, map,
# etc.), not Rubidium's SDL3/fonts tree. Forward so `./scripts/build-macos.sh
# --sync` works from the Rubidium repo too.
for _arg in "$@"; do
   if [[ "$_arg" == "--sync" || "$_arg" == "--verify" ]]; then
      if [[ ! -x "$SNEEZE_DIR/scripts/build-macos.sh" ]]; then
         echo "Sneeze build script not found or not executable:" >&2
         echo "  $SNEEZE_DIR/scripts/build-macos.sh" >&2
         echo "Restore it from git in the Sneeze repo:" >&2
         echo "  cd \"$SNEEZE_DIR\" && git checkout HEAD -- scripts/build-macos.sh scripts/build-deps.sh" >&2
         exit 1
      fi
      echo "==> Forwarding to Sneeze (deps sync/verify): $SNEEZE_DIR/scripts/build-macos.sh $*"
      exec "$SNEEZE_DIR/scripts/build-macos.sh" "$@"
   fi
done

CONFIG="Release"
DEPS=0
ALL=0
FRESH=0
REBUILD=0       # modifier; composes with other flags, does not imply deps mode
DEPS_FORWARD=0  # --only / --list set this (they target deps/)
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
   case "$1" in
      --deps)         DEPS=1 ;;
      --all)          ALL=1 ;;
      --fresh)        FRESH=1 ;;
      --rebuild)      REBUILD=1 ;;
      --config)       shift; CONFIG="$1" ;;
      --config=*)     CONFIG="${1#--config=}" ;;
      --only|--list)  DEPS_FORWARD=1; EXTRA_ARGS+=("$1") ;;
      --only=*)       DEPS_FORWARD=1; EXTRA_ARGS+=("$1") ;;
      *)              EXTRA_ARGS+=("$1") ;;
   esac
   shift
done

MODE_COUNT=$((DEPS + ALL + FRESH))
if [[ $MODE_COUNT -gt 1 ]]; then
   echo "--deps, --all, and --fresh are mutually exclusive" >&2
   exit 1
fi

case "$CONFIG" in
   Debug|Release) : ;;
   *) echo "--config must be Debug or Release (got '$CONFIG')" >&2; exit 1 ;;
esac

PLATFORM="macos-universal"

CFG_LOWER="$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')"
DEPS_BUILD_DIR="$RUBIDIUM_DIR/deps/builds/$PLATFORM/$CFG_LOWER/build"
RUBIDIUM_LIBS_DIR="$RUBIDIUM_DIR/deps/builds/$PLATFORM/$CFG_LOWER/libs"
SNEEZE_LIBS_DIR="$SNEEZE_DIR/deps/builds/$PLATFORM/$CFG_LOWER/libs"
# Single multi-config Rubidium tree. --config only drives `cmake --build --config`.
RUBIDIUM_OUT_DIR="$RUBIDIUM_DIR/builds/$PLATFORM"
RUBIDIUM_BUILD_DIR="$RUBIDIUM_OUT_DIR/build"
RUBIDIUM_INSTALL_DIR="$RUBIDIUM_OUT_DIR/install/$CFG_LOWER"

MACOS_ARGS=(
   -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
   -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
)

# --rebuild is a modifier, not a mode: it does NOT imply deps-targeting.
# HARD RULE: if none of --deps, --only, or --all is set, the deps folder
# must not be touched -- regardless of what --rebuild / --fresh are doing.
# (--list is read-only and handled inside build-deps.sh.)
DEPS_MODE=0
if [[ $DEPS -eq 1 || $ALL -eq 1 || $DEPS_FORWARD -eq 1 ]]; then
   DEPS_MODE=1
fi

RUBIDIUM_MODE=0
if [[ $DEPS_MODE -eq 0 || $ALL -eq 1 ]]; then
   RUBIDIUM_MODE=1
fi

# --rebuild forwards to build-deps.sh only when deps are actually in scope.
# When Rubidium-only, --rebuild is handled entirely below (wipe Rubidium tree).
if [[ $DEPS_MODE -eq 1 && $REBUILD -eq 1 ]]; then
   EXTRA_ARGS+=(--rebuild)
fi

# Reconfigure the Rubidium tree before building. Implied by --all and --fresh.
# --rebuild does NOT force reconfigure any more: it cleans via `cmake --build
# --target clean` which preserves the configured tree (CMakeCache, CMakeFiles,
# generated project files), so the IDE doesn't lose state. Exception: if
# --rebuild targets Rubidium but the tree has never been configured, fall back
# to configuring it -- otherwise the subsequent build would fail with a
# cryptic "CMakeCache.txt missing" error.
RECONFIGURE=0
if [[ $ALL -eq 1 || $FRESH -eq 1 ]]; then
   RECONFIGURE=1
fi
if [[ $REBUILD -eq 1 && $RUBIDIUM_MODE -eq 1 && ! -f "$RUBIDIUM_BUILD_DIR/CMakeCache.txt" ]]; then
   RECONFIGURE=1
fi

# Shared configure args for Rubidium (also used to repair a broken PCH tree).
RUBIDIUM_CONFIGURE_ARGS=(
   -G "Ninja Multi-Config"
   "${MACOS_ARGS[@]}"
   -DSNEEZE_DIR="$SNEEZE_DIR"
   -DSNEEZE_LIBS_DIR="$SNEEZE_LIBS_DIR"
   -DRUBIDIUM_LIBS_DIR="$RUBIDIUM_LIBS_DIR"
   -DRUBIDIUM_CONFIG="$CONFIG"
   -DRUBIDIUM_PLATFORM="$PLATFORM"
   -DRUBIDIUM_BUILD_ROOT="$RUBIDIUM_OUT_DIR"
)

# CMake generates cmake_pch*.hxx at configure time. Deleting those files (or
# their .objcxx counterparts) leaves Ninja with no rule to recreate them.
# Re-run configure when the build tree exists but the PCH headers are gone.
ensure_rubidium_pch () {
   if [[ ! -f "$RUBIDIUM_BUILD_DIR/CMakeCache.txt" ]]; then
      return
   fi

   local pch_dir="$RUBIDIUM_BUILD_DIR/CMakeFiles/Rubidium.dir"

   if [[ ! -d "$pch_dir" ]]; then
      return
   fi

   if find "$pch_dir" -maxdepth 4 -name 'cmake_pch*.hxx' -print -quit 2>/dev/null | grep -q .; then
      return
   fi

   echo ""
   echo "==> CMake precompiled-header files missing; reconfiguring $RUBIDIUM_BUILD_DIR"
   cmake -S "$RUBIDIUM_DIR/src" -B "$RUBIDIUM_BUILD_DIR" "${RUBIDIUM_CONFIGURE_ARGS[@]}"
}

# ---------------------------------------------------------------------------
# Deps mode
# ---------------------------------------------------------------------------

if [[ $DEPS_MODE -eq 1 ]]; then
   echo "==> macOS universal deps build ($PLATFORM host, arm64+x86_64 output, $CONFIG)"

   "$SCRIPT_DIR/build-deps.sh" \
      --config "$CONFIG" \
      --platform "$PLATFORM" \
      --build-dir "$DEPS_BUILD_DIR" \
      --libs-dir "$RUBIDIUM_LIBS_DIR" \
      "${MACOS_ARGS[@]}" \
      "${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}"
fi

# ---------------------------------------------------------------------------
# Rubidium mode -- configure (if --all / --fresh / --rebuild) + `cmake --build`.
# ---------------------------------------------------------------------------

if [[ $FRESH -eq 1 || $RUBIDIUM_MODE -eq 1 ]]; then
   # --rebuild with Rubidium in scope: clean only the CURRENT config's compiled
   # artifacts via `cmake --build --target clean --config <cfg>`. This preserves
   # the configured CMake tree (CMakeCache.txt, CMakeFiles/, generated project
   # files) so the IDE doesn't lose state, and it preserves the OTHER config's
   # intermediates and install tree. The selected config's install/<cfg>/ is
   # also wiped so stale binaries don't survive the rebuild.
   if [[ $REBUILD -eq 1 && $RUBIDIUM_MODE -eq 1 ]]; then
      if [[ -f "$RUBIDIUM_BUILD_DIR/CMakeCache.txt" ]]; then
         echo ""
         echo "==> Cleaning Rubidium $CONFIG build artifacts"
         cmake --build "$RUBIDIUM_BUILD_DIR" --target clean --config "$CONFIG"
      fi
      echo "==> Scrubbing Rubidium $CONFIG install: $RUBIDIUM_INSTALL_DIR"
      rm -rf "$RUBIDIUM_INSTALL_DIR"
   fi

   if [[ $RECONFIGURE -eq 1 ]]; then
      # --fresh (CMake 3.24+) wipes CMakeCache.txt + CMakeFiles/ before
      # reconfiguring -- makes --fresh the explicit "start over" path while
      # --all keeps the idempotent cache update for normal reconfigures.
      FRESH_ARG=()
      if [[ $FRESH -eq 1 ]]; then FRESH_ARG=(--fresh); fi

      echo ""
      echo "==> Configuring Rubidium tree at $RUBIDIUM_BUILD_DIR"
      # Ninja Multi-Config: one build tree, both Debug and Release selectable
      # via `cmake --build --config`. --config here seeds RUBIDIUM_CONFIG so
      # find_package resolves against the right deps tree at configure time;
      # actual emission per invocation is decided by --build --config below.
      cmake -S "$RUBIDIUM_DIR/src" -B "$RUBIDIUM_BUILD_DIR" \
         "${FRESH_ARG[@]+"${FRESH_ARG[@]}"}" \
         "${RUBIDIUM_CONFIGURE_ARGS[@]}"
   fi

   if [[ $FRESH -eq 1 && $REBUILD -eq 0 ]]; then
      echo ""
      echo "==> Reconfigure complete (no build)"
   else
      ensure_rubidium_pch

      echo ""
      echo "==> Building Rubidium ($PLATFORM, $CONFIG)"
      cmake --build "$RUBIDIUM_BUILD_DIR" --config "$CONFIG"
      echo "==> Rubidium macOS build complete ($CONFIG)"
      echo "    Rubidium.app -> $RUBIDIUM_INSTALL_DIR/bin"
   fi
fi
