#include "DCCExpressLite.h"

#include "DCCEX.h"
#include "HTTPServer.h"
#include "I2CManager.h"
#include "NetworkSettings.h"

bool DCCExpressLite::started = false;

namespace
{
const char *deviceStateName(IODevice::DeviceStateEnum state)
{
  switch (state)
  {
    case IODevice::DEVSTATE_DORMANT: return "dormant";
    case IODevice::DEVSTATE_PROBING: return "probing";
    case IODevice::DEVSTATE_INITIALISING: return "initialising";
    case IODevice::DEVSTATE_NORMAL: return "online";
    case IODevice::DEVSTATE_SCANNING: return "scanning";
    case IODevice::DEVSTATE_FAILED: return "offline";
    default: return "unknown";
  }
}

const char *configuredDeviceType(VPIN firstVpin, int pinCount, uint8_t address)
{
  if (address == 0 && firstVpin == 2) return "ESP32 GPIO";
  if (pinCount == 16 && ((firstVpin == 100 && address == 0x40) || (firstVpin == 116 && address == 0x41)))
    return "PCA9685 PWM / servo";
  if (pinCount == 16 && ((firstVpin == 164 && address == 0x20) || (firstVpin == 180 && address == 0x21)))
    return "MCP23017 GPIO expander";
  if (address >= 0x20 && address <= 0x27) return "I2C GPIO expander";
  if (address >= 0x40 && address <= 0x47) return "I2C PWM / servo device";
  if (address >= 0x48 && address <= 0x4f) return "I2C analogue / UART device";
  return address ? "Configured I2C HAL device" : "Configured HAL device";
}
}

void DCCExpressLite::begin()
{
  if (started) return;
  started = true;

  NetworkSettings::begin();
  if (NetworkSettings::hasStationConfig())
  {
    DIAG(F("DCCExpressLite: using saved WiFi network %s"), NetworkSettings::ssid().c_str());
    WifiESP::setup(NetworkSettings::ssid().c_str(), NetworkSettings::password().c_str(), WIFI_HOSTNAME, IP_PORT, WIFI_CHANNEL, false);
  }
  else
  {
    // Empty credentials make upstream WifiESP create its DCCEX_* / PASS_*
    // setup access point and show both values on the EX-CSB1 display.
    WifiESP::setup("", "", WIFI_HOSTNAME, IP_PORT, WIFI_CHANNEL, false);
  }

  setupHTTPServer();
  UserAddin::create(DCCExpressLite::loop, 1);
}

void DCCExpressLite::loop()
{
  if (!started) return;
  // WifiESP::loop() remains owned by the unmodified upstream main loop.
  NetworkSettings::loop();
  loopHTTPServer();
}

void DCCExpressLite::appendDeviceJson(String &json)
{
  json += '[';
  bool first = true;
  for (IODevice *device = IODevice::_firstDevice; device; device = device->_nextDevice)
  {
    // UserAddin supplies the Lite loop but does not represent hardware.
    if (device->_nPins <= 0) continue;
    if (!first) json += ',';
    first = false;

    const uint8_t address = device->_I2CAddress;
    json += "{\"type\":\"";
    json += configuredDeviceType(device->_firstVpin, device->_nPins, address);
    json += "\",\"bus\":\"";
    json += address ? "I2C" : "GPIO";
    json += "\",\"address\":";
    json += address ? String(address) : "null";
    json += ",\"addressHex\":";
    if (address)
    {
      json += '\"';
      json += device->_I2CAddress.toString();
      json += '\"';
    }
    else
    {
      json += "null";
    }
    json += ",\"firstVpin\":" + String(device->_firstVpin);
    json += ",\"lastVpin\":" + String(device->_firstVpin + device->_nPins - 1);
    json += ",\"pinCount\":" + String(device->_nPins);
    json += ",\"state\":\"";
    json += deviceStateName(device->_deviceState);
    json += "\",\"online\":";
    json += device->_deviceState != IODevice::DEVSTATE_FAILED ? "true" : "false";
    json += '}';
  }
  json += ']';
}

int DCCExpressLite::getMainCurrentmA()
{
  int total = 0;
  for (byte trackNumber = 0; trackNumber <= TrackManager::lastTrack; ++trackNumber)
  {
    MotorDriver *driver = TrackManager::track[trackNumber];
    if (!driver || !(driver->getMode() & TRACK_MODE_MAIN)) continue;
    int raw = driver->getCurrentRaw(false);
    if (raw < 0) raw = -raw;
    total += driver->raw2mA(raw);
  }
  return total;
}

int DCCExpressLite::getProgCurrentmA()
{
  for (byte trackNumber = 0; trackNumber <= TrackManager::lastTrack; ++trackNumber)
  {
    MotorDriver *driver = TrackManager::track[trackNumber];
    if (!driver || !(driver->getMode() & TRACK_MODE_PROG)) continue;
    int raw = driver->getCurrentRaw(false);
    if (raw < 0) raw = -raw;
    return driver->raw2mA(raw);
  }
  return 0;
}

// Generic weak hooks declared by the two small upstream integration patches.
bool myRawCommand(Print *stream, const char *command)
{
  return NetworkSettings::handleSerialCommand(stream, command);
}

void myCommandBroadcast(const char *command)
{
  if (command) sendFormattedInfo(String(command));
}
