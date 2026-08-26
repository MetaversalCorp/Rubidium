#!/usr/bin/env bash
# Rewrite LC_LOAD_DYLIB entries in a universal macOS bundle so every arch slice
# points at @executable_path/<basename> for dylibs shipped in Contents/MacOS/.
# Universal fat binaries can retain per-slice absolute CI paths after a single
# install_name_tool -change (e.g. x86_64 vs arm64 Wasmtime install names).

set -euo pipefail

BIN="${1:?Rubidium binary path required}"
MACOS="$(dirname "$BIN")"

rewrite_one() {
   local f="$1"
   local pass=0
   while [ "$pass" -lt 6 ]
   do
      local changed=0
      while IFS= read -r dep
      do
         case "$dep" in
            /usr/lib/*|/System/*) continue ;;
            @executable_path/*|@rpath/*|@loader_path/*) continue ;;
         esac
         local name
         name="$(basename "$dep")"
         if [ ! -f "$MACOS/$name" ]
         then
            continue
         fi
         local target="@executable_path/$name"
         if [ "$dep" = "$target" ]
         then
            continue
         fi
         if install_name_tool -change "$dep" "$target" "$f" 2>/dev/null
         then
            changed=1
            echo "fix-macos-bundle-loads: $(basename "$f") $dep -> $target"
         fi
      done < <(otool -L "$f" | grep $'^\t' | sed 's/^\t//; s/ (compatibility version.*//')
      if [ "$changed" -eq 0 ]
      then
         break
      fi
      pass=$((pass + 1))
   done
}

rewrite_one "$BIN"
for dylib in "$MACOS"/*.dylib
do
   [ -f "$dylib" ] || continue
   rewrite_one "$dylib"
done
