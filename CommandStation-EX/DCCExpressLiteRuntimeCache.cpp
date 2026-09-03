#include "DCCExpressLiteRuntimeCache.h"

#include <cstdlib>

namespace
{
constexpr size_t ACCESSORY_GROWTH = 8;
constexpr size_t LOCO_CONFIG_GROWTH = 8;
}

AccessoryStateCache::~AccessoryStateCache()
{
  clear();
}

bool AccessoryStateCache::ensureCapacity(size_t needed)
{
  if (needed <= capacity_)
    return true;

  size_t nextCapacity =
    capacity_ ? capacity_ : ACCESSORY_GROWTH;

  while (nextCapacity < needed)
    nextCapacity += ACCESSORY_GROWTH;

  void *memory =
    realloc(
      entries_,
      nextCapacity * sizeof(AccessoryStateEntry));

  if (!memory)
    return false;

  entries_ =
    static_cast<AccessoryStateEntry *>(memory);

  capacity_ = nextCapacity;
  return true;
}

int8_t AccessoryStateCache::get(
  uint16_t id,
  uint8_t channel) const
{
  for (size_t i = 0; i < count_; ++i)
    if (entries_[i].id == id &&
        entries_[i].channel == channel)
      return entries_[i].state;

  return -1;
}

bool AccessoryStateCache::set(
  uint16_t id,
  uint8_t channel,
  bool state)
{
  if (!id)
    return false;

  for (size_t i = 0; i < count_; ++i)
  {
    if (entries_[i].id != id ||
        entries_[i].channel != channel)
      continue;

    entries_[i].state =
      state ? 1 : 0;

    return true;
  }

  if (!ensureCapacity(count_ + 1))
    return false;

  entries_[count_].id = id;
  entries_[count_].channel = channel;
  entries_[count_].state = state ? 1 : 0;
  ++count_;

  return true;
}

void AccessoryStateCache::remove(
  uint16_t id,
  uint8_t channel)
{
  for (size_t i = 0; i < count_; ++i)
  {
    if (entries_[i].id != id ||
        entries_[i].channel != channel)
      continue;

    for (size_t j = i + 1; j < count_; ++j)
      entries_[j - 1] = entries_[j];

    --count_;
    return;
  }
}

void AccessoryStateCache::clear()
{
  if (entries_)
  {
    free(entries_);
    entries_ = nullptr;
  }

  count_ = 0;
  capacity_ = 0;
}

const AccessoryStateEntry *
AccessoryStateCache::at(size_t index) const
{
  return index < count_
    ? &entries_[index]
    : nullptr;
}

LocoConfigCache::~LocoConfigCache()
{
  clear();
}

bool LocoConfigCache::ensureCapacity(size_t needed)
{
  if (needed <= capacity_)
    return true;

  if (needed > MAX_ENTRIES)
    return false;

  size_t nextCapacity =
    capacity_ ? capacity_ : LOCO_CONFIG_GROWTH;

  while (nextCapacity < needed &&
         nextCapacity < MAX_ENTRIES)
    nextCapacity += LOCO_CONFIG_GROWTH;

  if (nextCapacity > MAX_ENTRIES)
    nextCapacity = MAX_ENTRIES;

  void *memory =
    realloc(
      entries_,
      nextCapacity * sizeof(ConfiguredLoco));

  if (!memory)
    return false;

  entries_ =
    static_cast<ConfiguredLoco *>(memory);

  capacity_ = nextCapacity;
  return true;
}

void LocoConfigCache::clear()
{
  if (entries_)
  {
    free(entries_);
    entries_ = nullptr;
  }

  count_ = 0;
  capacity_ = 0;
}

bool LocoConfigCache::set(
  uint16_t address,
  bool inverted)
{
  if (!address)
    return false;

  for (size_t i = 0; i < count_; ++i)
  {
    if (entries_[i].address != address)
      continue;

    entries_[i].inverted = inverted;
    return true;
  }

  if (count_ >= MAX_ENTRIES ||
      !ensureCapacity(count_ + 1))
    return false;

  entries_[count_].address = address;
  entries_[count_].inverted = inverted;
  ++count_;

  return true;
}

bool LocoConfigCache::isInverted(
  uint16_t address) const
{
  for (size_t i = 0; i < count_; ++i)
    if (entries_[i].address == address)
      return entries_[i].inverted;

  return false;
}
