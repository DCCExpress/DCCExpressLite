#pragma once

#include <Arduino.h>
#include <Stream.h>

class HTTPSerialWrapper : public Stream {
public:
  explicit HTTPSerialWrapper(Stream* baseStream);

  size_t write(uint8_t b) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;

  void begin(unsigned long baud = 115200); // csak proxy
  bool popLine(String &line);
  uint32_t droppedLines() const { return _droppedLines; }

   
  operator bool() const;


private:
  static const uint8_t LINE_QUEUE_SIZE = 8;
  static const size_t MAX_LINE_LENGTH = 512;
  Stream* _base;
  String _buffer;
  String _lines[LINE_QUEUE_SIZE];
  uint8_t _lineHead = 0;
  uint8_t _lineTail = 0;
  uint8_t _lineCount = 0;
  uint32_t _droppedLines = 0;

  void finishLine();
};
