#include "HTTPSerialWrapper.h"

HTTPSerialWrapper::HTTPSerialWrapper(Stream* baseStream)
  : _base(baseStream), _buffer("")
{
  _buffer.reserve(MAX_LINE_LENGTH);
}

size_t HTTPSerialWrapper::write(uint8_t b) {
  _base->write(b);
  if (b == '\n') {
    finishLine();
  } else {
    if (_buffer.length() < MAX_LINE_LENGTH)
      _buffer += (char)b;
  }
  return 1;
}

void HTTPSerialWrapper::finishLine() {
  if (!_buffer.length()) return;
  if (_lineCount >= LINE_QUEUE_SIZE) {
    ++_droppedLines;
    _buffer = "";
    return;
  }
  _lines[_lineTail] = _buffer;
  _lineTail = (_lineTail + 1) % LINE_QUEUE_SIZE;
  ++_lineCount;
  _buffer = "";
}

bool HTTPSerialWrapper::popLine(String &line) {
  if (!_lineCount) return false;
  line = _lines[_lineHead];
  _lines[_lineHead] = "";
  _lineHead = (_lineHead + 1) % LINE_QUEUE_SIZE;
  --_lineCount;
  return true;
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
