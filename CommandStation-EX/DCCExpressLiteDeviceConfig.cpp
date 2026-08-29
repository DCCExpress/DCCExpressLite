#include "DCCExpressLiteDeviceConfig.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "DIAG.h"
#include "IODevice.h"
#include "IO_MCP23017.h"
#include "IO_PCF8574.h"

namespace
{
constexpr const char *BACKUP_PATH = "/devices.json.bak";
constexpr uint8_t MAX_DEVICES = 16;
bool filesystemMounted = false;

const char DEFAULT_CONFIGURATION[] = R"json({
  "version": 1,
  "devices": [
    {"id":"pca9685-40","name":"PCA9685 0x40","type":"pca9685","enabled":true,"address":64,"firstVpin":100,"pinCount":16,"frequency":50},
    {"id":"pca9685-41","name":"PCA9685 0x41","type":"pca9685","enabled":true,"address":65,"firstVpin":116,"pinCount":16,"frequency":50},
    {"id":"mcp23017-20","name":"MCP23017 0x20","type":"mcp23017","enabled":true,"address":32,"firstVpin":164,"pinCount":16,"interruptPin":null},
    {"id":"mcp23017-21","name":"MCP23017 0x21","type":"mcp23017","enabled":true,"address":33,"firstVpin":180,"pinCount":16,"interruptPin":null}
  ]
})json";

struct ValidatedDevice
{
  const char *id = nullptr;
  const char *type = nullptr;
  bool enabled = true;
  uint8_t address = 0;
  VPIN firstVpin = 0;
  uint8_t pinCount = 0;
  uint16_t frequency = 50;
  int interruptPin = -1;
};

bool rangesOverlap(const ValidatedDevice &left, const ValidatedDevice &right)
{
  const uint32_t leftLast = static_cast<uint32_t>(left.firstVpin) + left.pinCount - 1;
  const uint32_t rightLast = static_cast<uint32_t>(right.firstVpin) + right.pinCount - 1;
  return left.firstVpin <= rightLast && right.firstVpin <= leftLast;
}

bool validateDocument(JsonDocument &document, ValidatedDevice *validated, uint8_t &count, String &error)
{
  if (!document.is<JsonObject>())
  {
    error = "Configuration root must be an object.";
    return false;
  }

  JsonArrayConst devices = document["devices"].as<JsonArrayConst>();
  if (devices.isNull())
  {
    error = "Configuration must contain a devices array.";
    return false;
  }
  if (devices.size() > MAX_DEVICES)
  {
    error = "At most 16 external devices can be configured.";
    return false;
  }

  count = 0;
  for (JsonObjectConst item : devices)
  {
    ValidatedDevice device;
    device.id = item["id"] | "";
    const char *name = item["name"] | "";
    device.type = item["type"] | "";
    device.enabled = item["enabled"] | true;
    const int address = item["address"] | -1;
    const int firstVpin = item["firstVpin"] | -1;
    const int pinCount = item["pinCount"] | -1;

    if (!device.id[0] || strlen(device.id) > 48)
    {
      error = "Every device needs a unique ID of at most 48 characters.";
      return false;
    }
    if (!name[0] || strlen(name) > 64)
    {
      error = "Every device needs a name of at most 64 characters.";
      return false;
    }
    for (uint8_t previous = 0; previous < count; ++previous)
    {
      if (!strcmp(validated[previous].id, device.id))
      {
        error = "Device IDs must be unique.";
        return false;
      }
    }

    if (!strcmp(device.type, "pca9685"))
    {
      device.pinCount = 16;
      device.frequency = item["frequency"] | 50;
      if (address < 0x40 || address > 0x7D || pinCount != 16 || device.frequency < 24 || device.frequency > 1526)
      {
        error = "Invalid PCA9685 address, channel count or frequency.";
        return false;
      }
    }
    else if (!strcmp(device.type, "mcp23017"))
    {
      device.pinCount = 16;
      if (address < 0x20 || address > 0x27 || pinCount != 16)
      {
        error = "Invalid MCP23017 address or channel count.";
        return false;
      }
    }
    else if (!strcmp(device.type, "pcf8574"))
    {
      device.pinCount = 8;
      if (address < 0x20 || address > 0x27 || pinCount != 8)
      {
        error = "Invalid PCF8574 address or channel count.";
        return false;
      }
    }
    else
    {
      error = "Unsupported device type.";
      return false;
    }

    if (firstVpin < 40 || static_cast<uint32_t>(firstVpin) + device.pinCount - 1 > 32767)
    {
      error = "Device VPIN range must be between 40 and 32767.";
      return false;
    }

    if (!item["interruptPin"].isNull())
    {
      device.interruptPin = item["interruptPin"].as<int>();
      if (device.interruptPin < 0 || device.interruptPin > 39)
      {
        error = "Interrupt GPIO must be between 0 and 39.";
        return false;
      }
    }

    device.address = static_cast<uint8_t>(address);
    device.firstVpin = static_cast<VPIN>(firstVpin);

    if (device.enabled)
    {
      for (uint8_t previous = 0; previous < count; ++previous)
      {
        if (!validated[previous].enabled)
          continue;
        if (validated[previous].address == device.address)
        {
          error = "Enabled devices cannot share an I2C address.";
          return false;
        }
        if (rangesOverlap(validated[previous], device))
        {
          error = "Enabled device VPIN ranges cannot overlap.";
          return false;
        }
      }
    }

    validated[count++] = device;
  }
  return true;
}

bool loadAndValidate(const char *path, JsonDocument &document, ValidatedDevice *devices, uint8_t &count, String &error)
{
  File file = LittleFS.open(path, "r");
  if (!file)
  {
    error = "Could not open device configuration.";
    return false;
  }
  if (file.size() > DCCExpressLiteDeviceConfig::MAX_CONFIG_BYTES)
  {
    file.close();
    error = "Device configuration is too large.";
    return false;
  }
  const DeserializationError jsonError = deserializeJson(document, file);
  file.close();
  if (jsonError)
  {
    error = "Device configuration contains invalid JSON.";
    return false;
  }
  return validateDocument(document, devices, count, error);
}
}

bool DCCExpressLiteDeviceConfig::ensureFilesystem()
{
  if (filesystemMounted)
    return true;
  filesystemMounted = LittleFS.begin(true);
  return filesystemMounted;
}

bool DCCExpressLiteDeviceConfig::ensureDefaultConfiguration()
{
  if (LittleFS.exists(CONFIG_PATH))
    return true;
  File file = LittleFS.open(CONFIG_PATH, "w");
  if (!file)
    return false;
  const size_t expected = strlen(DEFAULT_CONFIGURATION);
  const bool written = file.write(reinterpret_cast<const uint8_t *>(DEFAULT_CONFIGURATION), expected) == expected;
  file.close();
  return written;
}

bool DCCExpressLiteDeviceConfig::setupHal()
{
  if (!ensureFilesystem() || !ensureDefaultConfiguration())
  {
    DIAG(F("DCCExpressLite: device configuration unavailable; using upstream HAL defaults"));
    return false;
  }

  JsonDocument document;
  ValidatedDevice devices[MAX_DEVICES];
  uint8_t count = 0;
  String error;
  if (!loadAndValidate(CONFIG_PATH, document, devices, count, error))
  {
    DIAG(F("DCCExpressLite: invalid devices.json: %s; using upstream HAL defaults"), error.c_str());
    return false;
  }

  for (uint8_t index = 0; index < count; ++index)
  {
    const ValidatedDevice &device = devices[index];
    if (!device.enabled)
      continue;
    const I2CAddress address(device.address);
    if (!strcmp(device.type, "pca9685"))
      PCA9685::create(device.firstVpin, device.pinCount, address, device.frequency);
    else if (!strcmp(device.type, "mcp23017"))
      MCP23017::create(device.firstVpin, device.pinCount, address, device.interruptPin);
    else if (!strcmp(device.type, "pcf8574"))
      PCF8574::create(device.firstVpin, device.pinCount, address, device.interruptPin);
  }

  DIAG(F("DCCExpressLite: loaded %d configured HAL devices"), count);
  return true;
}

bool DCCExpressLiteDeviceConfig::validateAndCommit(const char *temporaryPath, String &error)
{
  JsonDocument document;
  ValidatedDevice devices[MAX_DEVICES];
  uint8_t count = 0;
  if (!loadAndValidate(temporaryPath, document, devices, count, error))
    return false;

  LittleFS.remove(BACKUP_PATH);
  if (LittleFS.exists(CONFIG_PATH) && !LittleFS.rename(CONFIG_PATH, BACKUP_PATH))
  {
    error = "Could not create configuration backup.";
    return false;
  }
  if (!LittleFS.rename(temporaryPath, CONFIG_PATH))
  {
    if (LittleFS.exists(BACKUP_PATH))
      LittleFS.rename(BACKUP_PATH, CONFIG_PATH);
    error = "Could not commit device configuration.";
    return false;
  }
  LittleFS.remove(BACKUP_PATH);
  return true;
}

bool dccExpressLiteHalSetup()
{
  return DCCExpressLiteDeviceConfig::setupHal();
}
