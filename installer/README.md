# DCCExpressLite Installer

Cross-platform graphical EX-CSB1 firmware installer built with C#/.NET 8 and Avalonia UI.

## Implemented

- Windows, macOS Intel/Apple Silicon and Linux desktop UI from one project.
- Native serial-port discovery without requiring PlatformIO.
- Remote GitHub or local JSON release manifests.
- Release ZIPs include their matching manifest, which the installer loads automatically on startup.
- Firmware/tool download and SHA-256 verification.
- ESP32 flashing through the official standalone Espressif `esptool` binary.
- Firmware/system and LittleFS selection.
- Layout and locomotive backup before LittleFS replacement, followed by automatic restore.
- Optional starter package with a sample layout, locomotives and locomotive images.
- Progress reporting, cancellation and full installation log.

## Run for development

```powershell
dotnet restore DCCExpressLite.Installer/DCCExpressLite.Installer.csproj --configfile NuGet.Config
dotnet run --project DCCExpressLite.Installer/DCCExpressLite.Installer.csproj
```

## Firmware release manifest

See `firmware-manifest.example.json`. Image and default-data URLs may be absolute GitHub Release URLs or relative to the manifest. Every release publishes the five ESP32 images, `default-data.zip`, and the manifest.

Generate a release package after the PlatformIO firmware and filesystem builds:

```powershell
./tools/New-FirmwareManifest.ps1 -Version 1.0.0 -BaseUrl https://github.com/OWNER/REPO/releases/download/v1.0.0
```

## Publish desktop applications

```powershell
./tools/Publish-Installers.ps1
```

This creates self-contained applications for `win-x64`, `osx-x64`, `osx-arm64`, `linux-x64`, and `linux-arm64`. macOS signing/notarization still requires Apple credentials and a Mac build/signing job. Linux users may need serial-port permission through the distribution's `dialout` or equivalent group.

For a published Windows release, download `DCCExpressLite-Installer-win-x64.zip`, extract it, and run `DCCExpressLite.Installer.exe`. The bundled manifest points to the firmware assets in that exact GitHub release, so no manifest URL needs to be entered manually.
