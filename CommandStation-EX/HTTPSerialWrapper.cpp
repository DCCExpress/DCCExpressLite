#include "HTTPSerialWrapper.h"

HTTPSerialWrapper::HTTPSerialWrapper(Stream* baseStream, AsyncWebSocket* ws)
  : _base(baseStream), _ws(ws), _buffer("") {}

size_t HTTPSerialWrapper::write(uint8_t b) {
  _base->write(b);
  if (b == '\n') {
    if (_ws) {
      //_ws->textAll(_buffer);
      String json = "{\"type\":\"rawInfo\",\"data\":{\"raw\":\"" + escapeJson(_buffer) + "\"}}";
      _ws->textAll(json);

    }
    _buffer = "";
  } else {
    _buffer += (char)b;
  }
  return 1;
}

size_t HTTPSerialWrapper::write(const uint8_t *buffer, size_t size) {
  size_t sent = 0;
  for (size_t i = 0; i < size; ++i) {
    sent += write(buffer[i]);
  }
  return sent;
}

int HTTPSerialWrapper::available() {
  return _base->available();
}

int HTTPSerialWrapper::read() {
  return _base->read();
}

int HTTPSerialWrapper::peek() {
  return _base->peek();
}

void HTTPSerialWrapper::flush() {
  _base->flush();
}

void HTTPSerialWrapper::begin(unsigned long baud) {
  if (reinterpret_cast<HardwareSerial*>(_base)) {
    reinterpret_cast<HardwareSerial*>(_base)->begin(baud);
  }
}

HTTPSerialWrapper::operator bool() const {
  return _base != nullptr;
}

String HTTPSerialWrapper::escapeJson(const String& input) {
  String output = "";
  for (unsigned int i = 0; i < input.length(); ++i) {
    char c = input.charAt(i);
    if (c == '\\') output += "\\\\";
    else if (c == '\"') output += "\\\"";
    else if (c == '\n') output += "\\n";
    else if (c == '\r') output += "\\r";
    else if (c == '\t') output += "\\t";
    else output += c;
  }
  return output;
}