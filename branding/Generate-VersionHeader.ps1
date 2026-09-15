# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Substitutes src/version.h.in from the VERSION file into -OutFile.
# Used by the hand-maintained MSVC PreBuildEvent (CMake uses configure_file).
# Writes the output only when the generated text differs, so a Sneeze rebuild
# (which still runs this PreBuildEvent) does not bump the header timestamp
# and force version.h consumers to recompile.

param (
   [Parameter (Mandatory = $true)]
   [string] $RepoRoot,
   [Parameter (Mandatory = $true)]
   [string] $OutFile
)

$ErrorActionPreference = 'Stop'

$aVersion = (Get-Content (Join-Path $RepoRoot 'VERSION') -Raw).Trim().Split('.')
$template = Get-Content (Join-Path $RepoRoot 'src\version.h.in') -Raw
$header = $template `
   -replace '@RUBIDIUM_VER_MAJOR@', $aVersion[0] `
   -replace '@RUBIDIUM_VER_MINOR@', $aVersion[1] `
   -replace '@RUBIDIUM_VER_PATCH@', $aVersion[2] `
   -replace '@PROJECT_VERSION@', ($aVersion -join '.')

$outDir = Split-Path -Parent $OutFile
if (-not (Test-Path $outDir))
{
   New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

$bWrite = $true
if (Test-Path $OutFile)
{
   if ([IO.File]::ReadAllText($OutFile) -eq $header)
   {
      $bWrite = $false
   }
}
if ($bWrite)
{
   [IO.File]::WriteAllText($OutFile, $header)
}
