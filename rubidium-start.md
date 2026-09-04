# Rubidium start — session handoff

**Audience:** a new Cursor session on the **Artemis** tree (branded Rubidium) and/or public **Rubidium**, continuing engine/app work without re-deriving the 2026-08 graphics and map-reload session.

**Written:** 2026-08-28. **Updated:** 2026-08-28 (fetched rewritten Artemis `origin/main` @ `ab73058`; Sneeze `main` @ `d61197f`; Release build of `Artemis.exe` against that Sneeze). Source conversation: local chat “Draco and large-mesh fix”.

This is **not** a substitute for `.cursor/rules/project.mdc` (this Artemis tree) or Sneeze’s `project.mdc`. Those describe the products as they are supposed to be. This file is the **working memory** of one long session: what is true on disk now, which documents are stale, which bugs were fixed, which hypotheses failed a retest, and which investigations you must not repeat.

---

## 0. Read this first (90 seconds)

| Fact | Detail |
|------|--------|
| **Repo split (as of this fetch)** | **Rubidium** = public app core (`RUBIDIUM::`). **Artemis** = private branded overlay of that history (`Artemis.exe`). **Sneeze** = public engine. **Gezundheit** = private Sneeze fork that *official* Artemis CI expects at `../Gezundheit`. |
| **This workspace** | `…/GitHub/cursors/Artemis` @ `ab73058` + sibling `Sneeze` @ `d61197f`. There is **no** `Gezundheit` folder here. Builds must set `SNEEZE_DIR` (or `$env:SNEEZE_DIR`) to the Sneeze path. |
| **Do not use `ARTEMIS::`** | All app C++ is `namespace RUBIDIUM`. CMake targets are `Rubidium` / `RubidiumSetup`. On-disk names come from `branding/Product.cmake` (`OUTPUT_NAME` → `Artemis.exe`). |
| **This session’s graphics work is Sneeze** | Draco/instancing/Halogen 1.1.10 live in the engine, not in chrome sources. |
| **Sneeze to continue** | **`main` @ `d61197f`** (PR #40). Same tree as `graphics_upgrade` @ `c7b84c4`. No `LnGLive`. `IsRegistered` kept. Halogen pin **v1.1.10** (`0dabca8`). |
| **glTF path that actually shipped** | CPU parse (`fastgltf` + vendored meshopt/Draco) → `GLTF_RENDER_MODEL` on **NODE** → compositor `MESH_DATA` → ANARI `"triangle"` instances. **Not** Filament `gltfio`, **not** `HALOGEN_GLTF_ASSET`. |
| **Earth second load** | After reverting nested-fabric, a run **could not reproduce** that failure. Do not restore `9f74544` to “fix reload.” See §2. |
| **Keep** | WASM collapse exemption, instancing, Draco, triangle budget, 60 Hz compositor floor, Halogen `v1.1.10`, in-tree `src/deps/meshoptimizer` + `src/deps/draco`. |
| **Stale specs** | Untracked `sneeze-gltf.md` (two-path Halogen extension — not implemented). Pre-split Artemis (`ARTEMIS::`, `Artemis-Legacy`) must not be merged into this tree. |

Layout that **built** on 2026-08-28:

```
…/GitHub/cursors/
  Artemis/     ← branded Rubidium; origin force-updated to this history
  Sneeze/      ← public engine (set SNEEZE_DIR here; default is ../Gezundheit)
```

Official Metaversal layout (from `project.mdc`, not present on this machine): `C:\Dev\OMB\Artemis` + `C:\Dev\OMB\Gezundheit` + `C:\Dev\OMB\Rubidium` as `upstream`.

---

## 1. What the products are

### Rubidium vs Artemis (direction of the fork)

**The earlier assumption in this file was backwards.** Rubidium is **not** a public fork of the old proprietary Artemis (`ARTEMIS::`). As of the 2026-08-28 fetch, **Artemis `origin/main` was force-updated** (`d06281e` → `ab73058`) to a **Rubidium history plus a branding overlay**.

| Repo | Role |
|------|------|
| [Rubidium](https://github.com/MetaversalCorp/Rubidium.git) | Public application core. `namespace RUBIDIUM`. CMake `project(Rubidium)`. Shared chrome work lands **here first**. |
| [Artemis](https://github.com/MetaversalCorp/Artemis.git) | Private branded derivative. Same sources; overlay `branding/Product.cmake` + `branding/Brand.props`. Binary `Artemis.exe`, appdata `Metaversal/Artemis`, home `https://cdn.rp1.com/fabric/artemis.msf`, CDN `https://cdn.rp1.com/artemis/`. `VERSION` **0.3.0**. |
| [Sneeze](https://github.com/MetaversalCorp/Sneeze.git) | Public engine. `namespace SNEEZE`. |
| Gezundheit | Private Sneeze fork. Artemis `project.mdc` / `msvc/Artemis.sln` default to `../Gezundheit`. Shared engine work: Sneeze first, then merge into Gezundheit. |

**Branding hook** (`branding/`): do not fork chrome/build files for product names. C++ uses `PRODUCT_*` macros from generated `Brand.h`. CMake targets stay `Rubidium` / `RubidiumSetup`; `set_target_properties(… OUTPUT_NAME "${PRODUCT_NAME}")` writes `Artemis.exe`. Keep `Product.cmake` and `Brand.props` in sync.

**Not in the hook:** Android Java package `com.rp1.Rubidium`; `pkg/macos/Rubidium.entitlements`; README/`RELEASE` prose still say Rubidium. `-DRUBIDIUM_CDN_URL=` still overrides CDN at configure time.

**Do not merge** local folders `Artemis2` / `Artemis-Legacy` or any `ARTEMIS::` history into this tree.

SDL3 belongs to the **application**, not Sneeze.

### Sneeze / Gezundheit

- Engine static library, Apache 2.0. App consumes via `add_subdirectory("${SNEEZE_DIR}/src" …)`.
- **This machine:** `SNEEZE_DIR=C:\Users\yinch\Documents\GitHub\cursors\Sneeze` (env). Script/CMake default is `../Gezundheit`, which **does not exist** here — a bare `.\scripts\build-windows.ps1` will fail `Resolve-Path`.
- Required: Sneeze prebuilt deps at `Sneeze/deps/builds/<plat>/<cfg>/libs/`.
- Inline and standalone Sneeze builds share `Sneeze/builds/<plat>/`.

### Halogen

- Clone: `Sneeze/deps/repos/Halogen`. Pinned **`v1.1.10`** (`0dabca8ebe`) — confirmed by `dep-verify` on the 2026-08-28 configure.
- `MISMATCH` halt if an existing clone is on the wrong commit. Sneeze **`-Sync`**.
- Edit `Sneeze/deps/repos/Halogen/src/`. `Sneeze/libs/Halogen` is not the commit source.

### Filament

- Often at `Sneeze/deps/repos/filament` (**gitignored**). CI sneeze jobs typically have install artifacts only.

---

## 2. Branch and commit reality (do not guess)

### Sneeze `main` (continue here)

`origin/main` @ **`d61197f`**. This workspace’s Sneeze was fast-forwarded there (was on `graphics_upgrade`; tree is identical). `git diff origin/graphics_upgrade origin/main` is empty.

Verified: no `LnGLive`; `MAPSVC::IsRegistered` and compositor `Node_IsMapManaged` remain. Configure log: `halogen: on ref 'v1.1.10' (0dabca8ebe)`.

| Commit | What |
|--------|------|
| `3c428aa` | Instancing of re-used 3D assets. CPU `GLTF_RENDER_MODEL` cache by **URL on NODE**. ANARI shared geometry/group; per-draw instance via `pInstanceOwner` + `nDrawIx`. Meshopt decode, triangle-only primitives, AABB bounds, UV V flip once. |
| `b45547c` | `KHR_draco_mesh_compression` decode. Triangle budget `MAX_NEW_TRIANGLES_PER_FRAME = 65536`. Compositor **60 Hz floor**. Optional test `SNEEZE_DRACO_GLB`. |
| `7e9cd4a` | Pin Halogen **`v1.1.9` → `v1.1.10`**. |
| `379af14` | Vendor meshopt + Draco decode into `src/deps/meshoptimizer` and `src/deps/draco` (~197 files) so `cmake -S src` on CI does not need `deps/repos/filament`. |
| `c7b84c4` | **Revert `9f74544`** (nested fabric reloading). |
| `d61197f` | PR #40: that stack (including the revert) **is `main`**. |

Also on `main` (not unique to graphics):

| Commit | What |
|--------|------|
| `e0dcbc9` | **WASM Objects are not subject to Collapse.** Compositor only Expand/Collapse nodes that `CONTAINER::Node_IsMapManaged` → `MAPSVC::IsRegistered`. **Do not revert this** when touching map/proximity code. |

Earlier loading WIP that **was not** part of `9f74544` and **was not** reverted: `b15bdd8` (AnariRenderer reload debugging), `3ddc4f7` (compositor + renderer partial load fix). Those are still on `main`.

### Nested fabric (`9f74544`) — what landed, what was undone

Timeline: PR #37 put `9f74544` on `main` → `c7b84c4` reversed it on `graphics_upgrade` (conflicts resolved; `IsRegistered` kept) → **PR #40 put that reverse on `main`**. Do not force-push history. `LnGLive_*` must not reappear unless someone cherry-picks `9f74544` again.

What `9f74544` mixed together (two problem classes):

1. **Map / LnG / fabric teardown** — process-wide LnG live map (`namespace|service`); land/load-children on a worker (`Child_Enum` on compositor vs Socket.IO `SafeKill`); Connect on a MAPSVC thread; nested `Fabric_Close` instead of a leak log; `NODE::Handle()`; `FILE::Listener(nullptr)` on close; dying-flag wait around `OnFileReady` / MSF fetch.
2. **Reload presentation** — `Camera_Flush`; box submit gated on `MIN_REACH`; `SceneNeedsRebuild` when the box pool is empty but the frame has boxes (Filament first-instance cull).

The revert removed **both**. Graphics/Halogen work (`3c428aa`…`7e9cd4a`) and the earlier WIP (`b15bdd8`, `3ddc4f7`) were kept.

### Earth second load — prediction vs what was run

The session predicted that undoing `9f74544` would bring back a broken second Earth load (live LnG with no tree, hang, or blank planet). The originating user accepted that risk.

**After the revert, running the built Artemis did not reproduce that bug.** Treat “Earth second loading is broken on this tree” as **false for the path that was tried**.

Why the prediction was too strong (do not invert it into “`9f74544` was a no-op”):

- The blank/hung second Earth the session had been staring at was largely **compositor/GPU**: `flushAndWait` after a large commit or TDR, so URL teardown waited forever and the next fabric never really started. Halogen `v1.1.10` (no native-path `flushAndWait`, large meshes do not *cast* shadows), the triangle budget, and the 60 Hz floor **are still on `main`**. That class of failure is not what the MapSvc revert removed.
- Reload-presentation hacks in `9f74544` (empty box-pool rebuild, `MIN_REACH` box gate, `Camera_Flush`) were redundant with, or less important than, those Halogen/instancing fixes plus `b15bdd8` / `3ddc4f7`.
- `Expand` already had synchronous `LoadChildren` behind `#if 0` **before** `9f74544`. The compositor `Child_Enum` + `SafeKill` deadlock is not the default URL-bar path.
- If teardown can finish, a new `MAPSVC` that `Connect`s in its constructor is enough. `LnGLive` was for Close/`SafeKill` that **never completed** — which is exactly what a wedged compositor caused.

What `9f74544` may still matter for (not seen on the post-revert Earth retest): in-flight MSF `delete this` vs a destroyed attach node; nested fabrics only logged as leaked so `~MAPSVC` never runs; `Child_Enum` of a `RECOVERED` model on the wrong thread under load. If a hang or blank nested fabric shows up again, read `9f74544` as a **catalog of races**, not as a patch to restore wholesale.

### Artemis `origin/main` (fetched 2026-08-28)

Force-update: local pre-split tip `d06281e` was **not** an ancestor of the new `ab73058`. This workspace was reset to `origin/main`. Old history remains only in reflog (`d06281e`).

| Commit | What |
|--------|------|
| `7763683` | Rubidium-shaped initial commit |
| `9476696` | Isolate product identity in `branding/` |
| `9f3874f` | Overlay Artemis names on that hook |
| `16f37d0` | Record GitHub origin for this Artemis tree |
| `f52c6e0` | Solution file named `msvc/Artemis.sln` wrapping **Rubidium** `.vcxproj` files that still reference `..\..\Gezundheit` |
| `4adb974` / `c9f06fc` | Typo; merge upstream `FindWindowA` + `PRODUCT_WINDOW_CLASS` |
| `ab73058` | *Artemis Build* (script/CI/CMake path nits) |

App chrome now includes RmlUi URL bar / `ChromeRml` (not just the old native-only frame). Inspector Preview3D is in-tree. CLI: `MSF_CLI` (`Artemis --sign` / `--verify` in prose still say Rubidium).

### Pairing

- **This Cursor workspace:** Artemis `ab73058` + public Sneeze `d61197f` via `$env:SNEEZE_DIR`. Built 2026-08-28.
- **Public Rubidium session:** same engine pairing. Overlay is identity-only; do not copy `branding/Product.cmake` from Artemis into Rubidium.
- **Official Artemis CI:** expects Gezundheit, not this Sneeze path.

### Git identities (originating machine)

- Feature commits: `yinch <yinch@playko.com>`
- Some merge PRs: `RP1-Yinch`
- GitHub: **yinchy**
- Do not rewrite git config. Do not force-push `main`/`master`.

---

## 3. Architecture that this session actually used

### glTF / mesh pipeline (current)

```
FILE bytes (cache)
  → NODE::OnFileReady
      sniff: GLB magic "glTF" or JSON '{' → glTF, else stb texture on MAP_OBJECT
  → DEP::GLTF::Load          (src/deps/gltf/Gltf.cpp)
      fastgltf + meshopt views + Draco primitives
      triangle primitives only; encoded images; per-primitive AABB
  → Gltf_Render_Model_Build  (src/context/viewport/GltfMesh.cpp)
      flatten hierarchy, UV V flip in place, decode albedo, 8-corner bounds
      URL cache: Acquire / Publish / Release (process-wide, refcounted)
  → NODE::Gltf_Render_Model  (not MAP_OBJECT)
  → TraverseNode             (Compositor.cpp)
      MESH_BUILD: node world × draw local, pInstanceOwner=NODE*, nDrawIx
  → RENDERER::ANARI::SubmitMeshes / SyncMeshes
      one ANARI geometry+group per unique primitive
      one ANARIInstance per placed draw
      admit new GPU geometry under triangle/create/instance/texture caps
```

**Jonathan Hale’s rule (followed):** glTF loading lives in the engine framework (Sneeze), not in the ANARI/Halogen abstraction. Do not add `HALOGEN_GLTF_ASSET` unless product direction changes.

**Inspector preview:** `SCENE::Gltf_Preview` uses the same Load + Build path onto the primary node (host-driven, empty URL).

### Instancing identity (easy to break)

- CPU cache key = **resolved URL string**, on **NODE**.
- GPU instance identity = **`pInstanceOwner` (NODE*) + `nDrawIx`**, not vertex pointer.
- Two nodes sharing one `GLTF_RENDER_MODEL` must still get two ANARI instances.
- UV flip happens **once** on the cached model. Do not flip per instance.

### Map proximity vs WASM (keep)

- Map registry is keyed by **composed OBJECTIX** (`OBJECTIX_COMPOSE(class, objectix)`), not raw `NODE::ObjectIx()`.
- Compositor composes the key itself (after nested-fabric revert, `NODE::Handle()` does not exist).
- Only `Node_IsMapManaged` nodes expand/collapse. WASM-injected / static-MSF nodes stay at any camera distance.
- Tester01 is WASM `Node_Open`. It does **not** use MAPSVC Expand/land workers. Do not debug Tester01 as a nested-Earth reload.

### Native rendering vs readback

- Primary path: Halogen `HALOGEN_NATIVE_SURFACE` — Filament presents to the HWND swapchain. No CPU blit.
- Readback path is a known non-functional fallback with the current compositor (blank SDL). Do not “fix colors” by swapping pixel formats until you have confirmed you are actually on readback.
- Renderer name from the app: `"halogen"`.

### Threads (symptoms look like “the app is dead”)

| Thread | Owns |
|--------|------|
| UI / Win32 pump | URL bar, menus, `SCENE::Url` / teardown, `Cancel()` waiting on compositor |
| Compositor agent | Traversal, ANARI submit, orbit camera, FPS log |
| RMAP / LnG | Map `onReadyState`, `Child_Enum` |
| Network fetch pool | `OnFileReady` / MSF fetch completions |

If camera, FPS log, and URL navigation freeze together, the compositor is stuck (historically inside Halogen `flushAndWait` after a TDR). The URL history popup can still open because it is Win32.

---

## 4. How to build (what actually worked)

### Day-to-day (Windows) — this workspace

`Gezundheit` is missing. **Must** point at public Sneeze. After switching onto the rewritten Artemis tree, **`-Fresh`** was required: the old CMake cache was `CMAKE_PROJECT_NAME=Artemis` and `project()` is now `Rubidium`.

```powershell
$env:SNEEZE_DIR = "C:\Users\yinch\Documents\GitHub\cursors\Sneeze"
.\scripts\build-windows.ps1 -Fresh          # configure only, first time on this tree
.\scripts\build-windows.ps1                 # Release
.\scripts\build-windows.ps1 -Config Debug
.\scripts\build-windows.ps1 -All            # first machine: SDL3 + configure + build
```

Verified 2026-08-28: configure ~9s, full Release ~2 min. CMake generator **Visual Studio 18 2026** / MSVC 19.50.

Output:

- App: `builds\windows-x64\install\release\bin\Artemis.exe` (CMake target `Rubidium`, `OUTPUT_NAME` Artemis)
- Setup: `…\bin\ArtemisSetup.exe` (target `RubidiumSetup`)
- Engine: `..\Sneeze\builds\windows-x64\install\release\lib\Sneeze.lib`

Sneeze compiles **inline**. Halogen `dep-verify` reported `v1.1.10`.

On a machine with `../Gezundheit`, omit `SNEEZE_DIR`.

### First-time / deps

Sneeze third-party libs at `Sneeze/deps/builds/windows-x64/{debug,release}/libs/`. The app does not rebuild Wasmtime/Halogen/curl. SDL3 is the app deps tree (`Artemis/deps/…`).

Halogen pin mismatch: Sneeze **`-Sync`**.

### What failed / will fail (do not repeat)

- **Bare `.\scripts\build-windows.ps1` here** — `Resolve-Path ../Gezundheit` throws.
- **Reuse the pre-split CMake cache** without `-Fresh` — project name / targets / `SNEEZE_DIR` are wrong.
- **`msvc/Artemis.sln` on this machine** — projects are `Rubidium.vcxproj` and Sneeze paths are **`..\..\Gezundheit\msvc\…`**. Use the CMake-generated sln under `builds/` or retarget. Hand-maintained vcxproj is still **v143**; VS 18 CMake used the VS 18 toolset successfully.
- **`msvc/Sneeze.vcxproj` with VS 18 without v143** — `MSB8020`.

### `-Rebuild` semantics (easy to get wrong)

`-Rebuild` is a **modifier**, not a mode. Alone it cleans **only the app tree, current config**. It must **not** touch `deps/` unless `-Deps`, `-Only`, or `-All` is also present.

After changing `add_subdirectory` binary dirs, **`-Fresh`** is required.

### Debug vs Release

- **Deps** are per-config folders. Never mix.
- **App** is one multi-config tree: `builds/<plat>/build/` → `install/{debug,release}/`.
- `CMAKE_CONFIGURATION_TYPES` is pinned to `Debug;Release`.

### New source files

Update **both** `src/CMakeLists.txt` and `msvc/Rubidium.vcxproj` (`.filters`). `sync-build` skill still applies. Do not add files only to a leftover `Artemis.vcxproj` name — those were renamed.

### Optional Draco test asset

```
https://cdn.rp1.com/fabric/res/doughnut1Mb.glb
```

~9 MB. `extensionsRequired` includes `KHR_draco_mesh_compression`. ~10 meshes, ~1,999,180 vertices, **~999,440 triangles**.

```powershell
$env:SNEEZE_DRACO_GLB = "C:\path\doughnut1Mb.glb"   # GltfTest Test 5
```

---

## 5. Learned efficiencies (do these first)

1. **Symptom-split UI vs compositor.** Frozen camera + dead FPS + URL change that never completes = compositor hung. URL popup still working = UI thread alive. Do not start in map/WASM.
2. **MissingExtensions on a GLB** is usually **`KHR_draco_mesh_compression` REQUIRED**, not meshopt. Enabling the fastgltf flag without a decoder still yields empty accessors (no ordinary bufferViews for POSITION/indices).
3. **Do not compile meshopt/Draco from `deps/repos/filament/third_party` in Sneeze’s `cmake -S src`.** CI does not have that clone. Decode units are **vendored** under `Sneeze/src/deps/`. Filament’s cmake-generated `draco_features.h` is replaced by the checked-in `src/deps/gltf/draco_config/`.
4. **Instancing cache belongs on NODE by URL**, not on MAP_OBJECT. Map objects are the wire/schema; the render model is a node resource.
5. **GPU admit is triangle-budgeted, not just “4 meshes/frame”.** Four ~100k-tri primitives in one Halogen flush was enough to TDR. First new geometry of a frame may exceed `65536` tris; further creates wait.
6. **Halogen native path must not `flushAndWait` every frame.** That wait is for CPU readback. Native swapchain: Filament `beginFrame` already syncs. After removing the wait, compositor can spin at absurd FPS (log shows 0.0 ms) — hence the **60 Hz sleep floor** in `AGENT::COMPOSITOR::Execute_Render`.
7. **Large meshes: disable shadow *casting*** (`idxCount <= 196608` in Halogen `Surface.cpp`). They still **receive**. 4x MSAA + PCF on ~100k-tri casters was the TDR partner of `flushAndWait`.
8. **Do not restore `9f74544` to “fix Earth reload.”** Post-revert Artemis could not reproduce that bug. `9f74544` mixed map-socket reuse with renderer hacks; the visible reload failure was mostly a wedged compositor (still fixed by Halogen 1.1.10). If a nested-fabric race returns, read that commit as a catalog — do not add a manager above LnG, and do not re-apply the patch wholesale.
9. **Revert conflicts on `AnariRenderer.cpp` were whole-file** (already resolved in `c7b84c4`). If someone replays that revert: keep **ours** (instancing/Draco), then delete only the nested-fabric box-pool rebuild. `--theirs` (parent of `9f74544`) would wipe instancing.
10. **`Resource_Load` must keep the URL argument** when mixing nested-fabric revert with instancing (`OnFileReady` captures `pFile->Url()`).
11. **Build with the script, not the hand-maintained vcxproj**, unless v143 is installed.
12. **Gitignored binaries exist even when Glob returns 0.** `Test-Path` the install tree.

---

## 6. Non-obvious gotchas (session + standing)

### Graphics / Halogen

- `flushAndWait` on native swapchain → compositor hang after large upload or TDR. Camera, FPS, `Cancel()` all wait. SteamVR overlay can flash; that is the XR runtime bouncing on a wedged Vulkan device, **not** a second VR session.
- After TDR, `flushAndWait` may **never return**. Killing the wait is not enough if you still TDR — hence triangle budget + no large shadow casters.
- Filament `beginFrame` skipper + no wait ⇒ busy-loop compositor. 60 Hz floor is load-bearing for orbit `dt` and the FPS trace.
- Box pool is **grow-only** and **not** counted in `SceneNeedsRebuild` (streaming tiles). Committing Earth-sized boxes at `dRenderScale == 1` can make Filament cull that first instance permanently. Nested-fabric tried to rebuild when the pool was empty; **that snippet is reverted**. If boxes vanish on first load, this is a suspect — it was **not** required for the post-revert Earth second-load that worked.
- ANARI destructor: `ReleaseScene` must render an empty frame (`ANARI_WAIT`) and drain before releasing the native surface, or the next context on the same HWND comes up blank.
- UV: glTF V=0 at top, ANARI V=0 at bottom. Flip **once** in `Gltf_Render_Model_Build`.
- Basisu / webp parse but stay encoded; stbi cannot decode them. Albedo empty until a later path.
- Non-triangle primitives are skipped. Do not expect points/lines/strips.

### Map / fabrics / files

- `NODE::ObjectIx()` is the **raw** index. MapSvc registry uses the **composed** handle. Expanding with the raw index silently no-ops.
- `Expand` on compositor + `Child_Enum` of a `RECOVERED` model can deadlock `SafeKill`. `9f74544` moved land to a worker. After revert, Expand is still subscribe-only with `#if 0` immediate `LoadChildren` (that `#if 0` predates `9f74544`). Do not assume the default Earth URL-bar path hits this deadlock.
- `~FABRIC` again only **logs** nested-fabric leaks rather than `Fabric_Close`ing them. That was half of `9f74544`. A second Earth load via the URL bar still worked after the revert, so this leak is not sufficient by itself to explain the old “no tree” failure — that failure needed a compositor that never finished teardown. Still worth watching if MAPSVC appears not to destruct.
- MSF fetches are fire-and-forget `delete this` with a raw NODE pointer. `Url()` / `Reload()` during an in-flight nested MSF is UAF territory (`Scene.md` Known Limitations). Nested-fabric added a dying wait; that is gone. Not observed on the post-revert Earth retest.
- `FILE` snapshot fields survive `Release()`. Detail/inspector attach uses `Request()` / `Release()`. List views must not require attach.
- `nlohmann::json j;` is **null**, not `{}`. `.value()` throws. App updater already hit this.

### App / SDL / inspector

- **One** `SDL_PollEvent` loop, in the platform `APPNATIVE` event loop (`App_Win32.cpp` in `namespace RUBIDIUM`). New SDL windows implement `ISDLWINDOW` and register. A second poll loop steals events.
- `HandleEvent` = per-event input. `ProcessInput` = once per frame, **all** AppFrames (unfocused scroll).
- RmlUi inspector renders on events, not on a timer. Async cache/log updates need a wake + dirty render (not implemented).
- Inspector CSS: `dp` for layout, `1px` for hairline borders. Font family injected via `[{FONT-FAMILY}]` **before** `LoadDocument`.

### Build / CI

- `deps/repos/` is gitignored. CI sneeze job ≠ a developer machine with Filament cloned.
- Helide is **off**. Device DLL is `anari_library_halogen.dll`. Wasmtime stays a DLL (~23 MB).
- Release PDBs: `/Zi` + `/DEBUG` + force `/OPT:REF` `/OPT:ICF` (`/DEBUG` alone disables those).
- Imported-target include dualization is **on-disk-conditional** (both debug and release includes must exist or CMake generate fails).

### Coding conventions (Sneeze + Artemis, carry into Rubidium unless the fork says otherwise)

- 3-space indent, Allman braces, Hungarian (`p` `n` `s` `b` `a` `d` `fn`).
- Types/namespaces ALL CAPS. Functions Capitalized. Getters have no `Get` (exception: `GetInstance()`).
- Single `return` at end of function.
- No `using namespace SNEEZE` in the app. Fully qualify `SNEEZE::…`.
- No `printf` in module code; use the logger.
- Include guards `SNEEZE_…` / `ARTEMIS_…`.
- If Rubidium is public, **do not blindly copy proprietary Artemis headers** onto new files without an explicit license decision.

---

## 7. File map (where to look)

### Sneeze — glTF / GPU

| Path | Role |
|------|------|
| `src/deps/gltf/Gltf.cpp` / `Gltf.md` | fastgltf parse, meshopt + Draco decode |
| `src/deps/meshoptimizer/` | vendored decode-only meshopt |
| `src/deps/draco/` + `src/deps/gltf/draco_config/` | vendored decode-only Draco + features header |
| `src/context/viewport/GltfMesh.cpp` | CPU model → `GLTF_RENDER_MODEL` + URL cache |
| `src/context/viewport/AnariRenderer.cpp` | ANARI triangles, instancing, admit caps, `SceneNeedsRebuild` |
| `src/context/viewport/Viewport.md` | canonical mesh/instance docs |
| `src/context/scene/Node.cpp` | fetch, sniff, cache acquire, dying-flag **must stay absent** on this branch |
| `src/sneeze/control/Compositor.cpp` | traverse, mesh emit, map-managed expand/collapse, 60 Hz floor |
| `tests/GltfTest.cpp` | Test 5 gated on `SNEEZE_DRACO_GLB` |

### Sneeze — map / nested fabrics

| Path | Role |
|------|------|
| `src/context/scene/MapSvc.cpp` | LnG connect (in Impl ctor after revert), Expand/Collapse, `IsRegistered` |
| `src/context/Container.cpp` | `Node_IsMapManaged` |
| `src/context/scene/Fabric.cpp` | nested close **reverted** (leak log / `Node_Close(ObjectIx())`) |
| `src/context/scene/Scene.md` | map-managed vs WASM, known reload limitations |

### Halogen (tag v1.1.10)

| Path | Role |
|------|------|
| `deps/repos/Halogen/src/Frame.cpp` | skip `flushAndWait` when `nativeSurface` |
| `deps/repos/Halogen/src/Surface.cpp` | `castShadows = idxCount <= 196608u` |
| `deps/dependencies.json` | `"halogen": { "ref": "v1.1.10" }` |

### App (this Artemis / Rubidium tree)

| Path | Role |
|------|------|
| `.cursor/rules/project.mdc` | **Current** product split, branding hook, Gezundheit default |
| `branding/Product.cmake` | Artemis-owned identity (names, CDN, mutex, home URL) |
| `branding/Brand.props` | MSVC `TargetName` — keep in sync with Product.cmake |
| `scripts/build-windows.ps1` | Reads branding; default engine `../Gezundheit` |
| `msvc/Artemis.sln` | Wrapper sln; **projects** are `Rubidium.vcxproj` + Gezundheit Sneeze |
| `sneeze-gltf.md` | Untracked leftover; **stale** two-path spec |

---

## 8. Open work / how to continue

**Safe to extend on Sneeze `main` (`d61197f`+)** — same tree as `graphics_upgrade`.

- More glTF material features (clearcoat etc. already *parsed*; PBR subset is what ANARI gets).
- KTX2 / WebP albedo decode.
- Tune `MAX_NEW_TRIANGLES_PER_FRAME` / shadow cutoff with real assets — do not remove them “for FPS”.
- Wire inspector Network/Performance to live Sneeze data (still listed as next in `project.mdc`).
- Preview3D / inspector host `Gltf_Preview` UX in the app.

**Do not re-apply `9f74544`.** It is not on the `main` working tree, Earth second load worked without it, and restoring it fights instancing/Draco (`Node.cpp` `OnFileReady`, whole-file `AnariRenderer.cpp`, `Scene.md` rows). Keep `IsRegistered`. If a nested-fabric race is proven again, take the smallest piece from that commit that matches the new evidence.

**Merge policy:** PR #40 already merged `graphics_upgrade` (including `c7b84c4`) into `main`. Further graphics work can land on `main` or a new branch from `d61197f`. Do not merge an old `main` that still has live `LnGLive` as if it were newer.

**Rubidium / Artemis first tasks for a new session**

1. Read `.cursor/rules/project.mdc` in **this** tree (branding + Gezundheit). Do not assume `ARTEMIS::`.
2. Engine: public Sneeze `main` ≥ `d61197f`, **or** Gezundheit if that sibling exists. Set `$env:SNEEZE_DIR` when the default path is missing.
3. `-Fresh` if the CMake cache predates `project(Rubidium)`.
4. `.\scripts\build-windows.ps1` — not `msvc/*.vcxproj` on VS 18 unless v143 + Gezundheit paths are real.
5. Shared chrome: land in **Rubidium**, `git fetch upstream` / merge into Artemis. Do not fork `AppFrame` for product strings.
6. Do not implement `sneeze-gltf.md` Path A.
7. Do not resurrect `9f74544` unless a nested-fabric failure is newly reproduced.

---

## 9. False paths already paid for (do not restart)

| Dead end | Why |
|----------|-----|
| Filament `gltfio` / `HALOGEN_GLTF_ASSET` for doughnut | Loader belongs in Sneeze; doughnut is Draco; GPU hang is Halogen frame/shadows, not parse. |
| “Just enable the Draco extension flag” | No CPU accessors until Draco decode fills streams. |
| Compile meshopt/Draco from Filament’s third_party in Sneeze CMake | Breaks macOS/Linux CI `cmake -S src`. |
| `flushAndWait` every native frame “to be safe” | Serializes compositor; TDR → permanent hang. |
| Admit 4 large primitives per frame | TDR with 4x MSAA + PCF shadows. |
| glTF cache on MAP_OBJECT | Wrong lifetime/identity; moved to NODE. |
| Strip `Node_IsMapManaged` while reverting nested fabric | That is `e0dcbc9`, a later keep. |
| `git checkout --theirs` on `AnariRenderer.cpp` during that revert | Drops instancing/Draco. |
| Force-push `main` to forget nested fabric | PR #40 already reverted it on `main`; rewriting history is unnecessary and hostile. |
| Re-apply `9f74544` because “second Earth must be broken” | Post-revert Artemis did not reproduce that bug. |
| Debugging second Earth as a WASM Tester01 bug | Different node-open path. |
| Second `SDL_PollEvent` loop for a new window | Events vanish. |
| Rename `namespace RUBIDIUM` to `ARTEMIS` in this tree | Branding overlay exists so you do not. Breaks upstream merges. |
| `.\scripts\build-windows.ps1` without `SNEEZE_DIR` on this machine | Default `../Gezundheit` does not exist. |
| Open `msvc/Artemis.sln` expecting `../Sneeze` | The sln references `..\..\Gezundheit`. |
| Merge pre-split `ARTEMIS::` / `Artemis-Legacy` into this main | Histories were force-replaced; `project.mdc` forbids it. |

---

## 10. Canonical docs vs this file

| Doc | Use for |
|-----|---------|
| App `.cursor/rules/project.mdc` | Artemis-as-branded-Rubidium, branding hook, Gezundheit, inspector, installer |
| Sneeze `project.mdc` | Engine modules, deps, Halogen pin, `-Sync` |
| `Sneeze/src/deps/gltf/Gltf.md` | Loader + Draco/meshopt as implemented |
| `Sneeze/src/context/viewport/Viewport.md` | Instancing, admit caps, invalidation |
| `Sneeze/src/context/scene/Scene.md` | Map vs WASM, threading, reload limitations |
| `Sneeze/src/sneeze/control/Control.md` | Compositor proximity (may not mention WASM exemption; **code** has `Node_IsMapManaged`) |
| **This file** | Session outcomes, PR #40, Earth retest, 2026-08-28 fetch/build, doughnut/TDR diagnosis |

When this file and `sneeze-gltf.md` disagree, **this file and `Gltf.md` / `Viewport.md` win**.
