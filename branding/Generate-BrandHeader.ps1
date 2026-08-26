# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Substitutes branding/Brand.h.in from branding/Product.cmake into -OutFile.
# Used by the hand-maintained MSVC PreBuildEvent (CMake uses configure_file).

param (
   [Parameter (Mandatory = $true)]
   [string] $RepoRoot,
   [Parameter (Mandatory = $true)]
   [string] $OutFile
)

$ErrorActionPreference = 'Stop'

$cmake = Get-Content (Join-Path $RepoRoot 'branding\Product.cmake') -Raw
$template = Get-Content (Join-Path $RepoRoot 'branding\Brand.h.in') -Raw

function Get-ProductValue ([string] $Name)
{
   $pattern = 'set \(' + [regex]::Escape($Name) + '\s+"([^"]*)"\)'
   $match = [regex]::Match($cmake, $pattern)
   if (-not $match.Success)
   {
      throw "branding/Product.cmake is missing $Name"
   }
   $match.Groups[1].Value
}

$header = [regex]::Replace($template, '@(\w+)@', {
   param ($Match)
   Get-ProductValue $Match.Groups[1].Value
})

$outDir = Split-Path -Parent $OutFile
if (-not (Test-Path $outDir))
{
   New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

[IO.File]::WriteAllText($OutFile, $header)
