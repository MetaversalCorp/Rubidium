# Android build — session handoff (Windows host → arm64 APK)

**Audience:** a new Cursor session on public **Rubidium** (sibling **Sneeze**), reproducing a signed `arm64-v8a` debug APK. Do not re-derive the 2026-08-21 Artemis-on-Windows Android session.

**Written:** 2026-08-30. Source: local chat that produced `artemis-0.3.0-android-arm64-signed.apk` on a Pixel 8, then fixed flash-on-launch.

This is **not** a substitute for `.cursor/rules/project.mdc`. Official GitHub Android jobs in both repos are **`if: false`**. `pkg/android/build-apk.sh` is a sketch aimed at Linux CI paths; it will not work as-is on a Windows host and it omits fonts.

**Assume `main` in Sneeze and Rubidium/Artemis has dropped the session patches.** Treat the “must re-apply” list in §5 as required work, not optional polish.

---

## 0. Read this first (90 seconds)

| Fact | Detail |
|------|--------|
| **What you are building** | One debug-signed APK. ABI **arm64-v8a** only. Min SDK **26**. SDL3 activity loads `libmain.so`. |
| **Host that actually worked** | Windows 10 + NDK **r27.0.12077973** + JDK 17 + Rust `aarch64-linux-android` + SDK CMake **3.22.1**/Ninja. Long paths enabled. |
| **Engine** | Sibling **Sneeze** (`add_subdirectory`). OpenXR **off** (`-DSNEEZE_ENABLE_XR=OFF`). |
| **SDL3** | Rubidium dep, **shared** `.so` (not static). Java comes from the SDL3 source tree, not from a CMake-built JAR. |
| **Output native name** | CMake target stays `Rubidium`. `OUTPUT_NAME` is `PRODUCT_NAME` → `libRubidium.so` on public Rubidium, `libArtemis.so` on the Artemis overlay. **APK always wants `lib/arm64-v8a/libmain.so`.** |
| **Java package** | `com.rp1.Rubidium` / `com.rp1.Rubidium.MainActivity` (already in `pkg/android/` on current trees). Do not “fix” this to Artemis. |
| **CI cannot save you** | Android / Quest / `apk-android` jobs are disabled. No artifact download. |
| **First launch crash if fonts missing** | Activity flashes on then off. Tombstone: `FORTIFY: pthread_mutex_lock called on a destroyed mutex` in `SNEEZE::POOL_CYCLE::Grab`. Cause is failed `LoadFonts()` tearing down the engine while the compositor thread is still running. |

Layout that built on 2026-08-21:

```
…/GitHub/cursors/
  Rubidium/   (or Artemis overlay of that history)
  Sneeze/
```

Set `SNEEZE_DIR` explicitly. Artemis CMake defaults to `../Gezundheit`; Rubidium/public may default to `../Sneeze`. A bare configure without the sibling will fail.

---

## 1. Architecture

```
Java SDLActivity (MainActivity)
        │  System.loadLibrary: SDL3, anari, anari_library_halogen, main
        ▼
libSDL3.so  →  SDL_main in libmain.so (CMake target Rubidium, renamed)
        │
        ▼
APPSDL / APPFRAME_SDL / CANVAS_NATIVE
        │  SNEEZE::ENGINE (inline static libSneeze.a)
        ▼
Halogen (Filament Vulkan) dlopens libanari.so
Wasmtime is usually **statically** linked into libmain.so
libc++_shared.so must ship (ANDROID_STL=c++_shared)
```

**Two CMake trees, same as desktop:**

1. **Sneeze deps** — `Sneeze/deps` → `Sneeze/deps/builds/android-arm64/release/libs/<dep>/install/`
2. **Rubidium deps** — `deps/sdl3.cmake` only → `deps/builds/android-arm64/release/libs/SDL3/install/`
3. **App** — `cmake -S src` with the NDK toolchain, Sneeze via `add_subdirectory`, output under `builds/android-arm64/install/release/bin/`

Do **not** use Sneeze’s standalone `Sneeze/scripts/build-*` as the source of `Sneeze.lib` for the app. Rubidium compiles Sneeze inline. What Rubidium **does** need from Sneeze is the **prebuilt Android deps tree**.

RMAP (and Map) are Sneeze ExternalProjects. They do **not** rebuild json/asio/websocketpp/BoringSSL/curl/socket.io; `RMAP_USE_SYSTEM_*=ON` points at those install trees. Cross-compile must pass `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH` (already in `deps/rmap.cmake`) so finds escape the NDK sysroot.

---

## 2. Prerequisites

| Tool | Notes |
|------|--------|
| Android SDK | Typical: `%LOCALAPPDATA%\Android\Sdk` |
| NDK r27 | Session used `ndk/27.0.12077973`. Toolchain: `build/cmake/android.toolchain.cmake` |
| CMake + Ninja from SDK | `Sdk/cmake/3.22.1/bin` — put this **first** on `PATH`. VS’s CMake/Ninja will confuse ExternalProjects. |
| build-tools | `34.0.0` worked (`aapt2.exe`, `d8.bat`, `zipalign.exe`, `apksigner.bat`) |
| `platforms;android-34` | Preferred for javac (SDL Java uses API 33 `Context.RECEIVER_EXPORTED`). If sdkmanager cannot fetch it, compile Java against **android-31** and drop the 4-arg `registerReceiver` in a **copy** of SDL sources (do not edit `deps/repos/SDL3`). Target SDK 31 is OK for a local debug APK. |
| JDK 17 | `javac` / `jar` / `keytool` |
| Rust | `rustup target add aarch64-linux-android` |
| Host Filament + glslang | Android Filament/Halogen need **host** `matc` and `glslangValidator`. Copy from an existing **Windows** Sneeze deps install: `Sneeze/deps/builds/windows-x64/release/libs/filament/install/` (`ImportExecutables-Release.cmake`, `bin/matc.exe`) and glslang’s `glslang.exe`. |
| Windows long paths | Required. Filament/Halogen paths explode without them. |

Enable long paths if needed (admin): `New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force`

---

## 3. Build order (what actually worked)

Do not run `pkg/android/build-apk.sh` as the driver on Windows. Drive CMake yourself, then package.

### 3.1 Sneeze Android deps

Configure `Sneeze/deps` with the NDK toolchain, Ninja, Release, XR off, and:

```
-DANDROID_ABI=arm64-v8a
-DANDROID_PLATFORM=android-26
-DANDROID_STL=c++_shared
-DWASMTIME_CARGO_TARGET=aarch64-linux-android
-DIMPORT_EXECUTABLES_HOST_FILE=<windows filament ImportExecutables-Release.cmake>
```

Install root: `Sneeze/deps/builds/android-arm64/release/libs/`.

Expect at least: ANARI (`libanari.so` + static), Halogen (`libanari_library_halogen.so`), Wasmtime (`libwasmtime.a` and/or `.so`), boringssl, curl, RmlUi, filament (`lib/arm64-v8a` **and** often `lib/aarch64`), SPIRV-Tools, SPIRV-Cross, FreeType, fastgltf, nlohmann-json, jwt-cpp, asio, websocketpp, socketio, rmap, map, vox, host glslang.

**Do not wait for a green `ninja` on every ExternalProject.** Several deps needed a manual inner build after a stamp lie (see §5). Touching outer ninja stamps after a manual install is how the session unblocked later deps.

### 3.2 SDL3 (Rubidium deps)

Shared library, same NDK flags. Install: `Rubidium/deps/builds/android-arm64/release/libs/SDL3/install/lib/libSDL3.so`.

The CMake **JAR** step can fail (`RECEIVER_EXPORTED` vs android-31). Ignore the JAR. `cmake --install` the native `.so` + CMake config. Java is compiled later from `deps/repos/SDL3/android-project/...`.

### 3.3 Rubidium `lib*.so`

```
cmake -S src -B builds/android-arm64/build -G Ninja
  -DCMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake
  -DANDROID_ABI=arm64-v8a
  -DANDROID_PLATFORM=android-26
  -DANDROID_STL=c++_shared
  -DCMAKE_BUILD_TYPE=Release
  -DSNEEZE_DIR=<abs path to Sneeze>
  -DSNEEZE_LIBS_DIR=<Sneeze>/deps/builds/android-arm64/release/libs
  -DSNEEZE_ENABLE_XR=OFF
  -DSDL3_ROOT=<Rubidium>/deps/builds/android-arm64/release/libs/SDL3/install
```

Then `cmake --build builds/android-arm64/build --parallel 8`.

Expect `builds/android-arm64/install/release/bin/libRubidium.so` (or `libArtemis.so` on the overlay).

Skip `GenerateManifest` / `package_register` on Android: they `find_package(OpenSSL REQUIRED)` for a **host** tool and break configure. Guard them with `if (NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")`.

### 3.4 APK (Windows PowerShell recipe)

Stage `lib/arm64-v8a/`:

| File | Source |
|------|--------|
| `libmain.so` | rename of `libRubidium.so` / `libArtemis.so` |
| `libSDL3.so` | Rubidium SDL3 install |
| `libanari.so` | Sneeze ANARI-SDK install |
| `libanari_library_halogen.so` | Sneeze Halogen install |
| `libc++_shared.so` | NDK `sysroot/usr/lib/aarch64-linux-android/libc++_shared.so` |

`libwasmtime.so` is **optional**. A successful `libmain.so` often has Wasmtime statically linked (`llvm-readelf -d` shows NEEDED: SDL3, vulkan, dl, log, android, m, c++_shared, c — not wasmtime). Shipping the extra 50 MB `.so` does not match `MainActivity.getLibraries()`.

Also stage **`assets/fonts/`** from `deps/fonts/` (Inter, JetBrains Mono, Material Symbols Outlined). Without this, launch aborts (see §6).

Java:

- SDL: `deps/repos/SDL3/android-project/app/src/main/java`
- App: `pkg/android/java`
- `javac -source 1.8 -target 1.8 -cp android.jar` (**no** `-bootclasspath` — that kills lambdas / `LambdaMetafactory`)
- `d8 --lib android.jar --output stage` → `classes.dex`

`aapt2 compile` `pkg/android/res`, `aapt2 link` with `pkg/android/AndroidManifest.xml`, min 26, target 34 or 31.

`jar uf unaligned.apk classes.dex lib assets`  
`zipalign -f 4`  
`apksigner` with a debug keystore (`CN=Rubidium Debug`).

Install: `adb install -r <signed.apk>`.

---

## 4. Identity (do not mix with the Artemis overlay)

| | Public Rubidium | Artemis overlay (this tree) |
|--|-----------------|-----------------------------|
| CMake target | `Rubidium` | same |
| `PRODUCT_NAME` / `.so` | `Rubidium` / `libRubidium.so` | `Artemis` / `libArtemis.so` |
| Java | `com.rp1.Rubidium` | **still** `com.rp1.Rubidium` in `pkg/android/` (not in the branding hook) |
| APK native | always `libmain.so` | same |
| App data | `Metaversal/Rubidium` | `Metaversal/Artemis` |

`build-apk.sh` searches for `libRubidium.so`. On an Artemis checkout that find is wrong — look for `lib${PRODUCT_NAME}.so` or `libArtemis.so`.

---

## 5. Non-obvious gotchas (re-apply on `main`)

These are the failures that burned hours. Most are **not** in current `main`.

### 5.1 Windows NDK + ExternalProject

**Forward `ANDROID_STL` and `CMAKE_MAKE_PROGRAM` into every Sneeze (and Rubidium SDL3) ExternalProject.** Inner configures otherwise pick a missing STL or cannot find ninja. Session patched `Sneeze/deps/CMakeLists.txt` and `Rubidium/deps/CMakeLists.txt`. Current Sneeze `CROSS_COMPILE_ARGS` forwards ABI/platform/NDK but **not** STL or ninja.

### 5.2 Wasmtime on Windows host

- NDK linker on Windows is `aarch64-linux-android26-clang.cmd`, not the extensionless `clang`. Cargo’s `linker = ".../clang"` produces a **Windows PE** `wasmtime.dll` in `deps/repos/Wasmtime/target/release` and the Android build looks like it succeeded.
- Pass `--target=aarch64-linux-android` as **one argv token**.
- Set Cargo `--target-dir` to the Android Wasmtime build tree so it does not collide with a host `target/release`.
- `CC_aarch64-linux-android` in PowerShell: `[System.Environment]::SetEnvironmentVariable("CC_aarch64-linux-android", $clang)`. `$env:CC_aarch64-linux-android` is a syntax error.
- `-Wl,--whole-archive` must be quoted or run via `cmd /c`.
- After cargo, if you only have `libwasmtime.a`, `clang -shared --whole-archive` can still produce `install/lib/libwasmtime.so`.
- Install script that `copy_if_different` **only** `libwasmtime.so` fails when cargo emitted `.a`. Empty `WASMTIME_COPY_CMDS` for ELF and copy whichever exists.
- `FindWasmtime.cmake` on current `main` searches `.a` for iOS only. On Android, also search `.a` then `.so` (session did this).

### 5.3 Filament / Halogen

- Filament’s IAS `.incbin shaders.bin` **fails on Windows-hosted Android clang**. Pass **`-DANDROID_ON_WINDOWS=ON`** into Filament (Filament’s own Android toolchain files set this; Sneeze’s `ExternalProject` NDK toolchain does not). Rebuild the **inner** Filament tree, not only the outer stamp.
- Copy host `ImportExecutables-Release.cmake` into the filament source tree (Sneeze already has `IMPORT_EXECUTABLES_HOST_FILE` for this). That file is generated by a **Windows** filament build.
- Halogen then fails with `matc not found`, then missing `matteBlend.filamat`. Point inner Halogen at host `matc.exe`: `-DFILAMENT_MATC=.../windows-x64/release/libs/filament/install/bin/matc.exe`. Current `halogen.cmake` only presets `FILAMENT_MATC` for **Debug host** builds, not Android cross.

### 5.4 Link: `-lpthread`

Android Bionic has pthreads in libc. NDK r21+ has **no** `libpthread`. Link line with `-Wl,--fatal-warnings` dies:

`ld.lld: error: unable to find library -lpthread`

Sneeze itself already links `log android` on ANDROID. The `-lpthread` comes from **RMAP/Map exported INTERFACE** (`$<LINK_ONLY:pthread>`) and sometimes curl/ANARI `-pthread`. After `find_package`, on ANDROID strip pthread from `RMAP::RMAP`, `Map::Map`, `CURL::libcurl`, `anari::anari_static`, and clear `Threads::Threads` INTERFACE_LINK_LIBRARIES. Do **not** rebuild RMAP just for this unless you also change `UNIX AND NOT APPLE` in RMAP/Map to exclude ANDROID (those clones live in `Sneeze/deps/repos` and get wiped).

`dl` is fine (`libdl.so` exists). Keep it.

### 5.5 SDL Java

- `Context.RECEIVER_EXPORTED` is API 33. android-31 javac fails. Runtime already has `SDK_INT >= 33` branches; for a targetSdk 31 APK, compile a staged copy that uses the 3-arg `registerReceiver`.
- Do not use `-bootclasspath android.jar` with `-source 1.8` — `MainActivity` lambdas fail (`LambdaMetafactory`).
- SDL Java lives at `deps/repos/SDL3/android-project/...`, **not** `libs/SDL3/src/...` (`build-apk.sh` is wrong).
- On Windows, `d8` / `apksigner` are `.bat`; `aapt2` / `zipalign` are `.exe`. The script hardcodes `.bat`.

### 5.6 Other CMake traps

- `GenerateManifest` + OpenSSL: skip on Android/iOS (see §3.3).
- Shared `Sneeze/deps/repos` with Windows builds. Filament patch-copy of `ImportExecutables-Release.cmake` into the filament **source** tree is OK for Windows filament afterward, but do not scrub `deps/repos` as part of an Android retry.
- After changing `add_subdirectory` binary dirs, `-Fresh` the app tree. Stale cache → `_ITERATOR_DEBUG_LEVEL` nonsense (less common on Android Ninja single-config, still true if you flip paths).

---

## 6. Launch crash that looks like “the APK is broken”

**Symptom:** icon tap → splash/frame for a split second → gone.

**Logcat (first session, no fonts):**

```
V SDL: Running main function SDL_main from library .../libmain.so
F libc: FORTIFY: pthread_mutex_lock called on a destroyed mutex
F DEBUG: ... SNEEZE::POOL_CYCLE::Grab ... AGENT::COMPOSITOR::Job ...
```

Libraries **did** load (`libSDL3`, `libanari`, `libanari_library_halogen`, `libmain`). This is not `UnsatisfiedLinkError`.

**Why:** `APPSDL::LoadFonts()` uses `SDL_GetBasePath() + "fonts/"`. On Android that is the **files dir**, not APK assets. Missing any font → `LoadFonts()` false → `Run()` skips the event loop → `delete ENGINE` while compositor is in `Grab()`. `POOL_CYCLE`’s `m_mxCycle` is destroyed in the derived destructor **before** `~POOL()` joins agents. Bionic aborts.

**Fixes (both):**

1. **Ship fonts** in `assets/fonts/` (copy `deps/fonts/`). On Android, extract each file from assets (`SDL_IOFromFile("fonts/...", "rb")`) into `SDL_GetAndroidInternalStoragePath()/fonts/` then `Rml::LoadFontFace` that path. Session added `ExtractAndroidAsset` in `App_SDL.cpp`; **gone from current `main`**.
2. **Join agents before destroying `m_mxCycle`:** `POOL::Shutdown()` then `delete pPool` in `CONTROL::Main`; compositor `Job()` should `break` on `IsShutdown()` **before** `Grab()`. Session patched `Control.h` / `Pool.cpp` / `Control.cpp` / `Compositor.cpp`; **gone from current `main`**. Without (2), any failed init (not only fonts) can SIGABRT.

**Logger:** `ILOGGER_NATIVE` uses `printf`. Android does not show that in logcat. Session routed stdout to `__android_log_print(..., "Artemis", ...)` — use tag **`Rubidium`**. Without this you will debug blind.

After fonts + shutdown join, Pixel 8 kept the process: Wasmtime/RmlUi/Sneeze init logs, Inter loaded from `/data/data/com.rp1.Artemis/files/fonts/...`. (Package name follows the Java id, `com.rp1.Rubidium` on public Rubidium.)

`adb` with the screen off (`isSleeping=true`) will `onResume` then immediate `onPause` — looks like a flash even when the process lives. Wake the device (`KEYCODE_WAKEUP`) before declaring failure.

---

## 7. What `pkg/android/build-apk.sh` gets wrong

Use it as a checklist, not a runner, until it is rewritten.

| Script assumes | Reality on the Windows session / current tree |
|----------------|-----------------------------------------------|
| `SNEEZE_DIR=../Gezundheit`, `SNEEZE_LIBS_DIR=$SNEEZE_DIR/libs-android` | Local Sneeze; libs at `deps/builds/android-arm64/release/libs` |
| `SDL3_ROOT=$REPO_ROOT/sdl3-android-install`, Java under `libs/SDL3/src` | SDL3 at `deps/builds/.../SDL3/install`; Java under `deps/repos/SDL3/android-project` |
| `libRubidium.so` next to a Linux cmake build dir | `OUTPUT_NAME` may be Artemis; outputs live in `builds/android-arm64/install/release/bin` |
| `platforms/android-34/android.jar` | May be missing; android-31 works with the RECEIVER workaround |
| `d8.bat` / `apksigner.bat` | Linux CI needs no `.bat` |
| No `assets/fonts` | Required or you hit §6 |
| It will cmake-build deps + app | On Windows, Wasmtime/Filament/Halogen need the patches in §5 first |

CI `apk-android` (`if: false`) is closer to the packaging half: compile SDL Java + `pkg/android/java`, aapt2, zip dex+`lib/`, zipalign, debug sign. It also omits fonts.

---

## 8. Final learnings (do not re-learn)

1. **Cross-compiling this stack on Windows is possible** but every “UNIX Android” assumption in Cargo, Filament IAS, and `-lpthread` is a landmine. Prefer a Linux builder if the Rubidium session has one; if you stay on Windows, budget the §5 patches first.
2. **Stamps lie.** ExternalProject configure stamps survive a failed inner build. Invalidate `<dep>-prefix/src/<dep>-stamp/<Config>/<dep>-configure` (Sneeze scripts already do this for deps). After a **manual** inner install, touch the outer ninja stamp or the next dep will rebuild from a half-tree.
3. **Do not treat SDL3-jar as required.** Native `.so` + compiling `android-project` Java is the APK.
4. **Do not rebuild RMAP to fix pthread.** Strip the imported INTERFACE on the Sneeze consumer side.
5. **Do not ship a second `libwasmtime.so` “because CI does”** unless `llvm-readelf -d libmain.so` lists it. Static into `libmain.so` was enough.
6. **`MainActivity.getLibraries()` order is load order.** `SDL3`, `anari`, `anari_library_halogen`, `main`. Halogen dlopens `libanari.so` — omit it and the activity dies after Java load with `library "libanari.so" not found`.
7. **Fonts are not optional chrome.** They gate `Run()`; failure is a compositor FORTIFY abort, not a missing-glyph UI.
8. **Host tools for a target build:** Android Filament/Halogen still run `matc` / `glslang` **on the PC**. A Windows filament Release install is a prerequisite, not a nice-to-have.
9. **Shared `deps/repos`:** Wasmtime/Filament clones are the same directories Windows desktop uses. Isolate Cargo `--target-dir`. Do not `git clean` those clones as an Android retry.
10. **Java package is not branded.** Keep `com.rp1.Rubidium`. JNI `Java_com_rp1_Rubidium_MainActivity_nativeUrlSubmitted` in `App_SDL.cpp` is currently `#if 0` — the overlay URL bar is not wired; that is unrelated to the flash-on-launch abort.

---

## 9. Suggested first actions for the Rubidium session

1. Confirm sibling `Sneeze` and set `SNEEZE_DIR`. Confirm NDK r27, SDK CMake 3.22.1 on PATH, `rustup target add aarch64-linux-android`, long paths.
2. Re-apply §5 CMake patches in **Sneeze** (STL/ninja forward, Wasmtime Windows linker + `.a` install, Filament `ANDROID_ON_WINDOWS`, Halogen host `matc`, FindWasmtime `.a` on Android, RMAP pthread strip, `POOL::Shutdown`).
3. Re-apply §5–§6 patches in **Rubidium** (skip GenerateManifest on Android, `LoadFonts` asset extract, `ILOGGER` → logcat, APK font assets).
4. Build Sneeze Android deps → SDL3 shared → `libRubidium.so` → package APK with fonts → `adb install -r` with the screen **on**.
5. If it flashes: `adb logcat` for `FORTIFY` vs `UnsatisfiedLinkError` vs `Font asset missing`. Do not start by “rebuilding Filament from scratch.”
