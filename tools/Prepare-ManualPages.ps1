param(
  [string]$ProjectRoot = (Resolve-Path "$PSScriptRoot\.."),
  [switch]$SkipDemoBuild
)

$ErrorActionPreference = 'Stop'

$siteRoot = Join-Path $ProjectRoot 'site'
$installerSource = Join-Path $ProjectRoot 'webinstaller'
$installerTarget = Join-Path $siteRoot 'installer'
$releaseArtifact = Join-Path $ProjectRoot 'artifacts\webinstaller'

$webUiRoot = Join-Path $ProjectRoot 'web-ui'
$demoDist = Join-Path $webUiRoot 'dist'
$demoTarget = Join-Path $siteRoot 'demo'

if (-not (Test-Path -LiteralPath $siteRoot)) {
  throw "Missing site directory: $siteRoot"
}

if (-not (Test-Path -LiteralPath $installerSource)) {
  throw "Missing webinstaller directory: $installerSource"
}

Write-Host ''
Write-Host 'DCCExpressLite manual Pages preparation' -ForegroundColor Cyan
Write-Host '---------------------------------------'

# ---------------------------------------------------------------------------
# 1. INSTALLER
# ---------------------------------------------------------------------------

if (Test-Path -LiteralPath $installerTarget) {
  Get-ChildItem -LiteralPath $installerTarget -Force |
    Remove-Item -Recurse -Force
} else {
  New-Item -ItemType Directory -Path $installerTarget -Force | Out-Null
}

Copy-Item `
  -Path (Join-Path $installerSource '*') `
  -Destination $installerTarget `
  -Recurse `
  -Force

Write-Host '[OK] Copied current webinstaller -> site/installer' -ForegroundColor Green

# If a release artifact already exists locally, its generated installer files
# take precedence over the static development copies.
if (Test-Path -LiteralPath $releaseArtifact) {
  Copy-Item `
    -Path (Join-Path $releaseArtifact '*') `
    -Destination $installerTarget `
    -Recurse `
    -Force

  Write-Host '[OK] Added local release artifact files to site/installer' -ForegroundColor Green

  $firmwareSource = Join-Path $releaseArtifact 'firmware'

  if (Test-Path -LiteralPath $firmwareSource) {
    $firmwareTarget = Join-Path $siteRoot 'firmware'

    if (Test-Path -LiteralPath $firmwareTarget) {
      Remove-Item -LiteralPath $firmwareTarget -Recurse -Force
    }

    Copy-Item `
      -LiteralPath $firmwareSource `
      -Destination $firmwareTarget `
      -Recurse `
      -Force

    Write-Host '[OK] Copied firmware release tree -> site/firmware' -ForegroundColor Green
  }
} else {
  Write-Host '[INFO] No artifacts/webinstaller directory found.' -ForegroundColor Yellow
  Write-Host '       Using the current webinstaller development files.' -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# 2. REACT DEMO
# ---------------------------------------------------------------------------

if (-not $SkipDemoBuild) {
  if (-not (Test-Path -LiteralPath $webUiRoot)) {
    throw "Missing web-ui directory: $webUiRoot"
  }

  Write-Host ''
  Write-Host '[DEMO] Building browser demo...' -ForegroundColor Cyan

  Push-Location $webUiRoot

  try {
    npm run build:demo

    if ($LASTEXITCODE -ne 0) {
      throw "npm run build:demo failed with exit code $LASTEXITCODE"
    }
  }
  finally {
    Pop-Location
  }
} else {
  Write-Host '[DEMO] Build skipped; using existing web-ui/dist.' -ForegroundColor Yellow
}

if (-not (Test-Path -LiteralPath (Join-Path $demoDist 'index.html'))) {
  throw "Demo build output is missing: $demoDist"
}

if (Test-Path -LiteralPath $demoTarget) {
  Get-ChildItem -LiteralPath $demoTarget -Force |
    Remove-Item -Recurse -Force
} else {
  New-Item -ItemType Directory -Path $demoTarget -Force | Out-Null
}

Copy-Item `
  -Path (Join-Path $demoDist '*') `
  -Destination $demoTarget `
  -Recurse `
  -Force

Write-Host '[OK] Copied React demo -> site/demo' -ForegroundColor Green

# GitHub Pages / static servers should not run Jekyll processing.
Set-Content `
  -LiteralPath (Join-Path $siteRoot '.nojekyll') `
  -Value '' `
  -Encoding ascii

Write-Host ''
Write-Host 'Prepared complete static site:' -ForegroundColor Cyan
Write-Host "  $siteRoot"
Write-Host ''
Write-Host 'VS Code Live Server:' -ForegroundColor Cyan
Write-Host '  Right-click site/index.html -> Open with Live Server'
Write-Host ''
Write-Host 'Expected paths:' -ForegroundColor Cyan
Write-Host '  /site/'
Write-Host '  /site/installer/'
Write-Host '  /site/demo/'
Write-Host '  /site/docs/'
Write-Host ''
