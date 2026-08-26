# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Azure Artifact Signing diagnostic — service principal only (no az login fallback).
#
# Set env vars (same bindings as Prod-Rubidium Jenkins job), then run:
#
#   $env:AZURE_TENANT_ID     = '<tenant-guid>'
#   $env:AZURE_CLIENT_ID     = '<app-client-id>'
#   $env:AZURE_CLIENT_SECRET = '<secret Value from Entra>'
#   pwsh -ExecutionPolicy Bypass -File scripts\test-azure-signing.ps1
#
# Optional:
#   -TestBinary 'C:\path\Rubidium.exe'
#   -MetadataPath 'C:\path\signing-metadata.json'
#   -SignTimeoutSec 300

[CmdletBinding()]
param (
   [string] $MetadataPath,
   [string] $TestBinary,
   [int]    $SignTimeoutSec = 300
)

$ErrorActionPreference = 'Stop'

$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$RubidiumDir = Resolve-Path (Join-Path $ScriptDir '..')

. (Join-Path $ScriptDir 'azure-signing-auth.ps1')

function Write-Step {
   param ([string] $Label)

   Write-Host ''
   Write-Host '============================================================'
   Write-Host "  $Label"
   Write-Host '============================================================'
}

if (-not $MetadataPath) {
   $MetadataPath = Join-Path $RubidiumDir 'pkg\signing-metadata.json'
}

Require-AzureSigningServicePrincipal

Write-Step 'Azure Artifact Signing diagnostic (service principal only)'
Write-Host "  Windows user   = $([Environment]::UserName)"
Write-Host "  Auth mode      = service-principal"
Write-Host "  Metadata       = $MetadataPath"
Write-Host '  az on PATH:'
where.exe az 2>$null | ForEach-Object { Write-Host "    $_" }
Write-Host '  az version:'
az version 2>&1 | ForEach-Object { Write-Host "    $_" }

if (-not (Test-Path $MetadataPath)) {
   Write-Error "Metadata not found: $MetadataPath"
   exit 1
}

$sEffectiveMetadata = Test-AzureSigningPreflightCore -BaseMetadataPath $MetadataPath -TimeoutSec 120

$dlib = $null
foreach ($searchDir in @("${env:ProgramFiles}\Azure Code Signing", 'C:\Tools\TrustedSigning')) {
   if (Test-Path $searchDir) {
      $dlib = Get-ChildItem -Path $searchDir -Filter 'Azure.CodeSigning.Dlib.dll' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
      if ($dlib) { break }
   }
}
if (-not $dlib) {
   Write-Error 'Azure.CodeSigning.Dlib.dll not found. Install Azure Artifact Signing Client Tools.'
   exit 1
}

Write-Step 'signtool test sign'
if (-not $TestBinary) {
   $TestBinary = Join-Path $env:TEMP "rubidium-sign-test-$PID.exe"
   Copy-Item "$env:SystemRoot\System32\notepad.exe" $TestBinary -Force
}

Write-Host "  dlib     = $($dlib.FullName)"
Write-Host "  metadata = $sEffectiveMetadata"
Write-Host "  file     = $TestBinary"
Write-Host "  timeout  = ${SignTimeoutSec}s"

$pProcess = Start-Process -FilePath 'signtool.exe' -ArgumentList @(
   'sign', '/v',
   '/fd', 'SHA256',
   '/tr', 'http://timestamp.acs.microsoft.com',
   '/td', 'SHA256',
   '/dlib', $dlib.FullName,
   '/dmdf', $sEffectiveMetadata,
   $TestBinary
) -PassThru -NoNewWindow -Wait:$false

if (-not $pProcess.WaitForExit(($SignTimeoutSec * 1000))) {
   try { $pProcess.Kill($true) } catch {}
   Write-Error "signtool hung > ${SignTimeoutSec}s."
   exit 1
}

if ($pProcess.ExitCode -ne 0) {
   Write-Error "signtool failed with exit $($pProcess.ExitCode)."
   exit 1
}

Write-Host ''
Write-Host '============================================================'
Write-Host '  ALL CHECKS PASSED — service principal signing OK'
Write-Host '============================================================'
