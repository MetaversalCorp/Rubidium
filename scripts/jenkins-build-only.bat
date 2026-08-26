@echo off
rem Jenkins / local: build Rubidium without CDN upload (script default).
rem Syncs/builds Sneeze sibling, builds Rubidium, signs, packages installer.
rem Do NOT run commit-manifest.cmd after this. For CDN publish use -Deploy
rem (see jenkins-server3.bat / Prod job).
pwsh -ExecutionPolicy Bypass -File "%~dp0ci-windows.ps1" -Config Release
exit /b %ERRORLEVEL%
