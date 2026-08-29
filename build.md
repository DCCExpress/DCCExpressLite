# If PlatformIO is not already available on PATH, add your own installation:
$env:Path += ";<your-platformio-cli-directory>"

pio run -e ESP32 -t upload
pio run -e ESP32 -t uploadfs

git add .
git commit -m "Prepare v0.1.0-alpha.3"
git push     

git tag v0.1.0-alpha.3
git push origin v0.1.0-alpha.3
