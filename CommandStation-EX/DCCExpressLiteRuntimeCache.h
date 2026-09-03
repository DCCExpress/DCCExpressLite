#pragma once

#include <Arduino.h>

struct AccessoryStateEntry
{
  uint16_t address = 0;
  int8_t state = -1;
};

class AccessoryStateCache
{
public:
  AccessoryStateCache() = default;
  ~AccessoryStateCache();

  AccessoryStateCache(const AccessoryStateCache &) = delete;
  AccessoryStateCache &operator=(const AccessoryStateCache &) = delete;

  int8_t get(uint16_t address) const;
  bool set(uint16_t address, bool state);
  void clear();

  size_t size() const { return count_; }
  const AccessoryStateEntry *at(size_t index) const;

private:
  AccessoryStateEntry *entries_ = nullptr;
  size_t count_ = 0;
  size_t capacity_ = 0;

  bool ensureCapacity(size_t needed);
};

struct ConfiguredLoco
{
  uint16_t address = 0;
  bool inverted = false;
};

class LocoConfigCache
{
public:
  static constexpr size_t MAX_ENTRIES = 32;

  LocoConfigCache() = default;
  ~LocoConfigCache();

  LocoConfigCache(const LocoConfigCache &) = delete;
  LocoConfigCache &operator=(const LocoConfigCache &) = delete;

  void clear();
  bool set(uint16_t address, bool inverted);
  bool isInverted(uint16_t address) const;
  size_t size() const { return count_; }

private:
  ConfiguredLoco *entries_ = nullptr;
  size_t count_ = 0;
  size_t capacity_ = 0;

  bool ensureCapacity(size_t needed);
};
