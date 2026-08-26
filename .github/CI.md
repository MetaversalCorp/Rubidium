# Rubidium CI

Per-platform build matrix for `linux`, `macos`, `ios`, `android`, `quest`.
Each Rubidium build consumes pre-built Sneeze artifacts from the Sneeze CI -
no Sneeze deps are built here.

## Workflow

Single file: **`build.yml`**. One job per platform + `find-sneeze` upfront.

**Triggers:** push to `main` that changes **`VERSION`** runs **Build** (release
cut) with macOS **sign + notarize + Deploy**. Manifest-only commits from Deploy /
`commit-manifest.cmd` do not retrigger Build. Manual **Build** (`workflow_dispatch`)
has **Deploy** and **Notarize** checkboxes (both unchecked by default); check
them for production manual runs.

**Jenkins (Prod-Rubidium):** **Poll SCM** + Git **Additional Behaviours** ->
**Polling ignores commits in certain paths** -> **Included Regions**:
`VERSION` (leave **Excluded Regions** empty). Manifest-only pushes are ignored.

## Flow

```
find-sneeze  -> discover the latest successful Sneeze CI run on main
   ↓
linux/macos/ios/android/quest  -> checkout Sneeze repo + download matching
                                  <platform>-* artifacts, build Rubidium,
                                  upload app artifact
```

All platform jobs run in parallel after `find-sneeze`.

## Cross-repo artifact download

`find-sneeze` queries Sneeze's recent successful **Build** workflow runs via
`gh api` and picks the latest run that still has dep artifacts (`linux-*`,
`macos-*`, `android-*`, etc. - roughly 10+ per platform). **The Rubidium
workflow fails** if no qualifying run exists (auth errors, no green Sneeze
build on the branch / `main`, or artifacts expired). Platform jobs do not run
silently without Sneeze.

Sneeze CI sets `retention-days: 90` on uploaded deps (public repo maximum). Each platform also
uploads a `${platform}-libs` bundle artifact. A run can stay green in the UI
while its artifacts are already gone - trigger a fresh **Build** on `main` in
`MetaversalCorp/Sneeze` if `find-sneeze` reports zero usable artifacts.

**Manual override:** Re-run Rubidium **Build** via `workflow_dispatch`. Optional
inputs: `sneeze_run_id` (numeric ID from the Sneeze run URL
`.../actions/runs/12345678`); **Notarize** (submit DMG to Apple - leave unchecked
for faster sign-only test builds); **Deploy** (run Deploy after success).

Each platform job then uses `actions/download-artifact@v4` with
`repository: MetaversalCorp/Sneeze` and `run-id` to pull pre-built deps.

Requires `SNEEZE_TOKEN` secret on **Rubidium** (read-only PAT for the private
**Sneeze** repo). **When Sneeze goes public, this token is no longer needed.**

### `SNEEZE_TOKEN` setup

1. GitHub -> **Settings -> Developer settings -> Personal access tokens** (classic
   or fine-grained).
2. Grant **read** access to `MetaversalCorp/Sneeze` (classic: `repo` scope;
   fine-grained: **Contents: Read**, **Actions: Read**).
3. If the org uses **SAML SSO**, open the token and click **Configure SSO** ->
   authorize **MetaversalCorp**.
4. Rubidium -> **Settings -> Secrets and variables -> Actions** -> secret name
   **`SNEEZE_TOKEN`** - paste the token only (no quotes, no trailing newline).

`HTTP 401 Bad credentials` in `find-sneeze` means the secret is missing,
expired, revoked, lacks scopes, or SSO was not authorized.

`No qualifying Sneeze CI run on main` with per-run counts like
`linux=0 macos=0 android=0` means auth works but artifacts are missing or
expired - re-run Sneeze **Build** on `main`, not Rubidium alone.

## Sneeze version policy

Rubidium always consumes the **latest successful Sneeze CI output on `main`**
(`find-sneeze` does not match feature branches). Both repos are expected to
auto-build on `main` updates; re-run Sneeze **Build** on `main` if Rubidium
fails with missing artifacts or API header checks.

**Jenkins (Windows):** `ci-windows.ps1` resets `../Sneeze` to `origin/main`,
rebuilds Sneeze (`build-windows.ps1 -All -Sync`), then builds Rubidium. Bind
the SCM SSH credential as `SNEEZE_CI_SSH_KEY` (+ `SNEEZE_CI_SSH_PASSPHRASE`)
so the sibling Sneeze `git fetch` works (same as the Sneeze Jenkins job).
`-Sync` moves persistent `deps/repos/*` clones to each recipe's pinned tag when
Sneeze bumps a dep (e.g. Halogen `v1.1.8`) without wiping the whole workspace.
There is no separate Windows Sneeze artifact download - the sibling checkout
**is** the engine build input. Pass `-Deploy` only when publishing to CDN.

**Standalone Sneeze Jenkins job** (`ci-sneeze-windows.ps1`): defaults to
`-Fresh -Rebuild` (Sneeze source only - **does not** build new deps). When
`origin/main` adds a dep (e.g. `sneeze-sdk` for `sneeze_abi.h`, `rmap-core`)
and the agent lacks the stamp/install tree, do **not** change the CI script for
a one-off - re-run once on the agent with deps:

```bat
pwsh -ExecutionPolicy Bypass -File scripts\ci-sneeze-windows.ps1 -Config Release -All
```

Stamp-cached: existing deps are skipped; only missing ones (and then Sneeze)
build. After that, the normal job (no `-All`) is fine until the next new dep.

**Server3-Rubidium** (staging): use `scripts\jenkins-server3.bat` as the sole
build step (includes `-Deploy`), or call `ci-windows.ps1` with `-Deploy`,
`-CdnRoot`, `-ManifestCdnUrl`, and `-Config Release`. Do **not** pass
`-DefaultHome` (deprecated, ignored).

**Prod-Rubidium** (production): pass `-Deploy` plus production `-CdnRoot` /
`-ManifestCdnUrl` as needed. Without `-Deploy`, CDN upload is skipped (build
+ package only - same as GitHub Build with Deploy unchecked).

### Jenkins - Windows code signing (Azure Artifact Signing)

Windows Jenkins builds sign via **Azure Artifact Signing** (Trusted Signing) in
`scripts/ci-windows.ps1` - not a `.pfx` in the repo or in GitHub Secrets.

| What | Where |
|------|--------|
| Account / profile / region | `pkg/signing-metadata.json` (public) |
| Private key | Microsoft cloud - never on disk |
| Auth | **Service principal** via Jenkins secret bindings (recommended) |

#### 1. Azure Portal - service principal

1. **Microsoft Entra ID** -> **App registrations** -> **New registration**
   (e.g. `rubidium-jenkins-signing`).
2. Note **Application (client) ID** and **Directory (tenant) ID**.
3. **Certificates & secrets** -> **New client secret** -> copy the value once.
4. **Azure Portal** -> **Artifact Signing** -> account **Metaversal** -> certificate
   profile **Rubidium** -> **Access control (IAM)** -> **Add role assignment**:
   - Role: **Artifact Signing Certificate Profile Signer**
   - Member: the app registration / service principal from step 1

Subscription ID (optional but recommended for `az account set`):
`2fa6a7a8-1c71-458f-a052-80c52d287387` (or your signing subscription).

#### 2. Jenkins - store secrets

**Manage Jenkins** -> **Credentials** -> **(global)** -> **Add Credentials**:

| Kind | ID (example) | Secret value |
|------|----------------|--------------|
| Secret text | `azure-tenant-id` | Tenant GUID (`880d87e2-1e58-43c1-9bb2-7fc55e2599fd`) |
| Secret text | `azure-client-id` | App registration client ID |
| Secret text | `azure-client-secret` | Client secret value |
| Secret text | `azure-subscription-id` | Subscription GUID (optional) |

#### 3. Prod-Rubidium job - bind secrets to env vars

**Configure** -> **Build Environment** -> check **Use secret text(s) or file(s)** ->
**Add** -> **Secret text** for each:

| Variable | Credentials |
|----------|-------------|
| `AZURE_TENANT_ID` | `azure-tenant-id` |
| `AZURE_CLIENT_ID` | `azure-client-id` |
| `AZURE_CLIENT_SECRET` | `azure-client-secret` |
| `AZURE_SUBSCRIPTION_ID` | `azure-subscription-id` (optional) |

Same pattern as `GITHUB_TOKEN` on the `commit-manifest.cmd` step.

Service principal is **required** for signing (no personal `az login` fallback).
`ci-windows.ps1` and `test-azure-signing.ps1`:

1. Run `az login --service-principal` (password via stdin, or `-p` on older az CLI).
2. Use a temp `signing-metadata.json` with **EnvironmentCredential only**
   (excludes `AzureCliCredential` and interactive providers).
3. Sign with `signtool` + `Azure.CodeSigning.Dlib.dll`.

**`pkg/signing-metadata.json`** (checked in) is the base profile/endpoint; CI
overrides credentials via the temp metadata file. Set `SKIP_SIGN=1` to build
unsigned locally without service principal vars.

**Diagnostics on the agent** (with Jenkins secrets bound, or env vars set manually):

```powershell
pwsh -ExecutionPolicy Bypass -File scripts\test-azure-signing.ps1
```

**Unblock a stuck build** (unsigned artifacts):

```powershell
$env:SKIP_SIGN = '1'
pwsh -ExecutionPolicy Bypass -File scripts\ci-windows.ps1 ...
```

`ci-windows.ps1` runs an Azure token preflight before signing and applies a
**per-file signtool timeout** (`SIGN_TIMEOUT_SEC`, default 600) so a hung Azure
call fails in minutes instead of blocking Jenkins for hours.

**Prod-Rubidium job steps** (typical):

1. `pwsh -ExecutionPolicy Bypass -File scripts\ci-windows.ps1 -Deploy -Config Release` (and production `-CdnRoot` / `-ManifestCdnUrl` as needed). Omit `-Deploy` for a test build with no CDN upload.
2. `call scripts\commit-manifest.cmd` with **GITHUB_TOKEN** bound via Credentials Binding (see `commit-manifest.cmd` header). Only after a `-Deploy` publish. The script rebases onto `origin/main` before push so a long build does not fail when main moved ahead during the job.

## Platform builds

Each job:
1. Checkout Rubidium + Sneeze (for toolchain + find modules).
2. Download + arrange Sneeze artifacts into `Sneeze/libs-<platform>/`.
3. Configure Rubidium via `cmake -S src` with `-DLIBS_DIR=...`.
4. Build + upload Rubidium binary/app bundle.

**Quest** reuses the Android Sneeze artifacts - same arm64 + OpenXR build.
Only difference is the manifest (`pkg/android/AndroidManifest-quest.xml`).

## Packaging

Desktop installers use CPack in `src/CMakeLists.txt` (NSIS, DMG, TGZ).

Mobile / VR ship as signed APKs (no CPack):
- **Android:** `apk-android` job -> `rubidium-android-arm64.apk`; local build via `pkg/android/build-apk.sh`
- **Quest:** `apk-quest` job -> `rubidium-quest.apk` (renamed to `Rubidium-<ver>-quest-arm64.apk` on CDN); local build via `pkg/android/build-quest-apk.sh` or `build-apk.sh --quest`

Quest uses `AndroidManifest-quest.xml` (OpenXR / `com.oculus.intent.category.VR`), min SDK 29, and `-DRUBIDIUM_PLATFORM=quest-arm64`.

Rubidium also builds a standalone `RubidiumSetup` updater executable on
desktop platforms.

## Linux / macOS runtime (build OK, app fails)

Desktop **Linux** and **macOS** use `App_SDL.cpp` (not the Win32 shell). The
engine still needs Halogen, Wasmtime, and the ANARI loader next to the binary
at runtime.

### Where to look

| Symptom | What to check |
|--------|----------------|
| `error while loading shared libraries` | Run from `builds/<plat>/install/release/bin/` (local) or ensure the CI artifact’s **whole** `bin/` directory is on `PATH` / cwd. `ldd ./Rubidium` and `ldd ./libanari_library_halogen.so` on Linux. |
| `Failed to load library` / no 3D view | Halogen + `libanari` missing or wrong cwd. POST_BUILD copies `libwasmtime.so`, `libanari.so`, `libanari_library_halogen.so` into `bin/`; Linux sets `INSTALL_RPATH` `$ORIGIN`. |
| RmlUi / “No font face defined” | Linux: install system fonts (`fonts-dejavu-core` or `fonts-liberation` on Debian/Ubuntu) or ensure `bin/fonts/` from `scripts/build-deps.sh --only fonts`. macOS: bundled `Rubidium.app/Contents/Resources/fonts/` (CI downloads fonts before build). Windows: bundled `bin/fonts/`. |
| Engine init | Stdout/stderr for `Paths initialized`, `ANARI renderer initialized`, `UI_CONTEXT`. macOS CI smoke test greps `Paths initialized` in the app log. |

### Local builds

```bash
# After scripts/build-linux.sh or build-macos.sh
./builds/linux-x64/install/release/bin/Rubidium    # Linux - stay in bin/
open builds/macos-universal/install/release/bin/Rubidium.app   # macOS - use the .app
```

### CI artifacts

- **linux:** `rubidium-linux-x64` uploads the full `build/install/release/bin/` (binary + `.so` files). Do not run only the bare `Rubidium` file elsewhere.
- **macos:** `rubidium-macos-universal` is a **zip** of `Rubidium.app` (use `ditto -xk` or double-click, then drag the `.app` - not the bare `Contents` folder). `rubidium-macos-universal-dmg` is the shipped installer. Both bundles must have `CFBundleExecutable` = `Rubidium` and `Contents/MacOS/Rubidium` executable.
- **macos DMG:** Drag **Rubidium.app** to **Applications**. Ignore any stale DMG that also shows a `bin/` folder at the top level - that layout shipped dylibs outside the bundle and breaks after drag-install. A slash on the app icon (“not compatible with this Mac”) means the binary lacks your CPU arch; CI requires fat `arm64` + `x86_64` on `Contents/MacOS/Rubidium` and the bundled dylibs (`lipo -info` on a mounted DMG).
- **macos MoltenVK:** CI builds a universal `libMoltenVK.dylib` via `scripts/build-moltenvk-universal.sh` (cached under `moltenvk-universal/`). Homebrew `molten-vk` is arm64-only and is not used in GHA. Local builds can pass `-DMOLTENVK_DYLIB=...` or use brew.
- **macos Gatekeeper (“cannot verify / may harm your Mac”):** Unsigned build. **Right-click -> Open -> Open**, or after install:
  ```bash
  xattr -cr /Applications/Rubidium.app
  open /Applications/Rubidium.app
  ```
  CI ad-hoc-signs by default. When Apple cert secrets are configured, CI signs
  `Rubidium.app` and the DMG. Notarization runs on **VERSION** pushes to `main`
  or when **Notarize** is checked on a manual Build.
- **macos “cannot be opened because of a problem”:** Usually an immediate crash (dyld) or abort during SDL/Vulkan/GPU init. **Do not double-click first** - run from Terminal and paste the full output:
  ```bash
  xattr -cr /Applications/Rubidium.app
  /Applications/Rubidium.app/Contents/MacOS/Rubidium
  ```
  Also open **Console.app -> Crash Reports** and search for `Rubidium`. CI smoke only proves the engine reaches `Paths initialized` and stays alive 15s on the runner; a GPU/Halogen crash on your Mac can still happen after that.

### macOS code signing + notarization (GitHub Actions)

To enable signed/notarized macOS artifacts in `build.yml`, add these repo
secrets in Rubidium:

| Secret | Value |
|--------|-------|
| `APPLE_DEVELOPER_ID_APP_CERT_P12_BASE64` | Base64 of your exported Developer ID Application cert `.p12` |
| `APPLE_DEVELOPER_ID_APP_CERT_PASSWORD` | Password used when exporting that `.p12` |
| `APPLE_DEVELOPER_ID_APPLICATION` | Signing identity string (for example `Developer ID Application: Company Name (TEAMID)`) |
| `APPLE_NOTARY_APPLE_ID` | Apple ID email used for notary submission |
| `APPLE_NOTARY_APP_PASSWORD` | App-specific password for that Apple ID |
| `APPLE_NOTARY_TEAM_ID` | Apple Developer Team ID |

Notes:
- Without these secrets, the macOS job still builds and uploads ad-hoc signed artifacts.
- With cert secrets set, CI runs `cmake --install`, then one `codesign --deep`
  pass with `pkg/macos/Rubidium.entitlements` (JIT allowances required for Wasmtime).
  pass on `Rubidium.app` (all nested Mach-O in a single step), builds the DMG with
  `hdiutil create`, and signs the DMG wrapper.
- **Notarize** checkbox (manual, default off) or **VERSION** push: adds
  `--timestamp` on the app + DMG, submits to `notarytool`, staples on success.
  Sign-only manual builds skip timestamp and notarization (~15-45 min faster).

### Jenkins vs GHA

Windows **Jenkins** rebuilds sibling `../Sneeze` and copies DLLs into `bin/`. Linux/macOS **GHA** consumes Sneeze dep artifacts; if Halogen/ANARI paths drift, re-run Sneeze **Build** on `main` and Rubidium with a fresh `sneeze_run_id` if needed.
