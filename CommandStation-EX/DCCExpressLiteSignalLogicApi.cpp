#include "DCCExpressLiteSignalLogicApi.h"

#include <LittleFS.h>

#include "DCCExpressLiteSignalLogic.h"
#include "HTTPServer.h"

namespace
{
constexpr const char *RULES_PATH = "/signal-rules.jsonl";
constexpr const char *TEMP_PATH = "/signal-rules.jsonl.tmp";
constexpr const char *BACKUP_PATH = "/signal-rules.jsonl.bak";

struct UploadState
{
  bool ok = false;
};

void closeUploadFile(
  AsyncWebServerRequest *request)
{
  if (!request->_tempFile)
    return;

  request->_tempFile.flush();
  request->_tempFile.close();
}

bool commitRules()
{
  LittleFS.remove(BACKUP_PATH);

  const bool hadOriginal =
    LittleFS.exists(RULES_PATH);

  if (hadOriginal &&
      !LittleFS.rename(
        RULES_PATH,
        BACKUP_PATH))
  {
    LittleFS.remove(TEMP_PATH);
    return false;
  }

  if (!LittleFS.rename(
        TEMP_PATH,
        RULES_PATH))
  {
    if (hadOriginal &&
        LittleFS.exists(BACKUP_PATH))
      LittleFS.rename(
        BACKUP_PATH,
        RULES_PATH);

    LittleFS.remove(TEMP_PATH);
    return false;
  }

  if (DCCExpressLiteSignalLogic::reload())
  {
    LittleFS.remove(BACKUP_PATH);
    return true;
  }

  // Invalid upload: restore the last known-good rules.
  LittleFS.remove(RULES_PATH);

  if (hadOriginal &&
      LittleFS.exists(BACKUP_PATH))
  {
    LittleFS.rename(
      BACKUP_PATH,
      RULES_PATH);

    DCCExpressLiteSignalLogic::reload();
  }

  LittleFS.remove(TEMP_PATH);
  return false;
}

} // namespace

void DCCExpressLiteSignalLogicApi::begin()
{
  httpServer.on(
    "/api/signal-logic",
    HTTP_GET,
    [](AsyncWebServerRequest *request)
    {
      if (!LittleFS.exists(RULES_PATH))
      {
        request->send(
          404,
          "text/plain; charset=utf-8",
          "Signal logic rules not found.");
        return;
      }

      AsyncWebServerResponse *response =
        request->beginResponse(
          LittleFS,
          RULES_PATH,
          "application/x-ndjson; charset=utf-8",
          false);

      response->addHeader(
        "Cache-Control",
        "no-store");

      request->send(response);
    });

  httpServer.on(
    "/api/signal-logic",
    HTTP_POST,
    [](AsyncWebServerRequest *request)
    {
      closeUploadFile(request);

      UploadState *state =
        static_cast<UploadState *>(
          request->_tempObject);

      const bool written =
        state && state->ok;

      bool committed = false;

      if (written)
        committed = commitRules();
      else
        LittleFS.remove(TEMP_PATH);

      delete state;
      request->_tempObject = nullptr;

      request->send(
        committed ? 200 : 400,
        "application/json",
        committed
          ? "{\"ok\":true}"
          : "{\"ok\":false,\"message\":\"Invalid or incomplete signal logic rules. Previous rules were restored.\"}");
    },
    nullptr,
    [](AsyncWebServerRequest *request,
       uint8_t *data,
       size_t len,
       size_t index,
       size_t total)
    {
      if (index == 0)
      {
        closeUploadFile(request);

        delete static_cast<UploadState *>(
          request->_tempObject);

        request->_tempObject =
          new UploadState();

        LittleFS.remove(TEMP_PATH);

        request->_tempFile =
          LittleFS.open(
            TEMP_PATH,
            "w");

        UploadState *state =
          static_cast<UploadState *>(
            request->_tempObject);

        state->ok =
          static_cast<bool>(
            request->_tempFile);

        if (!state->ok)
          return;
      }

      UploadState *state =
        static_cast<UploadState *>(
          request->_tempObject);

      if (!state ||
          !state->ok)
        return;

      if (len &&
          request->_tempFile.write(
            data,
            len) != len)
      {
        state->ok = false;
      }

      if (index + len == total)
        closeUploadFile(request);
    });
}
