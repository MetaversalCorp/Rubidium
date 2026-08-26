# Windows x64 build.
#
# Default: compile + link Rubidium only. Plain `cmake --build` against the
# Rubidium build tree. No dep checks, no configure step. Fails naturally if
# the tree or the dep libraries aren't there yet.
#
# The Rubidium src tree is a SINGLE multi-config tree at
#   builds/windows-x64/build/
# that emits Debug or Release into
#   builds/windows-x64/install/{debug,release}/bin/
# depending on the -Config flag (which drives `cmake --build --config`).
# Opening builds/windows-x64/build/Rubidium.sln in Visual Studio and switching
# the Debug/Release dropdown from the toolbar Just Works -- both configs
# build against their respective deps without any reconfigure.
#
# The DEPS trees stay per-config (deps/builds/windows-x64/{debug,release}/)
# and both must be built on disk before you can flip the VS dropdown to a
# config whose deps don't exist yet.
#
# Flags switch the script into deps mode, reconfigure mode, or deps+Rubidium mode:
#
#   -Deps         Build SDL3 into deps/builds/windows-x64/<config>/libs/.
#   -Fresh        Reconfigure the Rubidium tree from scratch (cmake -S src --fresh).
#                 Does NOT build -- just regenerates the project files.
#                 Wipes CMakeCache.txt + CMakeFiles/ so stale cached values
#                 can't linger. Deps tree is never touched.
#                 Requires CMake >= 3.24 (VS 2022 ships 3.28+).
#                 Compose with -Rebuild to reconfigure AND build:
#                 -Fresh -Rebuild => fresh configure + clean + build.
#                 The script ALSO auto-passes `cmake --fresh` whenever the
#                 cached RUBIDIUM_CONFIG in CMakeCache.txt differs from the
#                 requested -Config, to evict stale find_library entries
#                 inside Sneeze's inline configure that would otherwise pull
#                 in the previous config's Debug-suffix libs (LNK2038
#                 _ITERATOR_DEBUG_LEVEL / RuntimeLibrary mismatches).
#   -All          Build deps, then configure + build Rubidium.
#   -Only <dep>   Build a single dep (implies deps-targeting).
#   -List         Show dep stamp cache.
#   -Rebuild      Modifier: force a full rebuild of whatever target(s) are
#                 selected by the other flags, regardless of prior build state.
#                 NEVER crosses the src <-> deps wall on its own. Matrix:
#                   -Rebuild                  scrub + rebuild Rubidium only
#                   -Rebuild -Deps            scrub + rebuild all deps
#                   -Rebuild -Only <dep>      scrub + rebuild one dep
#                   -Rebuild -All             scrub + rebuild deps, then Rubidium
#                 Source clones in deps/repos/ are never scrubbed.
#
# HARD RULE: the deps folder (deps/builds/<platform>/<config>/) may only be
# modified when -Deps, -Only, or -All is present on the command line. An
# Rubidium-only invocation (anything else, including -Fresh or -Rebuild alone)
# cannot touch a single bit inside deps/. This parallels the CMakeLists-level
# invariant: deps/CMakeLists.txt and src/CMakeLists.txt never include or
# reference each other's trees. The scripts are the only glue, and they obey
# the same wall.
#
# The deps tree (deps/CMakeLists.txt) and the Rubidium tree (src/CMakeLists.txt)
# are two completely independent CMake projects. They share nothing. This
# script is the only glue: in -All mode it builds deps, then configures +
# builds Rubidium in a separate CMake invocation.
#
# Debug and Release live in fully separate DEPS trees under
# deps/builds/windows-x64/{debug,release}/ but share a single Rubidium build
# tree at builds/windows-x64/build/ and distinct install trees at
# builds/windows-x64/install/{debug,release}/.
#
# Usage:
#   .\scripts\build-windows.ps1                        # Rubidium (Release)
#   .\scripts\build-windows.ps1 -Config Debug          # Rubidium (Debug)
#   .\scripts\build-windows.ps1 -Fresh                 # Reconfigure only (no build)
#   .\scripts\build-windows.ps1 -Deps                  # Deps only (cached ones skipped)
#   .\scripts\build-windows.ps1 -All                   # Deps, then Rubidium
#   .\scripts\build-windows.ps1 -Only sdl3             # Build one dep (cached = skip)
#   .\scripts\build-windows.ps1 -Only fonts            # Download UI fonts only (no SDL3 rebuild)
#   .\scripts\build-windows.ps1 -Only sdl3 -Rebuild    # Full-scrub rebuild of one dep
#   .\scripts\build-windows.ps1 -Rebuild               # Full-scrub rebuild of Rubidium only
#   .\scripts\build-windows.ps1 -Deps -Rebuild         # Full-scrub rebuild of all deps
#   .\scripts\build-windows.ps1 -All -Rebuild          # Full-scrub rebuild of deps + Rubidium
#   .\scripts\build-windows.ps1 -List                  # Stamp cache state
#   .\scripts\build-windows.ps1 -Fresh -ManifestCdnUrl "https://cdn.dev.example/rubidium/"

[CmdletBinding()]
param (
   [ValidateSet ('Debug', 'Release')]
   [string]   $Config = 'Release',
   [string]   $Platform = 'windows-x64',
   [string]   $Only,
   [switch]   $Rebuild,
   [switch]   $List,
   [switch]   $Deps,
   [switch]   $All,
   [switch]   $Fresh,
   [string]   $ManifestCdnUrl,
   [Parameter (ValueFromRemainingArguments = $true)]
   [string[]] $CMakeExtraArgs
)

$ErrorActionPreference = 'Stop'

$modeCount = @($Deps, $All, $Fresh) | Where-Object { $_ } | Measure-Object | Select-Object -ExpandProperty Count
if ($modeCount -gt 1) {
   Write-Error '-Deps, -All, and -Fresh are mutually exclusive'
   exit 1
}

$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$RubidiumDir = Resolve-Path (Join-Path $ScriptDir '..')
$SneezeDir  = if ($env:SNEEZE_DIR) { $env:SNEEZE_DIR }
              else { Resolve-Path (Join-Path $RubidiumDir '../Sneeze') }

$ConfigLower     = $Config.ToLower()
$DepsSourceDir   = Join-Path $RubidiumDir 'deps'
$SrcSourceDir    = Join-Path $RubidiumDir 'src'
$DepRepo         = Join-Path $DepsSourceDir 'repos'
$DepRoot         = Join-Path $DepsSourceDir "builds/$Platform/$ConfigLower"
$DepsBuildDir    = Join-Path $DepRoot 'build'
$RubidiumLibsDir  = Join-Path $DepRoot 'libs'
$SneezeLibsDir   = Join-Path $SneezeDir "deps/builds/$Platform/$ConfigLower/libs"
# Single multi-config Rubidium tree. -Config only drives `cmake --build --config`.
$RubidiumOutDir   = Join-Path $RubidiumDir "builds/$Platform"
$RubidiumBuildDir = Join-Path $RubidiumOutDir 'build'
$RubidiumInstallDir = Join-Path $RubidiumOutDir "install/$ConfigLower"

$StampDir = Join-Path $DepsBuildDir '.dep-stamps'

# Only these flags => deps mode. -Rebuild is a modifier, not a mode: it
# composes with whatever target set is selected by the real mode flags.
# HARD RULE: if none of -Deps, -Only, or -All is set, the deps folder must
# never be touched -- regardless of what -Rebuild / -Fresh are doing.
# (-List is read-only and handled via its own early exit below.)
$DepsMode    = [bool]($Deps -or $All -or $Only -or $List)
$RubidiumMode = [bool]((-not $DepsMode) -or $All)
# Reconfigure the Rubidium tree before building. Implied by -All and -Fresh.
# -Rebuild does NOT force reconfigure any more: it cleans via `cmake --build
# --target clean` which preserves the configured tree (CMakeCache, CMakeFiles,
# .sln/.vcxproj), so the IDE doesn't lose state. Exception: if -Rebuild targets
# Rubidium but the tree has never been configured, fall back to configuring it
# -- otherwise the subsequent build would fail with a cryptic "CMakeCache.txt
# missing" error.
$Reconfigure = [bool]($All -or $Fresh)
if ($Rebuild -and $RubidiumMode -and -not (Test-Path (Join-Path $RubidiumBuildDir 'CMakeCache.txt'))) {
   $Reconfigure = $true
}

# ---------------------------------------------------------------------------
# Dependency graph -- order matters (deps before dependents).
# Keep in sync with scripts/build-deps.sh and deps/CMakeLists.txt.
# ---------------------------------------------------------------------------

$DepsOrdered = @(
   'sdl3'    # no deps
   'fonts'   # download-only (Inter, JetBrains Mono, Material Symbols) -> deps/fonts/
)

# ---------------------------------------------------------------------------
# Stamp helpers
# ---------------------------------------------------------------------------

function Test-Stamped ([string] $Dep) {
   Test-Path (Join-Path $StampDir "$Dep.done")
}

function Set-Stamped ([string] $Dep) {
   New-Item -ItemType Directory -Force -Path $StampDir | Out-Null
   New-Item -ItemType File      -Force -Path (Join-Path $StampDir "$Dep.done") | Out-Null
}

function Clear-Stamped ([string] $Dep) {
   Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $StampDir "$Dep.done")
}

function Test-RubidiumFontsPresent {
   Test-Path (Join-Path $RubidiumDir 'deps\fonts\Inter\Inter-Regular.ttf')
}

function Clear-StaleFontsStamp {
   if ((Test-Stamped 'fonts') -and -not (Test-RubidiumFontsPresent)) {
      Write-Host '  fonts.done stamp present but deps/fonts/ is missing; clearing stamp to re-download'
      Clear-Stamped 'fonts'
   }
}

# ExternalProject_Add keeps its own per-step stamps at
#   <DepsBuildDir>/<dep>-prefix/src/<dep>-stamp/<Config>/<dep>-configure
# and only re-runs configure if that file is missing. When a dep's configure
# succeeds but its build fails (e.g. link error), the configure stamp stays --
# so a later retry reuses cached CMAKE_ARGS even if deps/<dep>.cmake changed.
# Invalidate the configure stamp so the retry picks up our current args.
function Invalidate-DepConfigure ([string] $Dep) {
   $stamp = Join-Path $DepsBuildDir "$Dep-prefix/src/$Dep-stamp/$Config/$Dep-configure"
   Remove-Item -Force -ErrorAction SilentlyContinue $stamp
}

# -Rebuild: full scrub of a single dep's build state. Source clone in
# deps/repos/<dep>/ is preserved.
# Wipes:
#   1. Script-level .done stamp.
#   2. ExternalProject prefix dir: holds every EP stamp (download/update/
#      patch/configure/build/install), logs, tmp/. Nuking forces the full
#      EP chain to re-run top-to-bottom on next build.
#   3. Per-dep build + install trees under libs/<dep>/.
function Remove-DepState ([string] $Dep) {
   Clear-Stamped $Dep
   Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $DepsBuildDir "$Dep-prefix")
   Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $RubidiumLibsDir $Dep)
}

function Show-DepList {
   foreach ($dep in $DepsOrdered) {
      $status = if (Test-Stamped $dep) { 'cached' } else { 'pending' }
      "{0,-20} {1}" -f $dep, $status
   }
}

# ---------------------------------------------------------------------------
# -List is read-only, handle it first and exit.
# ---------------------------------------------------------------------------

if ($List) {
   Write-Host "Dependencies ($StampDir):"
   Show-DepList
   exit 0
}

# ---------------------------------------------------------------------------
# Deps mode -- configure + build deps/CMakeLists.txt
# ---------------------------------------------------------------------------

if ($DepsMode) {
   . (Join-Path $PSScriptRoot 'dep-sync.ps1')
   Clear-StaleFontsStamp

   if ($Rebuild) {
      if ($Only) {
         Remove-DepState $Only
         Write-Host "Scrubbed: $Only (stamp, EP prefix, build/, install/)"
      } else {
         # Nuke the entire per-config dep root: outer deps CMake build tree
         # + every dep's libs/<dep>/ + all stamps. Source clones untouched.
         Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $DepRoot
         Write-Host "Scrubbed: $DepRoot"
      }
   }

   $depsCMakeLists = Join-Path $DepsSourceDir 'CMakeLists.txt'
   $orderedForSync = @($DepsOrdered)
   Update-DepStampsFromCMake -DepsSourceDir $DepsSourceDir -Deps $orderedForSync `
      -StampDir $StampDir -CMakeListsPath $depsCMakeLists -ListName 'RUBIDIUM_DEPS' -ScriptLabel 'Rubidium'

   Write-Host "==> Rubidium Windows deps build"
   Write-Host "    Platform       = $Platform"
   Write-Host "    Config         = $Config"
   Write-Host "    Dep repo (src) = $DepRepo"
   Write-Host "    Dep build dir  = $DepsBuildDir"
   Write-Host "    Libs dir       = $RubidiumLibsDir"

   $depsConfigureArgs = @(
      '-S', $DepsSourceDir
      '-B', $DepsBuildDir
      "-DRUBIDIUM_CONFIG=$Config"
      "-DRUBIDIUM_PLATFORM=$Platform"
      "-DRUBIDIUM_DEP_REPO=$DepRepo"
      "-DLIBS_DIR=$RubidiumLibsDir"
   )
   if ($CMakeExtraArgs) { $depsConfigureArgs += $CMakeExtraArgs }

   & cmake @depsConfigureArgs
   if ($LASTEXITCODE -ne 0) {
      Write-Error 'Deps CMake configure failed'
      exit 1
   }

   $depsToBuild = if ($Only) { @($Only) } else { $DepsOrdered }

   $built   = @()
   $skipped = @()
   $failed  = @()

   foreach ($dep in $depsToBuild) {
      if (Test-Stamped $dep) {
         $skipped += $dep
         continue
      }

      Write-Host ''
      Write-Host "==> Building: $dep"

      # Force ExternalProject to re-run configure so arg changes take effect.
      Invalidate-DepConfigure $dep

      & cmake --build $DepsBuildDir --target $dep --config $Config
      if ($LASTEXITCODE -eq 0) {
         Set-Stamped $dep
         $built += $dep
         Write-Host "    [ok] $dep"
      } else {
         $failed += $dep
         Write-Host "    [FAIL] $dep"
         Write-Host ''
         Write-Host "Re-run with: .\scripts\build-windows.ps1 -Deps -Config $Config -Only $dep"
      }
   }

   Write-Host ''
   Write-Host '=== Summary ==='
   if ($skipped.Count) { Write-Host "Cached:  $($skipped -join ', ')" }
   if ($built.Count)   { Write-Host "Built:   $($built   -join ', ')" }
   if ($failed.Count)  { Write-Host "FAILED:  $($failed  -join ', ')" }
   Write-Host ''

   if ($failed.Count) {
      Write-Host 'Fix failures, then re-run. Only failed deps rebuild.'
      exit 1
   }
}

# ---------------------------------------------------------------------------
# Rubidium mode -- configure (if -All / -Fresh / -Rebuild) + `cmake --build`.
# ---------------------------------------------------------------------------

if ($Fresh -or $RubidiumMode) {
   # -Rebuild with Rubidium in scope: clean only the CURRENT config's compiled
   # artifacts via `cmake --build --target clean --config <cfg>`. This preserves
   # the configured CMake tree (CMakeCache.txt, CMakeFiles/, generated .sln and
   # .vcxproj) so Visual Studio doesn't lose IDE state, and it preserves the
   # OTHER config's intermediates and install tree. The selected config's
   # install/<cfg>/ is also wiped so stale binaries don't survive the rebuild.
   if ($Rebuild -and $RubidiumMode) {
      if (Test-Path (Join-Path $RubidiumBuildDir 'CMakeCache.txt')) {
         Write-Host ''
         Write-Host "==> Cleaning Rubidium $Config build artifacts"
         & cmake --build $RubidiumBuildDir --target clean --config $Config
         if ($LASTEXITCODE -ne 0) {
            Write-Error 'Rubidium clean failed'
            exit 1
         }
      }
      Write-Host "==> Scrubbing Rubidium $Config install: $RubidiumInstallDir"
      Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $RubidiumInstallDir
   }

   # -Fresh or -All or (-Rebuild with Rubidium in scope): reconfigure the
   # Rubidium tree from src/CMakeLists.txt before building. Default (no
   # -All, no -Fresh, no -Rebuild): skip configure; rely on an already-
   # configured tree. If the tree doesn't exist, `cmake --build` will fail
   # with a clear "CMakeCache.txt is missing" error -- the user should
   # re-run with -Fresh (or -All if deps are also missing).
   #
   # Configure passes -DRUBIDIUM_CONFIG = the -Config flag's value purely as
   # a hint to resolve find_package against the right deps tree; the ACTUAL
   # config emitted is chosen later via `cmake --build --config`. Once
   # configured, both Debug and Release can be built from the same tree.
   if ($Reconfigure) {
      Write-Host ''
      Write-Host "==> Configuring Rubidium tree at $RubidiumBuildDir"

      $rubidiumConfigureArgs = @(
         '-S', $SrcSourceDir
         '-B', $RubidiumBuildDir
         "-DSNEEZE_DIR=$SneezeDir"
         "-DSNEEZE_LIBS_DIR=$SneezeLibsDir"
         "-DRUBIDIUM_LIBS_DIR=$RubidiumLibsDir"
         "-DRUBIDIUM_CONFIG=$Config"
         "-DRUBIDIUM_PLATFORM=$Platform"
         "-DRUBIDIUM_BUILD_ROOT=$RubidiumOutDir"
      )
      if ($ManifestCdnUrl) { $rubidiumConfigureArgs += "-DRUBIDIUM_CDN_URL=$ManifestCdnUrl" }

      # -Fresh maps to `cmake --fresh` (CMake 3.24+): wipes CMakeCache.txt +
      # CMakeFiles/ before reconfiguring. Triggered explicitly by -Fresh, and
      # automatically when the cached RUBIDIUM_CONFIG in CMakeCache.txt differs
      # from the requested $Config -- find_library inside Sneeze's inline
      # configure caches absolute paths, so reconfiguring with a different
      # SNEEZE_LIBS_DIR does NOT update entries that were already resolved
      # under the previous config. Result: Release build pulls Debug-suffix
      # libs (openxr_loaderd.lib, spirv-cross-cored.lib, libcurl-d.lib) and
      # link fails with LNK2038 _ITERATOR_DEBUG_LEVEL / RuntimeLibrary
      # mismatches. Forcing --fresh evicts the stale cache so find_library
      # resolves cleanly against the new LIBS_DIR.
      $autoFresh = $false
      $cachePath = Join-Path $RubidiumBuildDir 'CMakeCache.txt'
      if (Test-Path $cachePath) {
         $cachedLine = Select-String -Path $cachePath -Pattern '^RUBIDIUM_CONFIG:[^=]*=(.+)$' | Select-Object -First 1
         if ($cachedLine) {
            $cachedConfig = $cachedLine.Matches[0].Groups[1].Value.Trim()
            if ($cachedConfig -and ($cachedConfig -ne $Config)) {
               Write-Host "==> Cached RUBIDIUM_CONFIG=$cachedConfig differs from requested $Config; forcing --fresh"
               $autoFresh = $true
            }
         }
      }
      if ($Fresh -or $autoFresh) { $rubidiumConfigureArgs += '--fresh' }

      & cmake @rubidiumConfigureArgs
      if ($LASTEXITCODE -ne 0) {
         Write-Error 'Rubidium CMake configure failed'
         exit 1
      }
   }

   if ($Fresh -and -not $Rebuild) {
      Write-Host ''
      Write-Host "==> Reconfigure complete (no build)"
   } else {
      Write-Host ''
      Write-Host "==> Building Rubidium ($Platform, $Config)"
      # /m:1 avoids MSBuild racing Sneeze.lib creation vs SneezeTest/SignMsf link (LNK1104 on Jenkins).
      & cmake --build $RubidiumBuildDir --config $Config -- /m:1
      if ($LASTEXITCODE -ne 0) {
         Write-Error 'Rubidium build failed'
         exit 1
      }
      Write-Host "==> Rubidium Windows build complete ($Config)"
      Write-Host "    Rubidium.exe -> $RubidiumInstallDir\bin"
   }
}
