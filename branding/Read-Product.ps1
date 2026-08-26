# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Dot-source from build scripts. Get-ProductIdentity returns Name / NameSetup
# from branding/Product.cmake.

function Get-ProductIdentity ([string] $RepoRoot)
{
   $cmake = Get-Content (Join-Path $RepoRoot 'branding\Product.cmake') -Raw
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

   [pscustomobject] @{
      Name      = Get-ProductValue 'PRODUCT_NAME'
      NameSetup = Get-ProductValue 'PRODUCT_NAME_SETUP'
      CdnUrl    = Get-ProductValue 'PRODUCT_CDN_URL'
   }
}
