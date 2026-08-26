@echo off
rem Server3-Rubidium Jenkins build step (Execute Windows batch command).
rem Replaces hand-rolled ci-windows.ps1 args; drop -DefaultHome from the job.
rem -Deploy is required to publish to the CDN (upload is off by default).
pwsh -ExecutionPolicy Bypass -File "%~dp0ci-windows.ps1" ^
  -Deploy ^
  -CdnRoot \\la2-rdweb0\cdn-server3.rp1.dev\rubidium\ ^
  -Config Release ^
  -ManifestCdnUrl "https://cdn-server3.rp1.dev/rubidium/"
