#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <ESPAsyncWebServer.h>

class HTTPSerialWrapper : public Stream {
public:
  HTTPSerialWrapper(Stream* baseStream, AsyncWebSocket* ws);

  size_t write(uint8_t b) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;

  void begin(unsigned long baud = 115200); // csak proxy

   
  operator bool() const;


private:
  Stream* _base;
  AsyncWebSocket* _ws;
  String _buffer; 
  String escapeJson(const String& input);  
};
