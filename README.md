# Rubidium — Open Metaverse Browser

Rubidium is the metaverse browser application, developed by the Open Metaverse Browser Initiative (OMBI), a project under the Metaverse Standards Forum. It is the single executable the user runs — analogous to Chromium in the web browser world.

Rubidium is a thin application layer built on top of the **Sneeze** engine (the Metaverse Browser Engine). Rubidium owns windowing (via SDL3), the user chrome (URL bar, toolbars, menus), the native application frame, and the `main()` entry point. Sneeze handles rendering (ANARI), sandboxed code execution (Wasmtime), GPU compute (Vox), XR (OpenXR), networking (curl), UI (RmlUi), and cryptographic trust (BoringSSL + jwt-cpp).

**Sneeze must be built first.** Rubidium compiles Sneeze inline via CMake `add_subdirectory`, so Sneeze's installed dependencies must exist before Rubidium can configure. See the [Sneeze README](https://github.com/MetaversalCorp/Sneeze/blob/main/README.md) for Sneeze build instructions.

Building Rubidium conceptually has two phases:

1. **Deps** — build SDL3 from source into `deps/builds/<platform>/<config>/libs/`. Fast, one-time (~5 minutes). Per-config tree: Debug and Release have fully separate dep roots.
2. **Rubidium** — compile + link the Rubidium executable. The Rubidium build tree is **multi-config**: a single tree at `builds/<platform>/build/` emits Debug or Release into `builds/<platform>/install/{debug,release}/bin/` depending on the config selected at build time. Fast, every edit (seconds).

One script per platform drives both. By default it runs phase 2 — that's the 99% command. Pass `-All` / `--all` for both phases (first-time setup), or `-Deps` / `--deps` for phase 1 only (dep refresh). Details in [Quick Start](#quick-start).

`<platform>` uses the same slugs as Sneeze — `windows-x64`, `linux-x64`, `macos-arm64`, etc. `<config>` is `debug` or `release`. The deps tree is per-config; the Rubidium tree is one tree per platform, with both configs selectable via the Visual Studio / Xcode dropdown or `cmake --build --config`. To build a given config, you need its deps tree on disk first.

Dependency builds are **stamp-cached** — once SDL3 builds successfully, the script skips it on every later deps run until you explicitly tell it otherwise. Source clones are shared across configs via `deps/repos/`, so switching between Debug and Release does not re-clone anything.

---

## Prerequisites

Rubidium itself has very few build requirements beyond what Sneeze already needs. If you can build Sneeze, you can build Rubidium.

| Tool | Purpose | Check command | Minimum version |
|------|---------|---------------|-----------------|
| **Git** | Clones this repo and SDL3 | `git --version` | any |
| **CMake** | Generates build files | `cmake --version` | 3.20 |
| **C/C++ compiler** | Compiles all C/C++ code | Windows: `cl` ^1 / Linux: `g++ --version` / macOS: `clang++ --version` | C++17 support |
| **NSIS** | Windows installer generation (packaging only) | `makensis /VERSION` | any |

^1 On Windows, run `cl` from a **"Developer PowerShell for VS 2022"** window (search for it in the Start Menu), not a regular terminal.

NSIS is only needed if you want to generate the Windows installer via CPack. It is not required for building or running Rubidium.

All of Sneeze's prerequisites (Rust, Python, Go, NASM) are **not** needed for Rubidium — those are only for building Sneeze's own dependencies.

### Linux system libraries

On Linux, SDL3 is built from source during the deps phase. Its CMake configure calls `find_package(OpenGL)` and probes the X11/Wayland video backends, so the corresponding system **dev** packages must be installed first — otherwise a fresh `--all` build fails early with errors like:

```
Could NOT find OpenGL (missing: OPENGL_opengl_LIBRARY OPENGL_glx_LIBRARY OPENGL_INCLUDE_DIR)
Couldn't find dependency package for XTEST.  Please install the needed packages ...
```

Install the OpenGL + windowing dev packages before building:

**Debian / Ubuntu / WSL:**

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git pkg-config \
  libglvnd-dev mesa-common-dev libgl1-mesa-dev libegl1-mesa-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
  libxfixes-dev libxss-dev libxtst-dev libxkbcommon-dev \
  libwayland-dev wayland-protocols libdecor-0-dev \
  libasound2-dev libpulse-dev libudev-dev libdbus-1-dev \
  libssl-dev
```

**Fedora / RHEL:**

```bash
sudo dnf install libglvnd-devel mesa-libGL-devel libX11-devel libXext-devel \
  libXrandr-devel libXcursor-devel libXi-devel libXtst-devel \
  wayland-devel libxkbcommon-devel openssl-devel
```

**Arch:**

```bash
sudo pacman -S libglvnd mesa libx11 libxext libxrandr libxcursor libxi libxtst wayland libxkbcommon openssl
```

`libglvnd-dev` provides `libOpenGL.so` / `libGLX.so`, `mesa-common-dev` provides `GL/gl.h`, `libxtst-dev` provides the X11 XTest extension SDL3 probes for, and `libssl-dev` provides `libcrypto` + headers for Rubidium's `GenerateManifest` tool. If SDL3's configure was already cached after a failed run, force it to re-run before retrying `--all`:

```bash
./scripts/build-linux.sh --rebuild --only sdl3
./scripts/build-linux.sh --all
```

### macOS — MoltenVK

On macOS, Rubidium bundles **MoltenVK** (the Vulkan-on-Metal translation layer) inside `Rubidium.app` — SDL requests a Vulkan window and the Vulkan loader resolves it through MoltenVK. The Rubidium CMake configure looks for `libMoltenVK.dylib` and **fails early** if it can't find one:

```
MoltenVK not found. Set -DMOLTENVK_DYLIB= to a universal libMoltenVK.dylib
(see scripts/build-moltenvk-universal.sh) or install with `brew install molten-vk`.
```

Install it before building:

```bash
brew install molten-vk
```

The configure step auto-detects the Homebrew install via `brew --prefix molten-vk`, so no extra flags are needed. On Apple Silicon, Homebrew's `molten-vk` is **arm64-only** — perfectly fine for running a local dev build on your own Mac, but not a true universal `.dylib`. For a distributable universal `.app` (arm64 + x86_64), build a fat dylib with `scripts/build-moltenvk-universal.sh` and pass its path via `-DMOLTENVK_DYLIB=<path>` at configure time (this is what CI does).

---

## Quick Start

One script per platform. No flag = build Rubidium (fast, what you want 99% of the time). `-All` / `--all` = build deps then Rubidium (first-time setup). `-Deps` / `--deps` = build deps only (rare refresh).

**Prerequisite:** Sneeze must be built first. See the [Sneeze README](../Sneeze/README.md).

### First time — build deps and Rubidium (~5 minutes)

**Windows** — open **"Developer PowerShell for VS 2022"**, then from the repo root:

```powershell
.\scripts\build-windows.ps1 -All
```

If PowerShell refuses to run unsigned scripts:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -All
```

**Linux:**

```bash
./scripts/build-linux.sh --all
```

**macOS:**

```bash
./scripts/build-macos.sh --all
```

Add `-Config Debug` / `--config Debug` to any of these for a Debug build. Debug and Release share the Rubidium build tree but emit into separate install trees (`install/debug/` vs `install/release/`), so running both populates both side-by-side without reconfiguring.

### Every day after that — build Rubidium (seconds)

```powershell
.\scripts\build-windows.ps1                    # Release (default)
.\scripts\build-windows.ps1 -Config Debug      # Debug
```

```bash
./scripts/build-linux.sh                       # Release (default)
./scripts/build-linux.sh --config Debug        # Debug
```

```bash
./scripts/build-macos.sh                       # Release (default)
./scripts/build-macos.sh --config Debug        # Debug
```

No dep checks, no configure step — this is a plain `cmake --build` against the Rubidium tree. If deps aren't there it will fail at link time; if the Rubidium tree itself doesn't exist yet, CMake will complain about a missing `CMakeCache.txt`. Fix by running with `-Fresh` / `--fresh` or `-All` / `--all`.

### Rare — refresh deps only (no Rubidium)

```powershell
.\scripts\build-windows.ps1 -Deps
```

```bash
./scripts/build-linux.sh --deps
./scripts/build-macos.sh --deps
```

---

## Verifying the Build

After the script finishes, the executable lives in `builds/<platform>/install/<config>/bin/`. Substitute the slug and config that matches your run.

**Windows (Release):**

```powershell
dir builds\windows-x64\install\release\bin\Rubidium.exe
builds\windows-x64\install\release\bin\Rubidium.exe
```

**Linux (Release, x64):**

```bash
ls builds/linux-x64/install/release/bin/Rubidium
./builds/linux-x64/install/release/bin/Rubidium
```

**macOS (Release, Apple Silicon):**

```bash
ls builds/macos-universal/install/release/bin/Rubidium.app
open builds/macos-universal/install/release/bin/Rubidium.app
```

### IDE use (Visual Studio, Xcode)

On Windows, the CMake tree at `builds/windows-x64/build/` contains `Rubidium.sln`. Open it once, and the Debug / Release dropdown in the toolbar switches between configs — both build against the right per-config deps tree without any reconfigure. Output lands in the matching `install/debug/` or `install/release/` sibling. macOS (Ninja Multi-Config) and Linux (Ninja Multi-Config) work the same way from the CLI: pass `--config Debug` or `--config Release` to `cmake --build` against the single tree.

---

## Rebuilding After Code Changes

### You edited Rubidium source

Just run the script with no flags. Typical rebuild on one changed `.cpp` is a few seconds.

```powershell
.\scripts\build-windows.ps1                    # Release
.\scripts\build-windows.ps1 -Config Debug      # Debug
```

```bash
./scripts/build-linux.sh
./scripts/build-macos.sh
```

### SDL3 changed upstream

Full-scrub rebuild SDL3 — wipes its build tree, install tree, and all stamps. The source clone in `deps/repos/` is preserved.

**Windows:**

```powershell
.\scripts\build-windows.ps1 -Only sdl3 -Rebuild
```

**Linux / macOS:**

```bash
./scripts/build-linux.sh --only sdl3 --rebuild
```

### You want to inspect which deps are cached

```powershell
.\scripts\build-windows.ps1 -List
```

```bash
./scripts/build-linux.sh --list
```

### Nuclear option — scrub and rebuild

`-Rebuild` is a **modifier**, not a mode. It forces a full-scrub rebuild of whatever target set is selected by the other flags — it does not pick a target set of its own. The matrix:

| Command | Behavior |
|---|---|
| `-Rebuild` | Scrub + rebuild **Rubidium, current config only**. Runs `cmake --build --target clean --config <cfg>` against the existing `builds/<platform>/build/` tree (cleans only the current config's compiled artifacts) and wipes `builds/<platform>/install/<cfg>/`. Preserves `CMakeCache.txt`, `CMakeFiles/`, and the generated `Rubidium.sln`/`.vcxproj`, so an open Visual Studio solution doesn't need to reload. The other config's install tree and intermediates are untouched. Deps tree is never touched. If the tree has never been configured, falls back to a full configure + build. |
| `-Rebuild -Deps` | Scrub + rebuild **all deps**. Wipes the entire per-config dep root (`deps/builds/<platform>/<config>/`), rebuilds every dep. Rubidium tree is untouched. |
| `-Rebuild -Only <dep>` | Scrub + rebuild **one dep**. Wipes that dep's stamp, ExternalProject prefix, and `libs/<dep>/` install tree. Other deps and Rubidium are untouched. |
| `-Rebuild -All` | Scrub + rebuild **deps, then Rubidium**. Full scorched earth. |

Source clones in `deps/repos/` are **never** scrubbed — no re-download, ever.

`-Rebuild` alone never crosses the src ↔ deps wall. If you want to start over on both sides, use `-Rebuild -All`.

**Windows:**

```powershell
.\scripts\build-windows.ps1 -Rebuild                    # Rubidium only
.\scripts\build-windows.ps1 -Rebuild -Deps              # All deps only
.\scripts\build-windows.ps1 -Rebuild -Only sdl3         # One dep only
.\scripts\build-windows.ps1 -Rebuild -All               # Deps + Rubidium
.\scripts\build-windows.ps1 -Fresh                      # Rubidium reconfigure (no scrub)
```

**Linux / macOS:**

```bash
./scripts/build-linux.sh --rebuild
./scripts/build-linux.sh --rebuild --deps
./scripts/build-linux.sh --rebuild --only sdl3
./scripts/build-linux.sh --rebuild --all
./scripts/build-linux.sh --fresh
```

`-Fresh` and `-Rebuild` differ in what they touch. `-Fresh` passes `cmake --fresh` (wipes `CMakeCache.txt` + `CMakeFiles/` inside the existing build tree and reconfigures), so the CMake state regenerates but compiled object files may survive. `-Rebuild` keeps the configured CMake tree intact (preserving the generated `.sln`/`.vcxproj` and `CMakeCache.txt` — so an open IDE keeps working) and instead runs `cmake --build --target clean --config <cfg>` to drop only the current config's compiled artifacts, then wipes `install/<cfg>/`. The other config's install tree is deliberately preserved.

### Build-script flags at a glance

Default (no flag) builds Rubidium only. The table groups flags by role — **mode flags** select the target set, **modifier flags** alter how that set is built.

| Windows (PowerShell) | Linux / macOS (bash) | Role | Purpose |
|----------------------|----------------------|------|---------|
| *(none)* | *(none)* | mode (default) | Build Rubidium only — fast, no dep checks, no reconfigure |
| `-Deps` | `--deps` | mode | Build dependencies only — Rubidium is not touched |
| `-Fresh` | `--fresh` | mode | Reconfigure the Rubidium tree **from scratch** (passes `cmake --fresh`, wiping `CMakeCache.txt` + `CMakeFiles/`), then build it. Deps tree not touched. Requires CMake >= 3.24. |
| `-All` | `--all` | mode | Build dependencies, then configure + build Rubidium |
| `-Only <dep>` | `--only <dep>` | mode (deps-targeting) | Build one dep if not cached |
| `-List` | `--list` | mode (deps-targeting) | Show dep order + cached/pending status |
| `-Rebuild` | `--rebuild` | **modifier** | Full-scrub rebuild of whatever target set the mode flags select. Alone: Rubidium only. With `-Deps`: all deps. With `-Only`: one dep. With `-All`: both. Source clones in `deps/repos/` are preserved. |
| `-Config Debug\|Release` | `--config Debug\|Release` | option | Build configuration (default: Release) |

`-Deps`, `-Fresh`, and `-All` are mutually exclusive. `-Rebuild` composes with any mode.

**HARD RULE:** the deps folder (`deps/builds/<platform>/<config>/`) is only ever modified when `-Deps`, `-Only`, or `-All` is present on the command line. `-Rebuild` alone cannot touch a single bit inside `deps/` — it only scrubs the Rubidium output tree. This parallels the CMakeLists-level invariant: `deps/CMakeLists.txt` and `src/CMakeLists.txt` never include or reference each other's trees.

---

## How the Build Works

### Two isolated trees, nothing crosses

The repo has **two completely independent CMake projects** and no top-level that spans both:

- **`deps/CMakeLists.txt`** — the deps project. Knows only about files under `deps/`. Its only job is to build SDL3. Never references `src/`, never writes outside `deps/`.
- **`src/CMakeLists.txt`** — the Rubidium project. Knows only about files under `src/`. Finds installed SDL3 under `${RUBIDIUM_LIBS_DIR}/SDL3/install/`. Compiles Sneeze inline via `add_subdirectory("${SNEEZE_DIR}/src")`. Never references `deps/`, never writes outside `builds/<platform>/`. Multi-config: one build tree, per-config install trees under `install/{debug,release}/`.

The scripts in `scripts/` are the only glue between the two. In `-All` / `--all` mode, a script builds the deps tree, then invokes CMake a second time on the Rubidium tree. Neither CMakeLists ever sees the other.

### The moving parts

- **`deps/CMakeLists.txt`** — standalone CMake project for the deps tree. Derives `RUBIDIUM_CONFIG`, `RUBIDIUM_PLATFORM`, `RUBIDIUM_DEP_REPO`, and `LIBS_DIR` if not passed explicitly.
- **`deps/sdl3.cmake`** — `ExternalProject_Add` for SDL3. Clones into `${RUBIDIUM_DEP_REPO}/SDL3/` (shared across configs), builds and installs under `${LIBS_DIR}/SDL3/`.
- **`src/CMakeLists.txt`** — standalone CMake project for Rubidium. `find_package(SDL3)` under `${RUBIDIUM_LIBS_DIR}/SDL3/install/`. Compiles Sneeze inline via `add_subdirectory` (passing `SNEEZE_LIBS_DIR` as `LIBS_DIR` so Sneeze finds its own deps). Forces output to `${RUBIDIUM_BUILD_ROOT}/install/<config>/bin/` via generator-expression-backed output directories — the same tree serves both Debug and Release at build time, each into its own install sibling.
- **`scripts/build-*.{sh,ps1}`** — the glue. Default mode: `cmake --build <rubidium-tree>`. `-Deps` mode: `cmake -S deps` + per-dep stamped loop. `-All` mode: deps flow, then `cmake -S src` configure + build.
- **`scripts/build-deps.sh`** — shared bash helper used by Linux/macOS scripts.

### Sneeze consumption

Rubidium compiles Sneeze inline via `add_subdirectory("${SNEEZE_DIR}/src")`. This means:

- One IDE solution contains both Sneeze and Rubidium targets — full navigation and debugging across repos.
- Editing a `.cpp` in either repo and hitting build rebuilds the right targets. No commit/push/fetch cycle.
- The build scripts pass `SNEEZE_LIBS_DIR` (pointing to `Sneeze/deps/builds/<platform>/<config>/libs/`) which Rubidium sets as `LIBS_DIR` before the `add_subdirectory` call, so Sneeze's `src/CMakeLists.txt` finds its own deps.

**Sneeze must be built first.** Its deps at `Sneeze/deps/builds/<platform>/<config>/libs/` must exist before Rubidium can configure.

---

## Dependencies

Rubidium has one direct dependency that it builds from source:

| Dependency | Version | Repository | Purpose |
|------------|---------|------------|---------|
| SDL3 | 3.4.2 | [libsdl-org/SDL](https://github.com/libsdl-org/SDL) | Windowing, input, framebuffer display (static linked on desktop, shared on Android) |

All of Sneeze's dependencies (ANARI, Wasmtime, SPIRV-Tools, OpenXR, curl, RmlUi, glslang, nlohmann/json, Filament, Halogen, Vox, BoringSSL, jwt-cpp, SPIRV-Cross) propagate automatically through CMake PUBLIC link declarations when Rubidium links against `Sneeze`.

---

## Packaging and Release

Publishing a new Rubidium version is a three-step flow, repeated for each platform:

1. **Create the installer** — CPack wraps the installed tree (`bin/`) into a platform-native installer (NSIS `.exe` on Windows, DMG on macOS, TGZ on Linux).
2. **Register it in the manifest** — adds a release entry to `pkg/manifest.json` under `releases[<platform>][<version>]`, computing the SHA-256 of the installer and recording the CDN URL.
3. **Promote it to a channel** — points `current[<channel>][<platform>]` at the version, making it the version clients consult.

`pkg/manifest.json` is checked into the repo and uploaded to the CDN. It is cumulative — old release entries stay forever so that any client can still resolve an old version. Only the `current` index moves.

The CDN base URL is controlled by the `RUBIDIUM_CDN_URL` cache variable (default `https://dean.rp1.dev:884/_rubidium/`), compiled into both `Rubidium.exe` and `RubidiumSetup.exe` at configure time. Override with `-DRUBIDIUM_CDN_URL=<url>` if you need a different CDN.

### Prerequisites

The regular build tools plus **NSIS** on Windows (for the installer). CPack ships with CMake, no separate install.

### Step 1 — Create the installer

Build Rubidium in Release first, then run CPack against the built tree.

**Windows:**

```powershell
.\scripts\build-windows.ps1 -All -Config Release
cpack --config builds\windows-x64\build\CPackConfig.cmake -C Release
```

**Linux:**

```bash
./scripts/build-linux.sh --all
cpack --config builds/linux-x64/build/CPackConfig.cmake -C Release
```

**macOS:**

```bash
./scripts/build-macos.sh --all
cpack --config builds/macos-universal/build/CPackConfig.cmake -C Release
```

Output lands in `builds/<platform>/install/<config>/pkg/` (alongside `bin/`). CPack picks the per-config output directory from `CPACK_BUILD_CONFIG` (set by `-C <cfg>`), so running CPack twice — once `-C Debug`, once `-C Release` — populates both pkg dirs without touching the other. The installer contains `Rubidium.exe` plus the runtime DLLs (`anari_library_halogen.dll`, `wasmtime.dll`) and `RubidiumSetup.exe` (the web installer / auto-updater, shipped alongside so Ctrl+Shift+U can launch it in-app).

The installer filename follows the pattern `Rubidium-<version>-<platform>.<ext>` — e.g. `Rubidium-0.0.2-windows-x64.exe`, `Rubidium-0.0.2-linux-x64.tar.gz`, `Rubidium-0.0.2-macos-universal.dmg`. The `<platform>` slug matches `RUBIDIUM_PLATFORM` and every other path in the project. (Note: releases prior to 0.0.2 on the CDN used `-win64.exe` — that was CPack's default slug before we pinned `CPACK_PACKAGE_FILE_NAME`. Those historical entries are still in the manifest.)

### Step 2 — Register the installer in the manifest

`GenerateManifest register` computes the SHA-256 of a built installer and inserts a release entry at `releases[<platform>][<version>]`. It does **not** touch the `current` index — registration just adds the entry to the archive.

Two ways to invoke it:

**Convenience CMake target** — uses the current `PROJECT_VERSION`, `RUBIDIUM_PLATFORM`, and `RUBIDIUM_CDN_URL` automatically:

```powershell
cmake --build builds\windows-x64\build --target package_register --config Release
```

```bash
cmake --build builds/linux-x64/build --target package_register --config Release
cmake --build builds/macos-universal/build --target package_register --config Release
```

**Direct invocation** — the build tool lives at `builds/<platform>/install/<config>/bin/GenerateManifest.exe` after the target is built (`cmake --build ... --target GenerateManifest --config <cfg>`):

```
GenerateManifest register <installer> <version> <platform> <cdn_url> <manifest>
```

Example:

```powershell
GenerateManifest register `
   builds\windows-x64\install\release\pkg\Rubidium-0.0.2-windows-x64.exe `
   0.0.2 `
   windows-x64 `
   https://dean.rp1.dev:884/_rubidium/ `
   pkg\manifest.json
```

Use direct invocation when the installer filename doesn't match the default, when registering an older installer, or when pointing at a non-default manifest.

After `register`, the entry under `releases[<platform>][<version>]` contains:

```json
{
   "date":   "2026-04-20",
   "url":    "https://dean.rp1.dev:884/_rubidium/Rubidium-0.0.2-windows-x64.exe",
   "sha256": "2e185909...",
   "notes":  ""
}
```

Edit `notes` by hand after registration if you want release notes to appear in-app.

### Step 3 — Upload the installer to the CDN

Upload the installer from `builds/<platform>/install/<config>/pkg/` to the CDN at the URL recorded in the manifest entry's `url` field. The file name on the CDN must match the one in the manifest — the SHA-256 check in `RubidiumSetup.exe` will fail otherwise.

### Step 4 — Promote the version to a channel

`GenerateManifest promote` points `current[<channel>][<platform>]` at a previously registered version. The target version must already exist under `releases[<platform>][<version>]` — promote fails otherwise.

**Convenience CMake target** — promotes the current `PROJECT_VERSION` on the current platform to the `stable` channel:

```powershell
cmake --build builds\windows-x64\build --target package_promote --config Release
```

```bash
cmake --build builds/linux-x64/build --target package_promote --config Release
cmake --build builds/macos-universal/build --target package_promote --config Release
```

**Direct invocation** — lets you promote any registered version to any channel:

```
GenerateManifest promote <version> <platform> <channel> <manifest>
```

Example:

```powershell
GenerateManifest promote 0.0.2 windows-x64 stable pkg\manifest.json
```

Channels are free-form strings — `stable` today, but `beta`, `nightly`, or any other name works. Clients only consult channels they know about.

### Step 5 — Commit and upload the manifest

Commit `pkg/manifest.json` to the repo, then upload the same file to the CDN at `${RUBIDIUM_CDN_URL}manifest.json`. Clients fetch this file on every update check, so both copies must stay in sync.

### Tools reference

| Tool | Location | Purpose |
|------|----------|---------|
| **CPack** | Ships with CMake | Wraps the installed tree into a platform-native installer. Config generated at `builds/<platform>/build/CPackConfig.cmake`. Per-config output directory — `install/debug/pkg/` or `install/release/pkg/` — is chosen by the `-C <cfg>` flag. |
| **GenerateManifest** | `pkg/generate_manifest.cpp`, built into `builds/<platform>/install/<config>/bin/GenerateManifest.exe`. `EXCLUDE_FROM_ALL` — only built on demand via the `GenerateManifest`, `package_register`, or `package_promote` targets. | Two subcommands: `register` (add release entry + SHA-256), `promote` (point channel at version). |
| **RubidiumSetup.exe** | `pkg/RubidiumSetup.cpp`, built alongside `Rubidium.exe`. | Distributable web installer (`RubidiumSetup.exe` with no args) and auto-updater (`--check`, `--apply`). Shipped inside the installer and distributed standalone. Reads the manifest from the CDN. |
| **`package_register` target** | Defined in `src/CMakeLists.txt` | Convenience wrapper — invokes `GenerateManifest register` with current `PROJECT_VERSION`, `RUBIDIUM_PLATFORM`, `RUBIDIUM_CDN_URL`, and the repo's `pkg/manifest.json`. |
| **`package_promote` target** | Defined in `src/CMakeLists.txt` | Convenience wrapper — invokes `GenerateManifest promote` with the current `PROJECT_VERSION` on the current `RUBIDIUM_PLATFORM`, promoting to the `stable` channel. Use direct `GenerateManifest promote` for other channels. |

### Manifest structure

```json
{
   "current": {
      "stable": {
         "windows-x64": "0.0.2"
      }
   },
   "releases": {
      "windows-x64": {
         "0.0.2": {
            "date":   "2026-04-20",
            "url":    "https://dean.rp1.dev:884/_rubidium/Rubidium-0.0.2-windows-x64.exe",
            "sha256": "2e185909...",
            "notes":  ""
         }
      }
   }
}
```

- **`current[<channel>][<platform>]`** — version string. The channel index that clients consult. Points into `releases`.
- **`releases[<platform>][<version>]`** — `{ date, url, sha256, notes }`. The archive. Platform and version are the keys — they are not duplicated inside the entry.
- Keyed lookup is O(1): `current["stable"]["windows-x64"]` → `"0.0.1"` → `releases["windows-x64"]["0.0.1"]` → direct hit.

### Typical full release (Windows, 0.0.1 → 0.0.2)

```powershell
# 1. Bump version in the VERSION file at the repo root, commit.
# 2. Build
.\scripts\build-windows.ps1 -Config Release

# 3. Create the installer
cpack --config builds\windows-x64\build\CPackConfig.cmake -C Release

# 4. Upload builds\windows-x64\install\release\pkg\Rubidium-0.0.2-windows-x64.exe to the CDN

# 5. Register in manifest (adds entry, does not promote)
cmake --build builds\windows-x64\build --target package_register --config Release

# 6. Promote to stable (makes clients see it)
cmake --build builds\windows-x64\build --target package_promote --config Release

# 7. Upload pkg\manifest.json to the CDN
# 8. Commit pkg\manifest.json
```

For a staged rollout, skip step 6 (or promote to a non-`stable` channel) until the build has been validated.

---

## Troubleshooting

| Problem | Likely cause | Fix |
|---------|-------------|-----|
| `cmake` command not found | CMake not installed or not on PATH | Install CMake (3.20+) and add to PATH |
| `cl` not recognized on Windows | Not in Developer PowerShell for VS 2022 | Open "Developer PowerShell for VS 2022" from Start Menu |
| `.\scripts\build-windows.ps1 cannot be loaded` | PowerShell execution policy | `powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1` |
| Rubidium configure fails with "Could not find Sneeze" | Sneeze not built or wrong path | Build Sneeze first. Set `SNEEZE_DIR` env var if not at `../Sneeze`. |
| Rubidium configure fails with "Could not find SDL3" | SDL3 not built | Run with `-All` or `-Deps` first |
| macOS configure fails with "MoltenVK not found" | MoltenVK not installed and `-DMOLTENVK_DYLIB` not set | `brew install molten-vk` (see [macOS — MoltenVK](#macos--moltenvk)) |
| Link errors referencing Sneeze deps | Sneeze deps not built for this platform+config | Build Sneeze's deps: `cd ../Sneeze && .\scripts\build-windows.ps1 -Deps` |
| ANARI "failed to load library" at runtime | DLL not copied next to Rubidium.exe | Check the POST_BUILD copy commands in `src/CMakeLists.txt`. Verify DLLs exist in Sneeze's deps install tree. |
| Build takes longer than expected on first run | SDL3 compilation | Normal — ~5 minutes. Subsequent runs skip via stamps. |
| `LNK2038 _ITERATOR_DEBUG_LEVEL mismatch` | Mixed Debug/Release configs | Ensure Sneeze and Rubidium use the same `-Config` |

---

## License

Rubidium is proprietary software. Copyright 2026 Metaversal Corporation. All rights reserved.

See `NOTICE` for third-party attribution (Sneeze: Apache 2.0, SDL3: zlib).
