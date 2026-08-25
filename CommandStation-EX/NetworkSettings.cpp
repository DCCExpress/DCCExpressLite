#include "NetworkSettings.h"

#include <Preferences.h>

namespace
{
constexpr const char *PREFERENCES_NAMESPACE = "dcclite";
constexpr const char *SSID_KEY = "ssid";
constexpr const char *PASSWORD_KEY = "pass";
}

String NetworkSettings::savedSsid;
String NetworkSettings::savedPassword;

void NetworkSettings::begin()
{
  savedSsid = "";
  savedPassword = "";

  Preferences preferences;

  // Open read/write on first boot so the namespace is created without
  // emitting an NVS NOT_FOUND diagnostic. No values are written here.
  if (!preferences.begin(PREFERENCES_NAMESPACE, false))
    return;

  if (preferences.isKey(SSID_KEY))
    savedSsid = preferences.getString(SSID_KEY, "");

  if (preferences.isKey(PASSWORD_KEY))
    savedPassword = preferences.getString(PASSWORD_KEY, "");
  preferences.end();

  if (savedSsid.length() > 32 || savedPassword.length() < 8 || savedPassword.length() > 63)
  {
    savedSsid = "";
    savedPassword = "";
  }
}

bool NetworkSettings::hasStationConfig()
{
  return savedSsid.length() > 0 && savedPassword.length() >= 8;
}

const String &NetworkSettings::ssid()
{
  return savedSsid;
}

const String &NetworkSettings::password()
{
  return savedPassword;
}

bool NetworkSettings::save(const String &ssid, const String &password, String &error)
{
  if (!ssid.length() || ssid.length() > 32)
  {
    error = "SSID must contain 1 to 32 characters.";
    return false;
  }

  if (password.length() < 8 || password.length() > 63)
  {
    error = "Password must contain 8 to 63 characters.";
    return false;
  }

  Preferences preferences;

  if (!preferences.begin(PREFERENCES_NAMESPACE, false))
  {
    error = "Could not open device settings storage.";
    return false;
  }

  const bool ssidSaved = preferences.putString(SSID_KEY, ssid) == ssid.length();
  const bool passwordSaved = preferences.putString(PASSWORD_KEY, password) == password.length();
  preferences.end();

  if (!ssidSaved || !passwordSaved)
  {
    error = "Could not save network settings.";
    return false;
  }

  savedSsid = ssid;
  savedPassword = password;
  error = "";
  return true;
}

bool NetworkSettings::clear(String &error)
{
  Preferences preferences;

  if (!preferences.begin(PREFERENCES_NAMESPACE, false))
  {
    error = "Could not open device settings storage.";
    return false;
  }

  const bool cleared = preferences.clear();
  preferences.end();

  if (!cleared)
  {
    error = "Could not clear network settings.";
    return false;
  }

  savedSsid = "";
  savedPassword = "";
  error = "";
  return true;
}
