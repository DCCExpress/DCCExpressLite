param(
  [string]$Configuration = 'Release',
  [string]$OutputDirectory = "$PSScriptRoot\..\artifacts\installer",
  [string[]]$Runtimes = @('win-x64', 'osx-x64', 'osx-arm64', 'linux-x64', 'linux-arm64')
)

$ErrorActionPreference = 'Stop'
$project = Resolve-Path "$PSScriptRoot\..\DCCExpressLite.Installer\DCCExpressLite.Installer.csproj"

foreach ($runtime in $Runtimes) {
  $target = Join-Path $OutputDirectory $runtime
  dotnet publish $project -c $Configuration -r $runtime --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:EnableCompressionInSingleFile=true -p:DebugSymbols=false -p:DebugType=None -o $target
  if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed for $runtime" }
  Get-ChildItem -LiteralPath $target -Filter '*.pdb' -File | Remove-Item -Force
}

Write-Host "Installers published in $OutputDirectory"
