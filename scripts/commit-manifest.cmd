@echo off
setlocal EnableDelayedExpansion

rem ============================================================================
rem commit-manifest.cmd
rem
rem Commits and pushes pkg\manifest.json to origin/main. Intended to run as a
rem dedicated Jenkins build step AFTER ci-windows.ps1 has finished registering
rem and promoting the new release entry.
rem
rem Why a separate step:
rem   - Jenkins SCM checkout runs in detached HEAD mode, so `git push origin
rem     HEAD` does NOT update main -- it pushes to a remote ref called HEAD.
rem     This script uses an explicit `HEAD:refs/heads/<branch>` refspec.
rem   - The push needs write credentials. We bind a GitHub PAT to the
rem     GITHUB_TOKEN env var via Jenkins Credentials Binding plugin and use
rem     it to synthesize a one-shot HTTPS URL for fetch and push. Jenkins SCM
rem     checkout often uses git@github.com (SSH), which this step does not have
rem     keys for. The token is never written to disk (no `git remote set-url`,
rem     no `git config`).
rem
rem Jenkins setup (Freestyle job):
rem   1. Build environment -> "Use secret text(s) or file(s)"
rem        Bindings -> Add -> Secret text
rem          Variable: GITHUB_TOKEN
rem          Credentials: <your stored github-pat secret-text credential>
rem   2. Add a Windows batch build step AFTER the PowerShell ci-windows.ps1 step:
rem        call scripts\commit-manifest.cmd
rem      Do NOT pass a hardcoded version (e.g. 0.0.1) — the commit message would
rem      disagree with pkg/manifest.json after every VERSION bump. With no args,
rem      version is read from the repo root VERSION file; platform defaults to
rem      windows-x64; branch defaults to main. Override only for unusual cases:
rem        call scripts\commit-manifest.cmd 0.0.99 windows-x64 main
rem   3. Do NOT pass -CommitManifest to ci-windows.ps1 -- let this script do it.
rem
rem Args (all optional):
rem   %1  Version  (default: read from VERSION file)
rem   %2  Platform (default: windows-x64)
rem   %3  Branch   (default: main)
rem
rem Required env vars:
rem   GITHUB_TOKEN  -- GitHub PAT with Contents:write on the Rubidium repo.
rem ============================================================================

set "RUBIDIUM_DIR=%~dp0.."
pushd "%RUBIDIUM_DIR%" || (
   echo ERROR: cannot cd to %RUBIDIUM_DIR%
   exit /b 1
)

if "%GITHUB_TOKEN%"=="" (
   echo ERROR: GITHUB_TOKEN env var is not set.
   echo        Configure Jenkins Credentials Binding to expose your GitHub PAT
   echo        as the GITHUB_TOKEN env var, then re-run.
   popd
   exit /b 1
)

set "VERSION=%~1"
set "PLATFORM=%~2"
set "BRANCH=%~3"

if "%VERSION%"=="" (
   if exist "VERSION" (
      set /p VERSION=<VERSION
   )
)
if "%VERSION%"=="" set "VERSION=unknown"
if "%PLATFORM%"=="" set "PLATFORM=windows-x64"
if "%BRANCH%"==""   set "BRANCH=main"

echo ============================================================
echo   Manifest: commit + push
echo ============================================================
echo   Repo     : %RUBIDIUM_DIR%
echo   Version  : %VERSION%
echo   Platform : %PLATFORM%
echo   Branch   : %BRANCH%
echo.

rem Derive the repo slug from origin URL so fetch/push work whether origin is
rem SSH (git@github.com:owner/repo.git) or HTTPS. Jenkins masks GITHUB_TOKEN in
rem console output; synthesizing the URL is safe to log without the secret.
for /f "delims=" %%i in ('git config --get remote.origin.url') do set "ORIGIN_URL=%%i"

set "SLUG=%ORIGIN_URL%"
set "SLUG=!SLUG:https://github.com/=!"
set "SLUG=!SLUG:git@github.com:=!"
set "SLUG=!SLUG:.git=!"

set "AUTH_URL=https://x-access-token:%GITHUB_TOKEN%@github.com/!SLUG!.git"

rem Fetch first so we can detect unpushed commits from a prior failed push and
rem rebase onto the latest main before pushing.
echo Fetching origin/%BRANCH% from https://github.com/!SLUG!.git ...
git fetch "%AUTH_URL%" +refs/heads/%BRANCH%:refs/remotes/origin/%BRANCH%
if errorlevel 1 (
   echo ERROR: git fetch failed.
   popd
   exit /b 1
)

git add pkg\manifest.json
if errorlevel 1 (
   echo ERROR: git add failed.
   popd
   exit /b 1
)

set "WILL_PUSH=0"

git diff --cached --quiet -- pkg/manifest.json
if errorlevel 1 (
   rem Identity is set inline via -c so we never mutate global git config on the
   rem agent. Without this, git falls back to user@hostname and emits a noisy
   rem "configured automatically" warning every CI run.
   git -c user.name="Rubidium CI" -c user.email="ci@metaversal.local" commit -m "release(manifest): v%VERSION% %PLATFORM%"
   if errorlevel 1 (
      echo ERROR: git commit failed.
      popd
      exit /b 1
   )
   set "WILL_PUSH=1"
)

if "!WILL_PUSH!"=="0" (
   set "AHEAD=0"
   for /f %%c in ('git rev-list origin/%BRANCH%..HEAD --count 2^>nul') do set "AHEAD=%%c"
   if "!AHEAD!"=="0" (
      echo manifest.json unchanged -- nothing to commit or push.
      popd
      exit /b 0
   )
   echo Retrying push for !AHEAD! unpushed commit^(s^) from a prior failed push.
   set "WILL_PUSH=1"
)

rem The build step checks out a fixed commit (detached HEAD). While the job runs,
rem other pushes can land on main (e.g. GitHub Actions, a parallel job). Rebase
rem the manifest commit onto the latest origin/%BRANCH% before pushing.
echo Rebasing manifest commit onto origin/%BRANCH%...
git rebase origin/%BRANCH%
if errorlevel 1 (
   echo ERROR: git rebase failed -- pkg/manifest.json may conflict with newer main.
   echo        Resolve on the agent or re-run the full build from a fresh checkout.
   popd
   exit /b 1
)

echo Pushing HEAD:refs/heads/%BRANCH% -^> https://github.com/!SLUG!.git
git push "%AUTH_URL%" HEAD:refs/heads/%BRANCH%
if errorlevel 1 (
   echo ERROR: git push failed.
   popd
   exit /b 1
)

echo Manifest committed and pushed to %BRANCH%.
popd
endlocal
exit /b 0