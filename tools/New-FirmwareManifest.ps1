param(
  [Parameter(Mandatory = $true)][string]$Version,
  [Parameter(Mandatory = $true)][string]$BaseUrl,
  [string]$ProjectRoot = (Resolve-Path "$PSScriptRoot\.."),
  [string]$OutputDirectory = "$PSScriptRoot\..\artifacts\firmware"
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $ProjectRoot 'CommandStation-EX/.pio/build/ESP32'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$bootApp = Get-ChildItem "$env:USERPROFILE/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $bootApp) {
  $bootApp = Get-ChildItem "$env:HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin" -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $bootApp) { throw 'boot_app0.bin was not found in the PlatformIO framework package.' }

$definitions = @(
  @{ Name='bootloader.bin'; Source=(Join-Path $build 'bootloader.bin'); Offset='0x1000'; Kind='system' },
  @{ Name='partitions.bin'; Source=(Join-Path $build 'partitions.bin'); Offset='0x8000'; Kind='system' },
  @{ Name='boot_app0.bin'; Source=$bootApp.FullName; Offset='0xE000'; Kind='system' },
  @{ Name='firmware.bin'; Source=(Join-Path $build 'firmware.bin'); Offset='0x10000'; Kind='firmware' },
  @{ Name='littlefs.bin'; Source=(Join-Path $build 'littlefs.bin'); Offset='0x290000'; Kind='filesystem' }
)

$images = foreach ($item in $definitions) {
  if (-not (Test-Path -LiteralPath $item.Source)) { throw "Missing build output: $($item.Source)" }
  $destination = Join-Path $OutputDirectory $item.Name
  Copy-Item -LiteralPath $item.Source -Destination $destination -Force
  [ordered]@{
    name = $item.Name
    url = "$($BaseUrl.TrimEnd('/'))/$($item.Name)"
    sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
    offset = $item.Offset
    kind = $item.Kind
  }
}

$defaultDataSource = Join-Path $ProjectRoot 'default-data/*'
$defaultDataArchive = Join-Path $OutputDirectory 'default-data.zip'
Compress-Archive -Path $defaultDataSource -DestinationPath $defaultDataArchive -Force
$defaultData = [ordered]@{
  url = "$($BaseUrl.TrimEnd('/'))/default-data.zip"
  sha256 = (Get-FileHash -LiteralPath $defaultDataArchive -Algorithm SHA256).Hash.ToLowerInvariant()
  description = 'Optional starter layout, locomotives and locomotive images.'
}

$toolBase = 'https://github.com/espressif/esptool/releases/download/v5.3.1'
$manifest = [ordered]@{
  schemaVersion = 1
  product = 'DCCExpressLite for EX-CSB1'
  version = $Version
  channel = if ($Version.Contains('-')) { 'prerelease' } else { 'stable' }
  publishedAt = [DateTimeOffset]::UtcNow.ToString('o')
  releaseNotes = 'Firmware, integrated web server and DCCExpressLite UI.'
  images = $images
  defaultData = $defaultData
  tools = [ordered]@{
    'win-x64' = @{ url="$toolBase/esptool-v5.3.1-windows-amd64.zip"; sha256=''; archiveEntry='esptool-windows-amd64/esptool.exe' }
    'osx-x64' = @{ url="$toolBase/esptool-v5.3.1-macos-amd64.tar.gz"; sha256=''; archiveEntry='esptool-macos-amd64/esptool' }
    'osx-arm64' = @{ url="$toolBase/esptool-v5.3.1-macos-arm64.tar.gz"; sha256=''; archiveEntry='esptool-macos-arm64/esptool' }
    'linux-x64' = @{ url="$toolBase/esptool-v5.3.1-linux-amd64.tar.gz"; sha256=''; archiveEntry='esptool-linux-amd64/esptool' }
    'linux-arm64' = @{ url="$toolBase/esptool-v5.3.1-linux-aarch64.tar.gz"; sha256=''; archiveEntry='esptool-linux-aarch64/esptool' }
  }
}

$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'firmware-manifest.json') -Encoding utf8NoBOM
Write-Host "Firmware release created in $OutputDirectory"
