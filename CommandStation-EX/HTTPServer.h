// HTTPServer.h

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

extern AsyncWebServer httpServer;
extern AsyncWebSocket ws;

void setupHTTPServer();
void loopHTTPServer();
void sendFormattedInfo(String s);


#endif
