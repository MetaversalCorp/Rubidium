# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Windows CI entry point.
#
# Designed for a Jenkins freestyle job with the GitHub plugin.
# Poll SCM + Git Additional Behaviours -> "Polling ignores commits in certain
# paths" -> Included Regions: VERSION (manifest-only pushes do not rebuild).
# Or disable Poll SCM / GitHub hook and use Build now only.
#
#   Build step: "Execute Windows PowerShell Command"
#   Command:    powershell -ExecutionPolicy Bypass -File scripts\ci-windows.ps1
#
# What it does:
#   1. Fast-forwards ../Sneeze to origin/main (unless SKIP_SNEEZE_SYNC=1).
#   2. Rebuilds Sneeze from that checkout (-All, stamp-cached deps) so headers,
#      Sneeze.lib, and third-party deps match the latest main output. Dep stamps
#      are cleared when deps/*.cmake or deps/CMakeLists.txt change; CI fails if
#      scripts/build-windows.ps1 $DepsOrdered drifts from deps/CMakeLists.txt.
#   3. Builds Rubidium deps + Rubidium.exe + RubidiumSetup.exe (stamp-cached).
#   4. Signs binaries with Azure Trusted Signing (Authenticode).
#   4. Generates the NSIS installer (CPack) - contains signed binaries.
#      Before CPack, removes any stale Rubidium-*.exe left in pkg/ from a prior
#      run so a VERSION bump cannot leave an old installer next to the new one
#      (every job gets a clean NSIS output slot; no reliance on Jenkins wiping
#      the whole workspace for that).
#   5. Signs the installer itself.
#   6. Registers the installer in pkg/manifest.json (SHA-256 + CDN URL).
#   7. Promotes the version to the stable channel.
#   8. Commits updated manifest.json back to the repo (requires -CommitManifest).
#      On Jenkins, do NOT pass -CommitManifest. Use scripts\commit-manifest.cmd
#      as a separate build step instead -- it handles detached-HEAD push and
#      reads a PAT bound to GITHUB_TOKEN via the Credentials Binding plugin.
#      -CommitManifest is intended for local-developer manual workflows where
#      the user already has push credentials configured.
#   9. Uploads installer + manifest to CDN only when -Deploy is passed
#      (and SKIP_UPLOAD is not '1'). Default is build-only, like GitHub Build
#      with Deploy unchecked.
#
# Default (build only - no CDN upload):
#   pwsh -ExecutionPolicy Bypass -File .\scripts\ci-windows.ps1 -Config Release
#   Do NOT run scripts\commit-manifest.cmd after a non-deploy build.
#
# Production / Server3 deploy:
#   pwsh ... -Config Release -Deploy -CdnRoot <unc> [-ManifestCdnUrl <url>]
#   then call scripts\commit-manifest.cmd on Jenkins.
#
# Prerequisites on the Jenkins agent:
#   Visual Studio 2022 (Desktop C++), CMake 3.24+, Git, NSIS.
#   Must run from a Developer PowerShell for VS 2022 (or equivalent env vars).
#   Sneeze is a sibling directory (../Sneeze). Each run syncs origin/main and
#   rebuilds Sneeze before Rubidium (set SKIP_SNEEZE_SYNC=1 to pin a local checkout).
#
#   For signing:
#     .NET 8.0 Runtime, Azure CLI (az), Azure Artifact Signing Client Tools.
#     Jenkins: bind AZURE_TENANT_ID, AZURE_CLIENT_ID, AZURE_CLIENT_SECRET via
#     Credentials Binding. Optional: AZURE_SUBSCRIPTION_ID. Service principal only
#     (no personal az login). Run scripts/test-azure-signing.ps1 to verify on the agent.
#
# Environment variables (set in Jenkins job config or globally):
#   SKIP_SIGN        - Set to '1' to skip code signing
#   SIGN_TIMEOUT_SEC - Per-file signtool timeout (default: 600). Prevents 16h hangs.
#   AZURE_SIGN_PREFLIGHT_TIMEOUT_SEC - az token probe timeout (default: 120)
#   AZURE_TENANT_ID / AZURE_CLIENT_ID / AZURE_CLIENT_SECRET - service principal (Jenkins secrets)
#   AZURE_SUBSCRIPTION_ID - optional; passed to `az account set` after SP login
#   SKIP_PACKAGE     - Set to '1' to skip installer + manifest steps
#   SKIP_PROMOTE     - Set to '1' to register but not promote to stable
#   SKIP_UPLOAD      - Set to '1' to force-skip CDN upload even with -Deploy
#   SKIP_SNEEZE_SYNC - Set to '1' to skip git pull on ../Sneeze (local / already synced)
#
# Jenkins SSH (sibling ../Sneeze fetch):
#   SCM Credentials do not apply to git fetch in this script. Bind the same
#   SSH User Private Key used for SCM (e.g. LA2-JENKINSOS):
#     Key File Variable:     SNEEZE_CI_SSH_KEY
#     Passphrase Variable:   SNEEZE_CI_SSH_PASSPHRASE
#   Same binding as the Sneeze Jenkins job (ci-sneeze-windows.ps1).
#
# Flags:
#   -Config Debug|Release   Build configuration (default: Release)
#   -CdnRoot <path>         UNC (or local) folder where installers/manifest are copied
#                           (only used when -Deploy)
#   -ManifestCdnUrl <url>   Public HTTPS base URL for manifest.json + RUBIDIUM_CDN_URL
#                           (e.g. https://cdn_server3.rp1.dev/rubidium/). Omit to use
#                           CMake default (production CDN). Not the same as -CdnRoot.
#   -CommitManifest         Commit + push updated manifest.json back to the repo
#   -Deploy                 Upload installer + manifest to -CdnRoot. Required for
#                           production/Server3 CDN publish. Default is no upload.
#
# Deprecated (accepted for older Jenkins jobs, ignored):
#   -DefaultHome <url>       Was never wired; remove from the job when convenient.
#                           See scripts/jenkins-server3.bat for a current Server3 entry.
#   -NoDeploy                No-op; build-without-upload is now the default.

[CmdletBinding()]
param (
   [ValidateSet ('Debug', 'Release')]
   [string] $Config = 'Release',

   [string] $CdnRoot = '\\prod-web0.mv.local\inetpub\cdn.rp1.com\rubidium',
   [string] $ManifestCdnUrl,

   [switch] $CommitManifest,
   [switch] $Deploy,

   # Legacy: upload is off by default; kept so old jobs that pass -NoDeploy still parse.
   [switch] $NoDeploy,

   # Legacy Jenkins Server3-Rubidium jobs still pass this; ignore so the job does not fail.
   [string] $DefaultHome
)

$ErrorActionPreference = 'Stop'

# Early diagnostic block for Jenkins/cmd quoting issues.
Write-Host '============================================================'
Write-Host '  CI invocation diagnostics'
Write-Host '============================================================'
Write-Host "  Raw args count        = $($args.Count)"
Write-Host "  Raw args              = $($args -join ' | ')"
Write-Host "  Parsed Config         = $Config"
Write-Host "  Parsed CdnRoot        = $CdnRoot"
Write-Host "  Parsed ManifestCdnUrl = $([string]::IsNullOrWhiteSpace($ManifestCdnUrl) ? '(CMake default)' : $ManifestCdnUrl)"
Write-Host "  Parsed CommitManifest = $CommitManifest"
Write-Host "  Parsed Deploy         = $Deploy"
if ($PSBoundParameters.ContainsKey('DefaultHome') -and -not [string]::IsNullOrWhiteSpace($DefaultHome)) {
   Write-Warning "-DefaultHome is deprecated and ignored (value was: $DefaultHome). Remove it from the Jenkins job; use scripts\jenkins-server3.bat or see .github/CI.md."
}
if ($NoDeploy) {
   Write-Warning '-NoDeploy is deprecated and ignored (build-without-CDN is already the default). Remove it; use -Deploy only when publishing.'
}
Write-Host ''

if (-not $Deploy) {
   Write-Host '  Deploy: off (default) - CDN upload skipped. Pass -Deploy to publish.'
   Write-Host ''
}

# ---------------------------------------------------------------------------
# Git SSH for sibling ../Sneeze fetch (and any later git@github.com calls).
# Jenkins SCM credentials never reach this process - bind SNEEZE_CI_SSH_KEY.
# ---------------------------------------------------------------------------
function Enable-CiSshAuth {
   if ($env:SNEEZE_CI_SSH_KEY) {
      $keySrc = $env:SNEEZE_CI_SSH_KEY.Trim()
      if (-not (Test-Path -LiteralPath $keySrc)) {
         Write-Error "SNEEZE_CI_SSH_KEY is set but file not found: $keySrc"
         exit 1
      }

      Write-Host '============================================================'
      Write-Host '  Git auth: SNEEZE_CI_SSH_KEY'
      Write-Host '============================================================'

      # Windows OpenSSH refuses Jenkins-bound keys (secretFiles ACLs too open).
      $keyDir = Join-Path $env:TEMP ("rubidium-ci-ssh-" + $PID)
      New-Item -ItemType Directory -Force -Path $keyDir | Out-Null
      $keyPath = Join-Path $keyDir 'id_ci'
      $raw = [IO.File]::ReadAllText($keySrc)
      $norm = ($raw -replace "`r`n", "`n" -replace "`r", "`n").TrimEnd() + "`n"
      [IO.File]::WriteAllText($keyPath, $norm, (New-Object System.Text.UTF8Encoding $false))

      $user = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
      $acl = Get-Acl -LiteralPath $keyPath
      $acl.SetAccessRuleProtection($true, $false)
      foreach ($rule in @($acl.Access)) {
         [void]$acl.RemoveAccessRule($rule)
      }
      $acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($user, 'Read', 'Allow')))
      Set-Acl -LiteralPath $keyPath -AclObject $acl
      Write-Host "  Prepared SSH key for user $user (ACL locked, LF normalized)"

      $keyFwd = ($keyPath -replace '\\', '/')
      $pass = $env:SNEEZE_CI_SSH_PASSPHRASE
      $looksEncrypted = ($norm -match '(?m)^Proc-Type:\s*4,ENCRYPTED') -or
         ($norm -match 'openssh-key-v1' -and $norm -match 'bcrypt')
      if ($looksEncrypted -and [string]::IsNullOrEmpty($pass)) {
         Write-Error @"
SNEEZE_CI_SSH_KEY is passphrase-protected but SNEEZE_CI_SSH_PASSPHRASE is empty.
In the Jenkins binding, set Passphrase Variable to SNEEZE_CI_SSH_PASSPHRASE.
"@
         exit 1
      }

      if (-not [string]::IsNullOrEmpty($pass)) {
         $passFile = Join-Path $keyDir 'passphrase'
         [IO.File]::WriteAllText($passFile, $pass, (New-Object System.Text.UTF8Encoding $false))
         $passAcl = Get-Acl -LiteralPath $passFile
         $passAcl.SetAccessRuleProtection($true, $false)
         foreach ($rule in @($passAcl.Access)) { [void]$passAcl.RemoveAccessRule($rule) }
         $passAcl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($user, 'Read', 'Allow')))
         Set-Acl -LiteralPath $passFile -AclObject $passAcl
         $askpass = Join-Path $keyDir 'askpass.cmd'
         "@echo off`r`ntype `"$passFile`"`r`n" | Set-Content -LiteralPath $askpass -Encoding Ascii
         $env:SSH_ASKPASS = $askpass
         $env:SSH_ASKPASS_REQUIRE = 'force'
         $env:GIT_TERMINAL_PROMPT = '0'
         $env:GIT_SSH_COMMAND = "ssh -i `"$keyFwd`" -o IdentitiesOnly=yes -o StrictHostKeyChecking=accept-new -o NumberOfPasswordPrompts=1"
         Write-Host '  Passphrase: SNEEZE_CI_SSH_PASSPHRASE via SSH_ASKPASS'
      }
      else {
         $env:GIT_SSH_COMMAND = "ssh -i `"$keyFwd`" -o IdentitiesOnly=yes -o BatchMode=yes -o StrictHostKeyChecking=accept-new"
      }
      Write-Host '  GIT_SSH_COMMAND: ssh -i <prepared key> -o IdentitiesOnly=yes'
      Write-Host ''
   }
   elseif ($env:SSH_AUTH_SOCK -or $env:GIT_SSH_COMMAND) {
      Write-Host '  Using existing SSH agent / GIT_SSH_COMMAND from the job environment'
      Write-Host ''
   }
   elseif ($env:SKIP_SNEEZE_SYNC -ne '1') {
      Write-Host '  WARNING: no SNEEZE_CI_SSH_KEY - sibling Sneeze git fetch may fail under Jenkins.'
      Write-Host '  Bind the SCM SSH credential as SNEEZE_CI_SSH_KEY (+ PASSPHRASE) - see script header.'
      Write-Host ''
   }
}

Enable-CiSshAuth

$ScriptDir    = Split-Path -Parent $MyInvocation.MyCommand.Path
$RubidiumDir   = Resolve-Path (Join-Path $ScriptDir '..')
. (Join-Path $RubidiumDir 'branding\Read-Product.ps1')
$Product = Get-ProductIdentity $RubidiumDir
$SneezeDir    = Join-Path (Split-Path -Parent $RubidiumDir) 'Sneeze'
$Platform     = 'windows-x64'
$ConfigLower  = $Config.ToLower()

$BuildDir     = Join-Path $RubidiumDir "builds\$Platform\build"
$BinDir       = Join-Path $RubidiumDir "builds\$Platform\install\$ConfigLower\bin"
$PkgDir       = Join-Path $RubidiumDir "builds\$Platform\install\$ConfigLower\pkg"
$Version      = (Get-Content (Join-Path $RubidiumDir 'VERSION') -Raw).Trim()
$SignMetadata = Join-Path $RubidiumDir 'pkg\signing-metadata.json'
$script:SignMetadataEffective = $SignMetadata
$SneezeLibs   = Join-Path $SneezeDir "deps\builds\$Platform\$ConfigLower\libs"

if (-not (Test-Path (Join-Path $SneezeDir '.git'))) {
   Write-Error "Sneeze not found at $SneezeDir - clone and build it as a sibling directory first."
   exit 1
}

function Test-SneezeNetworkApi {
   param ([string] $SneezeRoot)

   $sNetworkH      = Join-Path $SneezeRoot 'include\Network.h'
   $sNetworkImplH  = Join-Path $SneezeRoot 'src\sneeze\network\Network.h'
   $sSneezeH       = Join-Path $SneezeRoot 'include\Sneeze.h'
   $bOk            = $true

   if (-not (Select-String -Path $sNetworkH -Pattern 'void\s+File_Enum\s*\(' -Quiet)) {
      Write-Host "  MISSING: NETWORK::File_Enum in include/Network.h"
      $bOk = $false
   }
   if (-not (Select-String -Path $sSneezeH -Pattern 'virtual\s+bool\s+OnNetworkFileCreated' -Quiet)) {
      Write-Host '  MISSING: bool ICONTEXT::OnNetworkFileCreated in include/Sneeze.h'
      $bOk = $false
   }
   if (-not (Select-String -Path $sNetworkH -Pattern 'bool\s+Attach\s*\(\s*\)' -Quiet)) {
      Write-Host "  MISSING: NETWORK::FILE::Attach() in include/Network.h"
      $bOk = $false
   }
   if (-not (Select-String -Path $sNetworkH -Pattern 'void\s+Reset\s*\(\s*\)' -Quiet)) {
      Write-Host "  MISSING: NETWORK::FILE::Reset() in include/Network.h"
      $bOk = $false
   }
   if (-not (Select-String -Path $sNetworkH -Pattern 'ReadData\s*\(\s*std::vector<uint8_t>&' -Quiet)) {
      Write-Host "  MISSING: FILE::ReadData(std::vector<uint8_t>&) in include/Network.h"
      $bOk = $false
   }
   if (-not (Select-String -Path $sNetworkH -Pattern 'class\s+INETWORK_IMPL' -Quiet)) {
      Write-Host '  MISSING: INETWORK_IMPL in include/Network.h (Storage Cleanup pimpl split?)'
      $bOk = $false
   }
   if (-not (Test-Path $sNetworkImplH)) {
      Write-Host "  MISSING: src/context/network/Network.h (internal network impl header)"
      $bOk = $false
   }
   elseif (-not (Select-String -Path $sNetworkImplH -Pattern 'ASSET\s*\*\s*Asset_Open' -Quiet)) {
      Write-Host '  MISSING: INETWORK_IMPL::Asset_Open in src/context/network/Network.h'
      $bOk = $false
   }
   if (Select-String -Path $sNetworkH -Pattern '\bASSET\s*\*\s*Asset_Open' -Quiet) {
      Write-Host '  STALE: ASSET* Asset_Open in include/Network.h (belongs on INETWORK_IMPL in src/context/network/Network.h only)'
      $bOk = $false
   }
   $sConsoleH = Join-Path $SneezeRoot 'include\Console.h'
   if (-not (Select-String -Path $sConsoleH -Pattern 'enum\s+eENTRY_LEVEL' -Quiet)) {
      Write-Host '  MISSING: eENTRY_LEVEL in include/Console.h (need Sneeze console refactor on origin/main)'
      $bOk = $false
   }
   if (-not (Select-String -Path $sSneezeH -Pattern 'OnConsoleEntryCreated' -Quiet)) {
      Write-Host '  MISSING: ICONTEXT::OnConsoleEntryCreated in include/Sneeze.h'
      $bOk = $false
   }

   $sViewportH = Join-Path $SneezeRoot 'include\Viewport.h'
   if (-not (Select-String -Path $sViewportH -Pattern 'void\s+Input_Mouse\s*\(' -Quiet)) {
      Write-Host '  MISSING: VIEWPORT::Input_Mouse in include/Viewport.h (need Sneeze 4241df8 or newer)'
      $bOk = $false
   }
   if (-not (Select-String -Path $sViewportH -Pattern 'void\s+Input_Key\s*\(' -Quiet)) {
      Write-Host '  MISSING: VIEWPORT::Input_Key in include/Viewport.h'
      $bOk = $false
   }
   if (Select-String -Path $sViewportH -Pattern 'void\s+SetMouseInput\s*\(' -Quiet) {
      Write-Host '  STALE: SetMouseInput still in include/Viewport.h (rebuild Sneeze from current origin/main)'
      $bOk = $false
   }
   if (-not (Select-String -Path $sSneezeH -Pattern 'virtual\s+bool\s+FrameSize\s*\(' -Quiet)) {
      Write-Host '  MISSING: bool IVIEWPORT::FrameSize in include/Sneeze.h (need Sneeze 11eba47 or newer)'
      $bOk = $false
   }
   if (Select-String -Path $sSneezeH -Pattern 'virtual\s+void\s+FrameSize\s*\(' -Quiet) {
      Write-Host '  STALE: void IVIEWPORT::FrameSize in include/Sneeze.h (Rubidium c223fab+ needs bool return)'
      $bOk = $false
   }

   $sContextH = Join-Path $SneezeRoot 'include\Context.h'
   if (-not (Test-Path $sContextH)) {
      Write-Host '  MISSING: include/Context.h (need Sneeze ec32352 or newer on origin/main)'
      $bOk = $false
   }
   if (-not (Select-String -Path $sSneezeH -Pattern 'Context_Open\s*\(' -Quiet)) {
      Write-Host '  MISSING: ENGINE::Context_Open in include/Sneeze.h'
      $bOk = $false
   }

   return $bOk
}

function Clear-RubidiumPrecompiledHeaders {
   param ([string] $BuildRoot)

   if (-not (Test-Path $BuildRoot)) {
      return
   }

   # Only compiled PCH outputs - never cmake_pch.cxx / cmake_pch.hxx (CMake generates
   # those at configure; deleting them causes C1083 on the next build).
   $nRemoved = 0
   foreach ($name in @('cmake_pch.cxx.obj', 'cmake_pch.hxx.pch', 'cmake_pch.pch')) {
      Get-ChildItem -Path $BuildRoot -Recurse -Filter $name -ErrorAction SilentlyContinue |
         ForEach-Object {
            Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue
            $nRemoved++
         }
   }
   if ($nRemoved -gt 0) {
      Write-Host "  Cleared $nRemoved stale CMake PCH output(s) under $BuildRoot (will recompile cmake_pch.cxx)"
   }
}

function Test-RubidiumCanvasApi {
   param ([string] $RubidiumRoot)

   $sCanvasH = Join-Path $RubidiumRoot 'src\canvas\Canvas.h'
   $bOk      = $true

   if (-not (Select-String -Path $sCanvasH -Pattern 'void\s+SetVisible\s*\(' -Quiet)) {
      Write-Host '  MISSING: CANVAS::SetVisible in src/canvas/Canvas.h (stale Rubidium checkout?)'
      $bOk = $false
   }
   if (-not (Select-String -Path $sCanvasH -Pattern 'SDL_Window\s*\*\s*m_pWindow' -Quiet)) {
      Write-Host '  MISSING: CANVAS::m_pWindow in src/canvas/Canvas.h'
      $bOk = $false
   }

   return $bOk
}

if ($env:SKIP_SNEEZE_SYNC -ne '1') {
   Write-Host ''
   Write-Host '============================================================'
   Write-Host '  Sync Sneeze (origin/main)'
   Write-Host '============================================================'
   Push-Location $SneezeDir
   try {
      git fetch origin main
      if ($LASTEXITCODE -ne 0) {
         Write-Error "git fetch failed in $SneezeDir"
         exit 1
      }
      git merge --ff-only origin/main
      if ($LASTEXITCODE -ne 0) {
         Write-Error "Sneeze at $SneezeDir is not a fast-forward from origin/main - resolve manually, then re-run."
         exit 1
      }
      # Jenkins agents must never compile with a mixed src/include tree left by
      # a partial checkout or local edits on tracked headers.
      git reset --hard origin/main
      if ($LASTEXITCODE -ne 0) {
         Write-Error "git reset --hard origin/main failed in $SneezeDir"
         exit 1
      }
      $script:SneezeHead = git rev-parse HEAD
      $sHeadShort = git rev-parse --short HEAD
      Write-Host "  Sneeze HEAD = $sHeadShort ($(git log -1 --format='%s'))"
   }
   finally {
      Pop-Location
   }
}

if (-not (Test-SneezeNetworkApi $SneezeDir)) {
   Write-Error @"
Sneeze headers at $SneezeDir do not match this Rubidium revision (API surface check failed).
Ensure Sneeze origin/main has finished building, then re-run. If Rubidium merged first,
wait for the Sneeze main build to complete and try again.
"@
   exit 1
}

# ---- Build Sneeze (latest origin/main output) ------------------------------

function Clear-SneezeStaleRmlUiStamp {
   param (
      [string] $SneezeRoot,
      [string] $BuildConfig
   )
   $cfgSlug = $BuildConfig.ToLower()
   $cache   = Join-Path $SneezeRoot "deps\builds\windows-x64\$cfgSlug\libs\RmlUi\build\CMakeCache.txt"
   $stamp   = Join-Path $SneezeRoot "deps\builds\windows-x64\$cfgSlug\build\.dep-stamps\rmlui.done"
   if (-not (Test-Path $cache)) {
      return
   }
   $match = Select-String -Path $cache -Pattern '^RMLUI_FONT_ENGINE:STRING=(.+)$' -ErrorAction SilentlyContinue | Select-Object -First 1
   if ($match -and $match.Matches[0].Groups[1].Value.Trim() -eq 'none') {
      Write-Host '  Stale RmlUi (RMLUI_FONT_ENGINE=none); clearing rmlui stamp so -All rebuilds it with FreeType'
      Remove-Item -Force -ErrorAction SilentlyContinue $stamp
   }
}

Write-Host ''
Write-Host '============================================================'
Write-Host '  Build Sneeze (origin/main, stamp-cached)'
Write-Host '============================================================'
if ($script:SneezeHead) {
   Write-Host "  Sneeze commit = $(git -C $SneezeDir rev-parse --short $script:SneezeHead)"
}
Clear-SneezeStaleRmlUiStamp -SneezeRoot $SneezeDir -BuildConfig $Config
& "$SneezeDir\scripts\build-windows.ps1" -All -Config $Config -Sync
if ($LASTEXITCODE -ne 0) {
   Write-Error "Sneeze build failed at $SneezeDir - fix Sneeze main before building Rubidium."
   exit 1
}

Push-Location $RubidiumDir
try {
   git checkout -f HEAD
   if ($LASTEXITCODE -ne 0) {
      Write-Error "git checkout -f HEAD failed in $RubidiumDir"
      exit 1
   }
   git clean -fd -- src/
   if (-not (Test-RubidiumCanvasApi $RubidiumDir)) {
      Write-Error @"
Rubidium sources at $RubidiumDir do not match the expected canvas API (SetVisible, m_pWindow).
Ensure Jenkins checked out the latest main commit and re-run.
"@
      exit 1
   }
}
finally {
   Pop-Location
}

# CPack drops Rubidium-<ver>-<platform>.exe into pkg/. Older installers from
# prior runs can remain in the same folder. Get-ChildItem | Select-Object -First 1
# is not stable (lexicographic order can prefer 0.0.1 over 0.0.2), which then
# signs/uploads the wrong file into releases/<VERSION>/ while VERSION already
# bumped - producing paths like releases/0.0.2/Rubidium-0.0.1-windows-x64.exe.
function Get-RubidiumInstaller {
   param (
      [string] $PkgDirPath,
      [string] $Ver,
      [string] $Plat
   )
   $expectedName = "Rubidium-$Ver-$Plat.exe"
   $expectedPath = Join-Path $PkgDirPath $expectedName
   if (Test-Path $expectedPath) {
      return Get-Item $expectedPath
   }
   $candidates = Get-ChildItem -Path $PkgDirPath -Filter 'Rubidium-*.exe' -ErrorAction SilentlyContinue
   if (-not $candidates) {
      return $null
   }
   return $candidates | Sort-Object LastWriteTime -Descending | Select-Object -First 1
}

# ---- Signing helper -------------------------------------------------------

. (Join-Path $ScriptDir 'azure-signing-auth.ps1')

function Get-SignTimeoutSeconds {
   if (-not [string]::IsNullOrWhiteSpace($env:SIGN_TIMEOUT_SEC)) {
      return [int]$env:SIGN_TIMEOUT_SEC
   }
   return 600
}

function Get-AzureSignPreflightTimeoutSeconds {
   if (-not [string]::IsNullOrWhiteSpace($env:AZURE_SIGN_PREFLIGHT_TIMEOUT_SEC)) {
      return [int]$env:AZURE_SIGN_PREFLIGHT_TIMEOUT_SEC
   }
   return 120
}

function Wait-ProcessExitOrKill {
   param (
      [System.Diagnostics.Process] $Process,
      [int]                        $TimeoutSec,
      [string]                     $Label
   )

   $bTimedOut = -not $Process.WaitForExit($TimeoutSec * 1000)
   if ($bTimedOut) {
      try {
         $Process.Kill($true)
      }
      catch {
      }
      Write-Error "${Label} timed out after ${TimeoutSec}s. Kill hung signtool/az on the agent and fix Azure Artifact Signing auth (run scripts/test-azure-signing.ps1). Set SKIP_SIGN=1 to ship unsigned."
      exit 1
   }

   return $Process.ExitCode
}

function Test-AzureSigningPreflight {
   if ($env:SKIP_SIGN -eq '1') {
      return
   }

   $nTimeoutSec = Get-AzureSignPreflightTimeoutSeconds
   Write-Host "  SIGN_TIMEOUT_SEC      = $(Get-SignTimeoutSeconds)"
   $script:SignMetadataEffective = Test-AzureSigningPreflightCore -BaseMetadataPath $SignMetadata -TimeoutSec $nTimeoutSec
}

function Invoke-SignToolWithTimeout {
   param (
      [string] $DlibPath,
      [string] $MetadataPath,
      [string] $FilePath,
      [int]    $TimeoutSec
   )

   $pProcess = Start-Process -FilePath 'signtool.exe' -ArgumentList @(
      'sign', '/v',
      '/fd', 'SHA256',
      '/tr', 'http://timestamp.acs.microsoft.com',
      '/td', 'SHA256',
      '/dlib', $DlibPath,
      '/dmdf', $MetadataPath,
      $FilePath
   ) -PassThru -NoNewWindow -Wait:$false

   $nExitCode = Wait-ProcessExitOrKill -Process $pProcess -TimeoutSec $TimeoutSec -Label "signtool sign ($FilePath)"
   return $nExitCode
}

function Invoke-Sign ([string[]] $Files) {
   if ($env:SKIP_SIGN -eq '1') { return }

   $dlib = $null
   foreach ($searchDir in @("${env:ProgramFiles}\Azure Code Signing", 'C:\Tools\TrustedSigning')) {
      if (Test-Path $searchDir) {
         $dlib = Get-ChildItem -Path $searchDir -Filter 'Azure.CodeSigning.Dlib.dll' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
         if ($dlib) { break }
      }
   }
   if (-not $dlib) {
      Write-Warning 'Azure.CodeSigning.Dlib.dll not found - skipping signing. Install Azure Artifact Signing Client Tools.'
      return
   }

   $nTimeoutSec = Get-SignTimeoutSeconds

   foreach ($f in $Files) {
      if (-not (Test-Path $f)) {
         Write-Warning "File not found, skipping sign: $f"
         continue
      }
      Write-Host "  Signing $f (timeout ${nTimeoutSec}s)"
      Write-Host "    dlib = $($dlib.FullName)"
      Write-Host "    meta = $script:SignMetadataEffective"
      $nExitCode = Invoke-SignToolWithTimeout -DlibPath $dlib.FullName -MetadataPath $script:SignMetadataEffective -FilePath $f -TimeoutSec $nTimeoutSec
      Write-Host "    signtool exit code: $nExitCode"
      if ($nExitCode -ne 0) {
         Write-Error "Signing failed for $f (exit $nExitCode). See verbose signtool output above."
         exit 1
      }
   }
}

# ---- Build Rubidium (deps + exe) ------------------------------------------

Write-Host ''
Write-Host '============================================================'
Write-Host '  Rubidium: deps + exe'
Write-Host '============================================================'

$env:SNEEZE_DIR = $SneezeDir
Write-Host "  Effective build config = $Config"
Write-Host "  Effective manifest CDN = $([string]::IsNullOrWhiteSpace($ManifestCdnUrl) ? '(CMake default)' : $ManifestCdnUrl)"
# Drop stale CMake cache so SNEEZE_DIR and inline Sneeze paths re-resolve (-All alone
# does not pass cmake --fresh; -Fresh and -All are mutually exclusive in build-windows.ps1).
$cmakeCache = Join-Path $BuildDir 'CMakeCache.txt'
if (Test-Path $cmakeCache) {
   Remove-Item -Force $cmakeCache
}
# Deleting CMakeCache alone leaves cmake_pch.* from an older Sneeze/Canvas tree; MSVC
# then reuses a PCH where SNEEZE::CONTEXT and CANVAS members never existed.
Clear-RubidiumPrecompiledHeaders $BuildDir

$buildWindowsArgs = @{
   All              = $true
   Config           = $Config
}
if ($ManifestCdnUrl) { $buildWindowsArgs['ManifestCdnUrl'] = $ManifestCdnUrl }
& "$RubidiumDir\scripts\build-windows.ps1" @buildWindowsArgs
if ($LASTEXITCODE -ne 0) {
   Write-Error 'Rubidium build failed'
   exit 1
}

$fontProbe = Join-Path $BinDir 'fonts\Inter\Inter-Regular.ttf'
if (-not (Test-Path $fontProbe)) {
   Write-Error @"
Bundled UI fonts are missing from $BinDir (expected fonts\Inter\Inter-Regular.ttf).
Jenkins workspace may have a stale fonts.done stamp with an empty deps/fonts/ tree.
Re-run with: Remove-Item '$RubidiumDir\deps\builds\windows-x64\$ConfigLower\build\.dep-stamps\fonts.done' -Force; then rebuild.
Or from Rubidium repo root: .\scripts\build-windows.ps1 -Only fonts -Config $Config
"@
   exit 1
}

# ---- Sign binaries -------------------------------------------------------

if ($env:SKIP_SIGN -ne '1') {
   Test-AzureSigningPreflight
   Write-Host ''
   Write-Host '============================================================'
   Write-Host '  Sign: binaries'
   Write-Host '============================================================'

   Invoke-Sign @(
      "$BinDir\$($Product.Name).exe",
      "$BinDir\$($Product.NameSetup).exe",
      "$SneezeLibs\Wasmtime\install\lib\wasmtime.dll",
      "$SneezeLibs\Halogen\install\bin\anari_library_halogen.dll"
   )
}

# ---- Package (NSIS installer) --------------------------------------------

if ($env:SKIP_PACKAGE -ne '1') {
   Write-Host ''
   Write-Host '============================================================'
   Write-Host '  Package: NSIS installer'
   Write-Host '============================================================'

   $cpackConfig = Join-Path $BuildDir 'CPackConfig.cmake'

   if (-not (Test-Path $cpackConfig)) {
      Write-Error "CPackConfig.cmake not found at $cpackConfig - did the build configure correctly?"
      exit 1
   }

   # Every job: evict prior NSIS artifacts from this config's pkg/ folder. CPack
   # does not always remove older Rubidium-<ver>-<platform>.exe files; leaving
   # them breaks signing/upload/manifest heuristics after a VERSION bump.
   if (Test-Path $PkgDir) {
      $stale = Get-ChildItem -Path $PkgDir -Filter 'Rubidium-*.exe' -ErrorAction SilentlyContinue
      if ($stale) {
         $stale | Remove-Item -Force
         Write-Host "  Removed $($stale.Count) stale Rubidium-*.exe from pkg/ before CPack."
      }
   }

   # CPack runs the same install() rules as cmake --install. Probe them here;
   # do not scrape _CPack_Packages after cpack - NSIS often removes that tree
   # once Rubidium-*.exe is generated, which produced false failures.
   Write-Host '  Verifying install() rules (cmake --install probe)...'
   $installProbe = Join-Path $env:TEMP "rubidium-install-probe-$PID"
   if (Test-Path $installProbe) {
      Remove-Item -Recurse -Force $installProbe
   }
   & cmake --install $BuildDir --config $Config --prefix $installProbe
   if ($LASTEXITCODE -ne 0) {
      Write-Error 'cmake --install probe failed (install rules for fonts/DLLs)'
      exit 1
   }
   $installFontProbe = Join-Path $installProbe 'bin\fonts\Inter\Inter-Regular.ttf'
   if (-not (Test-Path $installFontProbe)) {
      Write-Error @"
Install probe missing bundled fonts at $installFontProbe.
CPack/NSIS would ship without UI fonts. Check src/CMakeLists.txt install(CODE) and deps/fonts/.
"@
      exit 1
   }
   Write-Host "  install() probe OK ($installFontProbe)"
   Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $installProbe

   & cpack --config $cpackConfig -C $Config
   if ($LASTEXITCODE -ne 0) {
      Write-Error 'CPack failed'
      exit 1
   }

   # ---- Sign the installer ------------------------------------------------

   if ($env:SKIP_SIGN -ne '1') {
      Write-Host ''
      Write-Host '============================================================'
      Write-Host '  Sign: installer'
      Write-Host '============================================================'

      $installer = Get-RubidiumInstaller $PkgDir $Version $Platform
      if ($installer -and ($installer.Name -ne "Rubidium-$Version-$Platform.exe")) {
         Write-Warning "Installer is $($installer.Name) but VERSION is $Version - expected Rubidium-$Version-$Platform.exe. Re-run with build-windows.ps1 -Fresh after bumping VERSION so CMake picks up the new VERSION."
      }
      if ($installer) {
         Invoke-Sign @($installer.FullName)
      }
   }

   # ---- Register in manifest ---------------------------------------------

   Write-Host ''
   Write-Host '============================================================'
   Write-Host '  Manifest: register'
   Write-Host '============================================================'

   & cmake --build $BuildDir --target package_register --config $Config
   if ($LASTEXITCODE -ne 0) {
      Write-Error 'Manifest register failed'
      exit 1
   }

   # ---- Promote to stable ------------------------------------------------

   if ($env:SKIP_PROMOTE -ne '1') {
      Write-Host ''
      Write-Host '============================================================'
      Write-Host '  Manifest: promote to stable'
      Write-Host '============================================================'

      & cmake --build $BuildDir --target package_promote --config $Config
      if ($LASTEXITCODE -ne 0) {
         Write-Error 'Manifest promote failed'
         exit 1
      }
   }

   # ---- Commit manifest back to repo ---------------------------------------

   if ($CommitManifest) {
      Write-Host ''
      Write-Host '============================================================'
      Write-Host '  Manifest: commit + push'
      Write-Host '============================================================'

      $manifestFile = Join-Path $RubidiumDir 'pkg\manifest.json'
      Push-Location $RubidiumDir
      & git add $manifestFile
      $status = & git status --porcelain -- pkg/manifest.json
      if ($status) {
         & git commit -m "release(manifest): v$Version $Platform"
         if ($LASTEXITCODE -ne 0) {
            Write-Warning 'git commit failed - manifest not pushed'
         } else {
            & git fetch origin main
            if ($LASTEXITCODE -ne 0) {
               Write-Warning 'git fetch failed - manifest not pushed'
            } else {
               & git rebase origin/main
               if ($LASTEXITCODE -ne 0) {
                  Write-Warning 'git rebase failed - manifest not pushed'
               } else {
                  & git push origin HEAD:refs/heads/main
                  if ($LASTEXITCODE -ne 0) {
                     Write-Warning 'git push failed - manifest committed locally but not pushed'
                  }
               }
            }
         }
      } else {
         Write-Host '  manifest.json unchanged - nothing to commit.'
      }
      Pop-Location
   }

   # ---- Upload to CDN -----------------------------------------------------
   #
# CDN folder structure (UNC path to web server). Matches pkg/manifest.json URLs
# (releases/<version>/...) and scripts/layout-cdn-artifacts.sh (GitHub deploy).
   #   rubidium/
   #      manifest.json                              channel index
   #      download/
   #         RubidiumSetup.exe                        stable URL for website
   #         Rubidium.dmg                             macOS (latest)
   #         Rubidium.tar.gz                          Linux (latest)
   #         Rubidium.apk                             Android sideload (latest)
   #      releases/<version>/
   #         Rubidium-<ver>-windows-x64.exe           full NSIS installer
   #         Rubidium-<ver>-macos-arm64.dmg
   #         Rubidium-<ver>-linux-x64.tar.gz
   #         Rubidium-<ver>-android-arm64.apk
   #         Rubidium-<ver>-quest-arm64.apk
   #
   #   Symbols (PDBs) are NOT uploaded to the public CDN - they stay on the
   #   Jenkins build server at builds/<platform>/install/<config>/bin/.

   if ($Deploy  -and  $env:SKIP_UPLOAD -ne '1') {
      Write-Host ''
      Write-Host '============================================================'
      Write-Host '  Upload: CDN'
      Write-Host '============================================================'

      if (-not (Test-Path $CdnRoot)) {
         Write-Error "CDN path not accessible: $CdnRoot"
         exit 1
      }

      $CdnReleases = Join-Path $CdnRoot "releases\$Version"
      $CdnDownload = Join-Path $CdnRoot 'download'
      New-Item -ItemType Directory -Force -Path $CdnReleases | Out-Null
      New-Item -ItemType Directory -Force -Path $CdnDownload | Out-Null

      $installer = Get-RubidiumInstaller $PkgDir $Version $Platform
      $manifest  = Join-Path $RubidiumDir 'pkg\manifest.json'
      $setup     = Join-Path $BinDir "$($Product.NameSetup).exe"

      if ($installer) {
         Write-Host "  Installer -> releases/$Version/$($installer.Name)"
         Copy-Item $installer.FullName $CdnReleases -Force
      }

      Write-Host "  Manifest  -> manifest.json"
      Copy-Item $manifest $CdnRoot -Force

      if (Test-Path $setup) {
         Write-Host "  Stub      -> download/$($Product.NameSetup).exe"
         Copy-Item $setup $CdnDownload -Force
      }

      Write-Host '  CDN upload complete.'
   }
}

# ---- Summary --------------------------------------------------------------

Write-Host ''
Write-Host '============================================================'
Write-Host "  Done  (v$Version, $Platform, $Config)"
Write-Host '============================================================'

if (Test-Path "$BinDir\$($Product.Name).exe") {
   Write-Host "  $($Product.Name).exe      -> $BinDir\$($Product.Name).exe"
}
if (Test-Path "$BinDir\$($Product.NameSetup).exe") {
   Write-Host "  $($Product.NameSetup).exe -> $BinDir\$($Product.NameSetup).exe"
}
if (Test-Path $PkgDir) {
   $installer = Get-RubidiumInstaller $PkgDir $Version $Platform
   if ($installer) {
      Write-Host "  Installer        -> $($installer.FullName)"
   }
}
