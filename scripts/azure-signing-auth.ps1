# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Shared Azure Artifact Signing auth for ci-windows.ps1 and test-azure-signing.ps1.
#
# Service principal only — bind secret-text credentials to:
#   AZURE_TENANT_ID, AZURE_CLIENT_ID, AZURE_CLIENT_SECRET
# Optional: AZURE_SUBSCRIPTION_ID
#
# No personal `az login` fallback. Use SKIP_SIGN=1 to skip signing locally.

function Test-AzureSigningServicePrincipalConfigured {
   $bResult = (-not [string]::IsNullOrWhiteSpace($env:AZURE_TENANT_ID)) `
      -and (-not [string]::IsNullOrWhiteSpace($env:AZURE_CLIENT_ID)) `
      -and (-not [string]::IsNullOrWhiteSpace($env:AZURE_CLIENT_SECRET))
   return $bResult
}

function Require-AzureSigningServicePrincipal {
   if (Test-AzureSigningServicePrincipalConfigured) {
      return
   }

   Write-Error @"
Service principal required. Set all three env vars:
  AZURE_TENANT_ID
  AZURE_CLIENT_ID
  AZURE_CLIENT_SECRET
Jenkins: Prod-Rubidium → Build Environment → secret text bindings.
Local: export the same vars, or set SKIP_SIGN=1 to skip signing.
"@
   exit 1
}

function Get-AzureSigningExcludeCredentials {
   return @(
      'ManagedIdentityCredential',
      'WorkloadIdentityCredential',
      'SharedTokenCacheCredential',
      'VisualStudioCredential',
      'VisualStudioCodeCredential',
      'AzurePowerShellCredential',
      'AzureDeveloperCliCredential',
      'InteractiveBrowserCredential',
      'AzureCliCredential'
   )
}

function Get-AzureSigningMetadataPath {
   param ([string] $BaseMetadataPath)

   $jBase = Get-Content $BaseMetadataPath -Raw | ConvertFrom-Json
   $jMeta = [ordered]@{
      Endpoint               = $jBase.Endpoint
      CodeSigningAccountName = $jBase.CodeSigningAccountName
      CertificateProfileName = $jBase.CertificateProfileName
      ExcludeCredentials     = @(Get-AzureSigningExcludeCredentials)
   }
   $sTempPath = Join-Path $env:TEMP "rubidium-signing-metadata-$PID.json"
   $jMeta | ConvertTo-Json -Depth 4 | Set-Content -Path $sTempPath -Encoding utf8
   return $sTempPath
}

function Invoke-AzureServicePrincipalLogin {
   param (
      [string] $ClientId,
      [string] $TenantId,
      [string] $Secret
   )

   $sHelp = & az login -h 2>&1 | Out-String
   if ($sHelp -match 'password-stdin') {
      return @($Secret | & az login --service-principal `
         -u $ClientId `
         --tenant $TenantId `
         --only-show-errors `
         --password-stdin 2>&1)
   }

   Write-Host '  az login              = using -p (this az CLI build has no --password-stdin)'
   return @(& az login --service-principal `
      -u $ClientId `
      -p $Secret `
      --tenant $TenantId `
      --only-show-errors 2>&1)
}

function Initialize-AzureSigningAuth {
   if (-not (Get-Command az -ErrorAction SilentlyContinue)) {
      Write-Error 'Azure CLI (az) not found on PATH. Install Azure CLI or set SKIP_SIGN=1.'
      exit 1
   }

   Require-AzureSigningServicePrincipal

   $sTenantId = $env:AZURE_TENANT_ID.Trim()
   $sClientId = $env:AZURE_CLIENT_ID.Trim()
   $sSecret   = $env:AZURE_CLIENT_SECRET.Trim()

   Write-Host '  Auth mode             = service-principal'
   Write-Host "  AZURE_TENANT_ID       = $sTenantId"
   Write-Host "  AZURE_CLIENT_ID       = $sClientId"
   Write-Host '  AZURE_CLIENT_SECRET   = (set)'

   $aLoginOutput = @(Invoke-AzureServicePrincipalLogin -ClientId $sClientId -TenantId $sTenantId -Secret $sSecret)
   $aLoginOutput | ForEach-Object { Write-Host $_ }
   if ($LASTEXITCODE -ne 0) {
      Write-Error @"
az login --service-principal failed (HTTP/AAD error above — not an Artifact Signing RBAC issue yet).
Checklist:
  - AZURE_CLIENT_SECRET must be the secret *Value* from Entra, not the Secret ID.
  - Secret not expired (Entra → app → Certificates & secrets).
  - AZURE_CLIENT_ID / AZURE_TENANT_ID match the app registration exactly.
  - Jenkins credential has no extra spaces or line breaks (re-paste the Value).
After login succeeds, RBAC needs 'Artifact Signing Certificate Profile Signer' on profile Rubidium.
"@
      exit 1
   }

   if (-not [string]::IsNullOrWhiteSpace($env:AZURE_SUBSCRIPTION_ID)) {
      $sSubscriptionId = $env:AZURE_SUBSCRIPTION_ID.Trim()
      & az account set --subscription $sSubscriptionId --only-show-errors 2>&1 | ForEach-Object { Write-Host $_ }
      if ($LASTEXITCODE -ne 0) {
         Write-Error "az account set failed for subscription $sSubscriptionId."
         exit 1
      }
      Write-Host "  AZURE_SUBSCRIPTION_ID = $sSubscriptionId"
   }
}

function Test-AzureSigningPreflightCore {
   param (
      [string] $BaseMetadataPath,
      [int]    $TimeoutSec
   )

   Write-Host ''
   Write-Host '============================================================'
   Write-Host '  Sign: Azure preflight'
   Write-Host '============================================================'
   Write-Host "  Windows user          = $([Environment]::UserName)"
   Write-Host '  Auth mode             = service-principal'
   Write-Host "  signing metadata      = $BaseMetadataPath"

   Initialize-AzureSigningAuth

   & az account show --output json 2>&1 | ForEach-Object { Write-Host $_ }
   if ($LASTEXITCODE -ne 0) {
      Write-Error 'az account show failed after service-principal login.'
      exit 1
   }

   $aTokenOutput = @(& az account get-access-token --resource https://codesigning.azure.net --output json 2>&1)
   if ($LASTEXITCODE -ne 0) {
      Write-Host ''
      Write-Host '--- az account get-access-token (codesigning.azure.net) ---'
      $aTokenOutput | ForEach-Object { Write-Host $_ }
      Write-Error @"
Azure Artifact Signing token request failed.
Grant the app 'Artifact Signing Certificate Profile Signer' on profile Rubidium (account Metaversal, West US 2).
"@
      exit 1
   }

   Write-Host '  Azure codesigning token OK'

   $sEffectiveMetadata = Get-AzureSigningMetadataPath -BaseMetadataPath $BaseMetadataPath
   Write-Host "  signing metadata (CI) = $sEffectiveMetadata (EnvironmentCredential only)"

   return $sEffectiveMetadata
}
