#ifndef DCCEXPRESSLITE_DEVICE_CONFIG_H
#define DCCEXPRESSLITE_DEVICE_CONFIG_H

#include <Arduino.h>

class DCCExpressLiteDeviceConfig
{
public:
  static constexpr const char *CONFIG_PATH = "/devices.json";
  static constexpr const char *TEMP_PATH = "/devices.tmp";
  static constexpr size_t MAX_CONFIG_BYTES = 32768;

  static bool ensureFilesystem();
  static bool setupHal();
  static bool setupPins();
  static bool validateAndCommit(const char *temporaryPath, String &error);

private:
  static bool ensureDefaultConfiguration();
};

// Small optional hook called by the upstream IODevice startup sequence.
bool dccExpressLiteHalSetup();

#endif
