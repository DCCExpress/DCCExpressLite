param(
  [Parameter(Mandatory = $true)][string]$Version,
  [string]$ProjectRoot = (Resolve-Path "$PSScriptRoot\.."),
  [string]$FirmwareOutputDirectory = "$PSScriptRoot\..\artifacts\firmware",
  [string]$WebInstallerOutputDirectory = "$PSScriptRoot\..\artifacts\webinstaller"
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $ProjectRoot '.pio/build/ESP32'
$flashSize = 4MB

$bootApp = Get-ChildItem "$env:USERPROFILE/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $bootApp -and $env:HOME) {
  $bootApp = Get-ChildItem "$env:HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin" -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $bootApp) { throw 'boot_app0.bin was not found in the PlatformIO framework package.' }

$parts = @(
  @{ Source=(Join-Path $build 'bootloader.bin'); Offset=0x1000 },
  @{ Source=(Join-Path $build 'partitions.bin'); Offset=0x8000 },
  @{ Source=$bootApp.FullName; Offset=0xE000 },
  @{ Source=(Join-Path $build 'firmware.bin'); Offset=0x10000 },
  @{ Source=(Join-Path $build 'littlefs.bin'); Offset=0x290000 }
)

$image = [byte[]]::new($flashSize)
[Array]::Fill($image, [byte]0xFF)

foreach ($part in $parts) {
  if (-not (Test-Path -LiteralPath $part.Source)) { throw "Missing build output: $($part.Source)" }
  $bytes = [IO.File]::ReadAllBytes($part.Source)
  if ($part.Offset + $bytes.Length -gt $image.Length) { throw "Image does not fit in 4 MB flash: $($part.Source)" }
  [Array]::Copy($bytes, 0, $image, $part.Offset, $bytes.Length)
}

New-Item -ItemType Directory -Path $FirmwareOutputDirectory,$WebInstallerOutputDirectory -Force | Out-Null
$releaseName = "DCCExpressLite-$Version-merged.bin"
$releasePath = Join-Path $FirmwareOutputDirectory $releaseName
[IO.File]::WriteAllBytes($releasePath, $image)
Copy-Item -LiteralPath $releasePath -Destination (Join-Path $WebInstallerOutputDirectory 'merged-firmware.bin') -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'webinstaller/index.html') -Destination (Join-Path $WebInstallerOutputDirectory 'index.html') -Force

$webManifest = [ordered]@{
  name = 'DCCExpressLite'
  version = $Version
  new_install_prompt_erase = $true
  builds = @(@{
    chipFamily = 'ESP32'
    improv = $false
    parts = @(@{ path = 'merged-firmware.bin'; offset = 0 })
  })
}
$webManifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $WebInstallerOutputDirectory 'manifest.json') -Encoding utf8NoBOM

$releaseIndex = @([ordered]@{
  tagName = "v$Version"
  prerelease = $Version.Contains('-')
  manifestUrl = 'manifest.json'
})
ConvertTo-Json -InputObject $releaseIndex -Depth 4 | Set-Content -LiteralPath (Join-Path $WebInstallerOutputDirectory 'releases.json') -Encoding utf8NoBOM

Write-Output $releasePath
