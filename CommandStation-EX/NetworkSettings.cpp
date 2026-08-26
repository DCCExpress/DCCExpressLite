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
uint32_t NetworkSettings::restartAtMs = 0;

namespace
{
void skipSpaces(const char *&cursor)
{
  while (*cursor == ' ' || *cursor == '\t')
    ++cursor;
}

bool readQuoted(const char *&cursor, String &value)
{
  skipSpaces(cursor);
  if (*cursor != '"')
    return false;

  ++cursor;
  value = "";
  while (*cursor && *cursor != '>' && *cursor != '"')
    value += *cursor++;

  if (*cursor != '"')
    return false;

  ++cursor;
  return true;
}

void sendReply(Print *stream, const __FlashStringHelper *message)
{
  if (stream)
  {
    stream->print(message);
    stream->print('\n');
  }
}
}

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

void NetworkSettings::loop()
{
  if (restartAtMs && static_cast<int32_t>(millis() - restartAtMs) >= 0)
    ESP.restart();
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

bool NetworkSettings::handleSerialCommand(Print *stream, const char *command)
{
  // Human-readable DCC-EX extension:
  //   <WIFI "Home SSID" "password">
  //   <WIFI?>
  //   <WIFI CLEAR>
  if (!command || strncmp(command, "WIFI", 4) != 0)
    return false;

  const char next = command[4];
  if (next && next != '>' && next != '?' && next != ' ' && next != '\t')
    return false;

  const char *cursor = command + 4;
  skipSpaces(cursor);

  if (*cursor == '?')
  {
    ++cursor;
    skipSpaces(cursor);
    if (*cursor && *cursor != '>')
      sendReply(stream, F("<WIFI ERROR SYNTAX>"));
    else if (hasStationConfig())
      sendReply(stream, F("<WIFI CONFIGURED 1>"));
    else
      sendReply(stream, F("<WIFI CONFIGURED 0>"));
    return true;
  }

  if (strncmp(cursor, "CLEAR", 5) == 0)
  {
    cursor += 5;
    skipSpaces(cursor);
    if (*cursor && *cursor != '>')
    {
      sendReply(stream, F("<WIFI ERROR SYNTAX>"));
      return true;
    }

    String error;
    if (!clear(error))
    {
      sendReply(stream, F("<WIFI ERROR STORAGE>"));
      return true;
    }

    sendReply(stream, F("<WIFI CLEARED RESTARTING>"));
    restartAtMs = millis() + 1000;
    return true;
  }

  String newSsid;
  String newPassword;
  if (!readQuoted(cursor, newSsid) || !readQuoted(cursor, newPassword))
  {
    sendReply(stream, F("<WIFI ERROR SYNTAX>"));
    return true;
  }

  skipSpaces(cursor);
  if (*cursor && *cursor != '>')
  {
    sendReply(stream, F("<WIFI ERROR SYNTAX>"));
    return true;
  }

  String error;
  if (!save(newSsid, newPassword, error))
  {
    if (newSsid.length() < 1 || newSsid.length() > 32)
      sendReply(stream, F("<WIFI ERROR SSID_LENGTH>"));
    else if (newPassword.length() < 8 || newPassword.length() > 63)
      sendReply(stream, F("<WIFI ERROR PASSWORD_LENGTH>"));
    else
      sendReply(stream, F("<WIFI ERROR STORAGE>"));
    return true;
  }

  sendReply(stream, F("<WIFI SAVED RESTARTING>"));
  restartAtMs = millis() + 1000;
  return true;
}
