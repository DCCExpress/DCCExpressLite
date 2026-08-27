 $env:Path += ";C:\Users\junge\.platformio\penv\Scripts"    

platformio.exe run -e ESP32 -t upload       
platformio.exe run -e ESP32 -t uploadfs     

git add .
git commit -m "Prepare v0.1.0-alpha.1"
git push     

git tag v0.1.0-alpha.1
git push origin v0.1.0-alpha.1
