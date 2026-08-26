@echo off
setlocal EnableExtensions
title DCCExpressLite Deploy

REM ============================================================
REM DCCExpressLite - local build + ESP32 deploy
REM Place this file in the repository root:
REM C:\ChatGPT\GitRepo\DCCExpressLite\deploy.bat
REM
REM Optional:
REM   deploy.bat COM5
REM If no COM port is supplied, PlatformIO auto-detects it.
REM ============================================================

set "PROJECT_DIR=%~dp0"
set "WEBUI_DIR=%PROJECT_DIR%web-ui"
set "PIO_ENV=ESP32"

REM PowerShell equivalent:
REM $env:Path += ";C:\Users\junge\.platformio\penv\Scripts"
set "PATH=%PATH%;C:\Users\junge\.platformio\penv\Scripts"

set "UPLOAD_PORT="
if not "%~1"=="" set "UPLOAD_PORT=--upload-port %~1"

echo.
echo ============================================================
echo   DCCExpressLite DEPLOY
echo ============================================================
echo Project: %PROJECT_DIR%
if not "%~1"=="" (
    echo Port:    %~1
) else (
    echo Port:    auto-detect
)
echo.

REM ------------------------------------------------------------
REM 0. Basic checks
REM ------------------------------------------------------------
where npm >nul 2>&1
if errorlevel 1 (
    echo [ERROR] npm not found in PATH.
    goto :fail
)

where platformio.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] platformio.exe not found.
    echo Expected:
    echo C:\Users\junge\.platformio\penv\Scripts\platformio.exe
    goto :fail
)

if not exist "%WEBUI_DIR%\package.json" (
    echo [ERROR] web-ui\package.json not found.
    echo Put deploy.bat into the DCCExpressLite repository root.
    goto :fail
)

if not exist "%PROJECT_DIR%platformio.ini" (
    echo [ERROR] platformio.ini not found.
    echo Put deploy.bat into the DCCExpressLite repository root.
    goto :fail
)

REM ------------------------------------------------------------
REM 1. Web UI dependencies
REM ------------------------------------------------------------
echo.
echo [1/6] Checking Web UI dependencies...
pushd "%WEBUI_DIR%"

if not exist "node_modules\" (
    echo node_modules missing - installing dependencies...

    if exist "package-lock.json" (
        call npm ci
    ) else (
        call npm install
    )

    if errorlevel 1 (
        popd
        echo [ERROR] npm dependency install failed.
        goto :fail
    )
) else (
    echo node_modules exists - skipping install.
)

REM ------------------------------------------------------------
REM 2. Build + embed Web UI into data\
REM ------------------------------------------------------------
echo.
echo [2/6] Building and embedding Web UI...
call npm run embed
if errorlevel 1 (
    popd
    echo [ERROR] Web UI build/embed failed.
    goto :fail
)

popd

REM ------------------------------------------------------------
REM 3. Build firmware
REM ------------------------------------------------------------
echo.
echo [3/6] Building firmware...
pushd "%PROJECT_DIR%"

pio run -e %PIO_ENV%
if errorlevel 1 (
    popd
    echo [ERROR] Firmware build failed.
    goto :fail
)

REM ------------------------------------------------------------
REM 4. Build LittleFS
REM ------------------------------------------------------------
echo.
echo [4/6] Building LittleFS image...

pio run -e %PIO_ENV% -t buildfs
if errorlevel 1 (
    popd
    echo [ERROR] LittleFS build failed.
    goto :fail
)

REM ------------------------------------------------------------
REM 5. Upload firmware
REM ------------------------------------------------------------
echo.
echo [5/6] Uploading firmware...

pio run -e %PIO_ENV% -t upload %UPLOAD_PORT%
if errorlevel 1 (
    popd
    echo [ERROR] Firmware upload failed.
    goto :fail
)

REM ------------------------------------------------------------
REM 6. Upload LittleFS
REM ------------------------------------------------------------
echo.
echo [6/6] Uploading LittleFS...

pio run -e %PIO_ENV% -t uploadfs %UPLOAD_PORT%
if errorlevel 1 (
    popd
    echo [ERROR] LittleFS upload failed.
    goto :fail
)

popd

echo.
echo ============================================================
echo   DEPLOY SUCCESSFUL
echo ============================================================
echo Web UI:   built + embedded
echo Firmware: built + uploaded
echo LittleFS: built + uploaded
echo.
echo The ESP32 may restart now.
echo.
pause
exit /b 0

:fail
echo.
echo ============================================================
echo   DEPLOY FAILED
echo ============================================================
echo Check the error above.
echo.
pause
exit /b 1
